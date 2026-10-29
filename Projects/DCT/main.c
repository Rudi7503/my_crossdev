#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <math.h>
#include <string.h>
#include "dct.h"

#define NUM_BLOCKS 100

static void *malloc_aligned(size_t size, void **raw_out) {
    void *p = malloc(size + 64);
    if (!p) { *raw_out = NULL; return NULL; }
    *raw_out = p;
    uintptr_t addr = (uintptr_t)p;
    return (void *)((addr + 63) & ~(uintptr_t)63);
}

int main(void) {
    printf("=================================================================================================================================\n");
    printf(" Apollo 68080 DCT Benchmark & Validation Suite\n");
    printf(" Real Hardware: Vampire V4 (Apollo 68080 @ Core Release)\n");
    printf(" Formats: 32-bit Single-Precision Float & 16-bit Q1.15 Fixed-Point (Scalar & AMMX SIMD)\n");
    printf(" Dimensions: 1D 8-Point DCT/IDCT & 2D 8x8 Block DCT/IDCT\n");
    printf("=================================================================================================================================\n\n");

    printf("Initializing DCT transformation tables...\n");
    dct_init_tables();
    printf("DCT tables initialized (Orthonormal 8x8 Float & Q1.15 Fixed-Point).\n\n");

    /* Allocate 64-byte aligned test buffers */
    void *raw_f_in, *raw_f_out_c, *raw_f_out_asm, *raw_f_rec;
    void *raw_q_in, *raw_q_out_c, *raw_q_out_s, *raw_q_out_m, *raw_q_rec;

    float *f_in      = (float *)malloc_aligned(DCT_BLOCK_SIZE * sizeof(float), &raw_f_in);
    float *f_out_c   = (float *)malloc_aligned(DCT_BLOCK_SIZE * sizeof(float), &raw_f_out_c);
    float *f_out_asm = (float *)malloc_aligned(DCT_BLOCK_SIZE * sizeof(float), &raw_f_out_asm);
    float *f_rec     = (float *)malloc_aligned(DCT_BLOCK_SIZE * sizeof(float), &raw_f_rec);

    int16_t *q_in    = (int16_t *)malloc_aligned(DCT_BLOCK_SIZE * sizeof(int16_t), &raw_q_in);
    int16_t *q_out_c = (int16_t *)malloc_aligned(DCT_BLOCK_SIZE * sizeof(int16_t), &raw_q_out_c);
    int16_t *q_out_s = (int16_t *)malloc_aligned(DCT_BLOCK_SIZE * sizeof(int16_t), &raw_q_out_s);
    int16_t *q_out_m = (int16_t *)malloc_aligned(DCT_BLOCK_SIZE * sizeof(int16_t), &raw_q_out_m);
    int16_t *q_rec   = (int16_t *)malloc_aligned(DCT_BLOCK_SIZE * sizeof(int16_t), &raw_q_rec);

    if (!f_in || !f_out_c || !f_out_asm || !f_rec ||
        !q_in || !q_out_c || !q_out_s || !q_out_m || !q_rec) {
        printf("ERROR: Memory allocation failed!\n");
        return 1;
    }

    /* Initialize test signal: realistic 2D image block gradient + cosine wave */
    for (int y = 0; y < DCT_SIZE; y++) {
        for (int x = 0; x < DCT_SIZE; x++) {
            float val = 0.5f * (float)cos(2.0 * 3.14159265 * (x + 2 * y) / 16.0) +
                        0.3f * (float)sin(2.0 * 3.14159265 * (2 * x - y) / 16.0);
            f_in[y * DCT_SIZE + x] = val;
            q_in[y * DCT_SIZE + x] = (int16_t)(val * 16384.0f);
        }
    }

    printf("=================================================================================================================================\n");
    printf(" Part 1: 1D 8-Point DCT & IDCT Benchmark (Average over %d runs)\n", NUM_BLOCKS);
    printf("---------------------------------------------------------------------------------------------------------------------------------\n");

    /* Warmup */
    dct1d_float_c(f_in, f_out_c);
    idct1d_float_c(f_out_c, f_rec);
    dct1d_fixed_c(q_in, q_out_c);
    idct1d_fixed_c(q_out_c, q_rec);

    /* Measure 1D Float C */
    uint32_t t0 = get_ccc();
    for (int i = 0; i < NUM_BLOCKS; i++) {
        dct1d_float_c(f_in, f_out_c);
    }
    uint32_t t1 = get_ccc();
    uint32_t cyc_1d_fc = (t1 - t0) / NUM_BLOCKS;

    /* Measure 1D Float IDCT C */
    t0 = get_ccc();
    for (int i = 0; i < NUM_BLOCKS; i++) {
        idct1d_float_c(f_out_c, f_rec);
    }
    t1 = get_ccc();
    uint32_t cyc_1d_ifc = (t1 - t0) / NUM_BLOCKS;

    /* Measure 1D Fixed C */
    t0 = get_ccc();
    for (int i = 0; i < NUM_BLOCKS; i++) {
        dct1d_fixed_c(q_in, q_out_c);
    }
    t1 = get_ccc();
    uint32_t cyc_1d_qc = (t1 - t0) / NUM_BLOCKS;

    /* Measure 1D Fixed IDCT C */
    t0 = get_ccc();
    for (int i = 0; i < NUM_BLOCKS; i++) {
        idct1d_fixed_c(q_out_c, q_rec);
    }
    t1 = get_ccc();
    uint32_t cyc_1d_iqc = (t1 - t0) / NUM_BLOCKS;

    /* Measure 1D Float ASM */
    t0 = get_ccc();
    for (int i = 0; i < NUM_BLOCKS; i++) {
        dct1d_float_asm(f_in, f_out_asm);
    }
    t1 = get_ccc();
    uint32_t cyc_1d_fa = (t1 - t0) / NUM_BLOCKS;

    /* Measure 1D Float IDCT ASM */
    t0 = get_ccc();
    for (int i = 0; i < NUM_BLOCKS; i++) {
        idct1d_float_asm(f_out_asm, f_rec);
    }
    t1 = get_ccc();
    uint32_t cyc_1d_ifa = (t1 - t0) / NUM_BLOCKS;

    /* Measure 1D Fixed Scalar ASM */
    t0 = get_ccc();
    for (int i = 0; i < NUM_BLOCKS; i++) {
        dct1d_fixed_scalar_asm(q_in, q_out_s);
    }
    t1 = get_ccc();
    uint32_t cyc_1d_qs = (t1 - t0) / NUM_BLOCKS;

    /* Measure 1D Fixed IDCT Scalar ASM */
    t0 = get_ccc();
    for (int i = 0; i < NUM_BLOCKS; i++) {
        idct1d_fixed_scalar_asm(q_out_s, q_rec);
    }
    t1 = get_ccc();
    uint32_t cyc_1d_iqs = (t1 - t0) / NUM_BLOCKS;

    /* Measure 1D Fixed AMMX SIMD */
    t0 = get_ccc();
    for (int i = 0; i < NUM_BLOCKS; i++) {
        dct1d_fixed_ammx(q_in, q_out_m);
    }
    t1 = get_ccc();
    uint32_t cyc_1d_qm = (t1 - t0) / NUM_BLOCKS;

    /* Measure 1D Fixed IDCT AMMX SIMD */
    t0 = get_ccc();
    for (int i = 0; i < NUM_BLOCKS; i++) {
        idct1d_fixed_ammx(q_out_m, q_rec);
    }
    t1 = get_ccc();
    uint32_t cyc_1d_iqm = (t1 - t0) / NUM_BLOCKS;

    printf(" 1D 8-Point Float FDCT (C-Ref):  %5lu cycles\n", (unsigned long)cyc_1d_fc);
    printf(" 1D 8-Point Float FDCT (FPU ASM):%5lu cycles (%.2fx vs C)\n", (unsigned long)cyc_1d_fa, (double)cyc_1d_fc / cyc_1d_fa);
    printf(" 1D 8-Point Float IDCT (C-Ref):  %5lu cycles\n", (unsigned long)cyc_1d_ifc);
    printf(" 1D 8-Point Float IDCT (FPU ASM):%5lu cycles (%.2fx vs C)\n\n", (unsigned long)cyc_1d_ifa, (double)cyc_1d_ifc / cyc_1d_ifa);

    printf(" 1D 8-Point Fixed FDCT (C-Ref):  %5lu cycles\n", (unsigned long)cyc_1d_qc);
    printf(" 1D 8-Point Fixed FDCT (Scalar): %5lu cycles (%.2fx vs C)\n", (unsigned long)cyc_1d_qs, (double)cyc_1d_qc / cyc_1d_qs);
    printf(" 1D 8-Point Fixed FDCT (AMMX):   %5lu cycles (%.2fx vs C, %.2fx vs Scal)\n",
           (unsigned long)cyc_1d_qm, (double)cyc_1d_qc / cyc_1d_qm, (double)cyc_1d_qs / cyc_1d_qm);

    printf(" 1D 8-Point Fixed IDCT (C-Ref):  %5lu cycles\n", (unsigned long)cyc_1d_iqc);
    printf(" 1D 8-Point Fixed IDCT (Scalar): %5lu cycles (%.2fx vs C)\n", (unsigned long)cyc_1d_iqs, (double)cyc_1d_iqc / cyc_1d_iqs);
    printf(" 1D 8-Point Fixed IDCT (AMMX):   %5lu cycles (%.2fx vs C, %.2fx vs Scal)\n\n",
           (unsigned long)cyc_1d_iqm, (double)cyc_1d_iqc / cyc_1d_iqm, (double)cyc_1d_iqs / cyc_1d_iqm);

    /* Verify 1D ASM against C */
    float max_fa_diff = 0.0f;
    int max_qs_diff = 0;
    int max_qm_diff = 0;
    for (int i = 0; i < DCT_SIZE; i++) {
        float fad = fabsf(f_out_c[i] - f_out_asm[i]);
        if (fad > max_fa_diff) max_fa_diff = fad;
        int qsd = abs(q_out_c[i] - q_out_s[i]);
        if (qsd > max_qs_diff) max_qs_diff = qsd;
        int qmd = abs(q_out_c[i] - q_out_m[i]);
        if (qmd > max_qm_diff) max_qm_diff = qmd;
    }
    printf(" 1D Verification: Float ASM diff=%e | Scal diff=%d LSB | AMMX diff=%d LSB [%s]\n",
           (double)max_fa_diff, max_qs_diff, max_qm_diff,
           (max_fa_diff < 1e-4f && max_qs_diff == 0 && max_qm_diff <= 15) ? "PASS" : "FAIL");

    /* Check reconstruction fidelity */
    float max_f_err = 0.0f;
    int max_q_err = 0;
    for (int i = 0; i < DCT_SIZE; i++) {
        float fe = fabsf(f_in[i] - f_rec[i]);
        if (fe > max_f_err) max_f_err = fe;
        int qe = abs(q_in[i] - q_rec[i]);
        if (qe > max_q_err) max_q_err = qe;
    }
    printf(" 1D Reconstruction Error: Float max_diff=%e, Fixed max_diff=%d LSB\n",
           (double)max_f_err, max_q_err);

    printf("\n=================================================================================================================================\n");
    printf(" Part 2: 2D 8x8 Block DCT & IDCT Benchmark (Average over %d runs)\n", NUM_BLOCKS);
    printf("---------------------------------------------------------------------------------------------------------------------------------\n");

    /* Warmup 2D */
    dct2d_8x8_float_c(f_in, f_out_c);
    idct2d_8x8_float_c(f_out_c, f_rec);
    dct2d_8x8_fixed_c(q_in, q_out_c);
    idct2d_8x8_fixed_c(q_out_c, q_rec);

    /* Measure 2D Float C */
    t0 = get_ccc();
    for (int i = 0; i < NUM_BLOCKS; i++) {
        dct2d_8x8_float_c(f_in, f_out_c);
    }
    t1 = get_ccc();
    uint32_t cyc_2d_fc = (t1 - t0) / NUM_BLOCKS;

    /* Measure 2D Float IDCT C */
    t0 = get_ccc();
    for (int i = 0; i < NUM_BLOCKS; i++) {
        idct2d_8x8_float_c(f_out_c, f_rec);
    }
    t1 = get_ccc();
    uint32_t cyc_2d_ifc = (t1 - t0) / NUM_BLOCKS;

    /* Measure 2D Fixed C */
    t0 = get_ccc();
    for (int i = 0; i < NUM_BLOCKS; i++) {
        dct2d_8x8_fixed_c(q_in, q_out_c);
    }
    t1 = get_ccc();
    uint32_t cyc_2d_qc = (t1 - t0) / NUM_BLOCKS;

    /* Measure 2D Fixed IDCT C */
    t0 = get_ccc();
    for (int i = 0; i < NUM_BLOCKS; i++) {
        idct2d_8x8_fixed_c(q_out_c, q_rec);
    }
    t1 = get_ccc();
    uint32_t cyc_2d_iqc = (t1 - t0) / NUM_BLOCKS;

    /* Measure 2D Float ASM */
    t0 = get_ccc();
    for (int i = 0; i < NUM_BLOCKS; i++) {
        dct2d_8x8_float_asm(f_in, f_out_asm);
    }
    t1 = get_ccc();
    uint32_t cyc_2d_fa = (t1 - t0) / NUM_BLOCKS;

    /* Measure 2D Float IDCT ASM */
    t0 = get_ccc();
    for (int i = 0; i < NUM_BLOCKS; i++) {
        idct2d_8x8_float_asm(f_out_asm, f_rec);
    }
    t1 = get_ccc();
    uint32_t cyc_2d_ifa = (t1 - t0) / NUM_BLOCKS;

    /* Measure 2D Fixed Scalar ASM */
    t0 = get_ccc();
    for (int i = 0; i < NUM_BLOCKS; i++) {
        dct2d_8x8_fixed_scalar_asm(q_in, q_out_s);
    }
    t1 = get_ccc();
    uint32_t cyc_2d_qs = (t1 - t0) / NUM_BLOCKS;

    /* Measure 2D Fixed IDCT Scalar ASM */
    t0 = get_ccc();
    for (int i = 0; i < NUM_BLOCKS; i++) {
        idct2d_8x8_fixed_scalar_asm(q_out_s, q_rec);
    }
    t1 = get_ccc();
    uint32_t cyc_2d_iqs = (t1 - t0) / NUM_BLOCKS;

    /* Measure 2D Fixed AMMX SIMD */
    t0 = get_ccc();
    for (int i = 0; i < NUM_BLOCKS; i++) {
        dct2d_8x8_fixed_ammx(q_in, q_out_m);
    }
    t1 = get_ccc();
    uint32_t cyc_2d_qm = (t1 - t0) / NUM_BLOCKS;

    /* Measure 2D Fixed IDCT AMMX SIMD */
    t0 = get_ccc();
    for (int i = 0; i < NUM_BLOCKS; i++) {
        idct2d_8x8_fixed_ammx(q_out_m, q_rec);
    }
    t1 = get_ccc();
    uint32_t cyc_2d_iqm = (t1 - t0) / NUM_BLOCKS;

    printf(" 2D 8x8 Block Float FDCT (C-Ref): %6lu cycles\n", (unsigned long)cyc_2d_fc);
    printf(" 2D 8x8 Block Float FDCT (FPU ASM):%5lu cycles (%.2fx vs C)\n", (unsigned long)cyc_2d_fa, (double)cyc_2d_fc / cyc_2d_fa);
    printf(" 2D 8x8 Block Float IDCT (C-Ref): %6lu cycles\n", (unsigned long)cyc_2d_ifc);
    printf(" 2D 8x8 Block Float IDCT (FPU ASM):%5lu cycles (%.2fx vs C)\n\n", (unsigned long)cyc_2d_ifa, (double)cyc_2d_ifc / cyc_2d_ifa);

    printf(" 2D 8x8 Block Fixed FDCT (C-Ref): %6lu cycles\n", (unsigned long)cyc_2d_qc);
    printf(" 2D 8x8 Block Fixed FDCT (Scalar):%6lu cycles (%.2fx vs C)\n", (unsigned long)cyc_2d_qs, (double)cyc_2d_qc / cyc_2d_qs);
    printf(" 2D 8x8 Block Fixed FDCT (AMMX):  %6lu cycles (%.2fx vs C, %.2fx vs Scal)\n",
           (unsigned long)cyc_2d_qm, (double)cyc_2d_qc / cyc_2d_qm, (double)cyc_2d_qs / cyc_2d_qm);

    printf(" 2D 8x8 Block Fixed IDCT (C-Ref): %6lu cycles\n", (unsigned long)cyc_2d_iqc);
    printf(" 2D 8x8 Block Fixed IDCT (Scalar):%6lu cycles (%.2fx vs C)\n", (unsigned long)cyc_2d_iqs, (double)cyc_2d_iqc / cyc_2d_iqs);
    printf(" 2D 8x8 Block Fixed IDCT (AMMX):  %6lu cycles (%.2fx vs C, %.2fx vs Scal)\n\n",
           (unsigned long)cyc_2d_iqm, (double)cyc_2d_iqc / cyc_2d_iqm, (double)cyc_2d_iqs / cyc_2d_iqm);

    /* Verify 2D ASM against C */
    max_fa_diff = 0.0f;
    max_qs_diff = 0;
    max_qm_diff = 0;
    for (int i = 0; i < DCT_BLOCK_SIZE; i++) {
        float fad = fabsf(f_out_c[i] - f_out_asm[i]);
        if (fad > max_fa_diff) max_fa_diff = fad;
        int qsd = abs(q_out_c[i] - q_out_s[i]);
        if (qsd > max_qs_diff) max_qs_diff = qsd;
        int qmd = abs(q_out_c[i] - q_out_m[i]);
        if (qmd > max_qm_diff) max_qm_diff = qmd;
    }
    printf(" 2D Verification: Float ASM diff=%e | Scal diff=%d LSB | AMMX diff=%d LSB [%s]\n",
           (double)max_fa_diff, max_qs_diff, max_qm_diff,
           (max_fa_diff < 1e-4f && max_qs_diff == 0 && max_qm_diff <= 40) ? "PASS" : "FAIL");
    if (max_qs_diff > 0) {
        printf("   Fixed debug: c[0..3]=[%d, %d, %d, %d] s[0..3]=[%d, %d, %d, %d] m[0..3]=[%d, %d, %d, %d]\n",
               q_out_c[0], q_out_c[1], q_out_c[2], q_out_c[3],
               q_out_s[0], q_out_s[1], q_out_s[2], q_out_s[3],
               q_out_m[0], q_out_m[1], q_out_m[2], q_out_m[3]);
    }

    max_f_err = 0.0f;
    max_q_err = 0;
    for (int i = 0; i < DCT_BLOCK_SIZE; i++) {
        float fe = fabsf(f_in[i] - f_rec[i]);
        if (fe > max_f_err) max_f_err = fe;
        int qe = abs(q_in[i] - q_rec[i]);
        if (qe > max_q_err) max_q_err = qe;
    }
    printf(" 2D Reconstruction Error: Float max_diff=%e, Fixed max_diff=%d LSB\n",
           (double)max_f_err, max_q_err);

    printf("=================================================================================================================================\n");

    free(raw_f_in);
    free(raw_f_out_c);
    free(raw_f_out_asm);
    free(raw_f_rec);
    free(raw_q_in);
    free(raw_q_out_c);
    free(raw_q_out_s);
    free(raw_q_out_m);
    free(raw_q_rec);

    return 0;
}
