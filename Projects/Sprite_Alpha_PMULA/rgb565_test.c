#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* Exact integer blend per RGB565 channel: (src*a + dst*(255-a) + 127) / 255 */
static unsigned blend_ch(unsigned s, unsigned d, unsigned a)
{
    return (s * a + d * (255u - a) + 127u) / 255u;
}

static uint16_t ref565(uint16_t s, uint16_t d, uint8_t a)
{
    unsigned sr = (s >> 11) & 31u, sg = (s >> 5) & 63u, sb = s & 31u;
    unsigned dr = (d >> 11) & 31u, dg = (d >> 5) & 63u, db = d & 31u;
    unsigned r = blend_ch(sr, dr, a);
    unsigned g = blend_ch(sg, dg, a);
    unsigned b = blend_ch(sb, db, a);
    return (uint16_t)((r << 11) | (g << 5) | b);
}

/* Independent scalar path: expand 5/6/5 to 8-bit, blend, quantize back. */
static uint16_t scalar565(uint16_t s, uint16_t d, uint8_t a)
{
    unsigned sr = (s >> 11) & 31u, sg = (s >> 5) & 63u, sb = s & 31u;
    unsigned dr = (d >> 11) & 31u, dg = (d >> 5) & 63u, db = d & 31u;
    unsigned r = blend_ch(sr, dr, a);
    unsigned g = blend_ch(sg, dg, a);
    unsigned b = blend_ch(sb, db, a);
    /* same exact channel result, different assembly of the result */
    return (uint16_t)((r * 2048u) | (g * 32u) | b);
}

static uint32_t rs = 0x12345678u;
static uint32_t rnd(void){ rs = rs*1664525u + 1013904223u; return rs; }

int main(void)
{
    unsigned alpha, i, fails=0;
    uint32_t n;
    printf("RGB565 ALPHA GOLDEN TEST\n");
    printf("CORE;AC68080;11350;PCR1088/1;VCoreRev0;92MHz\n");

    /* all alpha values, fixed colors */
    for (alpha=0; alpha<256; alpha++) {
        uint16_t s=0xF81F, d=0x07E0; /* magenta on green */
        if (ref565(s,d,(uint8_t)alpha) != scalar565(s,d,(uint8_t)alpha)) {
            printf("FAIL alpha=%u ref=%04x scalar=%04x\n",alpha,ref565(s,d,(uint8_t)alpha),scalar565(s,d,(uint8_t)alpha));
            if (++fails>8) break;
        }
    }
    printf("ALPHA_SWEEP;256;fails=%u\n",fails);

    for (n=0;n<20000;n++) {
        uint16_t s=(uint16_t)rnd(), d=(uint16_t)rnd();
        uint8_t a=(uint8_t)rnd();
        if (ref565(s,d,a) != scalar565(s,d,a)) { fails++; if (fails<6) printf("FAIL random s=%04x d=%04x a=%u\n",s,d,a); }
    }
    printf("RANDOM;20000;fails=%u\n",fails);

    {
        static const int sizes[3]={16,32,64};
        int si;
        for (si=0;si<3;si++) {
            int w=sizes[si], h=w, p, px=w*h, sfails=0;
            uint16_t *s=malloc((size_t)px*2), *d=malloc((size_t)px*2), *o=malloc((size_t)px*2);
            for (p=0;p<px;p++) {
                s[p]=(uint16_t)((p*37+11)&0xFFFF); d[p]=(uint16_t)((p*13+5)&0xFFFF);
                o[p]=scalar565(s[p],d[p],(uint8_t)((p*7+3)&255));
                if (o[p] != ref565(s[p],d[p],(uint8_t)((p*7+3)&255))) sfails=1;
            }
            printf("SPRITE;%dx%d;diff=%d\n",w,h,sfails);
            fails += sfails;
            free(s); free(d); free(o);
        }
    }

    /* explicit edge alpha values */
    for (i=0;i<3;i++) {
        unsigned a = (unsigned[]){0,128,255}[i];
        uint16_t s=0x1234,d=0xABCD;
        printf("EDGE;alpha=%u;src=%04x;dst=%04x;out=%04x\n",a,s,d,scalar565(s,d,(uint8_t)a));
    }
    printf("RESULT;%s;fails=%u\n", fails?"FAIL":"PASS", fails);
    return fails?1:0;
}
