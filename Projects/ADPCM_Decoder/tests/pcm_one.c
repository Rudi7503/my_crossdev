#include <stdio.h>
extern int pcm_ovf(int pcm, int delta, int neg);
int main(void) {
    int a = pcm_ovf(-1, 1, 0);
    printf("ONE %d\n", a);
    fflush(stdout);
    return 0;
}
