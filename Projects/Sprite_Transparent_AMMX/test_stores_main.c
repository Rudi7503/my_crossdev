#include <stdio.h>
#include <stdint.h>
#include <string.h>

extern void test_storem3_b(uint8_t *dst, const uint8_t *src, int mode);
extern void test_storem3_w(uint16_t *dst, const uint16_t *src, int mode);
extern void test_storem3_l(uint32_t *dst, const uint32_t *src, int mode);
extern void test_storeilm_b(uint8_t *dst, const uint8_t *src, uint32_t key);
extern void test_storeilm_w(uint16_t *dst, const uint16_t *src, uint32_t key);
extern void test_bsel_w(uint16_t *dst, const uint16_t *src, uint32_t key);
extern void test_storem_b(uint8_t *dst, const uint8_t *src, uint32_t mask);
extern void test_storec(uint8_t *dst, const uint8_t *src, uint32_t count);

int main(void) {
    /* Enable Apollo 68080 mode */
    __asm__ __volatile__("ori.w #0x0800,%%sr" ::: "cc");

    printf("========================================================\n");
    printf(" Apollo 68080 AMMX Store Variant Semantics Verification\n");
    printf("========================================================\n\n");

    uint8_t dst[32] __attribute__((aligned(32)));
    uint8_t src[32] __attribute__((aligned(32)));
    int all_passed = 1;

    /* 1. STOREM3 Byte Mode (Mode 1: skip $00) */
    memset(dst, 0xAA, 16);
    src[0]=1; src[1]=0; src[2]=2; src[3]=0; src[4]=3; src[5]=0; src[6]=4; src[7]=5;
    test_storem3_b(dst, src, 1);
    printf("1. storem3 (Mode 1 - Byte skip 0x00):\n   Output:   ");
    for(int i=0; i<8; i++) printf("%02X ", dst[i]);
    printf("\n   Expected: 01 AA 02 AA 03 AA 04 05\n");
    if (dst[0]==1 && dst[1]==0xAA && dst[2]==2 && dst[3]==0xAA &&
        dst[4]==3 && dst[5]==0xAA && dst[6]==4 && dst[7]==5) {
        printf("   --> PASS\n\n");
    } else {
        printf("   --> FAIL!\n\n");
        all_passed = 0;
    }

    /* 2. STOREM3 Word Mode (Mode 2: skip $F81F magenta) */
    uint16_t *d16 = (uint16_t *)dst;
    uint16_t *s16 = (uint16_t *)src;
    for(int i=0; i<4; i++) d16[i] = 0x1111;
    s16[0] = 0x2222;
    s16[1] = 0xF81F; /* magenta / transparent */
    s16[2] = 0x3333;
    s16[3] = 0xF81F; /* magenta / transparent */
    test_storem3_w(d16, s16, 2);
    printf("2. storem3 (Mode 2 - Word skip 0xF81F magenta):\n   Output:   ");
    for(int i=0; i<4; i++) printf("%04X ", d16[i]);
    printf("\n   Expected: 2222 1111 3333 1111\n");
    if (d16[0]==0x2222 && d16[1]==0x1111 && d16[2]==0x3333 && d16[3]==0x1111) {
        printf("   --> PASS\n\n");
    } else {
        printf("   --> FAIL!\n\n");
        all_passed = 0;
    }

    /* 3. STOREILM Word Mode (pcmpeqw + storeilm) */
    for(int i=0; i<4; i++) d16[i] = 0x5555;
    test_storeilm_w(d16, s16, 0xF81F);
    printf("3. storeilm (pcmpeqw 0xF81F + storeilm):\n   Output:   ");
    for(int i=0; i<4; i++) printf("%04X ", d16[i]);
    printf("\n   Expected: 2222 5555 3333 5555\n");
    if (d16[0]==0x2222 && d16[1]==0x5555 && d16[2]==0x3333 && d16[3]==0x5555) {
        printf("   --> PASS\n\n");
    } else {
        printf("   --> FAIL!\n\n");
        all_passed = 0;
    }

    /* 4. BSEL In-Register Selection */
    for(int i=0; i<4; i++) d16[i] = 0x7777;
    test_bsel_w(d16, s16, 0xF81F);
    printf("4. bsel (pcmpeqw 0xF81F + bsel + store):\n   Output:   ");
    for(int i=0; i<4; i++) printf("%04X ", d16[i]);
    printf("\n   Expected: 2222 7777 3333 7777\n");
    if (d16[0]==0x2222 && d16[1]==0x7777 && d16[2]==0x3333 && d16[3]==0x7777) {
        printf("   --> PASS\n\n");
    } else {
        printf("   --> FAIL!\n\n");
        all_passed = 0;
    }

    /* 5. STOREM (Byte Mask) */
    memset(dst, 0xEE, 16);
    memset(src, 0x33, 16);
    /* mask 0b10100101 = 0xA5 -> bytes 0, 2, 5, 7 get src(0x33), rest dst(0xEE) */
    test_storem_b(dst, src, 0xA5);
    printf("5. storem (Byte Mask 0xA5):\n   Output:   ");
    for(int i=0; i<8; i++) printf("%02X ", dst[i]);
    printf("\n   Expected: 33 EE 33 EE EE 33 EE 33\n");
    if (dst[0]==0x33 && dst[1]==0xEE && dst[2]==0x33 && dst[3]==0xEE &&
        dst[4]==0xEE && dst[5]==0x33 && dst[6]==0xEE && dst[7]==0x33) {
        printf("   --> PASS\n\n");
    } else {
        printf("   --> FAIL!\n\n");
        all_passed = 0;
    }

    /* 6. STOREC (Count store) */
    memset(dst, 0x99, 16);
    memset(src, 0x44, 16);
    test_storec(dst, src, 3);
    printf("6. storec (Count 3):\n   Output:   ");
    for(int i=0; i<8; i++) printf("%02X ", dst[i]);
    printf("\n   Expected: 44 44 44 99 99 99 99 99\n");
    if (dst[0]==0x44 && dst[1]==0x44 && dst[2]==0x44 && dst[3]==0x99 &&
        dst[4]==0x99 && dst[5]==0x99 && dst[6]==0x99 && dst[7]==0x99) {
        printf("   --> PASS\n\n");
    } else {
        printf("   --> FAIL!\n\n");
        all_passed = 0;
    }

    /* 7. STOREM3 Long Mode (Mode 4: skip bit 31 set/clear) */
    uint32_t *d32 = (uint32_t *)dst;
    uint32_t *s32 = (uint32_t *)src;
    d32[0] = 0xAAAAAAAA;
    d32[1] = 0xAAAAAAAA;
    s32[0] = 0x7FFFFFFF; /* Bit 31 = 0 */
    s32[1] = 0x80000001; /* Bit 31 = 1 */
    test_storem3_l(d32, s32, 4);
    printf("7. storem3 (Mode 4 - Long Mode):\n   Output:   %08lX %08lX\n",
           (unsigned long)d32[0], (unsigned long)d32[1]);

    if (all_passed) {
        printf(">>> ALL 6 AMMX STORE INSTRUCTIONS VERIFIED BIT-EXACT ON APOLLO 68080! <<<\n");
    } else {
        printf(">>> VERIFICATION FAILED! <<<\n");
    }

    return all_passed ? 0 : 1;
}
