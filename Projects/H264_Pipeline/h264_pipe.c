#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "h264.h"

extern uint32_t read_iep1(void);
extern uint32_t read_iep2(void);
extern uint32_t read_iep3(void);
extern uint32_t read_dch(void);
extern uint32_t read_dcm(void);
extern uint32_t read_bpc(void);
extern uint32_t read_bpw(void);

typedef struct {
    uint32_t ccc;
    uint32_t iep1;
    uint32_t iep2;
    uint32_t iep3;
    uint32_t dch;
    uint32_t dcm;
    uint32_t bpc;
    uint32_t bpw;
} PerfSample;

static void print_perf(const char *name, const PerfSample *p) {
    uint32_t ins = p->iep1 + p->iep2 + p->iep3;
    double p1_pct = (ins > 0) ? (100.0 * (double)p->iep1 / (double)ins) : 0.0;
    double p2_pct = (ins > 0) ? (100.0 * (double)p->iep2 / (double)ins) : 0.0;
    double p3_pct = (ins > 0) ? (100.0 * (double)p->iep3 / (double)ins) : 0.0;
    double ipc    = (p->ccc > 0) ? ((double)ins / (double)p->ccc) : 0.0;
    uint32_t total_dc = p->dch + p->dcm;
    double dch_pct = (total_dc > 0) ? (100.0 * (double)p->dch / (double)total_dc) : 100.0;

    printf(" %-30s | %7lu | %7lu | %5.2f | %5.1f%% | %5.1f%% | %5.1f%% | %5.1f%%\n",
           name, (unsigned long)p->ccc, (unsigned long)ins, ipc,
           p1_pct, p2_pct, p3_pct, dch_pct);
    fflush(stdout);
}

