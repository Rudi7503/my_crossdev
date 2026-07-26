#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include <exec/types.h>
#include <exec/memory.h>
#include <exec/interrupts.h>
#include <hardware/intbits.h>
#include <hardware/custom.h>
#include <proto/exec.h>
#include <dos/dos.h> 
#include <signal.h>

#define BUFFER_SECONDS 1

// ==============================================================================
// ISR (INTERRUPT SERVICE ROUTINE) VARIABLEN
// ==============================================================================
struct Task *main_task = NULL;
uint32_t audio_sig_mask = 0;
uint16_t aud_int_bit = 0;

// ==============================================================================
// DIE INTERRUPT-FUNKTION (Wird von der Hardware aufgerufen)
// ==============================================================================
#if defined(__VBCC__)
__saveds __interrupt void audio_isr(void)
#elif defined(__GNUC__)
void audio_isr(void)
#else
void audio_isr(void)
#endif
{
    // 1. Hardware Interrupt quittieren (in INTREQ)
    *((volatile uint16_t*)0xDFF09C) = aud_int_bit;
    
    // 2. Unserem schlafenden Haupt-Programm das Aufwach-Signal senden
    Signal(main_task, audio_sig_mask);
}

static inline int32_t clamp(int32_t val, int32_t min, int32_t max) {
    if (val > max) return max; if (val < min) return min; return val;
}

// ==============================================================================
// FIR & IIR POST-FILTER
// ==============================================================================
typedef struct {
    int16_t actual;
    int16_t prev1;
    int16_t prev2;
    int16_t prev3;
    int16_t prev4;
    int16_t prev5;
    int16_t prev6;
    float y_prev1; // WICHTIG: float für stabile IIR Rückkopplung!
    float y_prev2; // WICHTIG: float für stabile IIR Rückkopplung!
} PostFilterState;

PostFilterState filter_state[2] = { 
    {0, 0, 0, 0, 0, 0, 0, 0.0f, 0.0f}, 
    {0, 0, 0, 0, 0, 0, 0, 0.0f, 0.0f} 
};

// Vorberechnete Float-Koeffizienten (Werte / 32768.0) für IIR (Index 10-19)
static const float iir_coeffs_f[10][5] = {
    {7583.0f/32768.0f, 0.0f, -7583.0f/32768.0f, -44000.0f/32768.0f, 17600.0f/32768.0f},       // 10
    {5784.0f/32768.0f, 0.0f, -5784.0f/32768.0f, -46747.0f/32768.0f, 21201.0f/32768.0f},       // 11
    {1622.0f/32768.0f, 3244.0f/32768.0f, 1622.0f/32768.0f, -41929.0f/32768.0f, 15650.0f/32768.0f},     // 12
    {659.0f/32768.0f, 1317.0f/32768.0f, 659.0f/32768.0f, -51152.0f/32768.0f, 21018.0f/32768.0f},       // 13
    {1622.0f/32768.0f, 3244.0f/32768.0f, 1622.0f/32768.0f, -41929.0f/32768.0f, 15650.0f/32768.0f},     // 14
    {659.0f/32768.0f, 1317.0f/32768.0f, 659.0f/32768.0f, -51152.0f/32768.0f, 21018.0f/32768.0f},       // 15
    {31406.0f/32768.0f, -62813.0f/32768.0f, 31406.0f/32768.0f, -62762.0f/32768.0f, 30060.0f/32768.0f}, // 16
    {28180.0f/32768.0f, -56360.0f/32768.0f, 28180.0f/32768.0f, -55426.0f/32768.0f, 23478.0f/32768.0f}, // 17
    {26214.0f/32768.0f, 0.0f, -26214.0f/32768.0f, -49152.0f/32768.0f, 16384.0f/32768.0f},     // 18
    {32767.0f/32768.0f, -32767.0f/32768.0f, 0.0f, -32767.0f/32768.0f, 0.0f}          // 19
};


