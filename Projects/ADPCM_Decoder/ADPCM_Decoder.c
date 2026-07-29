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
#define STREAM_BUF_SIZE 2048 // 2 KB Lese-Puffer für die Festplatte

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
    float y_prev1; 
    float y_prev2; 
} PostFilterState;

PostFilterState filter_state[2] = { 
    {0, 0, 0, 0, 0, 0, 0, 0.0f, 0.0f}, 
    {0, 0, 0, 0, 0, 0, 0, 0.0f, 0.0f} 
};

static const float iir_coeffs_f[10][5] = {
    {7583.0f/32768.0f, 0.0f, -7583.0f/32768.0f, -44000.0f/32768.0f, 17600.0f/32768.0f},       
    {5784.0f/32768.0f, 0.0f, -5784.0f/32768.0f, -46747.0f/32768.0f, 21201.0f/32768.0f},       
    {1622.0f/32768.0f, 3244.0f/32768.0f, 1622.0f/32768.0f, -41929.0f/32768.0f, 15650.0f/32768.0f},     
    {659.0f/32768.0f, 1317.0f/32768.0f, 659.0f/32768.0f, -51152.0f/32768.0f, 21018.0f/32768.0f},       
    {1622.0f/32768.0f, 3244.0f/32768.0f, 1622.0f/32768.0f, -41929.0f/32768.0f, 15650.0f/32768.0f},     
    {659.0f/32768.0f, 1317.0f/32768.0f, 659.0f/32768.0f, -51152.0f/32768.0f, 21018.0f/32768.0f},       
    {31406.0f/32768.0f, -62813.0f/32768.0f, 31406.0f/32768.0f, -62762.0f/32768.0f, 30060.0f/32768.0f}, 
    {28180.0f/32768.0f, -56360.0f/32768.0f, 28180.0f/32768.0f, -55426.0f/32768.0f, 23478.0f/32768.0f}, 
    {26214.0f/32768.0f, 0.0f, -26214.0f/32768.0f, -49152.0f/32768.0f, 16384.0f/32768.0f},     
    {32767.0f/32768.0f, -32767.0f/32768.0f, 0.0f, -32767.0f/32768.0f, 0.0f}          
};

