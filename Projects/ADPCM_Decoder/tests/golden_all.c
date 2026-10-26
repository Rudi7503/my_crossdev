/* Generic bit-exact golden test for all ADPCM decoders (2/3/5/6 and mixed MS). */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

typedef struct __attribute__((packed, aligned(2))) { int32_t pcm; int16_t index; } Chan;

extern uint32_t decode_2bit_stereo_asm(const uint8_t*,int16_t*,void*,void*,uint32_t);
extern uint32_t decode_2bit_ms_asm(const uint8_t*,int16_t*,void*,void*,uint32_t);
extern uint32_t decode_2bit_mono_asm(const uint8_t*,int16_t*,void*,uint32_t);
extern uint32_t decode_3bit_stereo_asm(const uint8_t*,int16_t*,void*,void*,uint32_t);
extern uint32_t decode_3bit_ms_asm(const uint8_t*,int16_t*,void*,void*,uint32_t);
extern uint32_t decode_3bit_mono_asm(const uint8_t*,int16_t*,void*,uint32_t);
extern uint32_t decode_5bit_stereo_asm(const uint8_t*,int16_t*,void*,void*,uint32_t);
extern uint32_t decode_5bit_ms_asm(const uint8_t*,int16_t*,void*,void*,uint32_t);
extern uint32_t decode_5bit_mono_asm(const uint8_t*,int16_t*,void*,uint32_t);
extern uint32_t decode_6bit_stereo_asm(const uint8_t*,int16_t*,void*,void*,uint32_t);
extern uint32_t decode_6bit_ms_asm(const uint8_t*,int16_t*,void*,void*,uint32_t);
extern uint32_t decode_6bit_mono_asm(const uint8_t*,int16_t*,void*,uint32_t);
extern uint32_t decode_3_2bit_ms_asm(const uint8_t*,int16_t*,void*,void*,uint32_t);
extern uint32_t decode_4_2bit_ms_asm(const uint8_t*,int16_t*,void*,void*,uint32_t);
extern uint32_t decode_5_3bit_ms_asm(const uint8_t*,int16_t*,void*,void*,uint32_t);

static const int step_table[89] = {
 7,8,9,10,11,12,13,14,16,17,19,21,23,25,28,31,34,37,41,45,50,55,60,66,73,80,88,97,107,
 118,130,143,157,173,190,209,230,253,279,307,337,371,408,449,494,544,598,658,724,796,
 876,963,1060,1166,1282,1411,1552,1707,1878,2066,2272,2499,2749,3024,3327,3660,4026,
 4428,4871,5358,5894,6484,7132,7845,8630,9493,10442,11487,12635,13899,15289,16818,
 18500,20350,22385,24623,27086,29794,32767
};
static const int idx3[4]={-1,-1,1,2};
static const int idx4[8]={-1,-1,-1,-1,2,4,6,8};
static const int idx5[16]={-1,-1,-1,-1,-1,-1,-1,-1,1,2,4,6,8,10,13,16};
static const int idx6[32]={-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,1,2,3,4,5,6,7,8,10,12,14,16,19,22,25,28};

static int32_t clamp32(int32_t v,int32_t lo,int32_t hi){return v<lo?lo:(v>hi?hi:v);}
static int16_t dec(Chan*c,int code,int bps){
    int32_t step=step_table[c->index],d=0;
    if(bps==2){d=step>>1; if(code&1)d+=step; if(code&2)c->pcm-=d; else c->pcm+=d; c->index+=(code&1)*3-1;}
    else if(bps==3){d=step>>2; if(code&1)d+=step>>1; if(code&2)d+=step; if(code&4)c->pcm-=d; else c->pcm+=d; c->index+=idx3[code&3];}
    else if(bps==4){d=step>>3; if(code&1)d+=step>>2; if(code&2)d+=step>>1; if(code&4)d+=step; if(code&8)c->pcm-=d; else c->pcm+=d; c->index+=idx4[code&7];}
    else if(bps==5){d=step>>4; if(code&1)d+=step>>3; if(code&2)d+=step>>2; if(code&4)d+=step>>1; if(code&8)d+=step; if(code&16)c->pcm-=d; else c->pcm+=d; c->index+=idx5[code&15];}
    else {d=step>>5; if(code&1)d+=step>>4; if(code&2)d+=step>>3; if(code&4)d+=step>>2; if(code&8)d+=step>>1; if(code&16)d+=step; if(code&32)c->pcm-=d; else c->pcm+=d; c->index+=idx6[code&31];}
    c->index=clamp32(c->index,0,88); c->pcm=clamp32(c->pcm,-32768,32767); return (int16_t)c->pcm;
}
static uint32_t readbits(const uint8_t*b,int*pos,int n){uint32_t r=0; for(int i=0;i<n;i++){r|=((b[*pos>>3]>>(*pos&7))&1u)<<i; (*pos)++;} return r;}

struct Mode { const char*name; int bpsL,bpsR,mono,ms,pairs; void*fn; };
#define BLOCKS 512
static uint8_t in[15*BLOCKS];
static int16_t out_asm[2*120*BLOCKS+8], out_ref[2*120*BLOCKS+8];