static inline void apply_postfilter(int16_t *pcm, uint32_t count, uint8_t channels, int filter_index, PostFilterState *state) {
    if (channels == 2) {
        for (uint32_t i = 0; i < count; i++) {
            state[0].prev6 = state[0].prev5;
            state[0].prev5 = state[0].prev4;
            state[0].prev4 = state[0].prev3;
            state[0].prev3 = state[0].prev2;
            state[0].prev2 = state[0].prev1;
            state[0].prev1 = state[0].actual;
            state[0].actual = pcm[i*2]; // Left
            
            state[1].prev6 = state[1].prev5;
            state[1].prev5 = state[1].prev4;
            state[1].prev4 = state[1].prev3;
            state[1].prev3 = state[1].prev2;
            state[1].prev2 = state[1].prev1;
            state[1].prev1 = state[1].actual;
            state[1].actual = pcm[i*2 + 1]; // Right    
            
            if (filter_index < 0 || filter_index > 19) {
                pcm[i*2]     = state[0].actual; // Bypass Left
                pcm[i*2 + 1] = state[1].actual; // Bypass Right
            } 
            else if (filter_index <= 9) {
                // --- FIR FILTER LOGIK ---
                if (filter_index == 0) {
                    pcm[i*2]     = (int16_t)clamp((int32_t)((state[0].actual+state[0].prev2)*0.125+state[0].prev1*0.625), -32768, 32767);
                    pcm[i*2 + 1] = (int16_t)clamp((int32_t)((state[1].actual+state[1].prev2)*0.125+state[1].prev1*0.625), -32768, 32767);
                } else if (filter_index == 1) {
                    pcm[i*2]     = (int16_t)clamp((int32_t)((state[0].actual+state[0].prev2)*0.125+state[0].prev1*0.75), -32768, 32767);
                    pcm[i*2 + 1] = (int16_t)clamp((int32_t)((state[1].actual+state[1].prev2)*0.125+state[1].prev1*0.75), -32768, 32767);
                } else if (filter_index == 2) {
                    pcm[i*2]     = (int16_t)clamp((int32_t)((state[0].actual+state[0].prev2)*0.125+state[0].prev1*0.875), -32768, 32767);
                    pcm[i*2 + 1] = (int16_t)clamp((int32_t)((state[1].actual+state[1].prev2)*0.125+state[1].prev1*0.875), -32768, 32767);
                } else if (filter_index == 3) {
                    pcm[i*2]     = (int16_t)clamp((int32_t)((state[0].actual+state[0].prev2)*0.25+state[0].prev1*0.5), -32768, 32767);
                    pcm[i*2 + 1] = (int16_t)clamp((int32_t)((state[1].actual+state[1].prev2)*0.25+state[1].prev1*0.5), -32768, 32767);
                } else if (filter_index == 4) {
                    pcm[i*2]     = (int16_t)clamp((int32_t)((state[0].actual+state[0].prev2)*0.25+state[0].prev1*0.625), -32768, 32767);
                    pcm[i*2 + 1] = (int16_t)clamp((int32_t)((state[1].actual+state[1].prev2)*0.25+state[1].prev1*0.625), -32768, 32767);
                } else if (filter_index == 5) {
                    pcm[i*2]     = (int16_t)clamp((int32_t)((state[0].actual+state[0].prev6)*0.0625+(state[0].prev1+state[0].prev5)*0.125+(state[0].prev2+state[0].prev4)*0.1875+state[0].prev3*0.250), -32768, 32767);
                    pcm[i*2 + 1] = (int16_t)clamp((int32_t)((state[1].actual+state[1].prev6)*0.0625+(state[1].prev1+state[1].prev5)*0.125+(state[1].prev2+state[1].prev4)*0.1875+state[1].prev3*0.250), -32768, 32767);
                } else if (filter_index == 6) {
                    pcm[i*2]     = (int16_t)clamp((int32_t)((state[0].actual+state[0].prev6)*0.125+(state[0].prev1+state[0].prev5)*0.125+(state[0].prev2+state[0].prev4)*0.125+state[0].prev3*0.250), -32768, 32767);
                    pcm[i*2 + 1] = (int16_t)clamp((int32_t)((state[1].actual+state[1].prev6)*0.125+(state[1].prev1+state[1].prev5)*0.125+(state[1].prev2+state[1].prev4)*0.125+state[1].prev3*0.250), -32768, 32767);
                } else if (filter_index == 7) {
                    pcm[i*2]     = (int16_t)clamp((int32_t)((state[0].actual+state[0].prev6)*0.1875+(state[0].prev1+state[0].prev5)*0.125+(state[0].prev2+state[0].prev4)*0.0625+state[0].prev3*0.25), -32768, 32767);
                    pcm[i*2 + 1] = (int16_t)clamp((int32_t)((state[1].actual+state[1].prev6)*0.1875+(state[1].prev1+state[1].prev5)*0.125+(state[1].prev2+state[1].prev4)*0.0625+state[1].prev3*0.25), -32768, 32767);
                } else if (filter_index == 8) {
                    pcm[i*2]     = (int16_t)clamp((int32_t)((state[0].actual+state[0].prev6)*0.2500+(state[0].prev1+state[0].prev5)*0.125+(state[0].prev2+state[0].prev4)*0.0+(state[0].prev3*0.25)), -32768, 32767);
                    pcm[i*2 + 1] = (int16_t)clamp((int32_t)((state[1].actual+state[1].prev6)*0.2500+(state[1].prev1+state[1].prev5)*0.125+(state[1].prev2+state[1].prev4)*0.0+(state[1].prev3*0.25)), -32768, 32767);
                } else if (filter_index == 9) {
                    pcm[i*2]     = (int16_t)clamp((int32_t)((state[0].actual+state[0].prev6)*0.3125+(state[0].prev1+state[0].prev5)*0.625+(state[0].prev2+state[0].prev4)*0.0+(state[0].prev3*0.25)), -32768, 32767);
                    pcm[i*2 + 1] = (int16_t)clamp((int32_t)((state[1].actual+state[1].prev6)*0.3125+(state[1].prev1+state[1].prev5)*0.625+(state[1].prev2+state[1].prev4)*0.0+(state[1].prev3*0.25)), -32768, 32767);
                }
            }
            else if (filter_index >= 10 && filter_index <= 19) {
                // --- IIR BIQUAD LOGIK ---
                int idx = filter_index - 10;
                float b0 = iir_coeffs_f[idx][0], b1 = iir_coeffs_f[idx][1], b2 = iir_coeffs_f[idx][2];
                float a1 = iir_coeffs_f[idx][3], a2 = iir_coeffs_f[idx][4];

                // Left
                float xL = (float)state[0].actual;
                float xL1 = (float)state[0].prev1;
                float xL2 = (float)state[0].prev2;
                float yL = b0 * xL + b1 * xL1 + b2 * xL2 - a1 * state[0].y_prev1 - a2 * state[0].y_prev2;
                state[0].y_prev2 = state[0].y_prev1;
                state[0].y_prev1 = yL;
                pcm[i*2] = (int16_t)clamp((int32_t)yL, -32768, 32767);

                // Right
                float xR = (float)state[1].actual;
                float xR1 = (float)state[1].prev1;
                float xR2 = (float)state[1].prev2;
                float yR = b0 * xR + b1 * xR1 + b2 * xR2 - a1 * state[1].y_prev1 - a2 * state[1].y_prev2;
                state[1].y_prev2 = state[1].y_prev1;
                state[1].y_prev1 = yR;
                pcm[i*2 + 1] = (int16_t)clamp((int32_t)yR, -32768, 32767);
            }
        }
    } else if (channels == 1) {
        for (uint32_t i = 0; i < count; i++) {
            state[0].prev6 = state[0].prev5;
            state[0].prev5 = state[0].prev4;
            state[0].prev4 = state[0].prev3;
            state[0].prev3 = state[0].prev2;
            state[0].prev2 = state[0].prev1;
            state[0].prev1 = state[0].actual;
            state[0].actual = pcm[i]; // Mono
            
            if (filter_index < 0 || filter_index > 19) {
                pcm[i] = state[0].actual; // Bypass Mono
            } 
            else if (filter_index <= 9) {
                // --- FIR FILTER LOGIK ---
                if (filter_index == 0) {
                    pcm[i] = (int16_t)clamp((int32_t)((state[0].actual+state[0].prev2)*0.125+state[0].prev1*0.625), -32768, 32767);
                } else if (filter_index == 1) {
                    pcm[i] = (int16_t)clamp((int32_t)((state[0].actual+state[0].prev2)*0.125+state[0].prev1*0.75), -32768, 32767);
                } else if (filter_index == 2) {
                    pcm[i] = (int16_t)clamp((int32_t)((state[0].actual+state[0].prev2)*0.125+state[0].prev1*0.875), -32768, 32767);
                }
                else if (filter_index == 3) {
                    pcm[i] = (int16_t)clamp((int32_t)((state[0].actual+state[0].prev2)*0.25+state[0].prev1*0.5), -32768, 32767);
                }
                else if (filter_index == 4) {
                    pcm[i] = (int16_t)clamp((int32_t)((state[0].actual+state[0].prev2)*0.25+state[0].prev1*0.625), -32768, 32767);
                }
                else if (filter_index == 5) {
                    pcm[i] = (int16_t)clamp((int32_t)((state[0].actual+state[0].prev6)*0.0625+(state[0].prev1+state[0].prev5)*0.125+(state[0].prev2+state[0].prev4)*0.1875+state[0].prev3*0.250), -32768, 32767);
                }
                else if (filter_index == 6) {
                    pcm[i] = (int16_t)clamp((int32_t)((state[0].actual+state[0].prev6)*0.125+(state[0].prev1+state[0].prev5)*0.125+(state[0].prev2+state[0].prev4)*0.125+state[0].prev3*0.250), -32768, 32767);
                }
                else if (filter_index == 7) {
                    pcm[i] = (int16_t)clamp((int32_t)((state[0].actual+state[0].prev6)*0.1875+(state[0].prev1+state[0].prev5)*0.125+(state[0].prev2+state[0].prev4)*0.0625+state[0].prev3*0.25), -32768, 32767);
                }
                else if (filter_index == 8) {
                    pcm[i] = (int16_t)clamp((int32_t)((state[0].actual+state[0].prev6)*0.2500+(state[0].prev1+state[0].prev5)*0.125+(state[0].prev2+state[0].prev4)*0.0+(state[0].prev3*0.25)), -32768, 32767);
                }
                else if (filter_index == 9) {
                    pcm[i] = (int16_t)clamp((int32_t)((state[0].actual+state[0].prev6)*0.3125+(state[0].prev1+state[0].prev5)*0.625+(state[0].prev2+state[0].prev4)*0.0+(state[0].prev3*0.25)), -32768, 32767);
                }
            }
            else if (filter_index >= 10 && filter_index <= 19) {
                // --- IIR BIQUAD LOGIK ---
                int idx = filter_index - 10;
                float b0 = iir_coeffs_f[idx][0], b1 = iir_coeffs_f[idx][1], b2 = iir_coeffs_f[idx][2];
                float a1 = iir_coeffs_f[idx][3], a2 = iir_coeffs_f[idx][4];

                float xM = (float)state[0].actual;
                float xM1 = (float)state[0].prev1;
                float xM2 = (float)state[0].prev2;
                float yM = b0 * xM + b1 * xM1 + b2 * xM2 - a1 * state[0].y_prev1 - a2 * state[0].y_prev2;
                
                state[0].y_prev2 = state[0].y_prev1;
                state[0].y_prev1 = yM;
                pcm[i] = (int16_t)clamp((int32_t)yM, -32768, 32767);
            }
        }
    }
}

