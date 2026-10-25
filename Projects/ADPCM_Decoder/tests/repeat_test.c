/* 100x warmup + 100x measured single-block ADPCM 4-bit decode.
 * State is reset before every run; every output/state is compared to the C reference.
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include "lifecycle.h"
typedef struct __attribute__((packed, aligned(2))) { int32_t pcm; int16_t index; } Chan;
extern uint32_t decode_4bit_stereo_asm(const uint8_t*,int16_t*,void*,void*,uint32_t);
extern uint32_t decode_4bit_ms_asm(const uint8_t*,int16_t*,void*,void*,uint32_t);
extern uint32_t decode_4bit_mono_asm(const uint8_t*,int16_t*,void*,uint32_t);
static const int step_table[89]={7,8,9,10,11,12,13,14,16,17,19,21,23,25,28,31,34,37,41,45,50,55,60,66,73,80,88,97,107,118,130,143,157,173,190,209,230,253,279,307,337,371,408,449,494,544,598,658,724,796,876,963,1060,1166,1282,1411,1552,1707,1878,2066,2272,2499,2749,3024,3327,3660,4026,4428,4871,5358,5894,6484,7132,7845,8630,9493,10442,11487,12635,13899,15289,16818,18500,20350,22385,24623,27086,29794,32767};
static const int idx4[8]={-1,-1,-1,-1,2,4,6,8};
static int32_t cl(int32_t v,int32_t lo,int32_t hi){return v<lo?lo:(v>hi?hi:v);}
static int16_t refn(Chan*c,uint8_t n){int32_t s=step_table[c->index],d=s>>3;if(n&1)d+=s>>2;if(n&2)d+=s>>1;if(n&4)d+=s;if(n&8)c->pcm-=d;else c->pcm+=d;c->index+=idx4[n&7];c->index=(int16_t)cl(c->index,0,88);c->pcm=cl(c->pcm,-32768,32767);return (int16_t)c->pcm;}
static uint32_t rs=0x12345678u;
static uint32_t rng(void){rs^=rs<<13;rs^=rs>>17;rs^=rs<<5;return rs;}
static void setraw(Chan*c,uint32_t p,uint16_t i){uint8_t*b=(uint8_t*)c;b[0]=p>>24;b[1]=p>>16;b[2]=p>>8;b[3]=p;b[4]=i>>8;b[5]=i;}
static int cmp_u32(const void*a,const void*b){uint32_t x=*(const uint32_t*)a,y=*(const uint32_t*)b;return x<y?-1:(x>y?1:0);}
static FILE*rep;

static void run(const char*name,int mode){
    Chan bl,br,cl0,cr0; setraw(&bl,0xFFFF8000u,0); setraw(&br,0x00007FFFu,88);
    setraw(&cl0,0xFFFF8000u,0); setraw(&cr0,0x00007FFFu,88);
    uint8_t in[15]; for(int i=0;i<15;i++)in[i]=(uint8_t)rng();
    int16_t refout[30]; Chan rl=cl0,rr=cr0;
    for(int k=0;k<15;k++){uint8_t v=in[k];int16_t l=refn(&rl,v&15);
        if(mode==2){int16_t l1=refn(&rl,(v>>4)&15);refout[2*k]=l;refout[2*k+1]=l1;}
        else {int16_t r=refn(&rr,(v>>4)&15); if(mode==1){refout[2*k]=(int16_t)cl((int32_t)l+r,-32768,32767);refout[2*k+1]=(int16_t)cl((int32_t)l-r,-32768,32767);}else{refout[2*k]=l;refout[2*k+1]=r;}}}
    uart_puts("CASE;"); uart_puts(name); uart_puts("\n"); uart_flush();
    int16_t out[30]; uint32_t cyc[100]; int fail=0;
    for(int i=0;i<100;i++){Chan l=bl,r=br; if(mode==2)decode_4bit_mono_asm(in,out,&l,1); else if(mode==1)decode_4bit_ms_asm(in,out,&l,&r,1); else decode_4bit_stereo_asm(in,out,&l,&r,1);}
    for(int i=0;i<100;i++){
        Chan l=bl,r=br;
        if(mode==2)cyc[i]=decode_4bit_mono_asm(in,out,&l,1);
        else if(mode==1)cyc[i]=decode_4bit_ms_asm(in,out,&l,&r,1);
        else cyc[i]=decode_4bit_stereo_asm(in,out,&l,&r,1);
        if(memcmp(out,refout,60))fail=1;
        Chan el=cl0,er=cr0;
        for(int k=0;k<15;k++){uint8_t v=in[k];refn(&el,v&15); if(mode!=2)refn(&er,(v>>4)&15); else refn(&el,(v>>4)&15);}
        if(l.pcm!=el.pcm||l.index!=el.index) fail=1;
        if(mode!=2&&(r.pcm!=er.pcm||r.index!=er.index))fail=1;
    }
    uint32_t sorted[100]; memcpy(sorted,cyc,sizeof(cyc));
    qsort(sorted,100,4,cmp_u32);
    uint32_t sum=0; for(int i=0;i<100;i++)sum+=cyc[i];
    fprintf(rep,"%s;runs=100;min=%u;median=%u;max=%u;avg=%.1f;p95=%u;%s\n",
            name,sorted[0],sorted[50],sorted[99],sum/100.0,sorted[95],fail?"FAIL":"PASS");
    printf("REPEAT;%s;min=%u;median=%u;avg=%.1f;%s\n",name,sorted[0],sorted[50],sum/100.0,fail?"FAIL":"PASS");
}
int main(void){__asm__ __volatile__("ori.w #0x0800,%%sr":::"cc");TEST_START("repeat_test");rep=fopen("Ram Disk:ADPCM_REPEAT.txt","w");if(!rep)rep=stdout;fprintf(rep,"ADPCM 4-BIT 100x REPEAT (state reset, C reference)\n");run("STEREO",0);run("MS",1);run("MONO",2);TEST_DONE("repeat_test");if(rep!=stdout)fclose(rep);return 0;}
