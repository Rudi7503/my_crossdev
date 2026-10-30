#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "mpeg.h"

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

    printf(" %-32s | %7lu | %7lu | %5.2f | %5.1f%% | %5.1f%% | %5.1f%% | %5.1f%%\n",
           name, (unsigned long)p->ccc, (unsigned long)ins, ipc,
           p1_pct, p2_pct, p3_pct, dch_pct);
    fflush(stdout);
}

int main(void) {
    printf("========================================================================================================\n");
    printf(" Apollo 68080 MPEG Video Decoder Pipeline Superscalar Profiler (Harness v5)\n");
    printf(" Real Hardware: Vampire V4 Standalone (Core 1088/1 @ 85 MHz)\n");
    printf(" Performance Counters: IEP1 (Anton), IEP2 (Bruno), IEP3 (Cesar), DCH, DCM\n");
    printf("========================================================================================================\n\n");

    __asm__ __volatile__("ori.w #0x0800,%%sr" ::: "cc");

    mpeg_init_tables();

    int16_t raster64[64] __attribute__((aligned(32)));
    int16_t residual64[64] __attribute__((aligned(32)));
    uint8_t ref64[64] __attribute__((aligned(32)));
    uint8_t dst64[64] __attribute__((aligned(32)));

    for (int i = 0; i < 64; i++) {
        raster64[i] = (int16_t)(i * 3 - 60);
        residual64[i] = (int16_t)(i - 32);
        ref64[i] = 128;
        dst64[i] = 0;
    }

    int16_t dc_only[64] __attribute__((aligned(32)));
    memset(dc_only, 0, sizeof(dc_only));
    dc_only[0] = 160;

    int16_t mb_res[6][64] __attribute__((aligned(32)));
    memset(mb_res, 0, sizeof(mb_res));
    for (int b = 0; b < 6; b++) {
        for (int i = 0; i < 64; i++) mb_res[b][i] = (int16_t)(i - 32);
    }
    uint8_t ref_y[256] __attribute__((aligned(32)));
    uint8_t ref_c[64] __attribute__((aligned(32)));
    uint8_t dst_y[256] __attribute__((aligned(32)));
    uint8_t dst_c[64] __attribute__((aligned(32)));
    memset(ref_y, 128, sizeof(ref_y));
    memset(ref_c, 128, sizeof(ref_c));

    printf(" Routine                          | Cycles  | Insns   | IPC   | Anton  | Bruno  | Cesar  | D-Cache Hit\n");
    printf("----------------------------------+---------+---------+-------+--------+--------+--------+-------------\n");

    /* 1. MC Residual Add AMMX */
    {
        for (int w = 0; w < 10; w++) mpeg_mc_residual_add_ammx(ref64, residual64, dst64, 8);
        uint32_t s_ccc = get_ccc(), s_1 = read_iep1(), s_2 = read_iep2(), s_3 = read_iep3();
        uint32_t s_dch = read_dch(), s_dcm = read_dcm();
        for (int i = 0; i < 200; i++) mpeg_mc_residual_add_ammx(ref64, residual64, dst64, 8);
        uint32_t e_dcm = read_dcm(), e_dch = read_dch();
        uint32_t e_3 = read_iep3(), e_2 = read_iep2(), e_1 = read_iep1(), e_ccc = get_ccc();
        PerfSample p = { (e_ccc - s_ccc)/200, (e_1 - s_1)/200, (e_2 - s_2)/200, (e_3 - s_3)/200, (e_dch - s_dch)/200, (e_dcm - s_dcm)/200 };
        print_perf("MC Residual Add (AMMX)", &p);
    }

    /* 2. Fused IDCT + MC Full 8x8 */
    {
        for (int w = 0; w < 10; w++) mpeg_idct8x8_fused_mc_ammx(raster64, ref64, dst64, 8);
        uint32_t s_ccc = get_ccc(), s_1 = read_iep1(), s_2 = read_iep2(), s_3 = read_iep3();
        uint32_t s_dch = read_dch(), s_dcm = read_dcm();
        for (int i = 0; i < 200; i++) mpeg_idct8x8_fused_mc_ammx(raster64, ref64, dst64, 8);
        uint32_t e_dcm = read_dcm(), e_dch = read_dch();
        uint32_t e_3 = read_iep3(), e_2 = read_iep2(), e_1 = read_iep1(), e_ccc = get_ccc();
        PerfSample p = { (e_ccc - s_ccc)/200, (e_1 - s_1)/200, (e_2 - s_2)/200, (e_3 - s_3)/200, (e_dch - s_dch)/200, (e_dcm - s_dcm)/200 };
        print_perf("Fused IDCT+MC (Full 8x8 AMMX)", &p);
    }

    /* 3. Fused IDCT + MC DC Fast-Path */
    {
        for (int w = 0; w < 10; w++) mpeg_idct8x8_sparse_dc_mc_ammx(dc_only, ref64, dst64, 8);
        uint32_t s_ccc = get_ccc(), s_1 = read_iep1(), s_2 = read_iep2(), s_3 = read_iep3();
        uint32_t s_dch = read_dch(), s_dcm = read_dcm();
        for (int i = 0; i < 200; i++) mpeg_idct8x8_sparse_dc_mc_ammx(dc_only, ref64, dst64, 8);
        uint32_t e_dcm = read_dcm(), e_dch = read_dch();
        uint32_t e_3 = read_iep3(), e_2 = read_iep2(), e_1 = read_iep1(), e_ccc = get_ccc();
        PerfSample p = { (e_ccc - s_ccc)/200, (e_1 - s_1)/200, (e_2 - s_2)/200, (e_3 - s_3)/200, (e_dch - s_dch)/200, (e_dcm - s_dcm)/200 };
        print_perf("Fused IDCT+MC (DC Fast-Path)", &p);
    }

    /* 4. Macroblock MC AMMX (6 Blocks) */
    {
        for (int w = 0; w < 10; w++) mpeg_macroblock_mc_ammx(mb_res, ref_y, ref_c, ref_c, dst_y, dst_c, dst_c, 16, 8);
        uint32_t s_ccc = get_ccc(), s_1 = read_iep1(), s_2 = read_iep2(), s_3 = read_iep3();
        uint32_t s_dch = read_dch(), s_dcm = read_dcm();
        for (int i = 0; i < 100; i++) mpeg_macroblock_mc_ammx(mb_res, ref_y, ref_c, ref_c, dst_y, dst_c, dst_c, 16, 8);
        uint32_t e_dcm = read_dcm(), e_dch = read_dch();
        uint32_t e_3 = read_iep3(), e_2 = read_iep2(), e_1 = read_iep1(), e_ccc = get_ccc();
        PerfSample p = { (e_ccc - s_ccc)/100, (e_1 - s_1)/100, (e_2 - s_2)/100, (e_3 - s_3)/100, (e_dch - s_dch)/100, (e_dcm - s_dcm)/100 };
        print_perf("Complete 16x16 MB (6 Blocks)", &p);
    }

    printf("========================================================================================================\n");
    return 0;
}
