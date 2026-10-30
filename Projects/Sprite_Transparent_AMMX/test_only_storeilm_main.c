#include <stdio.h>
#include <stdint.h>
#include <string.h>

extern void test_only_storeilm(uint16_t *dst, const uint16_t *src, const uint16_t *key_vec);

int main(void) {
    __asm__ __volatile__("ori.w #0x0800,%%sr" ::: "cc");

    uint16_t src[4] = { 0x2222, 0xF81F, 0x3333, 0xF81F };
    uint16_t key_vec[4] = { 0xF81F, 0xF81F, 0xF81F, 0xF81F };
    uint16_t dst[4];

    for (int i = 0; i < 4; i++) dst[i] = 0x5555;
    test_only_storeilm(dst, src, key_vec);
    printf("storeilm (pcmpeqw vector + storeilm):\n");
    printf("dst:      %04X %04X %04X %04X\n", dst[0], dst[1], dst[2], dst[3]);
    printf("expected: 2222 5555 3333 5555\n");
    if (dst[0]==0x2222 && dst[1]==0x5555 && dst[2]==0x3333 && dst[3]==0x5555) {
        printf(">>> STOREILM PASS! <<<\n");
        return 0;
    } else {
        printf(">>> STOREILM FAIL! <<<\n");
        return 1;
    }
}