// ==============================================================================
// NAME-HELPER FÜR DIE KONSOLE
// ==============================================================================
const char* get_filter_name(int idx) {
    if (idx == -1) return "Bypass";
    if (idx == 0) return "FIR: 1/8,3/4,1/8";
    if (idx == 1) return "FIR: 1/8,6/8,1/8";
    if (idx == 2) return "FIR: 1/8,7/8,1/8";
    if (idx == 3) return "FIR: 2/8,4/8,2/8";
    if (idx == 4) return "FIR: 2/8,5/8,1/8";
    if (idx == 5) return "FIR: 1/16,3/16,3/16,1/4,3/16,3/16,1/16";
    if (idx == 6) return "FIR: 1/8,1/8,1/8,1/4,1/8,1/8,1/8";
    if (idx == 7) return "FIR: 3/16,2/16,1/16,1/4,1/16,2/16,3/16";
    if (idx == 8) return "FIR: 1/4,1/8,0,1/4,0,1/8,1/4";
    if (idx == 9) return "FIR: 5/16,5/16,0,1/4,0,-5/16,-5/16";
    if (idx == 10) return "IIR: Sprache Bandpass mild";
    if (idx == 11) return "IIR: Sprache Bandpass stark";
    if (idx == 12) return "IIR: Sprache Tiefpass mild";
    if (idx == 13) return "IIR: Sprache Tiefpass stark";
    if (idx == 14) return "IIR: Musik Tiefpass sanft";
    if (idx == 15) return "IIR: Musik Tiefpass moderat";
    if (idx == 16) return "IIR: Musik Bandpass Subsonic";
    if (idx == 17) return "IIR: Musik Hoehen sanft";
    if (idx == 18) return "IIR: Musik Praesenz";
    if (idx == 19) return "IIR: Musik Bass-Boost";
    return "Unknown";
}

