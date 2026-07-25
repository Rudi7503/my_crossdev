#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

#include <exec/types.h>
#include <exec/memory.h>
#include <proto/exec.h>
#include <devices/timer.h>
#include <proto/timer.h>
#include <dos/dos.h> // <--- NEU: Für SIGBREAKF_CTRL_C
#include <dos/dos.h> 
#include <signal.h> // <--- NEU: Für das Ignorieren des Standard-C-Abbruchs

// <--- NEU: Pufferlänge in Sekunden hier zentral definierbar
#define BUFFER_SECONDS 1 
// <--- NEU: Der ECHTE physikalische Hardware-Takt! 
    // PAL: 3546895Falls dein System in NTSC läuft, ändere dies zu 3579545
#define AMIGA_CLOCK 3579545 

// --- Globale Variablen für Timer-Zugriff ---
struct MsgPort *timerPort = NULL;
struct timerequest *timerReq = NULL;

// ==============================================================================
// MANUELLE ZEIT-HILFSFUNKTIONEN (Verhindert Linker-Probleme)
// ==============================================================================
static inline void add_time_micros(struct timeval *tv, uint32_t micros) {
    tv->tv_micro += micros;
    tv->tv_secs += (tv->tv_micro / 1000000);
    tv->tv_micro %= 1000000;
}

static inline int compare_time(struct timeval *t1, struct timeval *t2) {
    if (t1->tv_secs > t2->tv_secs) return 1;
    if (t1->tv_secs < t2->tv_secs) return -1;
    if (t1->tv_micro > t2->tv_micro) return 1;
    if (t1->tv_micro < t2->tv_micro) return -1;
    return 0;
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

static inline int32_t clamp(int32_t val, int32_t min, int32_t max) {
    if (val > max) return max; if (val < min) return min; return val;
}

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
    printf("Decoding chunk started\n");
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
    printf("Decoded ready\n");
}

// ==============================================================================
// SAGA AUDIO STEUERUNG
// ==============================================================================
uint8_t init_saga_channel() {
    for (uint8_t c = 0; c < 4; c++) {
        if ((*((volatile uint16_t*)0xDFF002) & (1<<c)) == 0) return c;
    }
    return 0xFF;
}

void start_saga_ringbuffer(uint8_t channel, uint8_t *buffer, uint32_t bytes, uint16_t period, bool stereo) {
    uint32_t base = 0xDFF400 + (channel * 0x10);
    
    // SAGA AUDLEN erwartet die Länge in "Paaren von Sample-Frames"
    // Stereo-Paar = 8 Bytes, Mono-Paar = 4 Bytes
    uint32_t audlen_divisor = stereo ? 8 : 4;

    *((volatile uint32_t*)(base + 0x00)) = (uint32_t)(buffer);                
    *((volatile uint32_t*)(base + 0x04)) = (uint32_t)(bytes / audlen_divisor);      
    *((volatile uint16_t*)(base + 0x08)) = 0xFFFF;                           
    *((volatile uint16_t*)(base + 0x0A)) = stereo ? 0x0004 : 0x0001; 
    *((volatile uint16_t*)(base + 0x0C)) = period;                            
    
    *((volatile uint16_t*)0xDFF096) = 0x8000 + (1<<channel); // DMA Start
}

void stop_saga_sound(uint8_t channel) {
    *((volatile uint16_t*)0xDFF096) = (1<<channel);
}

void InitTimer() {
    timerPort = CreateMsgPort();
    timerReq = (struct timerequest *)CreateIORequest(timerPort, sizeof(struct timerequest));
    OpenDevice("timer.device", UNIT_MICROHZ, (struct IORequest *)timerReq, 0);
}

void TimerDelay(uint32_t seconds, uint32_t micros) {
    timerReq->tr_node.io_Command = TR_ADDREQUEST;
    timerReq->tr_time.tv_secs = seconds;
    timerReq->tr_time.tv_micro = micros;
    DoIO((struct IORequest *)timerReq);
}

void GetSystemTime(struct timeval *current_tv) {
    timerReq->tr_node.io_Command = TR_GETSYSTIME;
    DoIO((struct IORequest *)timerReq);
    *current_tv = timerReq->tr_time;
}

