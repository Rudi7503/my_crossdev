#include <stdio.h>
#include <stdint.h>

void test_bflyw(int16_t *src1, int16_t *src2, int16_t *dst_sum, int16_t *dst_sub);

int main() {
    int16_t s1[4] = {100, 200, 300, 400};
    int16_t s2[4] = {10,  20,  30,  40};
    int16_t d_sum[4] = {0};
    int16_t d_sub[4] = {0};

    printf("Testing BFLYW on Apollo 68080...\n");
    test_bflyw(s1, s2, d_sum, d_sub);
    printf("Sum: %d %d %d %d\n", d_sum[0], d_sum[1], d_sum[2], d_sum[3]);
    printf("Sub: %d %d %d %d\n", d_sub[0], d_sub[1], d_sub[2], d_sub[3]);

    if (d_sum[0] == 110 && d_sub[0] == 90) {
        printf("BFLYW HARDWARE TEST PASSED!\n");
    } else {
        printf("BFLYW TEST MISMATCH!\n");
    }
    return 0;
}
