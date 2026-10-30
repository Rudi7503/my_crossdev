#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

extern void pmula8(const uint8_t *a, const uint8_t *b, uint8_t *d);
extern void pmula_blit(const uint8_t *a, const uint8_t *b, uint8_t *out, int pixels);
extern uint32_t get_ccc(void);

static void ref8(const uint8_t *a, const uint8_t *b, uint8_t *d)
{
    int g;
    for (g = 0; g < 8; g += 4) {
        unsigned alpha = a[g];
        int k;
        d[g] = 0;
        for (k = 1; k < 4; k++) {
            unsigned v;
            if (alpha == 255) v = b[g+k];
            else if (alpha == 0) v = a[g+k];
            else v = (unsigned)a[g+k] + ((alpha * (unsigned)b[g+k]) >> 8);
            d[g+k] = (uint8_t)(v > 255 ? 255 : v);
        }
    }
}

static int check8(const char *name, const uint8_t *a, const uint8_t *b)
{
    uint8_t got[8], exp[8];
    int i;
    pmula8(a, b, got);
    ref8(a, b, exp);
    for (i = 0; i < 8; i++) {
        if (got[i] != exp[i]) {
            printf("FAIL %s byte%d got=%u exp=%u a=%02x%02x%02x%02x%02x%02x%02x%02x b=%02x%02x%02x%02x%02x%02x%02x%02x\n",
                   name, i, got[i], exp[i],
                   a[0],a[1],a[2],a[3],a[4],a[5],a[6],a[7],
                   b[0],b[1],b[2],b[3],b[4],b[5],b[6],b[7]);
            return 1;
        }
    }
    return 0;
}

static void ref_blit(const uint8_t *a, const uint8_t *b, uint8_t *out, int pixels)
{
    int p;
    for (p = 0; p < pixels; p++) ref8(&a[p*4], &b[p*4], &out[p*4]);
}

static uint32_t rng_state = 0x12345678;
static uint32_t rnd(void) { rng_state = rng_state * 1664525u + 1013904223u; return rng_state; }