// ==============================================================================
// HIGH-SPEED MEMORY BITSTREAM READER
// ==============================================================================
typedef struct {
    uint8_t *data;
    uint32_t pos;
    uint32_t max_pos;
    uint8_t pool;
    int bits_left;
} MemBitStream;

static inline uint8_t read_bits_mem(MemBitStream *bs, int num_bits) {
    uint8_t result = 0;
    for (int i = 0; i < num_bits; i++) {
        if (bs->bits_left == 0) {
            if (bs->pos >= bs->max_pos) return result;
            bs->pool = bs->data[bs->pos++];
            bs->bits_left = 8;
        }
        result |= ((bs->pool & 1) << i);
        bs->pool >>= 1;
        bs->bits_left--;
    }
    return result;
}

// ==============================================================================
// ADPCM DECODER CORE
// ==============================================================================
static const int step_table[89] = {
    7,8,9,10,11,12,13,14,16,17,19,21,23,25,28,31,34,37,41,45,50,55,60,66,73,80,88,97,107,
    118,130,143,157,173,190,209,230,253,279,307,337,371,408,449,494,544,598,658,724,796,
    876,963,1060,1166,1282,1411,1552,1707,1878,2066,2272,2499,2749,3024,3327,3660,4026,
    4428,4871,5358,5894,6484,7132,7845,8630,9493,10442,11487,12635,13899,15289,16818,
    18500,20350,22385,24623,27086,29794,32767
};
static const int index_table_3[4] = {-1, -1, 1, 2};
static const int index_table_4[8] = {-1, -1, -1, -1, 2, 4, 6, 8};
static const int index_table_5[16] = {-1, -1, -1, -1, -1, -1, -1, -1, 1, 2, 4, 6, 8, 10, 13, 16};

