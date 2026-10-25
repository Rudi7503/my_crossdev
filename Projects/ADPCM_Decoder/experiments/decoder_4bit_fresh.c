/* Independent C implementation for comparison; not derived from the asm template. */
#include <stdint.h>
typedef struct __attribute__((packed, aligned(2))) { int32_t pcm; int16_t index; } Chan;
extern uint32_t get_ccc(void);

static const int16_t step_table[89] = {
7,8,9,10,11,12,13,14,16,17,19,21,23,25,28,31,
34,37,41,45,50,55,60,66,73,80,88,97,107,118,130,143,
157,173,190,209,230,253,279,307,337,371,408,449,494,544,598,658,
724,796,876,963,1060,1166,1282,1411,1552,1707,1878,2066,2272,2499,2749,3024,
3327,3660,4026,4428,4871,5358,5894,6484,7132,7845,8630,9493,10442,11487,12635,13899,
15289,16818,18500,20350,22385,24623,27086,29794,32767
};
static const int8_t idx4[8] = { -1,-1,-1,-1,2,4,6,8 };

static inline int32_t clamp32(int32_t v, int32_t lo, int32_t hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}
static inline int16_t dec(Chan *c, uint8_t n) {
    int32_t s = step_table[c->index];
    int32_t d = s >> 3;
    if (n & 1) d += s >> 2;
    if (n & 2) d += s >> 1;
    if (n & 4) d += s;
    if (n & 8) c->pcm -= d; else c->pcm += d;
    c->index += idx4[n & 7];
    c->index = (int16_t)clamp32(c->index, 0, 88);
    c->pcm = clamp32(c->pcm, -32768, 32767);
    return (int16_t)c->pcm;
}

uint32_t decode_4bit_stereo_asm(const uint8_t *in, int16_t *out, void *lp, void *rp, uint32_t blocks) {
    Chan *l = lp, *r = rp;
    uint32_t t0 = get_ccc();
    for (uint32_t b = 0; b < blocks; b++) {
        for (int k = 0; k < 15; k++) {
            uint8_t v = in[b * 15 + k];
            out[b * 30 + 2 * k] = dec(l, v & 15);
            out[b * 30 + 2 * k + 1] = dec(r, v >> 4);
        }
    }
    return get_ccc() - t0;
}

uint32_t decode_4bit_ms_asm(const uint8_t *in, int16_t *out, void *mp, void *sp, uint32_t blocks) {
    Chan *m = mp, *s = sp;
    uint32_t t0 = get_ccc();
    for (uint32_t b = 0; b < blocks; b++) {
        for (int k = 0; k < 15; k++) {
            uint8_t v = in[b * 15 + k];
            int16_t mid = dec(m, v & 15);
            int16_t side = dec(s, v >> 4);
            out[b * 30 + 2 * k] = (int16_t)clamp32((int32_t)mid + side, -32768, 32767);
            out[b * 30 + 2 * k + 1] = (int16_t)clamp32((int32_t)mid - side, -32768, 32767);
        }
    }
    return get_ccc() - t0;
}

uint32_t decode_4bit_mono_asm(const uint8_t *in, int16_t *out, void *mp, uint32_t blocks) {
    Chan *m = mp;
    uint32_t t0 = get_ccc();
    for (uint32_t b = 0; b < blocks; b++) {
        for (int k = 0; k < 15; k++) {
            uint8_t v = in[b * 15 + k];
            out[b * 30 + 2 * k] = dec(m, v & 15);
            out[b * 30 + 2 * k + 1] = dec(m, v >> 4);
        }
    }
    return get_ccc() - t0;
}
