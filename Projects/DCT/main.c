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
    void *raw_f_in, *raw_f_out_c, *raw_f_out_asm, *raw_f_out_fa, *raw_f_rec;
    void *raw_q_in, *raw_q_out_c, *raw_q_out_s, *raw_q_out_m, *raw_q_out_fm, *raw_q_rec;
    void *raw_q_inreg, *raw_q_rec_inreg, *raw_q_dc_in, *raw_q_dc_out, *raw_q_rec_sparse;
    void *raw_q_quant_c, *raw_q_quant_m, *raw_q_fused, *raw_recip_q;

    float *f_in          = (float *)malloc_aligned(DCT_BLOCK_SIZE * sizeof(float), &raw_f_in);
    float *f_out_c       = (float *)malloc_aligned(DCT_BLOCK_SIZE * sizeof(float), &raw_f_out_c);
    float *f_out_asm     = (float *)malloc_aligned(DCT_BLOCK_SIZE * sizeof(float), &raw_f_out_asm);
    float *f_out_fa      = (float *)malloc_aligned(DCT_BLOCK_SIZE * sizeof(float), &raw_f_out_fa);
    float *f_rec         = (float *)malloc_aligned(DCT_BLOCK_SIZE * sizeof(float), &raw_f_rec);

    int16_t *q_in        = (int16_t *)malloc_aligned(DCT_BLOCK_SIZE * sizeof(int16_t), &raw_q_in);
    int16_t *q_out_c     = (int16_t *)malloc_aligned(DCT_BLOCK_SIZE * sizeof(int16_t), &raw_q_out_c);
    int16_t *q_out_s     = (int16_t *)malloc_aligned(DCT_BLOCK_SIZE * sizeof(int16_t), &raw_q_out_s);
    int16_t *q_out_m     = (int16_t *)malloc_aligned(DCT_BLOCK_SIZE * sizeof(int16_t), &raw_q_out_m);
    int16_t *q_out_fm    = (int16_t *)malloc_aligned(DCT_BLOCK_SIZE * sizeof(int16_t), &raw_q_out_fm);
    int16_t *q_rec       = (int16_t *)malloc_aligned(DCT_BLOCK_SIZE * sizeof(int16_t), &raw_q_rec);
    int16_t *q_inreg     = (int16_t *)malloc_aligned(DCT_BLOCK_SIZE * sizeof(int16_t), &raw_q_inreg);
    int16_t *q_rec_inreg = (int16_t *)malloc_aligned(DCT_BLOCK_SIZE * sizeof(int16_t), &raw_q_rec_inreg);
    int16_t *q_dc_in     = (int16_t *)malloc_aligned(DCT_BLOCK_SIZE * sizeof(int16_t), &raw_q_dc_in);
    int16_t *q_dc_out    = (int16_t *)malloc_aligned(DCT_BLOCK_SIZE * sizeof(int16_t), &raw_q_dc_out);
    int16_t *q_rec_sparse= (int16_t *)malloc_aligned(DCT_BLOCK_SIZE * sizeof(int16_t), &raw_q_rec_sparse);
    int16_t *q_quant_c   = (int16_t *)malloc_aligned(DCT_BLOCK_SIZE * sizeof(int16_t), &raw_q_quant_c);
    int16_t *q_quant_m   = (int16_t *)malloc_aligned(DCT_BLOCK_SIZE * sizeof(int16_t), &raw_q_quant_m);
    int16_t *q_fused     = (int16_t *)malloc_aligned(DCT_BLOCK_SIZE * sizeof(int16_t), &raw_q_fused);
    int16_t *recip_q     = (int16_t *)malloc_aligned(DCT_BLOCK_SIZE * sizeof(int16_t), &raw_recip_q);

    if (!f_in || !f_out_c || !f_out_asm || !f_out_fa || !f_rec ||
        !q_in || !q_out_c || !q_out_s || !q_out_m || !q_out_fm || !q_rec ||
        !q_inreg || !q_rec_inreg || !q_dc_in || !q_dc_out || !q_rec_sparse ||
        !q_quant_c || !q_quant_m || !q_fused || !recip_q) {
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

    /* Measure 1D Fast Float ASM (Loeffler) */
    t0 = get_ccc();
    for (int i = 0; i < NUM_BLOCKS; i++) {
        dct1d_float_fast_asm(f_in, f_out_fa);
    }
    t1 = get_ccc();
    uint32_t cyc_1d_ffa = (t1 - t0) / NUM_BLOCKS;

    /* Measure 1D Fast Float IDCT ASM (Loeffler) */
    t0 = get_ccc();
    for (int i = 0; i < NUM_BLOCKS; i++) {
        idct1d_float_fast_asm(f_out_fa, f_rec);
    }
    t1 = get_ccc();
    uint32_t cyc_1d_iffa = (t1 - t0) / NUM_BLOCKS;

    /* Measure 1D Fast Fixed AMMX (Loeffler) */
    t0 = get_ccc();
    for (int i = 0; i < NUM_BLOCKS; i++) {
        dct1d_fixed_fast_ammx(q_in, q_out_fm);
    }
    t1 = get_ccc();
    uint32_t cyc_1d_qfm = (t1 - t0) / NUM_BLOCKS;

    /* Measure 1D Fast Fixed IDCT AMMX (Loeffler) */
    t0 = get_ccc();
    for (int i = 0; i < NUM_BLOCKS; i++) {
        idct1d_fixed_fast_ammx(q_out_fm, q_rec);
    }
    t1 = get_ccc();
    uint32_t cyc_1d_iqfm = (t1 - t0) / NUM_BLOCKS;

    printf(" 1D 8-Point Float FDCT (C-Ref):      %5lu cycles\n", (unsigned long)cyc_1d_fc);
    printf(" 1D 8-Point Float FDCT (Matrix ASM): %5lu cycles (%.2fx vs C)\n", (unsigned long)cyc_1d_fa, (double)cyc_1d_fc / cyc_1d_fa);
    printf(" 1D 8-Point Float FDCT (Fast ASM):   %5lu cycles (%.2fx vs C, %.2fx vs Mat)\n",
           (unsigned long)cyc_1d_ffa, (double)cyc_1d_fc / cyc_1d_ffa, (double)cyc_1d_fa / cyc_1d_ffa);
    printf(" 1D 8-Point Float IDCT (C-Ref):      %5lu cycles\n", (unsigned long)cyc_1d_ifc);
    printf(" 1D 8-Point Float IDCT (Matrix ASM): %5lu cycles (%.2fx vs C)\n", (unsigned long)cyc_1d_ifa, (double)cyc_1d_ifc / cyc_1d_ifa);
    printf(" 1D 8-Point Float IDCT (Fast ASM):   %5lu cycles (%.2fx vs C, %.2fx vs Mat)\n\n",
           (unsigned long)cyc_1d_iffa, (double)cyc_1d_ifc / cyc_1d_iffa, (double)cyc_1d_ifa / cyc_1d_iffa);

    printf(" 1D 8-Point Fixed FDCT (C-Ref):      %5lu cycles\n", (unsigned long)cyc_1d_qc);
    printf(" 1D 8-Point Fixed FDCT (Scalar):     %5lu cycles (%.2fx vs C)\n", (unsigned long)cyc_1d_qs, (double)cyc_1d_qc / cyc_1d_qs);
    printf(" 1D 8-Point Fixed FDCT (Matrix AMMX):%5lu cycles (%.2fx vs C, %.2fx vs Scal)\n",
           (unsigned long)cyc_1d_qm, (double)cyc_1d_qc / cyc_1d_qm, (double)cyc_1d_qs / cyc_1d_qm);
    printf(" 1D 8-Point Fixed FDCT (Fast AMMX):  %5lu cycles (%.2fx vs C, %.2fx vs Mat)\n",
           (unsigned long)cyc_1d_qfm, (double)cyc_1d_qc / cyc_1d_qfm, (double)cyc_1d_qm / cyc_1d_qfm);

    printf(" 1D 8-Point Fixed IDCT (C-Ref):      %5lu cycles\n", (unsigned long)cyc_1d_iqc);
    printf(" 1D 8-Point Fixed IDCT (Scalar):     %5lu cycles (%.2fx vs C)\n", (unsigned long)cyc_1d_iqs, (double)cyc_1d_iqc / cyc_1d_iqs);
    printf(" 1D 8-Point Fixed IDCT (Matrix AMMX):%5lu cycles (%.2fx vs C, %.2fx vs Scal)\n",
           (unsigned long)cyc_1d_iqm, (double)cyc_1d_iqc / cyc_1d_iqm, (double)cyc_1d_iqs / cyc_1d_iqm);
    printf(" 1D 8-Point Fixed IDCT (Fast AMMX):  %5lu cycles (%.2fx vs C, %.2fx vs Mat)\n\n",
           (unsigned long)cyc_1d_iqfm, (double)cyc_1d_iqc / cyc_1d_iqfm, (double)cyc_1d_iqm / cyc_1d_iqfm);

    /* Verify 1D ASM against C */
    float max_fa_diff = 0.0f;
    float max_ffa_diff = 0.0f;
    int max_qs_diff = 0;
    int max_qm_diff = 0;
    int max_qfm_diff = 0;
    for (int i = 0; i < DCT_SIZE; i++) {
        float fad = fabsf(f_out_c[i] - f_out_asm[i]);
        if (fad > max_fa_diff) max_fa_diff = fad;
        float ffad = fabsf(f_out_c[i] - f_out_fa[i]);
        if (ffad > max_ffa_diff) max_ffa_diff = ffad;
        int qsd = abs(q_out_c[i] - q_out_s[i]);
        if (qsd > max_qs_diff) max_qs_diff = qsd;
        int qmd = abs(q_out_c[i] - q_out_m[i]);
        if (qmd > max_qm_diff) max_qm_diff = qmd;
        int qfmd = abs(q_out_c[i] - q_out_fm[i]);
        if (qfmd > max_qfm_diff) max_qfm_diff = qfmd;
    }
    printf(" 1D Verification: Float ASM diff=%e | Fast Float diff=%e | Scal diff=%d | AMMX diff=%d | Fast AMMX diff=%d LSB [%s]\n",
           (double)max_fa_diff, (double)max_ffa_diff, max_qs_diff, max_qm_diff, max_qfm_diff,
           (max_fa_diff < 1e-4f && max_ffa_diff < 1e-4f && max_qs_diff == 0 && max_qm_diff <= 15 && max_qfm_diff <= 25) ? "PASS" : "FAIL");
    printf("   1D c[0..7]:  %d, %d, %d, %d, %d, %d, %d, %d\n",
           q_out_c[0], q_out_c[1], q_out_c[2], q_out_c[3], q_out_c[4], q_out_c[5], q_out_c[6], q_out_c[7]);
    printf("   1D m[0..7]:  %d, %d, %d, %d, %d, %d, %d, %d\n",
           q_out_m[0], q_out_m[1], q_out_m[2], q_out_m[3], q_out_m[4], q_out_m[5], q_out_m[6], q_out_m[7]);
    printf("   1D fm[0..7]: %d, %d, %d, %d, %d, %d, %d, %d\n",
           q_out_fm[0], q_out_fm[1], q_out_fm[2], q_out_fm[3], q_out_fm[4], q_out_fm[5], q_out_fm[6], q_out_fm[7]);

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

    /* Measure 2D Fast Float ASM */
    t0 = get_ccc();
    for (int i = 0; i < NUM_BLOCKS; i++) {
        dct2d_8x8_float_fast_asm(f_in, f_out_fa);
    }
    t1 = get_ccc();
    uint32_t cyc_2d_ffa = (t1 - t0) / NUM_BLOCKS;

    /* Measure 2D Fast Float IDCT ASM */
    t0 = get_ccc();
    for (int i = 0; i < NUM_BLOCKS; i++) {
        idct2d_8x8_float_fast_asm(f_out_fa, f_rec);
    }
    t1 = get_ccc();
    uint32_t cyc_2d_iffa = (t1 - t0) / NUM_BLOCKS;

    /* Measure 2D Fast Fixed AMMX */
    t0 = get_ccc();
    for (int i = 0; i < NUM_BLOCKS; i++) {
        dct2d_8x8_fixed_fast_ammx(q_in, q_out_fm);
    }
    t1 = get_ccc();
    uint32_t cyc_2d_qfm = (t1 - t0) / NUM_BLOCKS;

    /* Measure 2D Fast Fixed IDCT AMMX */
    t0 = get_ccc();
    for (int i = 0; i < NUM_BLOCKS; i++) {
        idct2d_8x8_fixed_fast_ammx(q_out_fm, q_rec);
    }
    t1 = get_ccc();
    uint32_t cyc_2d_iqfm = (t1 - t0) / NUM_BLOCKS;

    printf(" 2D 8x8 Block Float FDCT (C-Ref):      %6lu cycles\n", (unsigned long)cyc_2d_fc);
    printf(" 2D 8x8 Block Float FDCT (Matrix ASM): %6lu cycles (%.2fx vs C)\n", (unsigned long)cyc_2d_fa, (double)cyc_2d_fc / cyc_2d_fa);
    printf(" 2D 8x8 Block Float FDCT (Fast ASM):   %6lu cycles (%.2fx vs C, %.2fx vs Mat)\n",
           (unsigned long)cyc_2d_ffa, (double)cyc_2d_fc / cyc_2d_ffa, (double)cyc_2d_fa / cyc_2d_ffa);
    printf(" 2D 8x8 Block Float IDCT (C-Ref):      %6lu cycles\n", (unsigned long)cyc_2d_ifc);
    printf(" 2D 8x8 Block Float IDCT (Matrix ASM): %6lu cycles (%.2fx vs C)\n", (unsigned long)cyc_2d_ifa, (double)cyc_2d_ifc / cyc_2d_ifa);
    printf(" 2D 8x8 Block Float IDCT (Fast ASM):   %6lu cycles (%.2fx vs C, %.2fx vs Mat)\n\n",
           (unsigned long)cyc_2d_iffa, (double)cyc_2d_ifc / cyc_2d_iffa, (double)cyc_2d_ifa / cyc_2d_iffa);

    printf(" 2D 8x8 Block Fixed FDCT (C-Ref):      %6lu cycles\n", (unsigned long)cyc_2d_qc);
    printf(" 2D 8x8 Block Fixed FDCT (Scalar):     %6lu cycles (%.2fx vs C)\n", (unsigned long)cyc_2d_qs, (double)cyc_2d_qc / cyc_2d_qs);
    printf(" 2D 8x8 Block Fixed FDCT (Matrix AMMX):%6lu cycles (%.2fx vs C, %.2fx vs Scal)\n",
           (unsigned long)cyc_2d_qm, (double)cyc_2d_qc / cyc_2d_qm, (double)cyc_2d_qs / cyc_2d_qm);
    printf(" 2D 8x8 Block Fixed FDCT (Fast AMMX):  %6lu cycles (%.2fx vs C, %.2fx vs Mat)\n",
           (unsigned long)cyc_2d_qfm, (double)cyc_2d_qc / cyc_2d_qfm, (double)cyc_2d_qm / cyc_2d_qfm);

    printf(" 2D 8x8 Block Fixed IDCT (C-Ref):      %6lu cycles\n", (unsigned long)cyc_2d_iqc);
    printf(" 2D 8x8 Block Fixed IDCT (Scalar):     %6lu cycles (%.2fx vs C)\n", (unsigned long)cyc_2d_iqs, (double)cyc_2d_iqc / cyc_2d_iqs);
    printf(" 2D 8x8 Block Fixed IDCT (Matrix AMMX):%6lu cycles (%.2fx vs C, %.2fx vs Scal)\n",
           (unsigned long)cyc_2d_iqm, (double)cyc_2d_iqc / cyc_2d_iqm, (double)cyc_2d_iqs / cyc_2d_iqm);
    printf(" 2D 8x8 Block Fixed IDCT (Fast AMMX):  %6lu cycles (%.2fx vs C, %.2fx vs Mat)\n\n",
           (unsigned long)cyc_2d_iqfm, (double)cyc_2d_iqc / cyc_2d_iqfm, (double)cyc_2d_iqm / cyc_2d_iqfm);

    /* Verify 2D ASM against C */
    max_fa_diff = 0.0f;
    max_ffa_diff = 0.0f;
    max_qs_diff = 0;
    max_qm_diff = 0;
    max_qfm_diff = 0;
    int max_qfm_idx = 0;
    for (int i = 0; i < DCT_BLOCK_SIZE; i++) {
        float fad = fabsf(f_out_c[i] - f_out_asm[i]);
        if (fad > max_fa_diff) max_fa_diff = fad;
        float ffad = fabsf(f_out_c[i] - f_out_fa[i]);
        if (ffad > max_ffa_diff) max_ffa_diff = ffad;
        int qsd = abs(q_out_c[i] - q_out_s[i]);
        if (qsd > max_qs_diff) max_qs_diff = qsd;
        int qmd = abs(q_out_c[i] - q_out_m[i]);
        if (qmd > max_qm_diff) max_qm_diff = qmd;
        int qfmd = abs(q_out_c[i] - q_out_fm[i]);
        if (qfmd > max_qfm_diff) { max_qfm_diff = qfmd; max_qfm_idx = i; }
    }
    printf(" 2D Verification: Float diff=%e | Fast Float diff=%e | Scal diff=%d | AMMX diff=%d | Fast AMMX diff=%d LSB (idx %d: c=%d fm=%d) [%s]\n",
           (double)max_fa_diff, (double)max_ffa_diff, max_qs_diff, max_qm_diff, max_qfm_diff,
           max_qfm_idx, q_out_c[max_qfm_idx], q_out_fm[max_qfm_idx],
           (max_fa_diff < 1e-4f && max_ffa_diff < 1e-4f && max_qs_diff == 0 && max_qm_diff <= 40 && max_qfm_diff <= 70) ? "PASS" : "FAIL");
    printf("   2D c[0..7]:  %d, %d, %d, %d, %d, %d, %d, %d\n",
           q_out_c[0], q_out_c[1], q_out_c[2], q_out_c[3], q_out_c[4], q_out_c[5], q_out_c[6], q_out_c[7]);
    printf("   2D s[0..7]:  %d, %d, %d, %d, %d, %d, %d, %d\n",
           q_out_s[0], q_out_s[1], q_out_s[2], q_out_s[3], q_out_s[4], q_out_s[5], q_out_s[6], q_out_s[7]);
    printf("   2D m[0..7]:  %d, %d, %d, %d, %d, %d, %d, %d\n",
           q_out_m[0], q_out_m[1], q_out_m[2], q_out_m[3], q_out_m[4], q_out_m[5], q_out_m[6], q_out_m[7]);
    printf("   2D fm[0..7]: %d, %d, %d, %d, %d, %d, %d, %d\n",
           q_out_fm[0], q_out_fm[1], q_out_fm[2], q_out_fm[3], q_out_fm[4], q_out_fm[5], q_out_fm[6], q_out_fm[7]);
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

    printf("\n=================================================================================================================================\n");
    printf(" Part 3: Advanced Optimizations Benchmark (In-Register 2D, Sparse Early-Exit, Fused Quantization)\n");
    printf("---------------------------------------------------------------------------------------------------------------------------------\n");

    /* 1. In-Register 2D Fast DCT & IDCT */
    /* Warmup */
    dct2d_8x8_fixed_fast_inreg_ammx(q_in, q_inreg);
    idct2d_8x8_fixed_fast_inreg_ammx(q_inreg, q_rec_inreg);

    t0 = get_ccc();
    for (int i = 0; i < NUM_BLOCKS; i++) {
        dct2d_8x8_fixed_fast_inreg_ammx(q_in, q_inreg);
    }
    t1 = get_ccc();
    uint32_t cyc_2d_inreg = (t1 - t0) / NUM_BLOCKS;

    t0 = get_ccc();
    for (int i = 0; i < NUM_BLOCKS; i++) {
        idct2d_8x8_fixed_fast_inreg_ammx(q_inreg, q_rec_inreg);
    }
    t1 = get_ccc();
    uint32_t cyc_2d_iinreg = (t1 - t0) / NUM_BLOCKS;

    int max_inreg_diff = 0;
    int max_iinreg_diff = 0;
    for (int i = 0; i < DCT_BLOCK_SIZE; i++) {
        int d = abs(q_out_fm[i] - q_inreg[i]);
        if (d > max_inreg_diff) max_inreg_diff = d;
        int id = abs(q_rec[i] - q_rec_inreg[i]);
        if (id > max_iinreg_diff) max_iinreg_diff = id;
    }
    printf(" 2D In-Register Fast FDCT AMMX:        %6lu cycles (%.2fx vs C, %lu cyc saved vs Fast-AMMX) [diff=%d LSB, %s]\n",
           (unsigned long)cyc_2d_inreg, (double)cyc_2d_qc / cyc_2d_inreg,
           (unsigned long)(cyc_2d_qfm > cyc_2d_inreg ? cyc_2d_qfm - cyc_2d_inreg : 0),
           max_inreg_diff, (max_inreg_diff == 0) ? "PASS" : "FAIL");

    printf(" 2D In-Register Fast IDCT AMMX:        %6lu cycles (%.2fx vs C, %lu cyc saved vs Fast-AMMX) [diff=%d LSB, %s]\n\n",
           (unsigned long)cyc_2d_iinreg, (double)cyc_2d_iqc / cyc_2d_iinreg,
           (unsigned long)(cyc_2d_iqfm > cyc_2d_iinreg ? cyc_2d_iqfm - cyc_2d_iinreg : 0),
           max_iinreg_diff, (max_iinreg_diff == 0) ? "PASS" : "FAIL");

    /* 2. Sparse / DC-Only IDCT Early-Exit */
    memset(q_dc_in, 0, DCT_BLOCK_SIZE * sizeof(int16_t));
    q_dc_in[0] = 1000; /* DC coefficient */

    /* Warmup */
    idct2d_8x8_sparse_ammx(q_dc_in, q_dc_out);
    idct2d_8x8_sparse_ammx(q_inreg, q_rec_sparse);

    t0 = get_ccc();
    for (int i = 0; i < NUM_BLOCKS; i++) {
        idct2d_8x8_sparse_ammx(q_dc_in, q_dc_out);
    }
    t1 = get_ccc();
    uint32_t cyc_sparse_dc = (t1 - t0) / NUM_BLOCKS;

    t0 = get_ccc();
    for (int i = 0; i < NUM_BLOCKS; i++) {
        idct2d_8x8_sparse_ammx(q_inreg, q_rec_sparse);
    }
    t1 = get_ccc();
    uint32_t cyc_sparse_full = (t1 - t0) / NUM_BLOCKS;

    /* Verify DC-only output: every pixel = round(DC / 8) */
    int16_t expected_dc_pixel = (int16_t)((1000 + 4) >> 3);
    int dc_pass = 1;
    for (int i = 0; i < DCT_BLOCK_SIZE; i++) {
        if (q_dc_out[i] != expected_dc_pixel) {
            dc_pass = 0;
            break;
        }
    }
    int max_sparse_fallback_diff = 0;
    for (int i = 0; i < DCT_BLOCK_SIZE; i++) {
        int d = abs(q_rec_inreg[i] - q_rec_sparse[i]);
        if (d > max_sparse_fallback_diff) max_sparse_fallback_diff = d;
    }

    printf(" 2D Sparse IDCT (DC-Only Fast Path):   %6lu cycles (%.1fx vs Full InReg IDCT, %.1fx vs C) [%s]\n",
           (unsigned long)cyc_sparse_dc, (double)cyc_2d_iinreg / (cyc_sparse_dc > 0 ? cyc_sparse_dc : 1),
           (double)cyc_2d_iqc / (cyc_sparse_dc > 0 ? cyc_sparse_dc : 1),
           dc_pass ? "PASS" : "FAIL");
    printf("    DC=1000 -> Pixel[0..63]=%d (expected %d)\n", q_dc_out[0], expected_dc_pixel);
    printf(" 2D Sparse IDCT (Full Block Fallback): %6lu cycles (penalty=%ld cyc) [diff=%d LSB, %s]\n\n",
           (unsigned long)cyc_sparse_full, (long)(cyc_sparse_full - cyc_2d_iinreg),
           max_sparse_fallback_diff, (max_sparse_fallback_diff == 0) ? "PASS" : "FAIL");

    /* 3. Quantization & Fused AAN DCT */
    static const uint16_t jpeg_luma_quant[64] = {
        16, 11, 10, 16, 24, 40, 51, 61,
        12, 12, 14, 19, 26, 58, 60, 55,
        14, 13, 16, 24, 40, 57, 69, 56,
        14, 17, 22, 29, 51, 87, 80, 62,
        18, 22, 37, 56, 68, 109, 103, 77,
        24, 35, 55, 64, 81, 104, 113, 92,
        49, 64, 78, 87, 103, 121, 120, 101,
        72, 92, 95, 98, 112, 100, 103, 99
    };
    for (int i = 0; i < DCT_BLOCK_SIZE; i++) {
        recip_q[i] = (int16_t)((32768 + (jpeg_luma_quant[i] >> 1)) / jpeg_luma_quant[i]);
    }

    /* Warmup */
    quantize8x8_c(q_inreg, q_quant_c, jpeg_luma_quant);
    quantize8x8_ammx(q_inreg, q_quant_m, recip_q);
    dct2d_8x8_aan_quant_ammx(q_in, q_fused, recip_q);

    t0 = get_ccc();
    for (int i = 0; i < NUM_BLOCKS; i++) {
        quantize8x8_c(q_inreg, q_quant_c, jpeg_luma_quant);
    }
    t1 = get_ccc();
    uint32_t cyc_quant_c = (t1 - t0) / NUM_BLOCKS;

    t0 = get_ccc();
    for (int i = 0; i < NUM_BLOCKS; i++) {
        quantize8x8_ammx(q_inreg, q_quant_m, recip_q);
    }
    t1 = get_ccc();
    uint32_t cyc_quant_m = (t1 - t0) / NUM_BLOCKS;

    t0 = get_ccc();
    for (int i = 0; i < NUM_BLOCKS; i++) {
        dct2d_8x8_aan_quant_ammx(q_in, q_fused, recip_q);
    }
    t1 = get_ccc();
    uint32_t cyc_fused_m = (t1 - t0) / NUM_BLOCKS;

    int max_quant_diff = 0;
    int max_fused_diff = 0;
    for (int i = 0; i < DCT_BLOCK_SIZE; i++) {
        int d = abs(q_quant_c[i] - q_quant_m[i]);
        if (d > max_quant_diff) max_quant_diff = d;
        int fd = abs(q_quant_m[i] - q_fused[i]);
        if (fd > max_fused_diff) max_fused_diff = fd;
    }

    printf(" 8x8 Block Quantization (C-Ref):       %6lu cycles\n", (unsigned long)cyc_quant_c);
    printf(" 8x8 Block Quantization (AMMX SIMD):   %6lu cycles (%.2fx vs C) [diff=%d LSB, %s]\n",
           (unsigned long)cyc_quant_m, (double)cyc_quant_c / cyc_quant_m,
           max_quant_diff, (max_quant_diff <= 2) ? "PASS" : "FAIL");
    printf(" 8x8 Fused AAN DCT + Quantization AMMX:%6lu cycles (vs %lu cyc separate) [%s]\n",
           (unsigned long)cyc_fused_m, (unsigned long)(cyc_2d_inreg + cyc_quant_m),
           (max_fused_diff == 0) ? "PASS" : "FAIL");

    printf("=================================================================================================================================\n");

    free(raw_f_in);
    free(raw_f_out_c);
    free(raw_f_out_asm);
    free(raw_f_out_fa);
    free(raw_f_rec);
    free(raw_q_in);
    free(raw_q_out_c);
    free(raw_q_out_s);
    free(raw_q_out_m);
    free(raw_q_out_fm);
    free(raw_q_rec);
    free(raw_q_inreg);
    free(raw_q_rec_inreg);
    free(raw_q_dc_in);
    free(raw_q_dc_out);
    free(raw_q_rec_sparse);
    free(raw_q_quant_c);
    free(raw_q_quant_m);
    free(raw_q_fused);
    free(raw_recip_q);

    return 0;
}