typedef struct { int32_t pcm; int16_t index; } ADPCM_Channel;

static inline int16_t decode_nibble(ADPCM_Channel *chan, uint8_t nibble, uint8_t bps) {
    int32_t step = step_table[chan->index];
    int32_t delta;

    if (bps == 2) {
        delta = step >> 1; if (nibble & 1) delta += step;
        if (nibble & 2) chan->pcm -= delta; else chan->pcm += delta;
        chan->index += (nibble & 1) * 3 - 1;
    } else if (bps == 3) {
        delta = step >> 2; if (nibble & 1) delta += (step >> 1); if (nibble & 2) delta += step;
        if (nibble & 4) chan->pcm -= delta; else chan->pcm += delta;
        chan->index += index_table_3[nibble & 3];
    } else if (bps == 4) {
        delta = step >> 3; if (nibble & 1) delta += (step >> 2); if (nibble & 2) delta += (step >> 1); if (nibble & 4) delta += step;
        if (nibble & 8) chan->pcm -= delta; else chan->pcm += delta;
        chan->index += index_table_4[nibble & 7];
    } else if (bps == 5) {
        delta = step >> 4; if (nibble & 1) delta += (step >> 3); if (nibble & 2) delta += (step >> 2); if (nibble & 4) delta += (step >> 1); if (nibble & 8) delta += step;
        if (nibble & 16) chan->pcm -= delta; else chan->pcm += delta;
        chan->index += index_table_5[nibble & 15];
    }

    chan->index = clamp(chan->index, 0, 88);
    chan->pcm = clamp(chan->pcm, -32768, 32767);
    return (int16_t)chan->pcm;
}

