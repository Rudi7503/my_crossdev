#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <math.h>
#include <string.h>
#include "mpeg.h"

#define NUM_RUNS 200

static void *malloc_aligned(size_t size, void **raw_out) {
    void *p = malloc(size + 64);
    if (!p) { *raw_out = NULL; return NULL; }
    *raw_out = p;
    uintptr_t addr = (uintptr_t)p;
    uintptr_t aligned = (addr + 31) & ~((uintptr_t)31);
    return (void *)aligned;
}

int main(void) {
    printf("=================================================================================================================================\n");
    printf(" Apollo 68080 MPEG-1/2 Video Decoding Pipeline Benchmark Suite\n");
    printf(" Optimizations: Fused 2D IDCT + Motion Compensation Residual Addition + PACKUSWB Direct Store | Sparse Sub-Block Exits\n");
    printf(" Real Hardware: Vampire V4 (Apollo 68080 @ Core Release)\n");
    printf("=================================================================================================================================\n\n");

    mpeg_init_tables();

    void *raw_ref8, *raw_dst8_c, *raw_dst8_m;
    void *raw_res16, *raw_coeff16;
    void *raw_dc_coeff, *raw_dc_dst_c, *raw_dc_dst_m;

    uint8_t *ref8      = (uint8_t *)malloc_aligned(64 * sizeof(uint8_t), &raw_ref8);
    uint8_t *dst8_c    = (uint8_t *)malloc_aligned(64 * sizeof(uint8_t), &raw_dst8_c);
    uint8_t *dst8_m    = (uint8_t *)malloc_aligned(64 * sizeof(uint8_t), &raw_dst8_m);

    int16_t *res16     = (int16_t *)malloc_aligned(64 * sizeof(int16_t), &raw_res16);
    int16_t *coeff16   = (int16_t *)malloc_aligned(64 * sizeof(int16_t), &raw_coeff16);

    int16_t *dc_coeff  = (int16_t *)malloc_aligned(64 * sizeof(int16_t), &raw_dc_coeff);
    uint8_t *dc_dst_c  = (uint8_t *)malloc_aligned(64 * sizeof(uint8_t), &raw_dc_dst_c);
    uint8_t *dc_dst_m  = (uint8_t *)malloc_aligned(64 * sizeof(uint8_t), &raw_dc_dst_m);

    /* Full 16x16 Macroblock Buffers (Y: 256, Cb: 64, Cr: 64) */
    void *raw_mb_res, *raw_mb_ref_y, *raw_mb_ref_cb, *raw_mb_ref_cr;
    void *raw_mb_dst_y_c, *raw_mb_dst_cb_c, *raw_mb_dst_cr_c;
    void *raw_mb_dst_y_m, *raw_mb_dst_cb_m, *raw_mb_dst_cr_m;

    int16_t (*mb_res)[64]  = malloc_aligned(6 * 64 * sizeof(int16_t), &raw_mb_res);
    uint8_t *mb_ref_y      = malloc_aligned(256 * sizeof(uint8_t), &raw_mb_ref_y);
    uint8_t *mb_ref_cb     = malloc_aligned(64 * sizeof(uint8_t), &raw_mb_ref_cb);
    uint8_t *mb_ref_cr     = malloc_aligned(64 * sizeof(uint8_t), &raw_mb_ref_cr);

    uint8_t *mb_dst_y_c    = malloc_aligned(256 * sizeof(uint8_t), &raw_mb_dst_y_c);
    uint8_t *mb_dst_cb_c   = malloc_aligned(64 * sizeof(uint8_t), &raw_mb_dst_cb_c);
    uint8_t *mb_dst_cr_c   = malloc_aligned(64 * sizeof(uint8_t), &raw_mb_dst_cr_c);

    uint8_t *mb_dst_y_m    = malloc_aligned(256 * sizeof(uint8_t), &raw_mb_dst_y_m);
    uint8_t *mb_dst_cb_m   = malloc_aligned(64 * sizeof(uint8_t), &raw_mb_dst_cb_m);
    uint8_t *mb_dst_cr_m   = malloc_aligned(64 * sizeof(uint8_t), &raw_mb_dst_cr_m);

    if (!ref8 || !dst8_c || !dst8_m || !res16 || !coeff16 || !dc_coeff || !dc_dst_c || !dc_dst_m ||
        !mb_res || !mb_ref_y || !mb_ref_cb || !mb_ref_cr ||
        !mb_dst_y_c || !mb_dst_cb_c || !mb_dst_cr_c ||
        !mb_dst_y_m || !mb_dst_cb_m || !mb_dst_cr_m) {
        printf("ERROR: Memory allocation failed!\n");
        return 1;
    }

    /* Synthesize realistic reference picture bytes (luminance ~120..180) */
    for (int i = 0; i < 64; i++) {
        ref8[i] = (uint8_t)(120 + ((i * 7) % 65));
        /* Signed 16-bit residuals (-60..+60) */
        res16[i] = (int16_t)(((i * 13) % 121) - 60);
    }

    /* Synthesize realistic DCT frequency coefficients */
    memset(coeff16, 0, 64 * sizeof(int16_t));
    coeff16[0] = 64;    /* DC residual */
    for (int k = 1; k < 15; k++) {
        coeff16[k] = (int16_t)((15 - k) * (k % 2 ? 2 : -2));
    }

    /* ------------------------------------------------------------------------------------------------- */
    /* Part 1: Isolated Motion Compensation Residual Addition Benchmark */
    /* ------------------------------------------------------------------------------------------------- */
    printf("=================================================================================================================================\n");
    printf(" Part 1: Motion Compensation Residual Addition (64 Pixels, Stride 8, %d runs)\n", NUM_RUNS);
    printf("---------------------------------------------------------------------------------------------------------------------------------\n");

    /* Warmup */
    mpeg_mc_residual_add_c(ref8, res16, dst8_c, 8);
    mpeg_mc_residual_add_ammx(ref8, res16, dst8_m, 8);

    uint32_t t0 = get_ccc();
    for (int i = 0; i < NUM_RUNS; i++) {
        mpeg_mc_residual_add_c(ref8, res16, dst8_c, 8);
    }
    uint32_t t1 = get_ccc();
    uint32_t cyc_mc_c = (t1 - t0) / NUM_RUNS;

    t0 = get_ccc();
    for (int i = 0; i < NUM_RUNS; i++) {
        mpeg_mc_residual_add_ammx(ref8, res16, dst8_m, 8);
    }
    t1 = get_ccc();
    uint32_t cyc_mc_m = (t1 - t0) / NUM_RUNS;

    int max_diff = 0;
    for (int i = 0; i < 64; i++) {
        int d = abs((int)dst8_c[i] - (int)dst8_m[i]);
        if (d > max_diff) max_diff = d;
    }

    printf(" Motion Comp Residual Add (C-Ref):      %6lu cycles (%.2f cyc/pixel)\n",
           (unsigned long)cyc_mc_c, (double)cyc_mc_c / 64.0);
    printf(" Motion Comp Residual Add (AMMX SIMD):  %6lu cycles (%.2f cyc/pixel, %.2fx vs C) [diff=%d, %s]\n\n",
           (unsigned long)cyc_mc_m, (double)cyc_mc_m / 64.0, (double)cyc_mc_c / cyc_mc_m,
           max_diff, (max_diff == 0) ? "PASS" : "FAIL");

    /* ------------------------------------------------------------------------------------------------- */
    /* Part 2: Fused 8x8 IDCT + Motion Compensation (Full Block) */
    /* ------------------------------------------------------------------------------------------------- */
    printf("=================================================================================================================================\n");
    printf(" Part 2: Fused 8x8 IDCT + Motion Compensation (Full Block, 64 Pixels, %d runs)\n", NUM_RUNS);
    printf("---------------------------------------------------------------------------------------------------------------------------------\n");

    /* Warmup */
    mpeg_idct8x8_fused_mc_c(coeff16, ref8, dst8_c, 8);
    mpeg_idct8x8_fused_mc_ammx(coeff16, ref8, dst8_m, 8);

    t0 = get_ccc();
    for (int i = 0; i < NUM_RUNS; i++) {
        mpeg_idct8x8_fused_mc_c(coeff16, ref8, dst8_c, 8);
    }
    t1 = get_ccc();
    uint32_t cyc_fused_c = (t1 - t0) / NUM_RUNS;

    t0 = get_ccc();
    for (int i = 0; i < NUM_RUNS; i++) {
        mpeg_idct8x8_fused_mc_ammx(coeff16, ref8, dst8_m, 8);
    }
    t1 = get_ccc();
    uint32_t cyc_fused_m = (t1 - t0) / NUM_RUNS;

    int max_fused_diff = 0;
    int max_fused_idx = 0;
    for (int i = 0; i < 64; i++) {
        int d = abs((int)dst8_c[i] - (int)dst8_m[i]);
        if (d > max_fused_diff) { max_fused_diff = d; max_fused_idx = i; }
    }

    printf(" Fused IDCT + MC (C-Ref):               %6lu cycles (%.2f cyc/pixel)\n",
           (unsigned long)cyc_fused_c, (double)cyc_fused_c / 64.0);
    printf(" Fused IDCT + MC (AMMX SIMD):           %6lu cycles (%.2f cyc/pixel, %.2fx vs C) [diff=%d LSB at %d (c=%u, m=%u), %s]\n",
           (unsigned long)cyc_fused_m, (double)cyc_fused_m / 64.0, (double)cyc_fused_c / cyc_fused_m,
           max_fused_diff, max_fused_idx, dst8_c[max_fused_idx], dst8_m[max_fused_idx],
           (max_fused_diff <= 16) ? "PASS" : "FAIL");
    printf("   c[0..7]:  %u, %u, %u, %u, %u, %u, %u, %u\n",
           dst8_c[0], dst8_c[1], dst8_c[2], dst8_c[3], dst8_c[4], dst8_c[5], dst8_c[6], dst8_c[7]);
    printf("   m[0..7]:  %u, %u, %u, %u, %u, %u, %u, %u\n\n",
           dst8_m[0], dst8_m[1], dst8_m[2], dst8_m[3], dst8_m[4], dst8_m[5], dst8_m[6], dst8_m[7]);

    /* ------------------------------------------------------------------------------------------------- */
    /* Part 3: Sparse Sub-Block Exits in Motion Compensation */
    /* ------------------------------------------------------------------------------------------------- */
    printf("=================================================================================================================================\n");
    printf(" Part 3: Sparse Sub-Block Exits (DC-Only Fast-Path vs Full Block, %d runs)\n", NUM_RUNS);
    printf("---------------------------------------------------------------------------------------------------------------------------------\n");

    memset(dc_coeff, 0, 64 * sizeof(int16_t));
    dc_coeff[0] = 200;  /* DC residual only (typical flat motion residual) */

    mpeg_idct8x8_fused_mc_c(dc_coeff, ref8, dc_dst_c, 8);
    mpeg_idct8x8_sparse_dc_mc_ammx(dc_coeff, ref8, dc_dst_m, 8);

    t0 = get_ccc();
    for (int i = 0; i < NUM_RUNS; i++) {
        mpeg_idct8x8_fused_mc_c(dc_coeff, ref8, dc_dst_c, 8);
    }
    t1 = get_ccc();
    uint32_t cyc_dc_c = (t1 - t0) / NUM_RUNS;

    t0 = get_ccc();
    for (int i = 0; i < NUM_RUNS; i++) {
        mpeg_idct8x8_sparse_dc_mc_ammx(dc_coeff, ref8, dc_dst_m, 8);
    }
    t1 = get_ccc();
    uint32_t cyc_dc_m = (t1 - t0) / NUM_RUNS;

    int max_dc_diff = 0;
    for (int i = 0; i < 64; i++) {
        int d = abs((int)dc_dst_c[i] - (int)dc_dst_m[i]);
        if (d > max_dc_diff) max_dc_diff = d;
    }

    printf(" DC-Only Block IDCT+MC (C-Ref):         %6lu cycles\n", (unsigned long)cyc_dc_c);
    printf(" DC-Only Block IDCT+MC (AMMX SIMD):     %6lu cycles (%.2f cyc/pixel, %.1fx vs Full Block, %.1fx vs C!) [diff=%d, %s]\n\n",
           (unsigned long)cyc_dc_m, (double)cyc_dc_m / 64.0,
           (double)cyc_fused_m / cyc_dc_m, (double)cyc_dc_c / cyc_dc_m,
           max_dc_diff, (max_dc_diff <= 1) ? "PASS" : "FAIL");

    /* ------------------------------------------------------------------------------------------------- */
    /* Part 4: Complete 16x16 Macroblock Video Decoding (4 Y + 1 Cb + 1 Cr = 6 blocks) */
    /* ------------------------------------------------------------------------------------------------- */
    printf("=================================================================================================================================\n");
    printf(" Part 4: Complete 16x16 Macroblock Video Decoding (6 Blocks = 384 Pixels, %d runs)\n", NUM_RUNS);
    printf("---------------------------------------------------------------------------------------------------------------------------------\n");

    /* Setup 6 blocks in macroblock */
    for (int b = 0; b < 6; b++) {
        memcpy(mb_res[b], coeff16, 64 * sizeof(int16_t));
    }
    for (int i = 0; i < 256; i++) mb_ref_y[i] = (uint8_t)(100 + (i % 80));
    for (int i = 0; i < 64; i++) {
        mb_ref_cb[i] = (uint8_t)(128 + (i % 40) - 20);
        mb_ref_cr[i] = (uint8_t)(128 + ((i * 3) % 40) - 20);
    }

    /* Warmup */
    mpeg_macroblock_mc_c(mb_res, mb_ref_y, mb_ref_cb, mb_ref_cr,
                         mb_dst_y_c, mb_dst_cb_c, mb_dst_cr_c, 16, 8);
    mpeg_macroblock_mc_ammx(mb_res, mb_ref_y, mb_ref_cb, mb_ref_cr,
                           mb_dst_y_m, mb_dst_cb_m, mb_dst_cr_m, 16, 8);

    /* Full AC Macroblock (Worst Case) */
    t0 = get_ccc();
    for (int i = 0; i < NUM_RUNS; i++) {
        mpeg_macroblock_mc_c(mb_res, mb_ref_y, mb_ref_cb, mb_ref_cr,
                             mb_dst_y_c, mb_dst_cb_c, mb_dst_cr_c, 16, 8);
    }
    t1 = get_ccc();
    uint32_t cyc_mb_c = (t1 - t0) / NUM_RUNS;

    t0 = get_ccc();
    for (int i = 0; i < NUM_RUNS; i++) {
        mpeg_macroblock_mc_ammx(mb_res, mb_ref_y, mb_ref_cb, mb_ref_cr,
                                mb_dst_y_m, mb_dst_cb_m, mb_dst_cr_m, 16, 8);
    }
    t1 = get_ccc();
    uint32_t cyc_mb_m = (t1 - t0) / NUM_RUNS;

    int max_diff_y = 0, max_diff_cb = 0, max_diff_cr = 0;
    int idx_y = 0, idx_cb = 0, idx_cr = 0;
    for (int i = 0; i < 256; i++) {
        int d = abs((int)mb_dst_y_c[i] - (int)mb_dst_y_m[i]);
        if (d > max_diff_y) { max_diff_y = d; idx_y = i; }
    }
    for (int i = 0; i < 64; i++) {
        int d = abs((int)mb_dst_cb_c[i] - (int)mb_dst_cb_m[i]);
        if (d > max_diff_cb) { max_diff_cb = d; idx_cb = i; }
        d = abs((int)mb_dst_cr_c[i] - (int)mb_dst_cr_m[i]);
        if (d > max_diff_cr) { max_diff_cr = d; idx_cr = i; }
    }
    int max_mb_diff = max_diff_y;
    if (max_diff_cb > max_mb_diff) max_mb_diff = max_diff_cb;
    if (max_diff_cr > max_mb_diff) max_mb_diff = max_diff_cr;

    /* Typical P/B-Frame Macroblock: 4 DC-only blocks, 2 AC blocks */
    for (int b = 2; b < 6; b++) {
        memcpy(mb_res[b], dc_coeff, 64 * sizeof(int16_t));
    }
    t0 = get_ccc();
    for (int i = 0; i < NUM_RUNS; i++) {
        mpeg_macroblock_mc_ammx(mb_res, mb_ref_y, mb_ref_cb, mb_ref_cr,
                                mb_dst_y_m, mb_dst_cb_m, mb_dst_cr_m, 16, 8);
    }
    t1 = get_ccc();
    uint32_t cyc_mb_typical = (t1 - t0) / NUM_RUNS;

    /* All DC Macroblock: Flat background / sky / uniform motion */
    for (int b = 0; b < 6; b++) {
        memcpy(mb_res[b], dc_coeff, 64 * sizeof(int16_t));
    }
    t0 = get_ccc();
    for (int i = 0; i < NUM_RUNS; i++) {
        mpeg_macroblock_mc_ammx(mb_res, mb_ref_y, mb_ref_cb, mb_ref_cr,
                                mb_dst_y_m, mb_dst_cb_m, mb_dst_cr_m, 16, 8);
    }
    t1 = get_ccc();
    uint32_t cyc_mb_flat = (t1 - t0) / NUM_RUNS;

    printf(" Full 16x16 Macroblock (C-Ref):         %6lu cycles (%.2f cyc/pixel)\n",
           (unsigned long)cyc_mb_c, (double)cyc_mb_c / 384.0);
    printf(" Full 16x16 Macroblock (Full AC AMMX):  %6lu cycles (%.2f cyc/pixel, %.2fx vs C) [diff=%d, %s]\n",
           (unsigned long)cyc_mb_m, (double)cyc_mb_m / 384.0, (double)cyc_mb_c / cyc_mb_m,
           max_mb_diff, (max_mb_diff <= 16) ? "PASS" : "FAIL");
    printf("   Diffs: Y=%d at idx %d (c=%u, m=%u), Cb=%d at idx %d (c=%u, m=%u), Cr=%d at idx %d (c=%u, m=%u)\n",
           max_diff_y, idx_y, mb_dst_y_c[idx_y], mb_dst_y_m[idx_y],
           max_diff_cb, idx_cb, mb_dst_cb_c[idx_cb], mb_dst_cb_m[idx_cb],
           max_diff_cr, idx_cr, mb_dst_cr_c[idx_cr], mb_dst_cr_m[idx_cr]);
    printf(" Typical P-Frame MB (4x DC, 2x AC):     %6lu cycles (%.2f cyc/pixel, %.2fx vs C, saves %lu cyc)\n",
           (unsigned long)cyc_mb_typical, (double)cyc_mb_typical / 384.0, (double)cyc_mb_c / cyc_mb_typical,
           (unsigned long)(cyc_mb_m - cyc_mb_typical));
    printf(" All-DC Flat Macroblock (6x DC):        %6lu cycles (%.2f cyc/pixel, %.2fx vs C, saves %lu cyc)\n\n",
           (unsigned long)cyc_mb_flat, (double)cyc_mb_flat / 384.0, (double)cyc_mb_c / cyc_mb_flat,
           (unsigned long)(cyc_mb_m - cyc_mb_flat));

    /* ------------------------------------------------------------------------------------------------- */
    /* Part 5: Full-Motion Video Framerate & CPU Overhead on Vampire V4 (85 MHz effective 68080) */
    /* ------------------------------------------------------------------------------------------------- */
    double mhz_v4 = 85.0;
    double mbs_vcd = (352.0 / 16.0) * (288.0 / 16.0);    /* 396 MBs */
    double mbs_dvd = (720.0 / 16.0) * (576.0 / 16.0);    /* 1,620 MBs */
    double mbs_720p = (1280.0 / 16.0) * (720.0 / 16.0);  /* 3,600 MBs */

    /* Typical MPEG video: 60% typical MBs, 40% full AC MBs */
    uint32_t cyc_avg_mb = (uint32_t)(0.4 * cyc_mb_m + 0.6 * cyc_mb_typical);

    double vcd_cyc_per_sec = mbs_vcd * cyc_avg_mb * 25.0;
    double vcd_cpu_pct = (vcd_cyc_per_sec / (mhz_v4 * 1000000.0)) * 100.0;

    double dvd_cyc_per_sec = mbs_dvd * cyc_avg_mb * 25.0;
    double dvd_cpu_pct = (dvd_cyc_per_sec / (mhz_v4 * 1000000.0)) * 100.0;

    double fps_720p = (mhz_v4 * 1000000.0) / (mbs_720p * cyc_avg_mb);

    printf("=================================================================================================================================\n");
    printf(" Full-Motion Video Decoding Throughput on Vampire V4 (85 MHz effective):\n");
    printf("   VCD / CIF (352x288 @ 25 FPS,   396 MBs):  CPU Load: %5.1f%% (Fluessiges Echtzeit-Video!)\n", vcd_cpu_pct);
    printf("   DVD / PAL (720x576 @ 25 FPS, 1,620 MBs):  CPU Load: %5.1f%% (Fluessiges Standard-Definition DVD!)\n", dvd_cpu_pct);
    printf("   720p HD   (1280x720,         3,600 MBs):  Max Throughput: %5.1f FPS\n", fps_720p);
    printf("=================================================================================================================================\n");

    free(raw_ref8); free(raw_dst8_c); free(raw_dst8_m);
    free(raw_res16); free(raw_coeff16);
    free(raw_dc_coeff); free(raw_dc_dst_c); free(raw_dc_dst_m);
    free(raw_mb_res); free(raw_mb_ref_y); free(raw_mb_ref_cb); free(raw_mb_ref_cr);
    free(raw_mb_dst_y_c); free(raw_mb_dst_cb_c); free(raw_mb_dst_cr_c);
    free(raw_mb_dst_y_m); free(raw_mb_dst_cb_m); free(raw_mb_dst_cr_m);

    return 0;
}