static inline void apply_postfilter(int16_t *pcm, uint32_t count, uint8_t channels, int filter_index, PostFilterState *state) {
    if (channels == 2) {
        for (uint32_t i = 0; i < count; i++) {
            state[0].prev6 = state[0].prev5; state[0].prev5 = state[0].prev4; state[0].prev4 = state[0].prev3; state[0].prev3 = state[0].prev2; state[0].prev2 = state[0].prev1; state[0].prev1 = state[0].actual; state[0].actual = pcm[i*2];
            state[1].prev6 = state[1].prev5; state[1].prev5 = state[1].prev4; state[1].prev4 = state[1].prev3; state[1].prev3 = state[1].prev2; state[1].prev2 = state[1].prev1; state[1].prev1 = state[1].actual; state[1].actual = pcm[i*2 + 1];    
            
            if (filter_index < 0 || filter_index > 19) {
                pcm[i*2] = state[0].actual; pcm[i*2 + 1] = state[1].actual;
            } else if (filter_index <= 9) {
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
            } else {
                int idx = filter_index - 10;
                float b0 = iir_coeffs_f[idx][0], b1 = iir_coeffs_f[idx][1], b2 = iir_coeffs_f[idx][2], a1 = iir_coeffs_f[idx][3], a2 = iir_coeffs_f[idx][4];
                float xL = (float)state[0].actual, xL1 = (float)state[0].prev1, xL2 = (float)state[0].prev2, yL = b0 * xL + b1 * xL1 + b2 * xL2 - a1 * state[0].y_prev1 - a2 * state[0].y_prev2;
                state[0].y_prev2 = state[0].y_prev1; state[0].y_prev1 = yL; pcm[i*2] = (int16_t)clamp((int32_t)yL, -32768, 32767);
                float xR = (float)state[1].actual, xR1 = (float)state[1].prev1, xR2 = (float)state[1].prev2, yR = b0 * xR + b1 * xR1 + b2 * xR2 - a1 * state[1].y_prev1 - a2 * state[1].y_prev2;
                state[1].y_prev2 = state[1].y_prev1; state[1].y_prev1 = yR; pcm[i*2 + 1] = (int16_t)clamp((int32_t)yR, -32768, 32767);
            }
        }
    } else if (channels == 1) {
        for (uint32_t i = 0; i < count; i++) {
            state[0].prev6 = state[0].prev5; state[0].prev5 = state[0].prev4; state[0].prev4 = state[0].prev3; state[0].prev3 = state[0].prev2; state[0].prev2 = state[0].prev1; state[0].prev1 = state[0].actual; state[0].actual = pcm[i]; 
            
            if (filter_index < 0 || filter_index > 19) pcm[i] = state[0].actual;
            else if (filter_index <= 9) {
                if (filter_index == 0) pcm[i] = (int16_t)clamp((int32_t)((state[0].actual+state[0].prev2)*0.125+state[0].prev1*0.625), -32768, 32767);
                else if (filter_index == 1) pcm[i] = (int16_t)clamp((int32_t)((state[0].actual+state[0].prev2)*0.125+state[0].prev1*0.75), -32768, 32767);
                else if (filter_index == 2) pcm[i] = (int16_t)clamp((int32_t)((state[0].actual+state[0].prev2)*0.125+state[0].prev1*0.875), -32768, 32767);
                else if (filter_index == 3) pcm[i] = (int16_t)clamp((int32_t)((state[0].actual+state[0].prev2)*0.25+state[0].prev1*0.5), -32768, 32767);
                else if (filter_index == 4) pcm[i] = (int16_t)clamp((int32_t)((state[0].actual+state[0].prev2)*0.25+state[0].prev1*0.625), -32768, 32767);
                else if (filter_index == 5) pcm[i] = (int16_t)clamp((int32_t)((state[0].actual+state[0].prev6)*0.0625+(state[0].prev1+state[0].prev5)*0.125+(state[0].prev2+state[0].prev4)*0.1875+state[0].prev3*0.250), -32768, 32767);
                else if (filter_index == 6) pcm[i] = (int16_t)clamp((int32_t)((state[0].actual+state[0].prev6)*0.125+(state[0].prev1+state[0].prev5)*0.125+(state[0].prev2+state[0].prev4)*0.125+state[0].prev3*0.250), -32768, 32767);
                else if (filter_index == 7) pcm[i] = (int16_t)clamp((int32_t)((state[0].actual+state[0].prev6)*0.1875+(state[0].prev1+state[0].prev5)*0.125+(state[0].prev2+state[0].prev4)*0.0625+state[0].prev3*0.25), -32768, 32767);
                else if (filter_index == 8) pcm[i] = (int16_t)clamp((int32_t)((state[0].actual+state[0].prev6)*0.2500+(state[0].prev1+state[0].prev5)*0.125+(state[0].prev2+state[0].prev4)*0.0+(state[0].prev3*0.25)), -32768, 32767);
                else if (filter_index == 9) pcm[i] = (int16_t)clamp((int32_t)((state[0].actual+state[0].prev6)*0.3125+(state[0].prev1+state[0].prev5)*0.625+(state[0].prev2+state[0].prev4)*0.0+(state[0].prev3*0.25)), -32768, 32767);
            } else {
                int idx = filter_index - 10;
                float b0 = iir_coeffs_f[idx][0], b1 = iir_coeffs_f[idx][1], b2 = iir_coeffs_f[idx][2], a1 = iir_coeffs_f[idx][3], a2 = iir_coeffs_f[idx][4];
                float xM = (float)state[0].actual, xM1 = (float)state[0].prev1, xM2 = (float)state[0].prev2, yM = b0 * xM + b1 * xM1 + b2 * xM2 - a1 * state[0].y_prev1 - a2 * state[0].y_prev2;
                state[0].y_prev2 = state[0].y_prev1; state[0].y_prev1 = yM; pcm[i] = (int16_t)clamp((int32_t)yM, -32768, 32767);
            }
        }
    }
}

