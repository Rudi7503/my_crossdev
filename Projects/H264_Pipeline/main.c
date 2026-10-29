#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <math.h>
#include <string.h>
#include "h264.h"

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
    printf(" Apollo 68080 H.264 / AVC Video Decoding Engine Benchmark Suite\n");
    printf(" Optimizations: In-Register 4x4 Core IDCT + 4x4 AMMX Hadamard | Fused Motion Comp + PACKUSWB Direct Store\n");
    printf(" Real Hardware: Vampire V4 (Apollo 68080 @ Core Release)\n");
    printf("=================================================================================================================================\n\n");

    void *raw_in4x4, *raw_had_c, *raw_had_m;
    void *raw_idct_c, *raw_idct_m;
    void *raw_ref, *raw_dst_c, *raw_dst_m;
    void *raw_dc_in, *raw_dc_dst_c, *raw_dc_dst_m;

    int16_t (*in4x4)[4]    = malloc_aligned(16 * sizeof(int16_t), &raw_in4x4);
    int16_t (*had_c)[4]    = malloc_aligned(16 * sizeof(int16_t), &raw_had_c);
    int16_t (*had_m)[4]    = malloc_aligned(16 * sizeof(int16_t), &raw_had_m);
    int16_t (*idct_c)[4]   = malloc_aligned(16 * sizeof(int16_t), &raw_idct_c);
    int16_t (*idct_m)[4]   = malloc_aligned(16 * sizeof(int16_t), &raw_idct_m);

    uint8_t *ref           = malloc_aligned(64 * sizeof(uint8_t), &raw_ref);
    uint8_t *dst_c         = malloc_aligned(64 * sizeof(uint8_t), &raw_dst_c);
    uint8_t *dst_m         = malloc_aligned(64 * sizeof(uint8_t), &raw_dst_m);

    int16_t (*dc_in)[4]    = malloc_aligned(16 * sizeof(int16_t), &raw_dc_in);
    uint8_t *dc_dst_c      = malloc_aligned(64 * sizeof(uint8_t), &raw_dc_dst_c);
    uint8_t *dc_dst_m      = malloc_aligned(64 * sizeof(uint8_t), &raw_dc_dst_m);

    /* 16x16 Macroblock Buffers (16 4x4 blocks = 256 pixels) */
    void *raw_mb_blocks, *raw_mb_ref, *raw_mb_dst_c, *raw_mb_dst_m;
    int16_t (*mb_blocks)[16] = malloc_aligned(16 * 16 * sizeof(int16_t), &raw_mb_blocks);
    uint8_t *mb_ref        = malloc_aligned(256 * sizeof(uint8_t), &raw_mb_ref);
    uint8_t *mb_dst_c      = malloc_aligned(256 * sizeof(uint8_t), &raw_mb_dst_c);
    uint8_t *mb_dst_m      = malloc_aligned(256 * sizeof(uint8_t), &raw_mb_dst_m);

    if (!in4x4 || !had_c || !had_m || !idct_c || !idct_m ||
        !ref || !dst_c || !dst_m || !dc_in || !dc_dst_c || !dc_dst_m ||
        !mb_blocks || !mb_ref || !mb_dst_c || !mb_dst_m) {
        printf("ERROR: Memory allocation failed!\n");
        return 1;
    }

    /* Synthesize standard H.264 4x4 dequantized residual coefficients */
    in4x4[0][0] = 64;   in4x4[0][1] = -12;  in4x4[0][2] = 8;    in4x4[0][3] = -4;
    in4x4[1][0] = -16;  in4x4[1][1] = 10;   in4x4[1][2] = -6;   in4x4[1][3] = 2;
    in4x4[2][0] = 8;    in4x4[2][1] = -4;   in4x4[2][2] = 2;    in4x4[2][3] = 0;
    in4x4[3][0] = -4;   in4x4[3][1] = 2;    in4x4[3][2] = 0;    in4x4[3][3] = 0;

    for (int i = 0; i < 64; i++) {
        ref[i] = (uint8_t)(120 + ((i * 7) % 50));
    }

    /* ------------------------------------------------------------------------------------------------- */
    /* Part 1: 4x4 2D Hadamard Transform Benchmark */
    /* ------------------------------------------------------------------------------------------------- */
    printf("=================================================================================================================================\n");
    printf(" Part 1: 4x4 2D Hadamard Transform Benchmark (16 Coefficients, %d runs)\n", NUM_RUNS);
    printf("---------------------------------------------------------------------------------------------------------------------------------\n");

    /* Warmup */
    h264_hadamard4x4_c((const int16_t (*)[4])in4x4, had_c);
    h264_hadamard4x4_ammx((const int16_t (*)[4])in4x4, had_m);

    uint32_t t0 = get_ccc();
    for (int i = 0; i < NUM_RUNS; i++) {
        h264_hadamard4x4_c((const int16_t (*)[4])in4x4, had_c);
    }
    uint32_t t1 = get_ccc();
    uint32_t cyc_had_c = (t1 - t0) / NUM_RUNS;

    t0 = get_ccc();
    for (int i = 0; i < NUM_RUNS; i++) {
        h264_hadamard4x4_ammx((const int16_t (*)[4])in4x4, had_m);
    }
    t1 = get_ccc();
    uint32_t cyc_had_m = (t1 - t0) / NUM_RUNS;

    int max_had_diff = 0;
    for (int r = 0; r < 4; r++) {
        for (int c = 0; c < 4; c++) {
            int d = abs((int)had_c[r][c] - (int)had_m[r][c]);
            if (d > max_had_diff) max_had_diff = d;
        }
    }

    printf(" 4x4 2D Hadamard (C-Ref):               %6lu cycles (%.2f cyc/sample)\n",
           (unsigned long)cyc_had_c, (double)cyc_had_c / 16.0);
    printf(" 4x4 2D Hadamard (AMMX SIMD):           %6lu cycles (%.2f cyc/sample, %.2fx vs C) [diff=%d, %s]\n",
           (unsigned long)cyc_had_m, (double)cyc_had_m / 16.0, (double)cyc_had_c / cyc_had_m,
           max_had_diff, (max_had_diff == 0) ? "PASS" : "FAIL");
    printf("   had_c:\n");
    for (int r = 0; r < 4; r++)
        printf("     %5d, %5d, %5d, %5d\n", had_c[r][0], had_c[r][1], had_c[r][2], had_c[r][3]);
    printf("   had_m:\n");
    for (int r = 0; r < 4; r++)
        printf("     %5d, %5d, %5d, %5d\n", had_m[r][0], had_m[r][1], had_m[r][2], had_m[r][3]);
    printf("\n");

    /* ------------------------------------------------------------------------------------------------- */
    /* Part 2: 4x4 Core Inverse Integer Transform (H.264 IDCT) */
    /* ------------------------------------------------------------------------------------------------- */
    printf("=================================================================================================================================\n");
    printf(" Part 2: 4x4 Core Inverse Integer Transform (Bit-Exact IDCT, 16 Coefficients, %d runs)\n", NUM_RUNS);
    printf("---------------------------------------------------------------------------------------------------------------------------------\n");

    /* Warmup */
    h264_idct4x4_core_c((const int16_t (*)[4])in4x4, idct_c);
    h264_idct4x4_core_asm((const int16_t (*)[4])in4x4, idct_m);

    t0 = get_ccc();
    for (int i = 0; i < NUM_RUNS; i++) {
        h264_idct4x4_core_c((const int16_t (*)[4])in4x4, idct_c);
    }
    t1 = get_ccc();
    uint32_t cyc_idct_c = (t1 - t0) / NUM_RUNS;

    t0 = get_ccc();
    for (int i = 0; i < NUM_RUNS; i++) {
        h264_idct4x4_core_asm((const int16_t (*)[4])in4x4, idct_m);
    }
    t1 = get_ccc();
    uint32_t cyc_idct_m = (t1 - t0) / NUM_RUNS;

    int max_idct_diff = 0;
    for (int r = 0; r < 4; r++) {
        for (int c = 0; c < 4; c++) {
            int d = abs((int)idct_c[r][c] - (int)idct_m[r][c]);
            if (d > max_idct_diff) max_idct_diff = d;
        }
    }

    printf(" 4x4 Core IDCT (C-Ref):                 %6lu cycles (%.2f cyc/sample)\n",
           (unsigned long)cyc_idct_c, (double)cyc_idct_c / 16.0);
    printf(" 4x4 Core IDCT (68080 ASM):             %6lu cycles (%.2f cyc/sample, %.2fx vs C) [diff=%d, %s]\n",
           (unsigned long)cyc_idct_m, (double)cyc_idct_m / 16.0, (double)cyc_idct_c / cyc_idct_m,
           max_idct_diff, (max_idct_diff == 0) ? "PASS" : "FAIL");
    printf("   c[0][0..3]:  %d, %d, %d, %d\n", idct_c[0][0], idct_c[0][1], idct_c[0][2], idct_c[0][3]);
    printf("   m[0][0..3]:  %d, %d, %d, %d\n\n", idct_m[0][0], idct_m[0][1], idct_m[0][2], idct_m[0][3]);

    /* ------------------------------------------------------------------------------------------------- */
    /* Part 3: Fused 4x4 IDCT + Motion Compensation (Full Block & DC-Only Fast-Path) */
    /* ------------------------------------------------------------------------------------------------- */
    printf("=================================================================================================================================\n");
    printf(" Part 3: Fused 4x4 IDCT + Motion Comp (Full Block & DC-Only Fast-Path, %d runs)\n", NUM_RUNS);
    printf("---------------------------------------------------------------------------------------------------------------------------------\n");

    /* Full Block */
    h264_idct4x4_fused_mc_c((const int16_t (*)[4])in4x4, ref, dst_c, 4);
    h264_idct4x4_fused_mc_asm((const int16_t (*)[4])in4x4, ref, dst_m, 4);

    t0 = get_ccc();
    for (int i = 0; i < NUM_RUNS; i++) {
        h264_idct4x4_fused_mc_c((const int16_t (*)[4])in4x4, ref, dst_c, 4);
    }
    t1 = get_ccc();
    uint32_t cyc_fused_c = (t1 - t0) / NUM_RUNS;

    t0 = get_ccc();
    for (int i = 0; i < NUM_RUNS; i++) {
        h264_idct4x4_fused_mc_asm((const int16_t (*)[4])in4x4, ref, dst_m, 4);
    }
    t1 = get_ccc();
    uint32_t cyc_fused_m = (t1 - t0) / NUM_RUNS;

    int max_fused_diff = 0;
    for (int i = 0; i < 16; i++) {
        int d = abs((int)dst_c[i] - (int)dst_m[i]);
        if (d > max_fused_diff) max_fused_diff = d;
    }

    /* DC-Only Fast-Path */
    memset(dc_in, 0, 16 * sizeof(int16_t));
    dc_in[0][0] = 192;  /* DC-only residual */
    h264_idct4x4_fused_mc_c((const int16_t (*)[4])dc_in, ref, dc_dst_c, 4);
    h264_idct4x4_sparse_dc_mc_asm((const int16_t (*)[4])dc_in, ref, dc_dst_m, 4);

    t0 = get_ccc();
    for (int i = 0; i < NUM_RUNS; i++) {
        h264_idct4x4_fused_mc_c((const int16_t (*)[4])dc_in, ref, dc_dst_c, 4);
    }
    t1 = get_ccc();
    uint32_t cyc_dc_c = (t1 - t0) / NUM_RUNS;

    t0 = get_ccc();
    for (int i = 0; i < NUM_RUNS; i++) {
        h264_idct4x4_sparse_dc_mc_asm((const int16_t (*)[4])dc_in, ref, dc_dst_m, 4);
    }
    t1 = get_ccc();
    uint32_t cyc_dc_m = (t1 - t0) / NUM_RUNS;

    int max_dc_diff = 0;
    for (int i = 0; i < 16; i++) {
        int d = abs((int)dc_dst_c[i] - (int)dc_dst_m[i]);
        if (d > max_dc_diff) max_dc_diff = d;
    }

    printf(" Fused 4x4 IDCT+MC Full (C-Ref):        %6lu cycles (%.2f cyc/pixel)\n",
           (unsigned long)cyc_fused_c, (double)cyc_fused_c / 16.0);
    printf(" Fused 4x4 IDCT+MC Full (68080 ASM):    %6lu cycles (%.2f cyc/pixel, %.2fx vs C) [diff=%d, %s]\n",
           (unsigned long)cyc_fused_m, (double)cyc_fused_m / 16.0, (double)cyc_fused_c / cyc_fused_m,
           max_fused_diff, (max_fused_diff == 0) ? "PASS" : "FAIL");
    printf(" Fused 4x4 IDCT+MC DC-Only (68080 ASM): %6lu cycles (%.2f cyc/pixel, %.1fx vs Full, %.1fx vs C!) [diff=%d, %s]\n\n",
           (unsigned long)cyc_dc_m, (double)cyc_dc_m / 16.0,
           (double)cyc_fused_m / cyc_dc_m, (double)cyc_dc_c / cyc_dc_m,
           max_dc_diff, (max_dc_diff == 0) ? "PASS" : "FAIL");

    /* ------------------------------------------------------------------------------------------------- */
    /* Part 4: Complete 16x16 Luma Macroblock (16 4x4 Blocks = 256 Pixels) */
    /* ------------------------------------------------------------------------------------------------- */
    printf("=================================================================================================================================\n");
    printf(" Part 4: Complete 16x16 Luma Macroblock Decoding (16 4x4 Blocks = 256 Pixels, %d runs)\n", NUM_RUNS);
    printf("---------------------------------------------------------------------------------------------------------------------------------\n");

    /* Setup 16 blocks */
    for (int b = 0; b < 16; b++) {
        memcpy(&mb_blocks[b][0], in4x4, 16 * sizeof(int16_t));
    }
    for (int i = 0; i < 256; i++) {
        mb_ref[i] = (uint8_t)(100 + (i % 80));
    }

    /* Warmup */
    h264_macroblock_16x16_c(mb_blocks, mb_ref, mb_dst_c, 16);
    h264_macroblock_16x16_asm(mb_blocks, mb_ref, mb_dst_m, 16);

    t0 = get_ccc();
    for (int i = 0; i < NUM_RUNS; i++) {
        h264_macroblock_16x16_c(mb_blocks, mb_ref, mb_dst_c, 16);
    }
    t1 = get_ccc();
    uint32_t cyc_mb_c = (t1 - t0) / NUM_RUNS;

    t0 = get_ccc();
    for (int i = 0; i < NUM_RUNS; i++) {
        h264_macroblock_16x16_asm(mb_blocks, mb_ref, mb_dst_m, 16);
    }
    t1 = get_ccc();
    uint32_t cyc_mb_m = (t1 - t0) / NUM_RUNS;

    int max_mb_diff = 0;
    for (int i = 0; i < 256; i++) {
        int d = abs((int)mb_dst_c[i] - (int)mb_dst_m[i]);
        if (d > max_mb_diff) max_mb_diff = d;
    }

    /* Typical H.264 P-Frame MB: 12 DC-only blocks, 4 AC blocks */
    for (int b = 4; b < 16; b++) {
        memcpy(&mb_blocks[b][0], dc_in, 16 * sizeof(int16_t));
    }
    t0 = get_ccc();
    for (int i = 0; i < NUM_RUNS; i++) {
        h264_macroblock_16x16_asm(mb_blocks, mb_ref, mb_dst_m, 16);
    }
    t1 = get_ccc();
    uint32_t cyc_mb_typical = (t1 - t0) / NUM_RUNS;

    /* All-DC Flat Macroblock: 16 DC-only blocks */
    for (int b = 0; b < 16; b++) {
        memcpy(&mb_blocks[b][0], dc_in, 16 * sizeof(int16_t));
    }
    t0 = get_ccc();
    for (int i = 0; i < NUM_RUNS; i++) {
        h264_macroblock_16x16_asm(mb_blocks, mb_ref, mb_dst_m, 16);
    }
    t1 = get_ccc();
    uint32_t cyc_mb_flat = (t1 - t0) / NUM_RUNS;

    printf(" Full 16x16 MB (C-Ref):                 %6lu cycles (%.2f cyc/pixel)\n",
           (unsigned long)cyc_mb_c, (double)cyc_mb_c / 256.0);
    printf(" Full 16x16 MB (Full AC 68080 ASM):     %6lu cycles (%.2f cyc/pixel, %.2fx vs C) [diff=%d, %s]\n",
           (unsigned long)cyc_mb_m, (double)cyc_mb_m / 256.0, (double)cyc_mb_c / cyc_mb_m,
           max_mb_diff, (max_mb_diff == 0) ? "PASS" : "FAIL");
    printf(" Typical P-Frame MB (12x DC, 4x AC):    %6lu cycles (%.2f cyc/pixel, %.2fx vs C, saves %lu cyc)\n",
           (unsigned long)cyc_mb_typical, (double)cyc_mb_typical / 256.0, (double)cyc_mb_c / cyc_mb_typical,
           (unsigned long)(cyc_mb_m - cyc_mb_typical));
    printf(" All-DC Flat Macroblock (16x DC):       %6lu cycles (%.2f cyc/pixel, %.2fx vs C, saves %lu cyc)\n\n",
           (unsigned long)cyc_mb_flat, (double)cyc_mb_flat / 256.0, (double)cyc_mb_c / cyc_mb_flat,
           (unsigned long)(cyc_mb_m - cyc_mb_flat));

    /* ------------------------------------------------------------------------------------------------- */
    /* Part 5: H.264 Video Decoding Throughput on Vampire V4 (85 MHz effective 68080) */
    /* ------------------------------------------------------------------------------------------------- */
    double mhz_v4 = 85.0;
    double mbs_cif  = (352.0 / 16.0) * (288.0 / 16.0);    /* 396 MBs */
    double mbs_480p = (720.0 / 16.0) * (480.0 / 16.0);    /* 1,350 MBs */
    double mbs_720p = (1280.0 / 16.0) * (720.0 / 16.0);  /* 3,600 MBs */

    /* Typical H.264 video: 60% typical MBs, 40% full AC MBs */
    uint32_t cyc_avg_mb = (uint32_t)(0.4 * cyc_mb_m + 0.6 * cyc_mb_typical);

    double cif_cpu_pct  = ((mbs_cif  * cyc_avg_mb * 25.0) / (mhz_v4 * 1000000.0)) * 100.0;
    double p480_cpu_pct = ((mbs_480p * cyc_avg_mb * 30.0) / (mhz_v4 * 1000000.0)) * 100.0;
    double fps_720p     = (mhz_v4 * 1000000.0) / (mbs_720p * cyc_avg_mb);

    printf("=================================================================================================================================\n");
    printf(" Full-Motion H.264 Video Decoding Throughput on Vampire V4 (85 MHz effective):\n");
    printf("   CIF / SD  (352x288 @ 25 FPS,   396 MBs):  Avg MB: %4lu cyc  |  CPU Load: %5.1f%% (Echtzeit-H.264!)\n",
           (unsigned long)cyc_avg_mb, cif_cpu_pct);
    printf("   480p SD   (720x480 @ 30 FPS, 1,350 MBs):  Avg MB: %4lu cyc  |  CPU Load: %5.1f%%\n",
           (unsigned long)cyc_avg_mb, p480_cpu_pct);
    printf("   720p HD   (1280x720,         3,600 MBs):  Avg MB: %4lu cyc  |  Throughput: %5.1f FPS\n",
           (unsigned long)cyc_avg_mb, fps_720p);
    printf("=================================================================================================================================\n");

    free(raw_in4x4); free(raw_had_c); free(raw_had_m);
    free(raw_idct_c); free(raw_idct_m);
    free(raw_ref); free(raw_dst_c); free(raw_dst_m);
    free(raw_dc_in); free(raw_dc_dst_c); free(raw_dc_dst_m);
    free(raw_mb_blocks); free(raw_mb_ref); free(raw_mb_dst_c); free(raw_mb_dst_m);

    return 0;
}
