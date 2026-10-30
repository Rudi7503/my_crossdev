#include <stdio.h>
#include <stdint.h>

extern void do_pcmpeqw_vec(uint16_t *mask, const uint16_t *src, const uint16_t *key_vec);

int main(void) {
    __asm__ __volatile__("ori.w #0x0800,%%sr" ::: "cc");

    uint16_t src[4] = { 0x2222, 0xF81F, 0x3333, 0xF81F };
    uint16_t key_vec[4] = { 0xF81F, 0xF81F, 0xF81F, 0xF81F };
    uint16_t mask[4] = { 0 };

    do_pcmpeqw_vec(mask, src, key_vec);
    printf("pcmpeqw e1, e0, e2 (vector):\n");
    printf("src:  %04X %04X %04X %04X\n", src[0], src[1], src[2], src[3]);
    printf("mask: %04X %04X %04X %04X\n", mask[0], mask[1], mask[2], mask[3]);
    printf("Expected: 0000 FFFF 0000 FFFF\n");

    if (mask[0] == 0x0000 && mask[1] == 0xFFFF && mask[2] == 0x0000 && mask[3] == 0xFFFF) {
        printf(">>> VECTOR COMPARE MATCHES 100%% EXACT! <<<\n");
        return 0;
    } else {
        printf(">>> MISMATCH! <<<\n");
        return 1;
    }
}
