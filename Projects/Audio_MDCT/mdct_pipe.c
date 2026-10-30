#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "mdct.h"

extern uint32_t read_iep1(void);
extern uint32_t read_iep2(void);
extern uint32_t read_iep3(void);
extern uint32_t read_dch(void);
extern uint32_t read_dcm(void);

typedef struct {
    uint32_t ccc;
    uint32_t iep1;
    uint32_t iep2;
    uint32_t iep3;
    uint32_t dch;
    uint32_t dcm;
} PerfSample;

static void print_perf(const char *name, const PerfSample *p) {
    uint32_t ins = p->iep1 + p->iep2 + p->iep3;
    double p1_pct = (ins > 0) ? (100.0 * (double)p->iep1 / (double)ins) : 0.0;
    double p2_pct = (ins > 0) ? (100.0 * (double)p->iep2 / (double)ins) : 0.0;
    double p3_pct = (ins > 0) ? (100.0 * (double)p->iep3 / (double)ins) : 0.0;
    double ipc    = (p->ccc > 0) ? ((double)ins / (double)p->ccc) : 0.0;
    uint32_t total_dc = p->dch + p->dcm;
    double dch_pct = (total_dc > 0) ? (100.0 * (double)p->dch / (double)total_dc) : 100.0;

    printf(" %-34s | %7lu | %7lu | %5.2f | %5.1f%% | %5.1f%% | %5.1f%% | %5.1f%%\n",
           name, (unsigned long)p->ccc, (unsigned long)ins, ipc,
           p1_pct, p2_pct, p3_pct, dch_pct);
    fflush(stdout);
}

int main(void) {
    printf("========================================================================================================\n");
    printf(" Apollo 68080 Audio MDCT / MP3 Decoder Superscalar Profiler (Harness v5)\n");
    printf(" Real Hardware: Vampire V4 Standalone (Core 1088/1 @ 85 MHz)\n");
    printf(" Performance Counters: IEP1 (Anton), IEP2 (Bruno), IEP3 (Cesar), DCH, DCM\n");
    printf("========================================================================================================\n\n");

    __asm__ __volatile__("ori.w #0x0800,%%sr" ::: "cc");

    mdct_init_tables();

    float in18_f[18] __attribute__((aligned(32)));
    float out36_f[36] __attribute__((aligned(32)));
    int16_t in18_q15[18] __attribute__((aligned(32)));
    int16_t out36_q15[36] __attribute__((aligned(32)));
    int16_t prev18[18] __attribute__((aligned(32)));
    int16_t out18[18] __attribute__((aligned(32)));

    for (int i = 0; i < 18; i++) {
        in18_f[i] = (float)(i + 1) * 0.05f;
        in18_q15[i] = (int16_t)((i + 1) * 1500);
        prev18[i] = 1000;
    }

    printf(" Routine                            | Cycles  | Insns   | IPC   | Anton  | Bruno  | Cesar  | D-Cache Hit\n");
    printf("------------------------------------+---------+---------+-------+--------+--------+--------+-------------\n");

    /* 1. 36-Point IMDCT Float ASM (FPU) */
    {
        for (int w = 0; w < 10; w++) imdct36_float_asm(in18_f, out36_f);
        uint32_t s_ccc = get_ccc(), s_1 = read_iep1(), s_2 = read_iep2(), s_3 = read_iep3();
        uint32_t s_dch = read_dch(), s_dcm = read_dcm();
        for (int i = 0; i < 200; i++) imdct36_float_asm(in18_f, out36_f);
        uint32_t e_dcm = read_dcm(), e_dch = read_dch();
        uint32_t e_3 = read_iep3(), e_2 = read_iep2(), e_1 = read_iep1(), e_ccc = get_ccc();
        PerfSample p = { (e_ccc - s_ccc)/200, (e_1 - s_1)/200, (e_2 - s_2)/200, (e_3 - s_3)/200, (e_dch - s_dch)/200, (e_dcm - s_dcm)/200 };
        print_perf("36-pt IMDCT Float (FPU ASM)", &p);
    }

    /* 2. 36-Point IMDCT Fixed Q1.15 AMMX */
    {
        for (int w = 0; w < 10; w++) imdct36_fixed_ammx(in18_q15, out36_q15);
        uint32_t s_ccc = get_ccc(), s_1 = read_iep1(), s_2 = read_iep2(), s_3 = read_iep3();
        uint32_t s_dch = read_dch(), s_dcm = read_dcm();
        for (int i = 0; i < 200; i++) imdct36_fixed_ammx(in18_q15, out36_q15);
        uint32_t e_dcm = read_dcm(), e_dch = read_dch();
        uint32_t e_3 = read_iep3(), e_2 = read_iep2(), e_1 = read_iep1(), e_ccc = get_ccc();
        PerfSample p = { (e_ccc - s_ccc)/200, (e_1 - s_1)/200, (e_2 - s_2)/200, (e_3 - s_3)/200, (e_dch - s_dch)/200, (e_dcm - s_dcm)/200 };
        print_perf("36-pt IMDCT Fixed (AMMX PMULLW)", &p);
    }

    /* 3. Overlap-Add AMMX */
    {
        for (int w = 0; w < 10; w++) overlap_add_fixed_ammx(out36_q15, prev18, out18);
        uint32_t s_ccc = get_ccc(), s_1 = read_iep1(), s_2 = read_iep2(), s_3 = read_iep3();
        uint32_t s_dch = read_dch(), s_dcm = read_dcm();
        for (int i = 0; i < 200; i++) overlap_add_fixed_ammx(out36_q15, prev18, out18);
        uint32_t e_dcm = read_dcm(), e_dch = read_dch();
        uint32_t e_3 = read_iep3(), e_2 = read_iep2(), e_1 = read_iep1(), e_ccc = get_ccc();
        PerfSample p = { (e_ccc - s_ccc)/200, (e_1 - s_1)/200, (e_2 - s_2)/200, (e_3 - s_3)/200, (e_dch - s_dch)/200, (e_dcm - s_dcm)/200 };
        print_perf("Overlap-Add (AMMX PADDW)", &p);
    }

    /* 4. Complete Fused Windowed IMDCT + Overlap-Add */
    {
        for (int w = 0; w < 10; w++) imdct36_windowed_overlap_add_ammx(in18_q15, prev18, out18);
        uint32_t s_ccc = get_ccc(), s_1 = read_iep1(), s_2 = read_iep2(), s_3 = read_iep3();
        uint32_t s_dch = read_dch(), s_dcm = read_dcm();
        for (int i = 0; i < 200; i++) imdct36_windowed_overlap_add_ammx(in18_q15, prev18, out18);
        uint32_t e_dcm = read_dcm(), e_dch = read_dch();
        uint32_t e_3 = read_iep3(), e_2 = read_iep2(), e_1 = read_iep1(), e_ccc = get_ccc();
        PerfSample p = { (e_ccc - s_ccc)/200, (e_1 - s_1)/200, (e_2 - s_2)/200, (e_3 - s_3)/200, (e_dch - s_dch)/200, (e_dcm - s_dcm)/200 };
        print_perf("Fused Windowed IMDCT+OLA (AMMX)", &p);
    }

    printf("========================================================================================================\n");
    return 0;
}