void CloseTimer() {
    CloseDevice((struct IORequest *)timerReq);
    DeleteIORequest((struct IORequest *)timerReq);
    DeleteMsgPort(timerPort);
}

// ==============================================================================
// MAIN PROGRAMM
// ==============================================================================
int main(int argc, char *argv[]) {
    // <--- NEU: Verhindert, dass die C-Bibliothek das Programm bei Ctrl-C killt!
    // Dadurch erscheint kein "***Break" mehr und unser eigener Cleanup-Code läuft durch.
    signal(SIGINT, SIG_IGN);

    if (argc < 2) return 1;
    FILE *f = fopen(argv[1], "rb");
    if (!f) return 1;

    fseek(f, 0, SEEK_END);
    uint32_t file_size = ftell(f);
    fseek(f, 0, SEEK_SET);

    uint8_t *raw_file_buf = (uint8_t *)AllocVec(file_size, MEMF_PUBLIC);
    if (!raw_file_buf) {
        fclose(f); return 1;
    }
    fread(raw_file_buf, 1, file_size, f);
    fclose(f);
    
    InitTimer();

    uint32_t sample_rate = raw_file_buf[4] | (raw_file_buf[5]<<8) | (raw_file_buf[6]<<16) | (raw_file_buf[7]<<24);
    uint8_t channels     = raw_file_buf[8];
    uint8_t bps          = raw_file_buf[9];
    uint8_t use_ms       = raw_file_buf[10];
    uint32_t total_smpl  = raw_file_buf[11] | (raw_file_buf[12]<<8) | (raw_file_buf[13]<<16) | (raw_file_buf[14]<<24);
    
    ADPCM_Channel chL = { (int16_t)(raw_file_buf[15] | (raw_file_buf[16]<<8)), raw_file_buf[17] };
    ADPCM_Channel chR = { (int16_t)(raw_file_buf[18] | (raw_file_buf[19]<<8)), raw_file_buf[20] };

    uint8_t bpsL = bps, bpsR = bps;
    if (bps == 53 && channels == 2) { bpsL = 5; bpsR = 3; use_ms = 1; }
    printf("ADPCM Decoder v1.0\n Filesize: %u Bytes\n", file_size);
    printf("Sample Rate: %u Hz\n Channels: %u\n Bits Per Sample: %u/%u\n Use Mid Side Coding: %s\n Total Samples: %u\n", sample_rate, channels, bpsL, bpsR, use_ms ? "Yes" : "No", total_smpl);

    uint8_t active_ch = init_saga_channel();
    if (active_ch == 0xFF) {
        printf("Keine freien SAGA Audio-Kanäle verfügbar!\n");
        FreeVec(raw_file_buf); return 1;
    }
    
    // <--- NEU: Berechnung der Hardware-Periode und der daraus resultierenden ECHTEN Abspielrate
    uint16_t saga_period = AMIGA_CLOCK / sample_rate; 
    uint32_t real_hw_rate = AMIGA_CLOCK / saga_period; // Hebt den Rundungsfehler auf!
    
    printf("Saga Periode: %u (Hex: 0x%04X), Reale HW-Rate: %u Hz\n", saga_period, saga_period, real_hw_rate);

    // -------------------------------------------------------------------------
    // DER RINGPUFFER (X Sekunden gesamt, logisch in 2 Haelften unterteilt)
    // -------------------------------------------------------------------------
    // <--- NEU: Pufferlänge anhand des BUFFER_SECONDS Defines berechnen und DMA alignen
    uint32_t target_samples = sample_rate * BUFFER_SECONDS; 
    uint32_t half_smpl  = (target_samples + 7) & ~7; // Auf 32-Bit DMA Alignment runden
    uint32_t half_bytes = half_smpl * channels * sizeof(int16_t);
    uint32_t total_bytes = half_bytes * 2; 
    
    int16_t *ring_buffer = (int16_t *)AllocVec(total_bytes, MEMF_PUBLIC | MEMF_CLEAR);
    if (!ring_buffer) {
        FreeVec(raw_file_buf); return 1;
    }
    printf("Ringbuffer: %u Bytes (2 x halber Buffer: %u Bytes)\n", total_bytes, half_bytes);
    // <--- NEU: Anzeige für die definierte Puffer-Zeit
    printf("Geplante Abspielzeit des halben Buffers: %d Sekunden\n", BUFFER_SECONDS);
    
    int16_t *buf_half[2];
    buf_half[0] = ring_buffer;
    buf_half[1] = (int16_t *)((uint8_t*)ring_buffer + half_bytes);

    MemBitStream bs = { .data = raw_file_buf, .pos = 21, .max_pos = file_size, .pool = 0, .bits_left = 0 };

    printf("\n>>> Pre-Fill: Fuelle kompletten %ds Ringpuffer...\n", BUFFER_SECONDS * 2);
    
    // Erste Haelfte fuellen
    uint32_t smpl_0 = (total_smpl < half_smpl) ? total_smpl : half_smpl;
    decode_chunk(&bs, buf_half[0], smpl_0, channels, bpsL, bpsR, use_ms, &chL, &chR);
    total_smpl -= smpl_0;

    // Zweite Haelfte fuellen
    uint32_t smpl_1 = (total_smpl < half_smpl) ? total_smpl : half_smpl;
    if (smpl_1 > 0) {
        decode_chunk(&bs, buf_half[1], smpl_1, channels, bpsL, bpsR, use_ms, &chL, &chR);
        total_smpl -= smpl_1;
    }

    // --- SETUP FUER EXAKTES TIMING ---
    struct timeval target_tv, current_tv,start_tv;
   

    // Die Hardware wird mit Puffer 0 starten (Groesse: smpl_0). 
    // Wir setzen die Ziel-Zeit auf die Dauer dieses allerersten Puffers.
    uint32_t playing_smpl = smpl_0;
    uint32_t next_playing_smpl = smpl_1;
    
    // <--- NEU: Initialer Timer wird mit der ECHTEN Hardware-Rate berechnet
    uint32_t duration_micros = (uint32_t)(((uint64_t)playing_smpl * 1000000ULL) / real_hw_rate);
    printf(">>> Erste Pufferlaenge: %u Samples, Dauer: %lu.%06lu Sekunden\n", playing_smpl, duration_micros / 1000000, (duration_micros % 1000000) );
    
    // <--- NEU: Sicherheitspuffer (genau halbe Puffergröße) exakt nach Hardware-Rate berechnet
    uint32_t safety_smpl = half_smpl / 2;
    uint32_t safety_margin = (uint32_t)(((uint64_t)safety_smpl * 1000000ULL) / real_hw_rate); 
    printf(">>> Sicherheitszeitpuffer: %lu.%06lu Sekunden\n",safety_margin / 1000000, (safety_margin % 1000000) );
    

    // --- HARDWARE START ---
    printf(">>> START: Wiedergabe (Ringpuffer Loop) <<<\n");
    start_saga_ringbuffer(active_ch, (uint8_t*)ring_buffer, total_bytes, saga_period, channels == 2);
    GetSystemTime(&current_tv);
    start_tv = current_tv;
    target_tv = current_tv;
    add_time_micros(&target_tv, duration_micros);
    add_time_micros(&target_tv, safety_margin);
    printf(">>> Startzeit: %lu.%06lu, Zielzeit: %lu.%06lu\n", start_tv.tv_secs, start_tv.tv_micro, target_tv.tv_secs, target_tv.tv_micro);
    int free_half = 0; 

   // --- DIE RINGPUFFER-SCHLEIFE ---
    struct timeval decode_start, decode_end; 
    int schleife_counter = 0; 
    bool aborted = false; 

    while (total_smpl > 0) {
        schleife_counter++;
        printf("total_smpl=%u\n, Schleife: %d\n", total_smpl, schleife_counter);
        
        printf(">>> Warten auf Hardware-Wechsel... ");
        GetSystemTime(&current_tv);
        
        // <--- KORREKTUR: Die Abfrage MUSS in die Warteschleife!
        while (compare_time(&target_tv, &current_tv) == 1) {
            if (SetSignal(0, 0) & SIGBREAKF_CTRL_C) {
                aborted = true;
                break; // Bricht die kleine Warteschleife ab
            }
            TimerDelay(0, 20000);
            GetSystemTime(&current_tv);
        }
        
        // Wenn in der Warteschleife abgebrochen wurde, brechen wir auch die große Schleife ab
        if (aborted) {
            printf("\n>>> Abbruch durch Benutzer (Ctrl-C) erfasst! Beende sofort... <<<\n");
            break; 
        }

        printf("start_tv: %lu.%06lu, current_tv: %lu.%06lu, target_tv: %lu.%06lu\n", start_tv.tv_secs, start_tv.tv_micro, current_tv.tv_secs, current_tv.tv_micro, target_tv.tv_secs, target_tv.tv_micro);
        printf("Weiter gehts.\n");

        // 1. MESSUNG STARTEN
        GetSystemTime(&decode_start);

        // 2. DEKODIEREN
        uint32_t smpl_chunk = (total_smpl < half_smpl) ? total_smpl : half_smpl;
        decode_chunk(&bs, buf_half[free_half], smpl_chunk, channels, bpsL, bpsR, use_ms, &chL, &chR);
        
        // 3. MESSUNG STOPPEN
        GetSystemTime(&decode_end);

        // 4. DAUER BERECHNEN
        uint32_t diff_micros = (decode_end.tv_micro >= decode_start.tv_micro) ? 
                               (decode_end.tv_micro - decode_start.tv_micro) : 
                               (1000000 - decode_start.tv_micro + decode_end.tv_micro);
        
        printf(">>> Dekodieren dauerte: %lu ms\n", diff_micros / 1000);

        total_smpl -= smpl_chunk;
        next_playing_smpl = smpl_chunk;

        duration_micros = (uint32_t)(((uint64_t)next_playing_smpl * 1000000ULL) / real_hw_rate);
        add_time_micros(&target_tv, duration_micros);
        
        printf(">>> Nächster Pufferwechsel am: %lu.%06lu in: %lu.%06lu Sekunden\n", target_tv.tv_secs, target_tv.tv_micro, duration_micros / 1000000, (duration_micros % 1000000) );
        free_half = 1 - free_half;
    }

    // --- ENDPHASE ---
    if (!aborted) {
        printf(">>> ENDE: Warten bis der vorletzte Buffer fertig gespielt ist...\n");
        GetSystemTime(&current_tv);
        while (compare_time(&target_tv, &current_tv) == 1) {
            // <--- KORREKTUR: Auch hier den Abbruch zulassen!
            if (SetSignal(0, 0) & SIGBREAKF_CTRL_C) break; 
            TimerDelay(0, 20000);
            GetSystemTime(&current_tv);
        }
        
        // Den allerletzten Rest-Block ausklingen lassen
        if (next_playing_smpl > 0 && !(SetSignal(0, 0) & SIGBREAKF_CTRL_C)) {
            printf(">>> ENDE: Warten bis der allerletzte Rest-Buffer fertig gespielt ist...\n");
            playing_smpl = next_playing_smpl;
            
            duration_micros = (uint32_t)(((uint64_t)playing_smpl * 1000000ULL) / real_hw_rate);
            add_time_micros(&target_tv, duration_micros);
            
            GetSystemTime(&current_tv);
            while (compare_time(&target_tv, &current_tv) == 1) {
                // <--- KORREKTUR: Und auch ganz am Ende noch Abbrechen erlauben
                if (SetSignal(0, 0) & SIGBREAKF_CTRL_C) break;
                TimerDelay(0, 20000);
                GetSystemTime(&current_tv);
            }
        }
    }

    // --- CLEANUP ---
    stop_saga_sound(active_ch);
    printf("\nWiedergabe beendet.\n");

    CloseTimer();
    FreeVec(ring_buffer);
    FreeVec(raw_file_buf);
    
    // <--- KORREKTUR: Signal final und hart löschen, BEVOR wir zum OS zurückkehren.
    // Das verhindert die "unread signal" / "***Break" Fehlermeldung der Shell.
    SetSignal(0, SIGBREAKF_CTRL_C); 
    
    return 0;
}