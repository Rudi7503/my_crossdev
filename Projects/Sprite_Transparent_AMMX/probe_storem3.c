#include <stdio.h>
#include <stdint.h>
#include <string.h>

extern void test_storem3_b(uint8_t *dst, const uint8_t *src, int mode);

int main(void) {
    __asm__ __volatile__("ori.w #0x0800,%%sr" ::: "cc");

    uint8_t dst[16] __attribute__((aligned(16)));
    uint8_t src[16] __attribute__((aligned(16)));

    // Pattern: 00 11 22 00 FF F8 1F 80
    src[0] = 0x00; src[1] = 0x11; src[2] = 0x22; src[3] = 0x00;
    src[4] = 0xFF; src[5] = 0xF8; src[6] = 0x1F; src[7] = 0x80;

    printf("PROBING storem3 MODES 0..7 with src: 00 11 22 00 FF F8 1F 80\n");
    for (int mode = 0; mode <= 7; mode++) {
        memset(dst, 0xEE, 8);
        test_storem3_b(dst, src, mode);
        printf("Mode %d: ", mode);
        for (int i = 0; i < 8; i++) {
            printf("%02X ", dst[i]);
        }
        printf("\n");
    }

    return 0;
}