int main(void)
{
    uint8_t a[8], b[8], out[8], ref[8];
    int alpha, i, n, fails = 0;

    printf("PMULA GOLDEN TEST\n");
    printf("CORE;AC68080;11350;PCR1088/1;VCoreRev0;92MHz\n");

    /* all alpha values, both pixels, fixed colors */
    for (alpha = 0; alpha < 256; alpha++) {
        a[0]=alpha; a[1]=10;  a[2]=20;  a[3]=30;
        a[4]=alpha; a[5]=200; a[6]=150; a[7]=100;
        b[0]=0; b[1]=200; b[2]=100; b[3]=50;
        b[4]=0; b[5]=7;   b[6]=240; b[7]=255;
        if (check8("alpha", a, b)) { fails++; if (fails > 8) break; }
    }
    printf("ALPHA_SWEEP;256;fails=%d\n", fails);

    /* random vectors */
    for (n = 0; n < 10000; n++) {
        for (i = 0; i < 8; i++) { a[i]=(uint8_t)rnd(); b[i]=(uint8_t)rnd(); }
        if (check8("random", a, b)) { fails++; if (fails > 20) break; }
    }
    printf("RANDOM;10000;fails=%d\n", fails);

    /* small direct golden cases for printout */
    alpha = 128;
    a[0]=alpha; a[1]=10; a[2]=20; a[3]=30; a[4]=alpha; a[5]=200; a[6]=150; a[7]=100;
    b[0]=0; b[1]=200; b[2]=100; b[3]=50; b[4]=0; b[5]=7; b[6]=240; b[7]=255;
    pmula8(a,b,out); ref8(a,b,ref);
    printf("VEC128;got=%02x%02x%02x%02x%02x%02x%02x%02x;exp=%02x%02x%02x%02x%02x%02x%02x%02x\n",
           out[0],out[1],out[2],out[3],out[4],out[5],out[6],out[7],
           ref[0],ref[1],ref[2],ref[3],ref[4],ref[5],ref[6],ref[7]);

    /* real ARGB32 sprite: a = background*(1-s), b = source RGB, s in a[0] */
    {
        static const int sizes[3] = {16, 32, 64};
        int si;
        for (si = 0; si < 3; si++) {
            int w = sizes[si], h = sizes[si], pixels = w*h, p;
            uint8_t *sa = malloc((size_t)pixels*4);
            uint8_t *sb = malloc((size_t)pixels*4);
            uint8_t *got = malloc((size_t)pixels*4);
            uint8_t *exp = malloc((size_t)pixels*4);
            int sfails = 0;
            for (p = 0; p < pixels; p++) {
                int s = (p * 37 + 11) & 255;
                int sr = (p * 13 + 20) & 255;
                int sg = (p * 29 + 40) & 255;
                int sbv = (p * 7 + 80) & 255;
                int br = (p * 3 + 5) & 255;
                int bg = (p * 5 + 9) & 255;
                int bb = (p * 11 + 13) & 255;
                sa[p*4+0]=(uint8_t)s;
                sa[p*4+1]=(uint8_t)((br*(255-s))/255);
                sa[p*4+2]=(uint8_t)((bg*(255-s))/255);
                sa[p*4+3]=(uint8_t)((bb*(255-s))/255);
                sb[p*4+0]=0; sb[p*4+1]=(uint8_t)sr; sb[p*4+2]=(uint8_t)sg; sb[p*4+3]=(uint8_t)sbv;
            }
            pmula_blit(sa, sb, got, pixels);
            for (p = 0; p < pixels; p++) ref8(&sa[p*4], &sb[p*4], &exp[p*4]);
            if (memcmp(got, exp, (size_t)pixels*4) != 0) sfails = 1;
            printf("SPRITE;%dx%d;diff=%d\n", w, h, sfails);
            if (sfails) fails++;
            free(sa); free(sb); free(got); free(exp);
        }
    }

    /* PMULA vs scalar-C cycles, 64x64 ARGB32, 100 measured calls */
    {
        int w=64,h=64,pixels=w*h,p;
        uint8_t *sa=malloc((size_t)pixels*4), *sb=malloc((size_t)pixels*4), *out=malloc((size_t)pixels*4);
        uint32_t t0,t1,best=0xffffffffu,total=0;
        int run;
        for (p=0;p<pixels;p++) {
            int s=(p*37+11)&255;
            sa[p*4+0]=(uint8_t)s; sa[p*4+1]=(uint8_t)((p*3+5)*(255-s)/255);
            sa[p*4+2]=(uint8_t)((p*5+9)*(255-s)/255); sa[p*4+3]=(uint8_t)((p*11+13)*(255-s)/255);
            sb[p*4+0]=0; sb[p*4+1]=(uint8_t)(p*13+20); sb[p*4+2]=(uint8_t)(p*29+40); sb[p*4+3]=(uint8_t)(p*7+80);
        }
        (void)out;
        for (run=0;run<100;run++) {
            t0=get_ccc(); pmula_blit(sa,sb,out,pixels); t1=get_ccc();
            if ((uint32_t)(t1-t0) < best) best=(uint32_t)(t1-t0);
            total += (uint32_t)(t1-t0);
        }
        printf("BENCH;PMULA;64x64;runs=100;min=%lu;avg=%lu;cycles_per_pixel_x1000=%lu\n",
               (unsigned long)best,(unsigned long)(total/100),(unsigned long)(best*1000/pixels));
        free(sa); free(sb); free(out);
    }

    /* scalar C reference, same 64x64 workload */
    {
        int w=64,h=64,pixels=w*h,p;
        uint8_t *sa=malloc((size_t)pixels*4), *sb=malloc((size_t)pixels*4), *out=malloc((size_t)pixels*4);
        uint32_t t0,t1,best=0xffffffffu,total=0;
        int run;
        for (p=0;p<pixels;p++) {
            int s=(p*37+11)&255;
            sa[p*4+0]=(uint8_t)s; sa[p*4+1]=(uint8_t)((p*3+5)*(255-s)/255);
            sa[p*4+2]=(uint8_t)((p*5+9)*(255-s)/255); sa[p*4+3]=(uint8_t)((p*11+13)*(255-s)/255);
            sb[p*4+0]=0; sb[p*4+1]=(uint8_t)(p*13+20); sb[p*4+2]=(uint8_t)(p*29+40); sb[p*4+3]=(uint8_t)(p*7+80);
        }
        for (run=0;run<100;run++) {
            t0=get_ccc(); ref_blit(sa,sb,out,pixels); t1=get_ccc();
            if ((uint32_t)(t1-t0) < best) best=(uint32_t)(t1-t0);
            total += (uint32_t)(t1-t0);
        }
        printf("BENCH;REF;64x64;runs=100;min=%lu;avg=%lu;cycles_per_pixel_x1000=%lu\n",
               (unsigned long)best,(unsigned long)(total/100),(unsigned long)(best*1000/pixels));
        free(sa); free(sb); free(out);
    }

    printf("RESULT;%s;fails=%d\n", fails ? "FAIL" : "PASS", fails);
    return fails ? 1 : 0;
}
