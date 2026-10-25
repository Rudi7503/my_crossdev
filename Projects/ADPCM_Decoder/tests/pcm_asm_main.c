#include <stdio.h>
#include <stdint.h>
extern uint32_t pcm_ovf_check_asm(void);
#define EXPECTED 917504UL
int main(void) {
    uint32_t n = pcm_ovf_check_asm();
    printf("PCM_ASM;count=%lu;expected=%lu;%s\n",
           (unsigned long)n, (unsigned long)EXPECTED,
           n == EXPECTED ? "PASS" : "FAIL");
    fflush(stdout);
    return n == EXPECTED ? 0 : 1;
}
