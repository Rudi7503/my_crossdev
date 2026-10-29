#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <math.h>
#include <string.h>
#include "jpeg.h"

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
    printf(" Apollo 68080 JPEG End-to-End Decoding Pipeline Benchmark Suite\n");
    printf(" Optimizations: Fused IDCT + Level-Shift (+128) + PACKUSWB Direct 8-Bit Store | Fused Dequant | SIMD YCbCr->RGB565\n");
    printf(" Real Hardware: Vampire V4 (Apollo 68080 @ Core Release)\n");
    printf("=================================================================================================================================\n\n");

    jpeg_init_tables();

    void *raw_y_zz, *raw_cb_zz, *raw_cr_zz;
    void *raw_y_rast_c, *raw_y_rast_m;
    void *raw_y_pix_c, *raw_y_pix_m;
    void *raw_cb_pix, *raw_cr_pix;
    void *raw_rgb565_c, *raw_rgb565_m;
    void *raw_argb32_c, *raw_argb32_m;
    void *raw_dc_rast, *raw_dc_pix_c, *raw_dc_pix_m;
    void *raw_y_flat, *raw_cb_flat, *raw_cr_flat;

    int16_t *y_zz       = (int16_t *)malloc_aligned(64 * sizeof(int16_t), &raw_y_zz);
    int16_t *cb_zz      = (int16_t *)malloc_aligned(64 * sizeof(int16_t), &raw_cb_zz);
    int16_t *cr_zz      = (int16_t *)malloc_aligned(64 * sizeof(int16_t), &raw_cr_zz);

    int16_t *y_rast_c   = (int16_t *)malloc_aligned(64 * sizeof(int16_t), &raw_y_rast_c);
    int16_t *y_rast_m   = (int16_t *)malloc_aligned(64 * sizeof(int16_t), &raw_y_rast_m);

    uint8_t *y_pix_c    = (uint8_t *)malloc_aligned(64 * sizeof(uint8_t), &raw_y_pix_c);
    uint8_t *y_pix_m    = (uint8_t *)malloc_aligned(64 * sizeof(uint8_t), &raw_y_pix_m);
    uint8_t *cb_pix     = (uint8_t *)malloc_aligned(64 * sizeof(uint8_t), &raw_cb_pix);
    uint8_t *cr_pix     = (uint8_t *)malloc_aligned(64 * sizeof(uint8_t), &raw_cr_pix);

    uint16_t *rgb565_c  = (uint16_t *)malloc_aligned(64 * sizeof(uint16_t), &raw_rgb565_c);
    uint16_t *rgb565_m  = (uint16_t *)malloc_aligned(64 * sizeof(uint16_t), &raw_rgb565_m);

    uint32_t *argb32_c  = (uint32_t *)malloc_aligned(64 * sizeof(uint32_t), &raw_argb32_c);
    uint32_t *argb32_m  = (uint32_t *)malloc_aligned(64 * sizeof(uint32_t), &raw_argb32_m);

    int16_t *dc_rast    = (int16_t *)malloc_aligned(64 * sizeof(int16_t), &raw_dc_rast);
    uint8_t *dc_pix_c   = (uint8_t *)malloc_aligned(64 * sizeof(uint8_t), &raw_dc_pix_c);
    uint8_t *dc_pix_m   = (uint8_t *)malloc_aligned(64 * sizeof(uint8_t), &raw_dc_pix_m);

    int16_t *y_flat_zz  = (int16_t *)malloc_aligned(64 * sizeof(int16_t), &raw_y_flat);
    int16_t *cb_flat_zz = (int16_t *)malloc_aligned(64 * sizeof(int16_t), &raw_cb_flat);
    int16_t *cr_flat_zz = (int16_t *)malloc_aligned(64 * sizeof(int16_t), &raw_cr_flat);

    if (!y_zz || !cb_zz || !cr_zz || !y_rast_c || !y_rast_m ||
        !y_pix_c || !y_pix_m || !cb_pix || !cr_pix ||
        !rgb565_c || !rgb565_m || !argb32_c || !argb32_m ||
        !dc_rast || !dc_pix_c || !dc_pix_m ||
        !y_flat_zz || !cb_flat_zz || !cr_flat_zz) {
        printf("ERROR: Memory allocation failed!\n");
        return 1;
    }

    /* Synthesize realistic quantized JPEG DCT coefficients (natural image energy compaction) */
    y_zz[0] = 32;   /* DC coefficient */
    cb_zz[0] = 16;
    cr_zz[0] = -8;
    for (int k = 1; k < 64; k++) {
        /* AC coefficients decay rapidly in high frequencies */
        if (k < 15) {
            y_zz[k]  = (int16_t)((15 - k) * (k % 2 ? 1 : -1));
            cb_zz[k] = (int16_t)((10 - (k % 10)) * (k % 3 ? 1 : -1));
            cr_zz[k] = (int16_t)((8 - (k % 8)) * (k % 2 ? -1 : 1));
        } else {
            y_zz[k]  = 0;
            cb_zz[k] = 0;
            cr_zz[k] = 0;
        }
    }

    /* ------------------------------------------------------------------------------------------------- */
    /* Part 1: Dequantization + Zig-Zag Reorder Benchmark */
    /* ------------------------------------------------------------------------------------------------- */
    printf("=================================================================================================================================\n");
    printf(" Part 1: Dequantization & Zig-Zag Reordering Benchmark (64 Coefficients, %d runs)\n", NUM_RUNS);
    printf("---------------------------------------------------------------------------------------------------------------------------------\n");

    /* Warmup */
    dequant_zigzag_c(y_zz, jpeg_luma_quant, y_rast_c);
    dequant_zigzag_ammx(y_zz, jpeg_luma_quant, y_rast_m);

    uint32_t t0 = get_ccc();
    for (int i = 0; i < NUM_RUNS; i++) {
        dequant_zigzag_c(y_zz, jpeg_luma_quant, y_rast_c);
    }
    uint32_t t1 = get_ccc();
    uint32_t cyc_dq_c = (t1 - t0) / NUM_RUNS;

    t0 = get_ccc();
    for (int i = 0; i < NUM_RUNS; i++) {
        dequant_zigzag_ammx(y_zz, jpeg_luma_quant, y_rast_m);
    }
    t1 = get_ccc();
    uint32_t cyc_dq_m = (t1 - t0) / NUM_RUNS;

    int max_dq_diff = 0;
    for (int i = 0; i < 64; i++) {
        int d = abs(y_rast_c[i] - y_rast_m[i]);
        if (d > max_dq_diff) max_dq_diff = d;
    }

    printf(" Dequant + ZigZag (C-Ref):              %6lu cycles\n", (unsigned long)cyc_dq_c);
    printf(" Dequant + ZigZag (68080 ASM):          %6lu cycles (%.2fx vs C) [diff=%d, %s]\n\n",
           (unsigned long)cyc_dq_m, (double)cyc_dq_c / cyc_dq_m, max_dq_diff,
           (max_dq_diff == 0) ? "PASS" : "FAIL");

    /* ------------------------------------------------------------------------------------------------- */
    /* Part 2: Fused 8x8 IDCT + Level-Shift (+128) + PACKUSWB Direct 8-Bit Store */
    /* ------------------------------------------------------------------------------------------------- */
    printf("=================================================================================================================================\n");
    printf(" Part 2: Fused 8x8 IDCT + Level-Shift (+128) + PACKUSWB Direct 8-Bit Store (64 Pixels, %d runs)\n", NUM_RUNS);
    printf("---------------------------------------------------------------------------------------------------------------------------------\n");

    /* Warmup */
    idct8x8_levelshift_clamp_c(y_rast_c, y_pix_c);
    idct8x8_levelshift_packuswb_ammx(y_rast_m, y_pix_m);

    t0 = get_ccc();
    for (int i = 0; i < NUM_RUNS; i++) {
        idct8x8_levelshift_clamp_c(y_rast_c, y_pix_c);
    }
    t1 = get_ccc();
    uint32_t cyc_idct_c = (t1 - t0) / NUM_RUNS;

    t0 = get_ccc();
    for (int i = 0; i < NUM_RUNS; i++) {
        idct8x8_levelshift_packuswb_ammx(y_rast_m, y_pix_m);
    }
    t1 = get_ccc();
    uint32_t cyc_idct_m = (t1 - t0) / NUM_RUNS;

    /* Sparse IDCT on Full Block (checks detection overhead) */
    t0 = get_ccc();
    for (int i = 0; i < NUM_RUNS; i++) {
        idct8x8_sparse_levelshift_packuswb_ammx(y_rast_m, y_pix_m);
    }
    t1 = get_ccc();
    uint32_t cyc_idct_sparse_full = (t1 - t0) / NUM_RUNS;

    /* DC-Only Block Fast Path */
    memset(dc_rast, 0, 64 * sizeof(int16_t));
    dc_rast[0] = 512;
    idct8x8_levelshift_clamp_c(dc_rast, dc_pix_c);
    idct8x8_sparse_levelshift_packuswb_ammx(dc_rast, dc_pix_m);

    t0 = get_ccc();
    for (int i = 0; i < NUM_RUNS; i++) {
        idct8x8_sparse_levelshift_packuswb_ammx(dc_rast, dc_pix_m);
    }
    t1 = get_ccc();
    uint32_t cyc_idct_dc_only = (t1 - t0) / NUM_RUNS;

    int max_pix_diff = 0;
    int max_pix_idx = 0;
    for (int i = 0; i < 64; i++) {
        int d = abs((int)y_pix_c[i] - (int)y_pix_m[i]);
        if (d > max_pix_diff) { max_pix_diff = d; max_pix_idx = i; }
    }

    int max_dc_diff = 0;
    for (int i = 0; i < 64; i++) {
        int d = abs((int)dc_pix_c[i] - (int)dc_pix_m[i]);
        if (d > max_dc_diff) max_dc_diff = d;
    }

    printf(" Fused IDCT + Clamp (C-Ref):            %6lu cycles\n", (unsigned long)cyc_idct_c);
    printf(" Fused IDCT + PACKUSWB (AMMX SIMD):     %6lu cycles (%.2fx vs C) [diff=%d LSB, %s]\n",
           (unsigned long)cyc_idct_m, (double)cyc_idct_c / cyc_idct_m,
           max_pix_diff, (max_pix_diff <= 16) ? "PASS" : "FAIL");
    printf(" Sparse IDCT (Full Block, AMMX):        %6lu cycles (overhead check)\n",
           (unsigned long)cyc_idct_sparse_full);
    printf(" Sparse IDCT (DC-Only Fast-Path):       %6lu cycles (%.1fx vs Full Block, %.1fx vs C!) [diff=%d, %s]\n\n",
           (unsigned long)cyc_idct_dc_only,
           (double)cyc_idct_m / (cyc_idct_dc_only ? cyc_idct_dc_only : 1),
           (double)cyc_idct_c / (cyc_idct_dc_only ? cyc_idct_dc_only : 1),
           max_dc_diff, (max_dc_diff <= 1) ? "PASS" : "FAIL");
    printf("   c[0..7]:  %u, %u, %u, %u, %u, %u, %u, %u\n",
           y_pix_c[0], y_pix_c[1], y_pix_c[2], y_pix_c[3], y_pix_c[4], y_pix_c[5], y_pix_c[6], y_pix_c[7]);
    printf("   m[0..7]:  %u, %u, %u, %u, %u, %u, %u, %u\n\n",
           y_pix_m[0], y_pix_m[1], y_pix_m[2], y_pix_m[3], y_pix_m[4], y_pix_m[5], y_pix_m[6], y_pix_m[7]);

    /* Prepare Cb and Cr pixels for color conversion */
    idct8x8_levelshift_clamp_c(cb_zz, cb_pix);
    idct8x8_levelshift_clamp_c(cr_zz, cr_pix);

    /* ------------------------------------------------------------------------------------------------- */
    /* Part 3: YCbCr -> RGB565 / ARGB32 Color Space Conversion */
    /* ------------------------------------------------------------------------------------------------- */
    printf("=================================================================================================================================\n");
    printf(" Part 3: YCbCr -> RGB565 & ARGB32 Color Conversion (64 Pixels, %d runs)\n", NUM_RUNS);
    printf("---------------------------------------------------------------------------------------------------------------------------------\n");

    /* RGB565 (isolated unit test with identical inputs) */
    ycbcr_to_rgb565_c(y_pix_c, cb_pix, cr_pix, rgb565_c);
    ycbcr_to_rgb565_ammx(y_pix_c, cb_pix, cr_pix, rgb565_m);

    t0 = get_ccc();
    for (int i = 0; i < NUM_RUNS; i++) {
        ycbcr_to_rgb565_c(y_pix_c, cb_pix, cr_pix, rgb565_c);
    }
    t1 = get_ccc();
    uint32_t cyc_col565_c = (t1 - t0) / NUM_RUNS;

    t0 = get_ccc();
    for (int i = 0; i < NUM_RUNS; i++) {
        ycbcr_to_rgb565_ammx(y_pix_c, cb_pix, cr_pix, rgb565_m);
    }
    t1 = get_ccc();
    uint32_t cyc_col565_m = (t1 - t0) / NUM_RUNS;

    int max_col565_diff = 0;
    for (int i = 0; i < 64; i++) {
        int d = abs((int)rgb565_c[i] - (int)rgb565_m[i]);
        if (d > max_col565_diff) max_col565_diff = d;
    }

    printf(" YCbCr -> RGB565 (C-Ref):               %6lu cycles\n", (unsigned long)cyc_col565_c);
    printf(" YCbCr -> RGB565 (AMMX SIMD):           %6lu cycles (%.2fx vs C) [diff=%d, %s]\n\n",
           (unsigned long)cyc_col565_m, (double)cyc_col565_c / cyc_col565_m, max_col565_diff,
           (max_col565_diff == 0) ? "PASS" : "FAIL");

    /* ARGB32 */
    ycbcr_to_argb32_c(y_pix_c, cb_pix, cr_pix, argb32_c);
    ycbcr_to_argb32_ammx(y_pix_c, cb_pix, cr_pix, argb32_m);

    t0 = get_ccc();
    for (int i = 0; i < NUM_RUNS; i++) {
        ycbcr_to_argb32_c(y_pix_c, cb_pix, cr_pix, argb32_c);
    }
    t1 = get_ccc();
    uint32_t cyc_col32_c = (t1 - t0) / NUM_RUNS;

    t0 = get_ccc();
    for (int i = 0; i < NUM_RUNS; i++) {
        ycbcr_to_argb32_ammx(y_pix_c, cb_pix, cr_pix, argb32_m);
    }
    t1 = get_ccc();
    uint32_t cyc_col32_m = (t1 - t0) / NUM_RUNS;

    int max_col32_diff = 0;
    for (int i = 0; i < 64; i++) {
        int d_b = abs((int)(argb32_c[i] & 0xFF) - (int)(argb32_m[i] & 0xFF));
        int d_g = abs((int)((argb32_c[i] >> 8) & 0xFF) - (int)((argb32_m[i] >> 8) & 0xFF));
        int d_r = abs((int)((argb32_c[i] >> 16) & 0xFF) - (int)((argb32_m[i] >> 16) & 0xFF));
        if (d_b > max_col32_diff) max_col32_diff = d_b;
        if (d_g > max_col32_diff) max_col32_diff = d_g;
        if (d_r > max_col32_diff) max_col32_diff = d_r;
    }

    printf(" YCbCr -> ARGB32 (C-Ref):               %6lu cycles\n", (unsigned long)cyc_col32_c);
    printf(" YCbCr -> ARGB32 (AMMX SIMD):           %6lu cycles (%.2fx vs C) [diff=%d, %s]\n\n",
           (unsigned long)cyc_col32_m, (double)cyc_col32_c / cyc_col32_m, max_col32_diff,
           (max_col32_diff == 0) ? "PASS" : "FAIL");

    /* ------------------------------------------------------------------------------------------------- */
    /* Part 4: Complete End-to-End MCU Decoding Pipeline (Y + Cb + Cr Blocks -> RGB565) */
    /* ------------------------------------------------------------------------------------------------- */
    printf("=================================================================================================================================\n");
    printf(" Part 4: Complete End-to-End MCU Decoding Pipeline (Y+Cb+Cr Dequant + IDCT + RGB565)\n");
    printf("---------------------------------------------------------------------------------------------------------------------------------\n");

    /* Warmup */
    jpeg_pipeline_mcu_c(y_zz, cb_zz, cr_zz, jpeg_luma_quant, jpeg_chroma_quant, rgb565_c);
    jpeg_pipeline_mcu_ammx(y_zz, cb_zz, cr_zz, jpeg_luma_quant, jpeg_chroma_quant, rgb565_m);

    t0 = get_ccc();
    for (int i = 0; i < NUM_RUNS; i++) {
        jpeg_pipeline_mcu_c(y_zz, cb_zz, cr_zz, jpeg_luma_quant, jpeg_chroma_quant, rgb565_c);
    }
    t1 = get_ccc();
    uint32_t cyc_pipe_c = (t1 - t0) / NUM_RUNS;

    t0 = get_ccc();
    for (int i = 0; i < NUM_RUNS; i++) {
        jpeg_pipeline_mcu_ammx(y_zz, cb_zz, cr_zz, jpeg_luma_quant, jpeg_chroma_quant, rgb565_m);
    }
    t1 = get_ccc();
    uint32_t cyc_pipe_m = (t1 - t0) / NUM_RUNS;

    int max_dr = 0, max_dg = 0, max_db = 0;
    for (int i = 0; i < 64; i++) {
        int r_c = (rgb565_c[i] >> 11) & 31;
        int r_m = (rgb565_m[i] >> 11) & 31;
        int g_c = (rgb565_c[i] >> 5) & 63;
        int g_m = (rgb565_m[i] >> 5) & 63;
        int b_c = rgb565_c[i] & 31;
        int b_m = rgb565_m[i] & 31;
        if (abs(r_c - r_m) > max_dr) max_dr = abs(r_c - r_m);
        if (abs(g_c - g_m) > max_dg) max_dg = abs(g_c - g_m);
        if (abs(b_c - b_m) > max_db) max_db = abs(b_c - b_m);
    }

    /* Typical MCU: Chroma Cb/Cr are DC-only (typical flat chroma in real photos) */
    memset(cb_flat_zz, 0, 64 * sizeof(int16_t)); cb_flat_zz[0] = 16;
    memset(cr_flat_zz, 0, 64 * sizeof(int16_t)); cr_flat_zz[0] = -8;

    t0 = get_ccc();
    for (int i = 0; i < NUM_RUNS; i++) {
        jpeg_pipeline_mcu_ammx(y_zz, cb_flat_zz, cr_flat_zz, jpeg_luma_quant, jpeg_chroma_quant, rgb565_m);
    }
    t1 = get_ccc();
    uint32_t cyc_pipe_semi = (t1 - t0) / NUM_RUNS;

    /* Flat / Sky MCU: All Y, Cb, Cr are DC-only (~50% of real JPEG MCUs) */
    memset(y_flat_zz, 0, 64 * sizeof(int16_t)); y_flat_zz[0] = 32;

    t0 = get_ccc();
    for (int i = 0; i < NUM_RUNS; i++) {
        jpeg_pipeline_mcu_ammx(y_flat_zz, cb_flat_zz, cr_flat_zz, jpeg_luma_quant, jpeg_chroma_quant, rgb565_m);
    }
    t1 = get_ccc();
    uint32_t cyc_pipe_flat = (t1 - t0) / NUM_RUNS;

    printf(" Complete JPEG MCU Pipeline (C-Ref):        %6lu cycles\n", (unsigned long)cyc_pipe_c);
    printf(" Complete JPEG MCU Pipeline (Full AC AMMX): %6lu cycles (%.2fx vs C) [RGB max_diff: R=%d/31, G=%d/63, B=%d/31, %s]\n",
           (unsigned long)cyc_pipe_m, (double)cyc_pipe_c / cyc_pipe_m,
           max_dr, max_dg, max_db,
           (max_dr <= 4 && max_dg <= 4 && max_db <= 4) ? "PASS" : "FAIL");
    printf(" Complete JPEG MCU Pipeline (Chroma DC-Only):%6lu cycles (%.2fx vs C, saves %lu cyc)\n",
           (unsigned long)cyc_pipe_semi, (double)cyc_pipe_c / cyc_pipe_semi,
           (unsigned long)(cyc_pipe_m - cyc_pipe_semi));
    printf(" Complete JPEG MCU Pipeline (All DC-Only):  %6lu cycles (%.2fx vs C, saves %lu cyc)\n",
           (unsigned long)cyc_pipe_flat, (double)cyc_pipe_c / cyc_pipe_flat,
           (unsigned long)(cyc_pipe_m - cyc_pipe_flat));

    /* Frame Decoding Throughput on Vampire V4 (85 MHz effective 68080) */
    double mhz_v4 = 85.0;
    double mcus_720p  = (1280.0 / 8.0) * (720.0 / 8.0);   /* 14,400 MCUs */
    double mcus_1080p = (1920.0 / 8.0) * (1080.0 / 8.0);  /* 32,400 MCUs */

    /* Average real-world frame: 60% DC-only MCUs, 40% AC MCUs */
    uint32_t cyc_avg = (uint32_t)(0.4 * cyc_pipe_m + 0.6 * cyc_pipe_flat);
    double fps_720p_avg = (mhz_v4 * 1000000.0) / (mcus_720p * cyc_avg);
    double fps_1080p_avg = (mhz_v4 * 1000000.0) / (mcus_1080p * cyc_avg);

    printf("\n---------------------------------------------------------------------------------------------------------------------------------\n");
    printf(" Full-Resolution Video/Photo Decoding Performance on Vampire V4 (Avg Real-World 60%% DC):\n");
    printf("   720p HD  (1280x720,  14,400 MCUs):  Avg MCU: %5lu cyc  |  Throughput: %5.1f FPS\n",
           (unsigned long)cyc_avg, fps_720p_avg);
    printf("   1080p HD (1920x1080, 32,400 MCUs):  Avg MCU: %5lu cyc  |  Throughput: %5.1f FPS\n",
           (unsigned long)cyc_avg, fps_1080p_avg);
    printf("=================================================================================================================================\n");

    free(raw_y_zz); free(raw_cb_zz); free(raw_cr_zz);
    free(raw_y_rast_c); free(raw_y_rast_m);
    free(raw_y_pix_c); free(raw_y_pix_m); free(raw_cb_pix); free(raw_cr_pix);
    free(raw_rgb565_c); free(raw_rgb565_m);
    free(raw_argb32_c); free(raw_argb32_m);
    free(raw_dc_rast); free(raw_dc_pix_c); free(raw_dc_pix_m);
    free(raw_y_flat); free(raw_cb_flat); free(raw_cr_flat);

    return 0;
}
