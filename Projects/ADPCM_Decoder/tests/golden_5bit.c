#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
typedef struct __attribute__((packed, aligned(2))) { int32_t pcm; int16_t index; } Chan;
extern uint32_t decode_5bit_stereo_asm(const uint8_t*,int16_t*,void*,void*,uint32_t);
extern uint32_t decode_5bit_ms_asm(const uint8_t*,int16_t*,void*,void*,uint32_t);
extern uint32_t decode_5bit_mono_asm(const uint8_t*,int16_t*,void*,uint32_t);
static const int st[89]={7,8,9,10,11,12,13,14,16,17,19,21,23,25,28,31,34,37,41,45,50,55,60,66,73,80,88,97,107,118,130,143,157,173,190,209,230,253,279,307,337,371,408,449,494,544,598,658,724,796,876,963,1060,1166,1282,1411,1552,1707,1878,2066,2272,2499,2749,3024,3327,3660,4026,4428,4871,5358,5894,6484,7132,7845,8630,9493,10442,11487,12635,13899,15289,16818,18500,20350,22385,24623,27086,29794,32767};
static int32_t cl(int32_t v,int32_t a,int32_t b){return v<a?a:(v>b?b:v);}
static int16_t dec(Chan*c,int code){
    int32_t s=st[c->index],d=s>>4; if(code&1)d+=s>>3; if(code&2)d+=s>>2; if(code&4)d+=s>>1; if(code&8)d+=s;
    if(code&16)c->pcm-=d; else c->pcm+=d;
    { static const int ia[16]={-1,-1,-1,-1,-1,-1,-1,-1,1,2,4,6,8,10,13,16}; c->index+=ia[code&15]; }
    c->index=cl(c->index,0,88); c->pcm=cl(c->pcm,-32768,32767); return c->pcm;
}
static uint32_t readb(const uint8_t*b,int*p,int n){uint32_t r=0;for(int i=0;i<n;i++){r|=((b[*p>>3]>>(*p&7))&1u)<<i;(*p)++;}return r;}
#define B 512
static uint8_t in[15*B]; static int16_t ao[2*120*B+8],ro[2*120*B+8];
static uint32_t rs=0x12345678u; static uint32_t rng(void){rs^=rs<<13;rs^=rs>>17;rs^=rs<<5;return rs;}
static int run(int mode){
    Chan a={0,0},b={0,0},ra={0,0},rb={0,0}; int oi=0,pos=0; uint32_t cyc=0;
    for(int blk=0;blk<B;blk++){
        const uint8_t*bp=in+15*blk;
        if(mode==0)cyc+=decode_5bit_stereo_asm(bp,ao+oi,&a,&b,1);
        else if(mode==1)cyc+=decode_5bit_ms_asm(bp,ao+oi,&a,&b,1);
        else cyc+=decode_5bit_mono_asm(bp,ao+oi,&a,1);
        int pairs=(mode==2)?24:12;
        for(int i=0;i<pairs;i++){
            int l=dec(&ra,readb(in,&pos,5));
            if(mode==2){ro[oi++]=l;continue;}
            int r=dec(&rb,readb(in,&pos,5));
            if(mode==1){ro[oi++]=(int16_t)cl((int32_t)l+r,-32768,32767);ro[oi++]=(int16_t)cl((int32_t)l-r,-32768,32767);}
            else {ro[oi++]=(int16_t)l;ro[oi++]=(int16_t)r;}
        }
    }
    for(int i=0;i<oi;i++) if(ao[i]!=ro[i]){printf("FAIL mode=%d i=%d asm=%d ref=%d cyc=%u\n",mode,i,ao[i],ro[i],cyc);return 1;}
    if(a.pcm!=ra.pcm||a.index!=ra.index||(mode!=2&&(b.pcm!=rb.pcm||b.index!=rb.index))){printf("FAIL state mode=%d\n",mode);return 1;}
    printf("PASS mode=%d blocks=%d samples=%d cycles=%u\n",mode,B,oi,cyc);return 0;
}
int main(int argc,char**argv){setbuf(stdout,NULL);for(int i=0;i<15*B;i++)in[i]=rng();
    if(argc>1){int m=atoi(argv[1]);return run(m);}int f=0;for(int m=0;m<3;m++)f|=run(m);return f;}