const char* get_filter_name(int idx) {
    if (idx == -1) return "Bypass";
    if (idx == 0) return "FIR: 1/8,3/4,1/8"; if (idx == 1) return "FIR: 1/8,6/8,1/8"; if (idx == 2) return "FIR: 1/8,7/8,1/8";
    if (idx == 3) return "FIR: 2/8,4/8,2/8"; if (idx == 4) return "FIR: 2/8,5/8,1/8"; if (idx == 5) return "FIR: 1/16,3/16,3/16,1/4,3/16,3/16,1/16";
    if (idx == 6) return "FIR: 1/8,1/8,1/8,1/4,1/8,1/8,1/8"; if (idx == 7) return "FIR: 3/16,2/16,1/16,1/4,1/16,2/16,3/16";
    if (idx == 8) return "FIR: 1/4,1/8,0,1/4,0,1/8,1/4"; if (idx == 9) return "FIR: 5/16,5/16,0,1/4,0,5/16,5/16";
    if (idx == 10) return "IIR: Sprache Bandpass mild"; if (idx == 11) return "IIR: Sprache Bandpass stark";
    if (idx == 12) return "IIR: Sprache Tiefpass mild"; if (idx == 13) return "IIR: Sprache Tiefpass stark";
    if (idx == 14) return "IIR: Musik Tiefpass sanft"; if (idx == 15) return "IIR: Musik Tiefpass moderat";
    if (idx == 16) return "IIR: Musik Bandpass Subsonic"; if (idx == 17) return "IIR: Musik Hoehen sanft";
    if (idx == 18) return "IIR: Musik Praesenz"; if (idx == 19) return "IIR: Musik Bass-Boost";
    return "Unknown";
}

// ==============================================================================
// STREAMING FILE BITSTREAM READER (DISK I/O CHUNKS)
// ==============================================================================
typedef struct {
    FILE *f;
    uint8_t buffer[STREAM_BUF_SIZE];
    uint32_t pos;           // Byte-Position innerhalb unseres 2KB-Puffers
    uint32_t valid_bytes;   // Wie viele Bytes aktuell im Puffer gültig sind
    uint32_t bit_pos;       // 0-7
    bool eof;               // Festplatten-Ende erreicht
} FileBitStream;

static inline uint32_t read_bits_file(FileBitStream *bs, int num_bits) {
    uint32_t result = 0;
    int bits_read = 0;

    while (bits_read < num_bits) {
        // Falls unser kleiner Chunk-Puffer leer ist, lade den nächsten Schwung von Platte
        if (bs->pos >= bs->valid_bytes) {
            if (bs->eof) break; 
            bs->valid_bytes = fread(bs->buffer, 1, STREAM_BUF_SIZE, bs->f);
            bs->pos = 0;
            if (bs->valid_bytes == 0) {
                bs->eof = true;
                break;
            }
        }

        int bits_available = 8 - bs->bit_pos;
        int bits_to_take = num_bits - bits_read;
        if (bits_to_take > bits_available) bits_to_take = bits_available;

        uint32_t byte_val = bs->buffer[bs->pos];
        uint32_t mask = (1 << bits_to_take) - 1;
        
        uint32_t extracted = (byte_val >> bs->bit_pos) & mask;
        result |= (extracted << bits_read);

        bits_read += bits_to_take;
        bs->bit_pos += bits_to_take;

        if (bs->bit_pos >= 8) {
            bs->bit_pos = 0;
            bs->pos++;
        }
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
static const int index_table_6[32] = {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
                                       1, 2, 3, 4, 5, 6, 7, 8, 10, 12, 14, 16, 19, 22, 25, 28};

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
    } else if (bps == 6) {
        delta = step >> 5; 
        if (nibble & 1) delta += (step >> 4);
        if (nibble & 2) delta += (step >> 3);
        if (nibble & 4) delta += (step >> 2);
        if (nibble & 8) delta += (step >> 1);
        if (nibble & 16) delta += step;
        if (nibble & 32) chan->pcm -= delta; else chan->pcm += delta;
        chan->index += index_table_6[nibble & 31];
    }

    chan->index = clamp(chan->index, 0, 88);
    chan->pcm = clamp(chan->pcm, -32768, 32767);
    return (int16_t)chan->pcm;
}

