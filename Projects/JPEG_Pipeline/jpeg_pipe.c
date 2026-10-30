#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "jpeg.h"

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
    printf(" Apollo 68080 JPEG Decoder Pipeline Superscalar Profiler (Harness v5)\n");
    printf(" Real Hardware: Vampire V4 Standalone (Core 1088/1 @ 85 MHz)\n");
    printf(" Performance Counters: IEP1 (Anton), IEP2 (Bruno), IEP3 (Cesar), DCH, DCM\n");
    printf("========================================================================================================\n\n");

    __asm__ __volatile__("ori.w #0x0800,%%sr" ::: "cc");

    jpeg_init_tables();

    int16_t zz_in[64] __attribute__((aligned(32)));
    int16_t raster64[64] __attribute__((aligned(32)));
    uint8_t pixels64[64] __attribute__((aligned(32)));
    uint8_t y64[64] __attribute__((aligned(32)));
    uint8_t cb64[64] __attribute__((aligned(32)));
    uint8_t cr64[64] __attribute__((aligned(32)));
    uint16_t rgb565[64] __attribute__((aligned(32)));
    uint32_t argb32[64] __attribute__((aligned(32)));

    memset(zz_in, 0, sizeof(zz_in));
    zz_in[0] = 120; zz_in[1] = -15; zz_in[2] = 8; zz_in[8] = 22;
    for (int i = 0; i < 64; i++) {
        raster64[i] = (int16_t)(i * 4 - 128);
        y64[i] = 128; cb64[i] = 128; cr64[i] = 128;
    }

    int16_t zz_dc_only[64] __attribute__((aligned(32)));
    memset(zz_dc_only, 0, sizeof(zz_dc_only));
    zz_dc_only[0] = 240;

    printf(" Routine                          | Cycles  | Insns   | IPC   | Anton  | Bruno  | Cesar  | D-Cache Hit\n");
    printf("----------------------------------+---------+---------+-------+--------+--------+--------+-------------\n");

    /* 1. Dequant Zigzag AMMX */
    {
        for (int w = 0; w < 10; w++) dequant_zigzag_ammx(zz_in, jpeg_luma_quant, raster64);
        uint32_t s_ccc = get_ccc(), s_1 = read_iep1(), s_2 = read_iep2(), s_3 = read_iep3();
        uint32_t s_dch = read_dch(), s_dcm = read_dcm();
        for (int i = 0; i < 200; i++) dequant_zigzag_ammx(zz_in, jpeg_luma_quant, raster64);
        uint32_t e_dcm = read_dcm(), e_dch = read_dch();
        uint32_t e_3 = read_iep3(), e_2 = read_iep2(), e_1 = read_iep1(), e_ccc = get_ccc();
        PerfSample p = { (e_ccc - s_ccc)/200, (e_1 - s_1)/200, (e_2 - s_2)/200, (e_3 - s_3)/200, (e_dch - s_dch)/200, (e_dcm - s_dcm)/200 };
        print_perf("Dequant + Zigzag (AMMX)", &p);
    }

    /* 2. 8x8 IDCT Full AC AMMX */
    {
        for (int w = 0; w < 10; w++) idct8x8_levelshift_packuswb_ammx(raster64, pixels64);
        uint32_t s_ccc = get_ccc(), s_1 = read_iep1(), s_2 = read_iep2(), s_3 = read_iep3();
        uint32_t s_dch = read_dch(), s_dcm = read_dcm();
        for (int i = 0; i < 200; i++) idct8x8_levelshift_packuswb_ammx(raster64, pixels64);
        uint32_t e_dcm = read_dcm(), e_dch = read_dch();
        uint32_t e_3 = read_iep3(), e_2 = read_iep2(), e_1 = read_iep1(), e_ccc = get_ccc();
        PerfSample p = { (e_ccc - s_ccc)/200, (e_1 - s_1)/200, (e_2 - s_2)/200, (e_3 - s_3)/200, (e_dch - s_dch)/200, (e_dcm - s_dcm)/200 };
        print_perf("8x8 IDCT + Pack (Full AC AMMX)", &p);
    }

    /* 3. 8x8 IDCT DC Fast-Path */
    {
        for (int w = 0; w < 10; w++) idct8x8_sparse_levelshift_packuswb_ammx(zz_dc_only, pixels64);
        uint32_t s_ccc = get_ccc(), s_1 = read_iep1(), s_2 = read_iep2(), s_3 = read_iep3();
        uint32_t s_dch = read_dch(), s_dcm = read_dcm();
        for (int i = 0; i < 200; i++) idct8x8_sparse_levelshift_packuswb_ammx(zz_dc_only, pixels64);
        uint32_t e_dcm = read_dcm(), e_dch = read_dch();
        uint32_t e_3 = read_iep3(), e_2 = read_iep2(), e_1 = read_iep1(), e_ccc = get_ccc();
        PerfSample p = { (e_ccc - s_ccc)/200, (e_1 - s_1)/200, (e_2 - s_2)/200, (e_3 - s_3)/200, (e_dch - s_dch)/200, (e_dcm - s_dcm)/200 };
        print_perf("8x8 IDCT + Pack (DC Fast-Path)", &p);
    }

    /* 4. YCbCr -> RGB565 AMMX */
    {
        for (int w = 0; w < 10; w++) ycbcr_to_rgb565_ammx(y64, cb64, cr64, rgb565);
        uint32_t s_ccc = get_ccc(), s_1 = read_iep1(), s_2 = read_iep2(), s_3 = read_iep3();
        uint32_t s_dch = read_dch(), s_dcm = read_dcm();
        for (int i = 0; i < 200; i++) ycbcr_to_rgb565_ammx(y64, cb64, cr64, rgb565);
        uint32_t e_dcm = read_dcm(), e_dch = read_dch();
        uint32_t e_3 = read_iep3(), e_2 = read_iep2(), e_1 = read_iep1(), e_ccc = get_ccc();
        PerfSample p = { (e_ccc - s_ccc)/200, (e_1 - s_1)/200, (e_2 - s_2)/200, (e_3 - s_3)/200, (e_dch - s_dch)/200, (e_dcm - s_dcm)/200 };
        print_perf("YCbCr -> RGB565 (AMMX)", &p);
    }

    /* 5. Complete Pipeline MCU */
    {
        for (int w = 0; w < 10; w++) jpeg_pipeline_mcu_ammx(zz_in, zz_in, zz_in, jpeg_luma_quant, jpeg_chroma_quant, rgb565);
        uint32_t s_ccc = get_ccc(), s_1 = read_iep1(), s_2 = read_iep2(), s_3 = read_iep3();
        uint32_t s_dch = read_dch(), s_dcm = read_dcm();
        for (int i = 0; i < 100; i++) jpeg_pipeline_mcu_ammx(zz_in, zz_in, zz_in, jpeg_luma_quant, jpeg_chroma_quant, rgb565);
        uint32_t e_dcm = read_dcm(), e_dch = read_dch();
        uint32_t e_3 = read_iep3(), e_2 = read_iep2(), e_1 = read_iep1(), e_ccc = get_ccc();
        PerfSample p = { (e_ccc - s_ccc)/100, (e_1 - s_1)/100, (e_2 - s_2)/100, (e_3 - s_3)/100, (e_dch - s_dch)/100, (e_dcm - s_dcm)/100 };
        print_perf("Complete MCU Pipeline (AMMX)", &p);
    }

    printf("========================================================================================================\n");
    return 0;
}
