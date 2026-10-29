#include <stdio.h>
#include <stdint.h>

void test_pavgb_asm(uint8_t *src1, uint8_t *src2, uint8_t *dst);

int main() {
    uint8_t s1[8] = {10, 20, 30, 40, 50, 60, 70, 80};
    uint8_t s2[8] = {20, 40, 60, 80, 100, 120, 140, 160};
    uint8_t dst[8] = {0};

    test_pavgb_asm(s1, s2, dst);
    printf("PAVGB Result: %d %d %d %d %d %d %d %d\n",
           dst[0], dst[1], dst[2], dst[3], dst[4], dst[5], dst[6], dst[7]);

    // pavgb computes (s1 + s2 + 1) >> 1
    // (10 + 20 + 1) / 2 = 15
    // (20 + 40 + 1) / 2 = 30
    if (dst[0] == 15 && dst[1] == 30) {
        printf("PAVGB HARDWARE VERIFIED: PASS!\n");
    } else {
        printf("PAVGB MISMATCH!\n");
    }
    return 0;
}
