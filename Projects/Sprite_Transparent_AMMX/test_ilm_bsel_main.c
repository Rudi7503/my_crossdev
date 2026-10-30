#include <stdio.h>
#include <stdint.h>
#include <string.h>

extern void test_storeilm_vec(uint16_t *dst, const uint16_t *src, const uint16_t *key_vec);
extern void test_bsel_vec(uint16_t *dst, const uint16_t *src, const uint16_t *key_vec);

int main(void) {
    __asm__ __volatile__("ori.w #0x0800,%%sr" ::: "cc");

    uint16_t src[4] = { 0x2222, 0xF81F, 0x3333, 0xF81F };
    uint16_t key_vec[4] = { 0xF81F, 0xF81F, 0xF81F, 0xF81F };
    uint16_t dst[4];

    /* 1. Test storeilm */
    for (int i = 0; i < 4; i++) dst[i] = 0x5555;
    test_storeilm_vec(dst, src, key_vec);
    printf("1. storeilm (pcmpeqw vector + storeilm):\n");
    printf("   dst:      %04X %04X %04X %04X\n", dst[0], dst[1], dst[2], dst[3]);
    printf("   expected: 2222 5555 3333 5555\n");
    int ilm_ok = (dst[0]==0x2222 && dst[1]==0x5555 && dst[2]==0x3333 && dst[3]==0x5555);
    printf("   --> %s\n\n", ilm_ok ? "PASS" : "FAIL");

    /* 2. Test bsel + store */
    for (int i = 0; i < 4; i++) dst[i] = 0x7777;
    test_bsel_vec(dst, src, key_vec);
    printf("2. bsel (pcmpeqw vector + bsel + store):\n");
    printf("   dst:      %04X %04X %04X %04X\n", dst[0], dst[1], dst[2], dst[3]);
    printf("   expected: 2222 7777 3333 7777\n");
    int bsel_ok = (dst[0]==0x2222 && dst[1]==0x7777 && dst[2]==0x3333 && dst[3]==0x7777);
    printf("   --> %s\n\n", bsel_ok ? "PASS" : "FAIL");

    return (ilm_ok && bsel_ok) ? 0 : 1;
}
