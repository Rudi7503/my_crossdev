#include <stdio.h>
#include <stdint.h>
extern int32_t pcm_ovf(int32_t pcm, int32_t delta, int32_t neg);
int main(void) {
    int32_t a = pcm_ovf(-1, 1, 0);
    int32_t b = pcm_ovf(32767, 1, 0);
    int32_t c = pcm_ovf(-32768, 1, 1);
    printf("ONE32 %ld %ld %ld\n", (long)a, (long)b, (long)c);
    fflush(stdout);
    return 0;
}