// ==============================================================================
// TODO: HARDWARE OPTIMIERTE ASSEMBLER KERNEL (AMMX 68080)
// ==============================================================================
//
// WARUM 15-BYTE BLÖCKE? (Die "Magie" der 120 Bits)
// Ein 15-Byte-Block entspricht exakt 120 Bits. Die Zahl 120 ist ein ideales 
// gemeinsames Vielfaches für all unsere krummen Bit-Tiefen. Das bedeutet, JEDES 
// unserer Formate passt RESTLOS in diese 15 Bytes, ohne dass am Ende eines 
// Blocks krumme Rest-Bits in den nächsten Block überlappen:
//  - 3 Bit (Mono)      -> 120 / 3 = exakt 40 Samples
//  - 4 Bit (Mono)      -> 120 / 4 = exakt 30 Samples
//  - 5 Bit (Mono)      -> 120 / 5 = exakt 24 Samples
//  - 6 Bit (Mono)      -> 120 / 6 = exakt 20 Samples
//  - 8 Bit (5/3 M/S)   -> 120 / 8 = exakt 15 Stereo-Sample-Paare
//  - 6 Bit (4/2 M/S)   -> 120 / 6 = exakt 20 Stereo-Sample-Paare
//  - 5 Bit (3/2 M/S)   -> 120 / 5 = exakt 24 Stereo-Sample-Paare
// 
// Vorteil für AMMX: Der 68080 kann die 15 Bytes (z. B. als zwei 64-Bit Quadwords)
// direkt in die Vektor-Register (e0, e1) laden, parallel via 'pand' und 'lsrq' 
// die Bits extrahieren und exakt eine feste Anzahl an Samples ausspucken, ohne 
// jemals bitweise über Speichergrenzen springen zu müssen.
//
// PARAMETER FÜR DIE ASSEMBLER-AUFRUFE:
// 1. in_buffer  -> Pointer auf den Bitstream (Start des 15-Byte Blocks).
// 2. out_buffer -> Pointer auf den Output (wo die decodierten PCM 16-Bit Samples landen).
//                  Achtung: M/S Formate schreiben direkt L und R interleaved.
// 3. chL / chR  -> Pointer auf das ADPCM_Channel Struct (zum Laden/Speichern von pcm & index).
//                  Bei Stereo/MS werden beide Channel-Structs übergeben.
// 4. block_cnt  -> Wie viele 15-Byte Blöcke in dieser Schleife am Stück verarbeitet werden.
//
/*
extern void decode_3bit_ammx_asm(const uint8_t *in_buffer, int16_t *out_buffer, ADPCM_Channel *ch, uint32_t block_cnt);
extern void decode_ms_53_ammx_asm(const uint8_t *in_buffer, int16_t *out_buffer, ADPCM_Channel *chL, ADPCM_Channel *chR, uint32_t block_cnt);
// ... weitere Formate ...
*/

// DUMMY Rumpf-Funktion als Platzhalter
void decode_block_asm_dummy(void) {
    // TODO: Assembler Kern-Aufruf oder Inline-ASM für 15-Byte Blockverarbeitung
    // Lade Daten in e0, maskiere (pand) in e1, schiebe (lsrq).
}
// ==============================================================================