int main(void) {
    printf("========================================================================================================\n");
    printf(" Apollo 68080 H.264 Video Decoder Superscalar Pipeline Profiler (Harness v5)\n");
    printf(" Real Hardware: Vampire V4 Standalone (Core 1088/1 @ 85 MHz)\n");
    printf(" Performance Counters: IEP1 (Anton), IEP2 (Bruno), IEP3 (Cesar), DCH, DCM\n");
    printf("========================================================================================================\n\n");

    __asm__ __volatile__("ori.w #0x0800,%%sr" ::: "cc");

    int16_t in_h4[4][4] __attribute__((aligned(32))) = {
        {  42, -15,   8,  -3 },
        { -21,  35, -12,   7 },
        {  14, -18,  29,  -9 },
        {  -5,   9, -11,  18 }
    };
    int16_t out_h4[4][4] __attribute__((aligned(32)));

    int16_t in_idct[4][4] __attribute__((aligned(32))) = {
        {  64, -18,  14,  -7 },
        { -24,  32, -15,   9 },
        {  18, -12,  22,  -8 },
        {  -9,   8, -10,  12 }
    };
    int16_t in_dc_only[4][4] __attribute__((aligned(32))) = {
        {  120,   0,   0,   0 },
        {    0,   0,   0,   0 },
        {    0,   0,   0,   0 },
        {    0,   0,   0,   0 }
    };
    int16_t out_idct[4][4] __attribute__((aligned(32)));

    uint8_t ref[64] __attribute__((aligned(32)));
    uint8_t dst[64] __attribute__((aligned(32)));
    memset(ref, 128, sizeof(ref));
    memset(dst, 0, sizeof(dst));

    int16_t mb_blocks[16][16] __attribute__((aligned(32)));
    uint8_t mb_ref[16 * 16] __attribute__((aligned(32)));
    uint8_t mb_dst[16 * 16] __attribute__((aligned(32)));
    memset(mb_blocks, 0, sizeof(mb_blocks));
    for (int b = 0; b < 16; b++) mb_blocks[b][0] = 80; /* DC coeff */
    for (int b = 0; b < 4; b++) mb_blocks[b][1] = -15;  /* 4 blocks with AC */
    memset(mb_ref, 128, sizeof(mb_ref));

    printf(" Routine                        | Cycles  | Insns   | IPC   | Anton  | Bruno  | Cesar  | D-Cache Hit\n");
    printf("--------------------------------+---------+---------+-------+--------+--------+--------+-------------\n");

    /* 1. Hadamard 4x4 AMMX */
    {
        for (int w = 0; w < 10; w++) h264_hadamard4x4_ammx(in_h4, out_h4);
        uint32_t s_ccc = get_ccc(), s_1 = read_iep1(), s_2 = read_iep2(), s_3 = read_iep3();
        uint32_t s_dch = read_dch(), s_dcm = read_dcm();
        for (int i = 0; i < 200; i++) h264_hadamard4x4_ammx(in_h4, out_h4);
        uint32_t e_dcm = read_dcm(), e_dch = read_dch();
        uint32_t e_3 = read_iep3(), e_2 = read_iep2(), e_1 = read_iep1(), e_ccc = get_ccc();
        PerfSample p = { (e_ccc - s_ccc)/200, (e_1 - s_1)/200, (e_2 - s_2)/200, (e_3 - s_3)/200, (e_dch - s_dch)/200, (e_dcm - s_dcm)/200, 0, 0 };
        print_perf("Hadamard 4x4 (AMMX)", &p);
    }

    /* 2. Core 4x4 IDCT */
    {
        for (int w = 0; w < 10; w++) h264_idct4x4_core_asm(in_idct, out_idct);
        uint32_t s_ccc = get_ccc(), s_1 = read_iep1(), s_2 = read_iep2(), s_3 = read_iep3();
        uint32_t s_dch = read_dch(), s_dcm = read_dcm();
        for (int i = 0; i < 200; i++) h264_idct4x4_core_asm(in_idct, out_idct);
        uint32_t e_dcm = read_dcm(), e_dch = read_dch();
        uint32_t e_3 = read_iep3(), e_2 = read_iep2(), e_1 = read_iep1(), e_ccc = get_ccc();
        PerfSample p = { (e_ccc - s_ccc)/200, (e_1 - s_1)/200, (e_2 - s_2)/200, (e_3 - s_3)/200, (e_dch - s_dch)/200, (e_dcm - s_dcm)/200, 0, 0 };
        print_perf("Core IDCT 4x4 (In-Reg ASM)", &p);
    }

    /* 3. Fused 4x4 IDCT + MC (Full AC) */
    {
        for (int w = 0; w < 10; w++) h264_idct4x4_fused_mc_asm(in_idct, ref, dst, 4);
        uint32_t s_ccc = get_ccc(), s_1 = read_iep1(), s_2 = read_iep2(), s_3 = read_iep3();
        uint32_t s_dch = read_dch(), s_dcm = read_dcm();
        for (int i = 0; i < 200; i++) h264_idct4x4_fused_mc_asm(in_idct, ref, dst, 4);
        uint32_t e_dcm = read_dcm(), e_dch = read_dch();
        uint32_t e_3 = read_iep3(), e_2 = read_iep2(), e_1 = read_iep1(), e_ccc = get_ccc();
        PerfSample p = { (e_ccc - s_ccc)/200, (e_1 - s_1)/200, (e_2 - s_2)/200, (e_3 - s_3)/200, (e_dch - s_dch)/200, (e_dcm - s_dcm)/200, 0, 0 };
        print_perf("Fused IDCT+MC (Full AC)", &p);
    }

    /* 4. Fused 4x4 IDCT + MC (DC-Only Fast-Path) */
    {
        for (int w = 0; w < 10; w++) h264_idct4x4_sparse_dc_mc_asm(in_dc_only, ref, dst, 4);
        uint32_t s_ccc = get_ccc(), s_1 = read_iep1(), s_2 = read_iep2(), s_3 = read_iep3();
        uint32_t s_dch = read_dch(), s_dcm = read_dcm();
        for (int i = 0; i < 200; i++) h264_idct4x4_sparse_dc_mc_asm(in_dc_only, ref, dst, 4);
        uint32_t e_dcm = read_dcm(), e_dch = read_dch();
        uint32_t e_3 = read_iep3(), e_2 = read_iep2(), e_1 = read_iep1(), e_ccc = get_ccc();
        PerfSample p = { (e_ccc - s_ccc)/200, (e_1 - s_1)/200, (e_2 - s_2)/200, (e_3 - s_3)/200, (e_dch - s_dch)/200, (e_dcm - s_dcm)/200, 0, 0 };
        print_perf("Fused IDCT+MC (DC Fast-Path)", &p);
    }

    /* 5. 16x16 Macroblock (Typical P-Frame) */
    {
        for (int w = 0; w < 5; w++) h264_macroblock_16x16_asm(mb_blocks, mb_ref, mb_dst, 16);
        uint32_t s_ccc = get_ccc(), s_1 = read_iep1(), s_2 = read_iep2(), s_3 = read_iep3();
        uint32_t s_dch = read_dch(), s_dcm = read_dcm();
        for (int i = 0; i < 50; i++) h264_macroblock_16x16_asm(mb_blocks, mb_ref, mb_dst, 16);
        uint32_t e_dcm = read_dcm(), e_dch = read_dch();
        uint32_t e_3 = read_iep3(), e_2 = read_iep2(), e_1 = read_iep1(), e_ccc = get_ccc();
        PerfSample p = { (e_ccc - s_ccc)/50, (e_1 - s_1)/50, (e_2 - s_2)/50, (e_3 - s_3)/50, (e_dch - s_dch)/50, (e_dcm - s_dcm)/50, 0, 0 };
        print_perf("16x16 MB (Typical P-Frame)", &p);
    }

    printf("========================================================================================================\n");
    return 0;
}
