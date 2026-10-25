#include <stdio.h>
int main(void) {
    printf("CRASH;START\n");
    fflush(stdout);
    __asm__ __volatile__("illegal");
    return 0;
}