static uint32_t rng_state=0x12345678u;
static uint32_t rng(void){rng_state^=rng_state<<13;rng_state^=rng_state>>17;rng_state^=rng_state<<5;return rng_state;}

static int run(struct Mode*m){
    Chan a={0,0},b={0,0},ra={0,0},rs={0,0};
    memset(out_asm,0xAA,sizeof(out_asm)); memset(out_ref,0xAA,sizeof(out_ref));
    uint32_t cyc=0;
    int pos=0, oi=0;
    for(int blk=0;blk<BLOCKS;blk++){
        const uint8_t *bp=in+15*blk;
        uint32_t cnt = (!strcmp(m->name,"5_3ms")) ? (uint32_t)m->pairs : 1u;
        if(m->mono) cyc+=((uint32_t(*)(const uint8_t*,int16_t*,void*,uint32_t))m->fn)(bp,out_asm+oi,&a,cnt);
        else cyc+=((uint32_t(*)(const uint8_t*,int16_t*,void*,void*,uint32_t))m->fn)(bp,out_asm+oi,&a,&b,cnt);
        int pairs=m->pairs;
        if(m->mono){
            for(int i=0;i<pairs;i++) out_ref[oi++]=dec(&ra,(int)readbits(in,&pos,m->bpsL),m->bpsL);
        }else{
            for(int i=0;i<pairs;i++){
                int l=dec(&ra,(int)readbits(in,&pos,m->bpsL),m->bpsL);
                int r=dec(&rs,(int)readbits(in,&pos,m->bpsR),m->bpsR);
                if(m->ms){
                    int L=(int)clamp32((int32_t)l+r,-32768,32767);
                    int R=(int)clamp32((int32_t)l-r,-32768,32767);
                    out_ref[oi++]=(int16_t)L; out_ref[oi++]=(int16_t)R;
                }else{out_ref[oi++]=(int16_t)l;out_ref[oi++]=(int16_t)r;}
            }
        }
    }
    for(int i=0;i<oi;i++) if(out_asm[i]!=out_ref[i]){
        printf("%s FAIL out[%d] asm=%d ref=%d cycles=%u\n",m->name,i,out_asm[i],out_ref[i],cyc); return 1;
    }
    if(a.pcm!=ra.pcm||a.index!=ra.index||(!m->mono&&(b.pcm!=rs.pcm||b.index!=rs.index))){
        printf("%s FAIL state a=%ld/%d ref=%ld/%d\n",m->name,(long)a.pcm,a.index,(long)ra.pcm,ra.index); return 1;
    }
    printf("%s PASS blocks=%d samples=%d cycles=%u\n",m->name,BLOCKS,oi,cyc); fflush(stdout); return 0;
}

int main(int argc, char **argv){
    setbuf(stdout,NULL);
    for(int i=0;i<15*BLOCKS;i++) in[i]=(uint8_t)rng();
    struct Mode ms[]={
        {"2bit-stereo",2,2,0,0,30,(void*)decode_2bit_stereo_asm},
        {"2bit-ms",2,2,0,1,30,(void*)decode_2bit_ms_asm},
        {"2bit-mono",2,0,1,0,60,(void*)decode_2bit_mono_asm},
        {"3bit-stereo",3,3,0,0,20,(void*)decode_3bit_stereo_asm},
        {"3bit-ms",3,3,0,1,20,(void*)decode_3bit_ms_asm},
        {"3bit-mono",3,0,1,0,40,(void*)decode_3bit_mono_asm},
        {"5bit-stereo",5,5,0,0,12,(void*)decode_5bit_stereo_asm},
        {"5bit-ms",5,5,0,1,12,(void*)decode_5bit_ms_asm},
        {"5bit-mono",5,0,1,0,24,(void*)decode_5bit_mono_asm},
        {"6bit-stereo",6,6,0,0,10,(void*)decode_6bit_stereo_asm},
        {"6bit-ms",6,6,0,1,10,(void*)decode_6bit_ms_asm},
        {"6bit-mono",6,0,1,0,20,(void*)decode_6bit_mono_asm},
        {"3_2ms",3,2,0,1,24,(void*)decode_3_2bit_ms_asm},
        {"4_2ms",4,2,0,1,20,(void*)decode_4_2bit_ms_asm},
        {"5_3ms",5,3,0,1,15,(void*)decode_5_3bit_ms_asm},
        {0,0,0,0,0}
    };
    if(argc>1){ int i=atoi(argv[1]); if(i<0||i>14){printf("bad mode\n");return 2;} printf("RUN %s bps=%d/%d mono=%d ms=%d\n",ms[i].name,ms[i].bpsL,ms[i].bpsR,ms[i].mono,ms[i].ms); return run(&ms[i]); }
    int fail=0;
    for(int i=0;ms[i].name;i++){ printf("RUN %s bps=%d/%d mono=%d ms=%d\n",ms[i].name,ms[i].bpsL,ms[i].bpsR,ms[i].mono,ms[i].ms); fail|=run(&ms[i]); }
    printf("RESULT;%s\n",fail?"FAIL":"PASS");
    return fail;
}