#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

typedef struct __attribute__((packed, aligned(2))) { int32_t pcm; int16_t index; } Chan;
extern uint32_t decode_6bit_stereo_asm(const uint8_t*,int16_t*,void*,void*,uint32_t);
extern uint32_t decode_6bit_ms_asm(const uint8_t*,int16_t*,void*,void*,uint32_t);
extern uint32_t decode_6bit_mono_asm(const uint8_t*,int16_t*,void*,uint32_t);

static uint8_t in[15];
static int16_t out[120];
static uint32_t rs=0x12345678u;
static uint32_t rng(void){rs^=rs<<13;rs^=rs>>17;rs^=rs<<5;return rs;}
static void setraw(Chan*c,uint32_t p,uint16_t i){uint8_t*b=(uint8_t*)c;b[0]=p>>24;b[1]=p>>16;b[2]=p>>8;b[3]=p;b[4]=i>>8;b[5]=i;}
static int cmp_u32(const void*a,const void*b){uint32_t x=*(const uint32_t*)a,y=*(const uint32_t*)b;return x<y?-1:(x>y?1:0);}

static void run(const char*name,int mode){
    Chan bl,br; setraw(&bl,0xFFFF8000u,0); setraw(&br,0x00007FFFu,88);
    rs=0x12345678u;
    for(int i=0;i<15;i++) in[i]=(uint8_t)rng();
    uint32_t cyc[100];
    for(int i=0;i<100;i++){
        Chan l=bl,r=br;
        if(mode==2) decode_6bit_mono_asm(in,out,&l,1);
        else if(mode==1) decode_6bit_ms_asm(in,out,&l,&r,1);
        else decode_6bit_stereo_asm(in,out,&l,&r,1);
    }
    for(int i=0;i<100;i++){
        Chan l=bl,r=br;
        if(mode==2) cyc[i]=decode_6bit_mono_asm(in,out,&l,1);
        else if(mode==1) cyc[i]=decode_6bit_ms_asm(in,out,&l,&r,1);
        else cyc[i]=decode_6bit_stereo_asm(in,out,&l,&r,1);
    }
    qsort(cyc,100,4,cmp_u32);
    printf("REPEAT6;%s;min=%lu;median=%lu;p95=%lu\n",name,(unsigned long)cyc[0],(unsigned long)cyc[50],(unsigned long)cyc[95]);
}

int main(void){
    __asm__ __volatile__("ori.w #0x0800,%%sr":::"cc");
    setbuf(stdout,NULL);
    run("STEREO",0);
    run("MS",1);
    run("MONO",2);
    return 0;
}
