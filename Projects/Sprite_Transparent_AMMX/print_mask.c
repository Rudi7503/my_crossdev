#include <stdio.h>
#include <stdint.h>

extern void do_pcmpeqw_imm(uint16_t *mask, const uint16_t *src);
extern void do_pcmpeqw_vec(uint16_t *mask, const uint16_t *src, const uint16_t *key_vec);
extern void do_pcmpeqw_reg(uint16_t *mask, const uint16_t *src, uint32_t key_packed);

int main(void) {
    __asm__ __volatile__("ori.w #0x0800,%%sr" ::: "cc");

    uint16_t src[4] = { 0x2222, 0xF81F, 0x3333, 0xF81F };
    uint16_t key_vec[4] = { 0xF81F, 0xF81F, 0xF81F, 0xF81F };
    uint16_t mask[4] = { 0 };

    /* 1. Immediate #$F81F */
    do_pcmpeqw_imm(mask, src);
    printf("1. pcmpeqw #$F81F, e0, e2:\n   mask = %04X %04X %04X %04X\n",
           mask[0], mask[1], mask[2], mask[3]);

    /* 2. Vector e1 with 4x 0xF81F */
    do_pcmpeqw_vec(mask, src, key_vec);
    printf("2. pcmpeqw e1, e0, e2 (vector):\n   mask = %04X %04X %04X %04X\n",
           mask[0], mask[1], mask[2], mask[3]);

    /* 3. Register d0 with 0xF81FF81F */
    do_pcmpeqw_reg(mask, src, 0xF81FF81F);
    printf("3. pcmpeqw d0, e0, e2 (d0=0xF81FF81F):\n   mask = %04X %04X %04X %04X\n",
           mask[0], mask[1], mask[2], mask[3]);

    return 0;
}
