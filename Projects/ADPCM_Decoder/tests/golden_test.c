/* Golden reference / byte-diff test for decoder_4bit.s.
 * Tests 4-bit stereo, mid-side and mono against a C reference.
 * Input: deterministic 15-byte blocks; output and final state must match.
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "uart_asm.h"

typedef struct __attribute__((packed, aligned(2))) { int32_t pcm; int16_t index; } Chan;

extern uint32_t decode_4bit_stereo_asm(const uint8_t *in, int16_t *out, void *chL, void *chR, uint32_t blocks);
extern uint32_t decode_4bit_ms_asm(const uint8_t *in, int16_t *out, void *chL, void *chR, uint32_t blocks);
extern uint32_t decode_4bit_mono_asm(const uint8_t *in, int16_t *out, void *chL, uint32_t blocks);

static const int step_table[89] = {
 7,8,9,10,11,12,13,14,16,17,19,21,23,25,28,31,34,37,41,45,50,55,60,66,73,80,88,97,107,
 118,130,143,157,173,190,209,230,253,279,307,337,371,408,449,494,544,598,658,724,796,
 876,963,1060,1166,1282,1411,1552,1707,1878,2066,2272,2499,2749,3024,3327,3660,4026,
 4428,4871,5358,5894,6484,7132,7845,8630,9493,10442,11487,12635,13899,15289,16818,
 18500,20350,22385,24623,27086,29794,32767
};
static const int idx4[8] = {-1,-1,-1,-1,2,4,6,8};

static int32_t clamp32(int32_t v, int32_t lo, int32_t hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}
static void set_chan_raw(Chan *c, uint32_t pcm, uint16_t index) {
    uint8_t *p = (uint8_t *)c;
    p[0] = (uint8_t)(pcm >> 24);
    p[1] = (uint8_t)(pcm >> 16);
    p[2] = (uint8_t)(pcm >> 8);
    p[3] = (uint8_t)pcm;
    p[4] = (uint8_t)(index >> 8);
    p[5] = (uint8_t)index;
}

static int16_t ref_nibble(Chan *c, uint8_t nib) {
    int32_t step = step_table[c->index];
    int32_t delta = step >> 3;
    if (nib & 1) delta += step >> 2;
    if (nib & 2) delta += step >> 1;
    if (nib & 4) delta += step;
    if (nib & 8) c->pcm -= delta; else c->pcm += delta;
    c->index += idx4[nib & 7];
    c->index = (int16_t)clamp32(c->index, 0, 88);
    c->pcm = clamp32(c->pcm, -32768, 32767);
    return (int16_t)c->pcm;
}

static uint32_t rng_state = 0x12345678u;
static uint32_t rng(void) { rng_state ^= rng_state << 13; rng_state ^= rng_state >> 17; rng_state ^= rng_state << 5; return rng_state; }

#define MAXB 512
static uint8_t inbuf[15 * MAXB];
static int16_t refout[30 * MAXB];
static int16_t asmout[30 * MAXB + 4];
static FILE *rep;

static int run_case(const char *name, int mode, uint32_t blocks, int edge) {
    /* static/stable addresses: no compiler coalescing with the reference */
    static Chan rl, rr, al, ar;
    if (edge) {
        if (edge == 1) {
            set_chan_raw(&rl, 0xFFFF8000u, 0);
            set_chan_raw(&al, 0xFFFF8000u, 0);
            set_chan_raw(&rr, 0x00007FFFu, 88);
            set_chan_raw(&ar, 0x00007FFFu, 88);
        } else {
            set_chan_raw(&rl, 0x00007FFFu, 88);
            set_chan_raw(&al, 0x00007FFFu, 88);
            set_chan_raw(&rr, 0xFFFF8000u, 0);
            set_chan_raw(&ar, 0xFFFF8000u, 0);
        }
    } else {
        rl.pcm = (int32_t)(rng() & 0xffff) - 32768;
        al.pcm = rl.pcm;
        rl.index = (int16_t)(rng() % 89);
        al.index = rl.index;
        rr.pcm = (int32_t)(rng() & 0xffff) - 32768;
        ar.pcm = rr.pcm;
        rr.index = (int16_t)(rng() % 89);
        ar.index = rr.index;
    }
    for (uint32_t i = 0; i < 15 * blocks; i++) inbuf[i] = (uint8_t)rng();

    for (uint32_t b = 0; b < blocks; b++) {
        for (int k = 0; k < 15; k++) {
            uint8_t v = inbuf[b * 15 + k];
            if (mode == 2) {
                int16_t l0 = ref_nibble(&rl, v & 15);
                int16_t l1 = ref_nibble(&rl, (v >> 4) & 15);
                refout[b * 30 + 2 * k] = l0;
                refout[b * 30 + 2 * k + 1] = l1;
            } else {
                int16_t l = ref_nibble(&rl, v & 15);
                int16_t r = ref_nibble(&rr, (v >> 4) & 15);
                if (mode == 1) {
                    refout[b * 30 + 2 * k] = (int16_t)clamp32((int32_t)l + r, -32768, 32767);
                    refout[b * 30 + 2 * k + 1] = (int16_t)clamp32((int32_t)l - r, -32768, 32767);
                } else {
                    refout[b * 30 + 2 * k] = l;
                    refout[b * 30 + 2 * k + 1] = r;
                }
            }
        }
    }

    fprintf(rep, "  PRE;%s;blocks=%u;ref0=%d;al=%ld/%d;ar=%ld/%d\n",
            name, blocks, refout[0], (long)al.pcm, al.index, (long)ar.pcm, ar.index);
    uart_puts("CASE;"); uart_puts(name); uart_puts("\n"); uart_flush();
    memset(asmout, 0xA5, sizeof(asmout));
    asmout[30 * blocks] = (int16_t)0x5A5A;
    uint32_t cycles;
    if (mode == 2) cycles = decode_4bit_mono_asm(inbuf, asmout, &al, blocks);
    else if (mode == 1) cycles = decode_4bit_ms_asm(inbuf, asmout, &al, &ar, blocks);
    else cycles = decode_4bit_stereo_asm(inbuf, asmout, &al, &ar, blocks);

    int fail = 0;
    const char *why = "OK";
    int unwritten = 0;
    for (uint32_t i = 0; i < 30 * blocks; i++)
        if (asmout[i] == (int16_t)0xA5A5) unwritten++;
    if (asmout[30 * blocks] != (int16_t)0x5A5A) { fail = 1; why = "OUTPUT_GUARD"; }
    if (!fail) {
        for (uint32_t i = 0; i < 30 * blocks; i++) {
            if (asmout[i] != refout[i]) {
                fprintf(rep, "  %s first mismatch: block=%u sample=%u asm=%d ref=%d\n",
                        name, i / 30, i % 30, asmout[i], refout[i]);
                fail = 1; why = "PCM"; break;
            }
        }
    }
    if (!fail && (al.pcm != rl.pcm || al.index != rl.index)) { fail = 1; why = "STATE_L"; }
    if (!fail && mode != 2 && (ar.pcm != rr.pcm || ar.index != rr.index)) { fail = 1; why = "STATE_R"; }
    if (!fail && cycles == 0) { fail = 1; why = "CYCLES_ZERO"; }

    int bad = fail || (why[0] != 'O' || why[1] != 'K' || why[2] != 0);
    fprintf(rep, "%s;%s;blocks=%u;cycles=%u;stateL=%ld/%d;stateR=%ld/%d;unwritten=%d;%s\n",
            name, bad ? "FAIL" : "PASS", blocks, cycles,
            (long)al.pcm, al.index, (long)ar.pcm, ar.index, unwritten, why);
    printf("GOLDEN;%s;%s;blocks=%u;cycles=%u;%s\n", name, bad ? "FAIL" : "PASS", blocks, cycles, why);
    return bad;
}

int main(void) {
    __asm__ __volatile__("ori.w #0x0800,%%sr" ::: "cc");
    uart_init();
    uart_puts("BEGIN;GOLDENOPT\n"); uart_flush(); /* Apollo E-register task state */
    rep = fopen("Ram Disk:ADPCM_GOLDEN.txt", "w");
    if (!rep) rep = stdout;
    fprintf(rep, "ADPCM 4-BIT GOLDEN TEST\n");
    printf("ADPCM 4-BIT GOLDEN TEST\n");

    int fail = 0;
    fail |= run_case("STEREO", 0, 1, 1);
    fail |= run_case("STEREO", 0, 512, 0);
    fail |= run_case("MS", 1, 1, 1);
    fail |= run_case("MS", 1, 512, 0);
    fail |= run_case("MONO", 2, 1, 1);
    fail |= run_case("MONO", 2, 512, 0);

    fprintf(rep, "RESULT;%s\n", fail ? "FAIL" : "PASS");
    printf("RESULT;%s\n", fail ? "FAIL" : "PASS");
    uart_puts("END\n");
    if (rep != stdout) fclose(rep);
    return fail ? 1 : 0;
}
