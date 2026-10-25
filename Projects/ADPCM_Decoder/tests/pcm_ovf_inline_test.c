/* Isolated PCM overflow test via inline asm; no external call ABI involved. */
#include <stdio.h>
#include <stdint.h>

__attribute__((noinline)) static int pcm_ovf_inline(int pcm, int delta, int neg) {
    __asm__ volatile(
        "        tst.w   %2\n\t"
        "        bne.s   1f\n\t"
        "        add.w   %1,%0\n\t"
        "        bra.s   2f\n"
        "1:      sub.w   %1,%0\n"
        "2:      bvc.s   5f\n\t"
        "        tst.w   %2\n\t"
        "        bne.s   3f\n\t"
        "        move.w  #32767,%0\n\t"
        "        bra.s   5f\n"
        "3:      move.w  #-32768,%0\n"
        "5:      ext.l   %0\n\t"
        : "+d"(pcm)
        : "d"(delta), "d"(neg)
        : "cc");
    return pcm;
}

static int ref(int pcm, int delta, int neg) {
    int32_t v = neg ? (int32_t)pcm - delta : (int32_t)pcm + delta;
    if (v > 32767) v = 32767;
    if (v < -32768) v = -32768;
    return (int)v;
}
static uint32_t rng_state = 0x12345678u;
static uint32_t rng(void) {
    rng_state ^= rng_state << 13;
    rng_state ^= rng_state >> 17;
    rng_state ^= rng_state << 5;
    return rng_state;
}
int main(void) {
    static const int deltas[] = {0,1,2,3,7,8,15,16,31,32,63,64,127,128,255,256,
                                 511,512,1023,1024,2047,2048,4095,4096,8191,8192,
                                 16383,16384,32766,32767};
    const int nd = (int)(sizeof deltas / sizeof deltas[0]);
    unsigned long count = 0;
    for (int d = 0; d < nd; d++)
        for (int neg = 0; neg < 2; neg++)
            for (int val = -32768; val <= 32767; val++) {
                int got = pcm_ovf_inline(val, deltas[d], neg);
                int exp = ref(val, deltas[d], neg);
                if (got != exp) { printf("FAIL pcm=%d delta=%d neg=%d got=%d exp=%d\n", val, deltas[d], neg, got, exp); return 1; }
                count++;
            }
    for (unsigned long i = 0; i < 1000000UL; i++) {
        int val = (int)(int16_t)rng();
        int d = (int)(rng() & 0x7fff);
        int neg = (int)(rng() & 1);
        int got = pcm_ovf_inline(val, d, neg);
        int exp = ref(val, d, neg);
        if (got != exp) { printf("FAIL random pcm=%d delta=%d neg=%d got=%d exp=%d\n", val, d, neg, got, exp); return 1; }
        count++;
    }
    printf("PCM_OVF_INLINE;checked=%lu;PASS\n", count);
    fflush(stdout);
    return 0;
}