void decode_chunk(FileBitStream *bs, int16_t *audio_buf, uint32_t chunk_smpl, uint8_t channels, uint8_t bpsL, uint8_t bpsR, bool use_ms, ADPCM_Channel *chL, ADPCM_Channel *chR) {
    
    // ==========================================================================
    // TODO: ASSEMBLER-BRANCH EINHÄNGEN
    // ==========================================================================
    // if (hardware_has_ammx && bpsL == 3 && channels == 1) {
    //     decode_3bit_ammx_asm(bs->buffer, audio_buf, chL, chunk_smpl / 40); // 40 samples = 1 Block
    //     return;
    // }
    // ==========================================================================

    for (uint32_t i = 0; i < chunk_smpl; i++) {
        if (bs->eof) break;

        uint8_t nibL = (uint8_t)read_bits_file(bs, bpsL); 
        int16_t valL = decode_nibble(chL, nibL, bpsL);
        
        if (channels == 2) {
            uint8_t nibR = (uint8_t)read_bits_file(bs, bpsR);
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

void stop_saga_sound(uint8_t channel) {
    *((volatile uint16_t*)0xDFF096) = (1 << channel); // DMA STOP
}

// ==============================================================================
// MAIN PROGRAMM
// ==============================================================================
int main(int argc, char *argv[]) {
    signal(SIGINT, SIG_IGN); 
    SetSignal(0, SIGBREAKF_CTRL_C);

    int FIR_filter = -1; // -1 = Standard Bypass
    FILE *f = NULL;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-f") == 0 && i + 1 < argc) {
            FIR_filter = atoi(argv[++i]);
            if (FIR_filter > 19) FIR_filter = 19;
            if (FIR_filter < -1) FIR_filter = -1;
        } else {
             f = fopen(argv[i],"rb");
        }
    }

    if (!f) {
        printf("Nutzung: %s [-f -1..19] datei.adpx\n\n", argv[0] ? argv[0] : "ADPCM_Decoder");
        printf("Verfuegbare Post-Filter:\n");
        for (int i = -1; i <= 19; i++) {
            printf(" [%2d] %s\n", i, get_filter_name(i));
        }
        fflush(stdout);
        return 1;
    }

    fseek(f, 0, SEEK_END);
    uint32_t file_size = ftell(f);
    fseek(f, 0, SEEK_SET);

    // Header auslesen (21 Bytes) ohne die Datei komplett ins RAM zu laden
    uint8_t header[21];
    if (fread(header, 1, 21, f) != 21) {
        fclose(f);
        return 1;
    }
    
    uint32_t sample_rate = header[4] | (header[5]<<8) | (header[6]<<16) | (header[7]<<24);
    uint8_t channels     = header[8];
    uint8_t bps          = header[9];
    uint8_t use_ms       = header[10];
    uint32_t total_smpl  = header[11] | (header[12]<<8) | (header[13]<<16) | (header[14]<<24);
    
    ADPCM_Channel chL = { (int16_t)(header[15] | (header[16]<<8)), header[17] };
    ADPCM_Channel chR = { (int16_t)(header[18] | (header[19]<<8)), header[20] };
    
    uint8_t bpsL = bps, bpsR = bps;
    if (channels == 2) {
        if (bps == 53) { bpsL = 5; bpsR = 3; use_ms = 1; }
        else if (bps == 42) { bpsL = 4; bpsR = 2; use_ms = 1; }
        else if (bps == 32) { bpsL = 3; bpsR = 2; use_ms = 1; }
    }

    uint8_t active_ch = 0xFF;
    for (uint8_t c = 0; c < 4; c++) {
        if ((*((volatile uint16_t*)0xDFF002) & (1<<c)) == 0) { active_ch = c; break; }
    }
    if (active_ch == 0xFF) { fclose(f); return 1; }
    
    uint16_t saga_period = 3546895 / sample_rate; 

    uint32_t target_samples = sample_rate * BUFFER_SECONDS; 
    uint32_t half_smpl  = (target_samples + 7) & ~7; 
    uint32_t half_bytes = half_smpl * channels * sizeof(int16_t);
    uint32_t total_bytes = half_bytes * 2; 
    
    int16_t *ring_buffer = (int16_t *)AllocVec(total_bytes, MEMF_PUBLIC | MEMF_CLEAR);
    if (!ring_buffer) { fclose(f); return 1; }
    
    int16_t *buf_half[2];
    buf_half[0] = ring_buffer;
    buf_half[1] = (int16_t *)((uint8_t*)ring_buffer + half_bytes);

    // ==============================================================================
    // FESTPLATTEN-STREAM INITIALISIEREN
    // (f steht durch das Lesen des Headers bereits automatisch an Byte 21!)
    // ==============================================================================
    FileBitStream bs = { 
        .f = f, 
        .pos = 0, 
        .valid_bytes = 0, // 0 erzwingt sofort das Nachladen des ersten 2KB-Blocks!
        .bit_pos = 0,
        .eof = false 
    };

    float duration = (float)total_smpl / (float)sample_rate;
    float kbps = ((float)sample_rate * (float)(bpsL + (channels == 2 ? bpsR : 0))) / 1000.0f;
    float comp_ratio = (float)(file_size * 100) / (float)(total_smpl * channels * sizeof(int16_t));

    printf(">>> ADPCM Decoder (Disk-Streaming, %d KB Buffer) <<<\n", STREAM_BUF_SIZE / 1024);
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
    apply_postfilter(buf_half[0], smpl_0, channels, FIR_filter, filter_state);
    total_smpl -= smpl_0;

    int free_half = 1; 

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

    if (!aborted) {
        printf(">>> ENDE: Lied komplett, warte auf Ausklingen...\n");
        fflush(stdout);
        Wait(audio_sig_mask); 
        Wait(audio_sig_mask); 
    }

    *((volatile uint16_t*)0xDFF096) = (1 << active_ch); 
    *((volatile uint16_t*)0xDFF09A) = aud_int_bit;      

    RemIntServer(INTB_AUD0 + active_ch, &audio_int_node); 
    FreeSignal(sig_bit);                                

    printf("\nWiedergabe beendet.\n");
    fflush(stdout);
    
    FreeVec(ring_buffer);
    fclose(f); // Datei am Ende sauber schließen!
    SetSignal(0, SIGBREAKF_CTRL_C); 
    
    return 0;
}