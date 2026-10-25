#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "lifecycle.h"
typedef struct __attribute__((packed, aligned(2))) { int32_t pcm; int16_t index; } Chan;
extern uint32_t decode_4bit_stereo_asm(const uint8_t*,int16_t*,void*,void*,uint32_t);
extern uint32_t decode_4bit_ms_asm(const uint8_t*,int16_t*,void*,void*,uint32_t);
extern uint32_t decode_4bit_mono_asm(const uint8_t*,int16_t*,void*,uint32_t);
extern uint32_t prof_prim_cycles, prof_sec_cycles, prof_prim_calls, prof_sec_calls;
static uint32_t rs=0x12345678u;
static uint32_t rng(void){rs^=rs<<13;rs^=rs>>17;rs^=rs<<5;return rs;}
#define B 512
static uint8_t inbuf[15*B]; static int16_t out[30*B];
static FILE *rep;
static void run(const char *name,int mode){
    Chan l={0,0},r={0,0};
    l.pcm=(int32_t)(rng()&0xffff)-32768; l.index=(int16_t)(rng()%89);
    r.pcm=(int32_t)(rng()&0xffff)-32768; r.index=(int16_t)(rng()%89);
    for(int i=0;i<15*B;i++)inbuf[i]=(uint8_t)rng();
    uart_puts("CASE;"); uart_puts(name); uart_puts("\n"); uart_flush();
    Chan wl=l,wr=r;
    if(mode==2) decode_4bit_mono_asm(inbuf,out,&wl,B);
    else if(mode==1) decode_4bit_ms_asm(inbuf,out,&wl,&wr,B);
    else decode_4bit_stereo_asm(inbuf,out,&wl,&wr,B);
    prof_prim_cycles=prof_sec_cycles=prof_prim_calls=prof_sec_calls=0;
    uint32_t cyc;
    if(mode==2) cyc=decode_4bit_mono_asm(inbuf,out,&l,B);
    else if(mode==1) cyc=decode_4bit_ms_asm(inbuf,out,&l,&r,B);
    else cyc=decode_4bit_stereo_asm(inbuf,out,&l,&r,B);
    uint32_t rest=cyc-prof_prim_cycles-prof_sec_cycles;
    fprintf(rep,"%s;blocks=%d;total=%u;prim=%u;prim_calls=%u;prim_avg=%.1f;sec=%u;sec_calls=%u;sec_avg=%.1f;rest=%u;rest_per_block=%.1f\n",
            name,B,cyc,prof_prim_cycles,prof_prim_calls,prof_prim_calls?1.0*prof_prim_cycles/prof_prim_calls:0.0,
            prof_sec_cycles,prof_sec_calls,prof_sec_calls?1.0*prof_sec_cycles/prof_sec_calls:0.0,
            rest,1.0*rest/B);
    printf("PROF;%s;total=%u;prim=%u;sec=%u;rest=%u\n",name,cyc,prof_prim_cycles,prof_sec_cycles,rest);
}
int main(void){
    __asm__ __volatile__("ori.w #0x0800,%%sr" ::: "cc");
    TEST_START("profile_test");
    rep=fopen("Ram Disk:ADPCM_PROFILE.txt","w"); if(!rep)rep=stdout;
    fprintf(rep,"ADPCM 4-BIT SECTION PROFILE (instrumented)\n");
    printf("ADPCM 4-BIT SECTION PROFILE\n");
    run("STEREO",0); run("STEREO2",0); run("MS",1); run("MS2",1); run("MONO",2);
    TEST_DONE("profile_test");
    if(rep!=stdout)fclose(rep);
    return 0;
}