void decode_chunk(MemBitStream *bs, int16_t *audio_buf, uint32_t chunk_smpl, uint8_t channels, uint8_t bpsL, uint8_t bpsR, bool use_ms, ADPCM_Channel *chL, ADPCM_Channel *chR) {
    for (uint32_t i = 0; i < chunk_smpl; i++) {
        if (bs->pos >= bs->max_pos) break;

        uint8_t nibL = read_bits_mem(bs, bpsL);
        int16_t valL = decode_nibble(chL, nibL, bpsL);
        
        if (channels == 2) {
            uint8_t nibR = read_bits_mem(bs, bpsR);
            int16_t valR = decode_nibble(chR, nibR, bpsR);
            
            if (use_ms) {
                audio_buf[i*2]     = clamp(valL + valR, -32768, 32767);
                audio_buf[i*2 + 1] = clamp(valL - valR, -32768, 32767);
            } else {
                audio_buf[i*2]     = valL;
                audio_buf[i*2 + 1] = valR;
            }
        } else {
            audio_buf[i] = valL;
        }
    }
}

// ==============================================================================
// SAGA AUDIO STEUERUNG
// ==============================================================================
void stop_saga_sound(uint8_t channel) {
    *((volatile uint16_t*)0xDFF096) = (1 << channel); // DMA STOP
}

