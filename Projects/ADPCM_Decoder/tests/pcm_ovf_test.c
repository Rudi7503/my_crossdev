/* Isolated reference test for pcm_ovf.s: 16-bit add/sub + overflow clamp. */
#include <stdio.h>
#include <stdint.h>

extern int32_t pcm_ovf(int32_t pcm, int32_t delta, int32_t neg);

static int32_t ref(int32_t pcm, int32_t delta, int32_t neg) {
    int32_t v = neg ? (int32_t)pcm - delta : (int32_t)pcm + delta;
    if (v > 32767) v = 32767;
    if (v < -32768) v = -32768;
    return v;
}

static uint32_t rng_state = 0x12345678u;
static uint32_t rng(void) {
    rng_state ^= rng_state << 13;
    rng_state ^= rng_state >> 17;
    rng_state ^= rng_state << 5;
    return rng_state;
}

int main(void) {
    static const int deltas[] = {
        0, 1, 2, 3, 7, 8, 15, 16, 31, 32, 63, 64, 127, 128, 255, 256,
        511, 512, 1023, 1024, 2047, 2048, 4095, 4096, 8191, 8192,
        16383, 16384, 32766, 32767
    };
    const int nd = (int)(sizeof(deltas) / sizeof(deltas[0]));
    unsigned long count = 0;
    int32_t pcm, d, neg, got, exp;



    for (d = 0; d < nd; d++) {
        for (neg = 0; neg < 2; neg++) {
            for (pcm = -32768; pcm <= 32767; pcm++) {
                got = pcm_ovf(pcm, deltas[d], neg);
                exp = ref(pcm, deltas[d], neg);
                if (got != exp) {
                    printf("FAIL selected pcm=%ld delta=%ld neg=%ld got=%ld exp=%ld\n",
                           (long)pcm, (long)deltas[d], (long)neg, (long)got, (long)exp);
                    return 1;
                }
                count++;
            }
        }
    }

    for (unsigned long i = 0; i < 1000000UL; i++) {
        pcm = (int)(int16_t)rng();
        d = (int)(rng() & 0x7fff);
        neg = (int)(rng() & 1);
        got = pcm_ovf(pcm, d, neg);
        exp = ref(pcm, d, neg);
        if (got != exp) {
            printf("FAIL random pcm=%ld delta=%ld neg=%ld got=%ld exp=%ld\n",
                   (long)pcm, (long)d, (long)neg, (long)got, (long)exp);
            return 1;
        }
        count++;
    }

    printf("PCM_OVF;checked=%lu;selected=%d;random=1000000;PASS\n", count, nd * 2 * 65536);
    fflush(stdout);
    return 0;
}
