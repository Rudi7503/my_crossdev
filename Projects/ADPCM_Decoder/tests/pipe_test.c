#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "uart_asm.h"
typedef struct __attribute__((packed, aligned(2))) { int32_t pcm; int16_t index; } Chan;
extern uint32_t decode_4bit_stereo_asm(const uint8_t*,int16_t*,void*,void*,uint32_t);
extern uint32_t decode_4bit_ms_asm(const uint8_t*,int16_t*,void*,void*,uint32_t);
extern uint32_t decode_4bit_mono_asm(const uint8_t*,int16_t*,void*,uint32_t);
extern uint32_t read_iep1(void), read_iep2(void), read_iep3(void);
static uint32_t rs=0x12345678u;
static uint32_t rng(void){rs^=rs<<13;rs^=rs>>17;rs^=rs<<5;return rs;}
#define B 512
static uint8_t inbuf[15*B]; static int16_t out[30*B]; static FILE *rep;
static void run(const char *name,int mode){
    Chan l={0,0},r={0,0};
    l.pcm=(int32_t)(rng()&0xffff)-32768; l.index=(int16_t)(rng()%89);
    r.pcm=(int32_t)(rng()&0xffff)-32768; r.index=(int16_t)(rng()%89);
    Chan il=l,ir=r;
    for(int i=0;i<15*B;i++)inbuf[i]=(uint8_t)rng();
    uart_puts("CASE;"); uart_puts(name); uart_puts("\n"); uart_flush();
    Chan wl=l,wr=r;
    if(mode==2) decode_4bit_mono_asm(inbuf,out,&wl,128);
    else if(mode==1) decode_4bit_ms_asm(inbuf,out,&wl,&wr,128);
    else decode_4bit_stereo_asm(inbuf,out,&wl,&wr,128);
    l=il; r=ir;
    uint32_t s1=read_iep1(),s2=read_iep2(),s3=read_iep3();
    uint32_t cyc;
    if(mode==2) cyc=decode_4bit_mono_asm(inbuf,out,&l,B);
    else if(mode==1) cyc=decode_4bit_ms_asm(inbuf,out,&l,&r,B);
    else cyc=decode_4bit_stereo_asm(inbuf,out,&l,&r,B);
    uint32_t e1=read_iep1(),e2=read_iep2(),e3=read_iep3();
    uint32_t d1=e1-s1,d2=e2-s2,d3=e3-s3;
    fprintf(rep,"%s;blocks=%d;cycles=%u;IEP1=%u;IEP2=%u;IEP3=%u;per_block=%u,%u,%u\n",
            name,B,cyc,d1,d2,d3,d1/B,d2/B,d3/B);
    printf("PIPE;%s;IEP1=%u;IEP2=%u;IEP3=%u\n",name,d1,d2,d3);
}
int main(void){
    __asm__ __volatile__("ori.w #0x0800,%%sr" ::: "cc");
    uart_init(); uart_puts("BEGIN;PIPE3\n"); uart_flush();
    rep=fopen("Ram Disk:ADPCM_PIPE.txt","w"); if(!rep)rep=stdout;
    fprintf(rep,"ADPCM 4-BIT PIPE USAGE (512 blocks, 128 warmup)\n");
    run("STEREO",0); run("MS",1); run("MONO",2);
    uart_puts("END\n"); uart_flush();
    if(rep!=stdout)fclose(rep);
    return 0;
}