// ==============================================================================
// MAIN PROGRAMM
// ==============================================================================
int main(int argc, char *argv[]) {
    signal(SIGINT, SIG_IGN); 
    SetSignal(0, SIGBREAKF_CTRL_C);

   // --- ARGUMENTEN-PARSING & USAGE PRÜFUNG ---
    int FIR_filter = -1; // -1 = Standard Bypass
    FILE *f = NULL;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-f") == 0 && i + 1 < argc) {
            FIR_filter = atoi(argv[++i]);
            if (FIR_filter > 19) FIR_filter = 19;
            if (FIR_filter < -1) FIR_filter = -1;
        } else {
             f = fopen( argv[i],"rb");
        }
    }

    if (!f) {
        printf("Nutzung: %s [-f 0..19] datei.adpx\n", argv[0] ? argv[0] : "ADPCM_Decoder");
        fflush(stdout);
        return 1;
    }

    fseek(f, 0, SEEK_END);
    uint32_t file_size = ftell(f);
    fseek(f, 0, SEEK_SET);

    uint8_t *raw_file_buf = (uint8_t *)AllocVec(file_size, MEMF_PUBLIC);
    if (!raw_file_buf) { fclose(f); return 1; }
    fread(raw_file_buf, 1, file_size, f);
    fclose(f);
    
    uint32_t sample_rate = raw_file_buf[4] | (raw_file_buf[5]<<8) | (raw_file_buf[6]<<16) | (raw_file_buf[7]<<24);
    uint8_t channels     = raw_file_buf[8];
    uint8_t bps          = raw_file_buf[9];
    uint8_t use_ms       = raw_file_buf[10];
    uint32_t total_smpl  = raw_file_buf[11] | (raw_file_buf[12]<<8) | (raw_file_buf[13]<<16) | (raw_file_buf[14]<<24);
    
    ADPCM_Channel chL = { (int16_t)(raw_file_buf[15] | (raw_file_buf[16]<<8)), raw_file_buf[17] };
    ADPCM_Channel chR = { (int16_t)(raw_file_buf[18] | (raw_file_buf[19]<<8)), raw_file_buf[20] };
    uint8_t bpsL = bps, bpsR = bps;
    if (bps == 53 && channels == 2) { bpsL = 5; bpsR = 3; use_ms = 1; }

    // Kanal suchen
    uint8_t active_ch = 0xFF;
    for (uint8_t c = 0; c < 4; c++) {
        if ((*((volatile uint16_t*)0xDFF002) & (1<<c)) == 0) { active_ch = c; break; }
    }
    if (active_ch == 0xFF) { FreeVec(raw_file_buf); return 1; }
    
    // Basis-Takt Amiga PAL (3.546.895 Hz)
    uint16_t saga_period = 3546895 / sample_rate; 

    // --- DER RINGPUFFER ---
    uint32_t target_samples = sample_rate * BUFFER_SECONDS; 
    uint32_t half_smpl  = (target_samples + 7) & ~7; 
    uint32_t half_bytes = half_smpl * channels * sizeof(int16_t);
    uint32_t total_bytes = half_bytes * 2; 
    
    int16_t *ring_buffer = (int16_t *)AllocVec(total_bytes, MEMF_PUBLIC | MEMF_CLEAR);
    if (!ring_buffer) { FreeVec(raw_file_buf); return 1; }
    
    int16_t *buf_half[2];
    buf_half[0] = ring_buffer;
    buf_half[1] = (int16_t *)((uint8_t*)ring_buffer + half_bytes);

    MemBitStream bs = { .data = raw_file_buf, .pos = 21, .max_pos = file_size, .pool = 0, .bits_left = 0 };

     // --- INFOS BERECHNEN UND AUSGEBEN ---
    float duration = (float)total_smpl / (float)sample_rate;
    float kbps = ((float)sample_rate * (float)(bpsL + (channels == 2 ? bpsR : 0))) / 1000.0f;
    float comp_ratio = (float)(file_size * 100) / (float)(total_smpl * channels * sizeof(int16_t));

    printf(">>> ADPCM Decoder <<<\n");
    printf("Sample Rate: %u Hz\n", sample_rate);
    printf("Channels: %u\n", channels);
    printf("%s%u Bit %s%u Bit\n", use_ms ? "Mid:" : "Left:", bpsL, use_ms ? "Side" : "Right:", bpsR);
    printf("Samples: %u length in seconds: %.2f\n", total_smpl, duration);
    printf("Compressed File Size: %u bytes compression ratio: %.2f %% of uncompressed (%.2f kbps)\n", file_size, comp_ratio, kbps);
    printf("Post-Filter: [%d] %s\n", FIR_filter, get_filter_name(FIR_filter));
    printf("Active Audio Channel: %u\n", active_ch);
    printf(">>> Pre-Fill: Fuelle erste Hälfte...\n");
    fflush(stdout);
    
    uint32_t smpl_0 = (total_smpl < half_smpl) ? total_smpl : half_smpl;
    decode_chunk(&bs, buf_half[0], smpl_0, channels, bpsL, bpsR, use_ms, &chL, &chR);
    
    // Filter anwenden
    apply_postfilter(buf_half[0], smpl_0, channels, FIR_filter, filter_state);
   
    total_smpl -= smpl_0;

    int free_half = 1; 

    // ==============================================================================
    // SYSTEM INTERRUPT SETUP (Amiga Exec)
    // ==============================================================================
    main_task = FindTask(NULL);
    int8_t sig_bit = AllocSignal(-1);
    audio_sig_mask = 1 << sig_bit;
    aud_int_bit = 1 << (7 + active_ch); 

    struct Interrupt audio_int_node;
    audio_int_node.is_Node.ln_Type = NT_INTERRUPT;
    audio_int_node.is_Node.ln_Pri  = 10;
    audio_int_node.is_Node.ln_Name = "ADPCM_Decoder";
    audio_int_node.is_Data         = NULL;
    audio_int_node.is_Code         = (void(*)())audio_isr;

    AddIntServer(INTB_AUD0 + active_ch, &audio_int_node);

    // ==============================================================================
    // SAGA PING-PONG START (DUMMY TRICK)
    // ==============================================================================
    uint32_t base = 0xDFF400 + (active_ch * 0x10);
    uint32_t audlen_divisor = (channels == 2) ? 8 : 4;
    uint32_t hw_len = half_bytes / audlen_divisor;

    *((volatile uint16_t*)(base + 0x08)) = 0xFFFF; // Vol
    *((volatile uint16_t*)(base + 0x0A)) = (channels == 2) ? 0x0004 : 0x0001; 
    *((volatile uint16_t*)(base + 0x0C)) = saga_period;

    static int16_t dummy_buf[4] = {0, 0, 0, 0}; 

    *((volatile uint32_t*)(base + 0x00)) = (uint32_t)(dummy_buf);
    *((volatile uint32_t*)(base + 0x04)) = 1; 

    *((volatile uint16_t*)0xDFF09C) = aud_int_bit;
    *((volatile uint16_t*)0xDFF09A) = 0xC000 | aud_int_bit; 
    *((volatile uint16_t*)0xDFF096) = 0x8000 + (1 << active_ch); 

    *((volatile uint32_t*)(base + 0x00)) = (uint32_t)(buf_half[0]);
    *((volatile uint32_t*)(base + 0x04)) = hw_len;

    bool aborted = false;
    printf(">>> START: Wiedergabe via OS-Interrupts (0%% CPU Last im Wait) <<<\n");
    fflush(stdout);

    // --- DIE PING-PONG SCHLEIFE ---
    while (total_smpl > 0) {
        if (SetSignal(0, 0) & SIGBREAKF_CTRL_C) {
            printf("\n>>> Abbruch durch Benutzer (Ctrl-C)! <<<\n");
            fflush(stdout);
            aborted = true;
            break;
        }

        Wait(audio_sig_mask);

        if (SetSignal(0, 0) & SIGBREAKF_CTRL_C) {
            printf("\n>>> Abbruch durch Benutzer (Ctrl-C)! <<<\n");
            fflush(stdout);
            aborted = true;
            break;
        }

        uint32_t smpl_chunk = (total_smpl < half_smpl) ? total_smpl : half_smpl;
        decode_chunk(&bs, buf_half[free_half], smpl_chunk, channels, bpsL, bpsR, use_ms, &chL, &chR);
        
        // Filter anwenden
        apply_postfilter(buf_half[free_half], smpl_chunk, channels, FIR_filter, filter_state);
        
        if (smpl_chunk < half_smpl) {
            uint32_t bytes_to_clear = (half_smpl - smpl_chunk) * channels * sizeof(int16_t);
            uint8_t *clear_ptr = (uint8_t*)(buf_half[free_half]) + (smpl_chunk * channels * sizeof(int16_t));
            for (uint32_t c = 0; c < bytes_to_clear; c++) clear_ptr[c] = 0;
        }
        
        total_smpl -= smpl_chunk;

        *((volatile uint32_t*)(base + 0x00)) = (uint32_t)(buf_half[free_half]);
        *((volatile uint32_t*)(base + 0x04)) = hw_len;
        
        free_half = 1 - free_half; 
    }

    // --- ENDPHASE ---
    if (!aborted) {
        printf(">>> ENDE: Lied komplett, warte auf Ausklingen...\n");
        fflush(stdout);
        Wait(audio_sig_mask); 
        Wait(audio_sig_mask); 
    }

    // --- CLEANUP ---
    *((volatile uint16_t*)0xDFF096) = (1 << active_ch); 
    *((volatile uint16_t*)0xDFF09A) = aud_int_bit;      

    RemIntServer(INTB_AUD0 + active_ch, &audio_int_node); 
    FreeSignal(sig_bit);                                

    printf("\nWiedergabe beendet.\n");
    fflush(stdout);
    
    FreeVec(ring_buffer);
    FreeVec(raw_file_buf);
    SetSignal(0, SIGBREAKF_CTRL_C); 
    
    return 0;
}