#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <math.h>
#include <string.h>
#include "mdct.h"

#define NUM_RUNS 200

static void *malloc_aligned(size_t size, void **raw_out) {
    void *p = malloc(size + 64);
    if (!p) { *raw_out = NULL; return NULL; }
    *raw_out = p;
    uintptr_t addr = (uintptr_t)p;
    return (void *)((addr + 63) & ~(uintptr_t)63);
}

int main(void) {
    printf("=================================================================================================================================\n");
    printf(" Apollo 68080 Audio MDCT & IMDCT Benchmark Suite\n");
    printf(" Application: MP3 / AAC Audio Decoding Engine (36-Point Windowed IMDCT & 50%% Overlap-Add)\n");
    printf(" Real Hardware: Vampire V4 (Apollo 68080 @ Core Release)\n");
    printf(" Formats: 32-bit Single-Precision Float & 16-bit Q1.15 Fixed-Point (FPU & AMMX SIMD)\n");
    printf("=================================================================================================================================\n\n");

    printf("Initializing MP3 IMDCT & Sine Window tables...\n");
    mdct_init_tables();
    printf("Tables initialized (36x18 Basis Matrices & 36-Point Sine Window).\n\n");

    /* Allocate aligned test buffers */
    void *raw_f_in, *raw_f_out_c, *raw_f_out_asm;
    void *raw_q_in, *raw_q_out_c, *raw_q_out_m;
    void *raw_prev_c, *raw_prev_m, *raw_pcm_c, *raw_pcm_m;

    float *f_in        = (float *)malloc_aligned(MP3_N * sizeof(float), &raw_f_in);
    float *f_out_c     = (float *)malloc_aligned(MP3_2N * sizeof(float), &raw_f_out_c);
    float *f_out_asm   = (float *)malloc_aligned(MP3_2N * sizeof(float), &raw_f_out_asm);

    int16_t *q_in      = (int16_t *)malloc_aligned(32 * sizeof(int16_t), &raw_q_in); /* padded */
    int16_t *q_out_c   = (int16_t *)malloc_aligned(MP3_2N * sizeof(int16_t), &raw_q_out_c);
    int16_t *q_out_m   = (int16_t *)malloc_aligned(MP3_2N * sizeof(int16_t), &raw_q_out_m);

    int16_t *prev_c    = (int16_t *)malloc_aligned(32 * sizeof(int16_t), &raw_prev_c);
    int16_t *prev_m    = (int16_t *)malloc_aligned(32 * sizeof(int16_t), &raw_prev_m);
    int16_t *pcm_c     = (int16_t *)malloc_aligned(32 * sizeof(int16_t), &raw_pcm_c);
    int16_t *pcm_m     = (int16_t *)malloc_aligned(32 * sizeof(int16_t), &raw_pcm_m);

    if (!f_in || !f_out_c || !f_out_asm ||
        !q_in || !q_out_c || !q_out_m ||
        !prev_c || !prev_m || !pcm_c || !pcm_m) {
        printf("ERROR: Memory allocation failed!\n");
        return 1;
    }

    /* Synthesize realistic MP3 audio subband frequencies (multitone + exponential decay) */
    for (int k = 0; k < MP3_N; k++) {
        float val = 0.6f * (float)cos(2.0 * 3.14159265 * k / 6.0) * expf(-0.08f * k);
        f_in[k] = val;
        q_in[k] = (int16_t)(val * 8192.0f);
    }
    for (int k = MP3_N; k < 32; k++) {
        q_in[k] = 0;
    }

    /* Initialize prev overlap buffers with previous subband tail */
    for (int k = 0; k < MP3_N; k++) {
        prev_c[k] = (int16_t)(2000 * sin(2.0 * 3.14159265 * k / 9.0));
        prev_m[k] = prev_c[k];
    }
    for (int k = MP3_N; k < 32; k++) {
        prev_c[k] = 0;
        prev_m[k] = 0;
    }

    /* ------------------------------------------------------------------------------------------------- */
    /* 1. 32-bit Float IMDCT-36 Benchmark */
    /* ------------------------------------------------------------------------------------------------- */
    printf("=================================================================================================================================\n");
    printf(" Part 1: 32-bit Float IMDCT-36 Benchmark (Average over %d runs)\n", NUM_RUNS);
    printf("---------------------------------------------------------------------------------------------------------------------------------\n");

    /* Warmup */
    imdct36_float_c(f_in, f_out_c);
    imdct36_float_asm(f_in, f_out_asm);

    uint32_t t0 = get_ccc();
    for (int i = 0; i < NUM_RUNS; i++) {
        imdct36_float_c(f_in, f_out_c);
    }
    uint32_t t1 = get_ccc();
    uint32_t cyc_fc = (t1 - t0) / NUM_RUNS;

    t0 = get_ccc();
    for (int i = 0; i < NUM_RUNS; i++) {
        imdct36_float_asm(f_in, f_out_asm);
    }
    t1 = get_ccc();
    uint32_t cyc_fa = (t1 - t0) / NUM_RUNS;

    float max_f_diff = 0.0f;
    for (int i = 0; i < MP3_2N; i++) {
        float d = fabsf(f_out_c[i] - f_out_asm[i]);
        if (d > max_f_diff) max_f_diff = d;
    }

    printf(" 36-Point Float IMDCT (C-Ref):         %6lu cycles\n", (unsigned long)cyc_fc);
    printf(" 36-Point Float IMDCT (68080 FPU):     %6lu cycles (%.2fx vs C) [diff=%e, %s]\n\n",
           (unsigned long)cyc_fa, (double)cyc_fc / cyc_fa, (double)max_f_diff,
           (max_f_diff < 1e-4f) ? "PASS" : "FAIL");

    /* ------------------------------------------------------------------------------------------------- */
    /* 2. 16-bit Q1.15 Fixed-Point IMDCT-36 Benchmark */
    /* ------------------------------------------------------------------------------------------------- */
    printf("=================================================================================================================================\n");
    printf(" Part 2: 16-bit Q1.15 Fixed-Point IMDCT-36 Benchmark (Average over %d runs)\n", NUM_RUNS);
    printf("---------------------------------------------------------------------------------------------------------------------------------\n");

    /* Warmup */
    imdct36_fixed_c(q_in, q_out_c);
    imdct36_fixed_ammx(q_in, q_out_m);

    t0 = get_ccc();
    for (int i = 0; i < NUM_RUNS; i++) {
        imdct36_fixed_c(q_in, q_out_c);
    }
    t1 = get_ccc();
    uint32_t cyc_qc = (t1 - t0) / NUM_RUNS;

    t0 = get_ccc();
    for (int i = 0; i < NUM_RUNS; i++) {
        imdct36_fixed_ammx(q_in, q_out_m);
    }
    t1 = get_ccc();
    uint32_t cyc_qm = (t1 - t0) / NUM_RUNS;

    int max_q_diff = 0;
    int max_q_idx = 0;
    for (int i = 0; i < MP3_2N; i++) {
        int d = abs(q_out_c[i] - q_out_m[i]);
        if (d > max_q_diff) {
            max_q_diff = d;
            max_q_idx = i;
        }
    }

    printf(" 36-Point Fixed IMDCT (C-Ref):         %6lu cycles\n", (unsigned long)cyc_qc);
    printf(" 36-Point Fixed IMDCT (AMMX SIMD):     %6lu cycles (%.2fx vs C) [max_diff=%d LSB at idx %d (c=%d, m=%d), %s]\n",
           (unsigned long)cyc_qm, (double)cyc_qc / cyc_qm, max_q_diff, max_q_idx,
           q_out_c[max_q_idx], q_out_m[max_q_idx],
           (max_q_diff <= 50) ? "PASS" : "FAIL");
    printf("   c[0..7]:  %d, %d, %d, %d, %d, %d, %d, %d\n",
           q_out_c[0], q_out_c[1], q_out_c[2], q_out_c[3], q_out_c[4], q_out_c[5], q_out_c[6], q_out_c[7]);
    printf("   m[0..7]:  %d, %d, %d, %d, %d, %d, %d, %d\n\n",
           q_out_m[0], q_out_m[1], q_out_m[2], q_out_m[3], q_out_m[4], q_out_m[5], q_out_m[6], q_out_m[7]);

    /* ------------------------------------------------------------------------------------------------- */
    /* 3. Overlap-Add Kernel Benchmark */
    /* ------------------------------------------------------------------------------------------------- */
    printf("=================================================================================================================================\n");
    printf(" Part 3: 50%% Overlap-Add Kernel Benchmark (18 Audio Samples)\n");
    printf("---------------------------------------------------------------------------------------------------------------------------------\n");

    t0 = get_ccc();
    for (int i = 0; i < NUM_RUNS; i++) {
        for (int k = 0; k < MP3_N; k++) {
            pcm_c[k] = q_out_c[k] + prev_c[k];
        }
    }
    t1 = get_ccc();
    uint32_t cyc_ola_c = (t1 - t0) / NUM_RUNS;

    t0 = get_ccc();
    for (int i = 0; i < NUM_RUNS; i++) {
        overlap_add_fixed_ammx(q_out_m, prev_m, pcm_m);
    }
    t1 = get_ccc();
    uint32_t cyc_ola_m = (t1 - t0) / NUM_RUNS;

    int max_ola_diff = 0;
    int max_ola_idx = 0;
    for (int k = 0; k < MP3_N; k++) {
        int d = abs(pcm_c[k] - pcm_m[k]);
        if (d > max_ola_diff) { max_ola_diff = d; max_ola_idx = k; }
    }
    printf("   OLA k=0..3: c=[%d, %d, %d, %d], m=[%d, %d, %d, %d]\n",
           pcm_c[0], pcm_c[1], pcm_c[2], pcm_c[3], pcm_m[0], pcm_m[1], pcm_m[2], pcm_m[3]);
    printf("   OLA k=14..17: c=[%d, %d, %d, %d], m=[%d, %d, %d, %d]\n",
           pcm_c[14], pcm_c[15], pcm_c[16], pcm_c[17], pcm_m[14], pcm_m[15], pcm_m[16], pcm_m[17]);
    printf("   Max OLA diff at %d: c=%d, m=%d\n", max_ola_idx, pcm_c[max_ola_idx], pcm_m[max_ola_idx]);

    printf(" 18-Sample Overlap-Add (C-Ref):        %6lu cycles\n", (unsigned long)cyc_ola_c);
    printf(" 18-Sample Overlap-Add (AMMX SIMD):    %6lu cycles (%.2fx vs C) [diff=%d LSB, %s]\n\n",
           (unsigned long)cyc_ola_m, (double)cyc_ola_c / (cyc_ola_m > 0 ? cyc_ola_m : 1),
           max_ola_diff, (max_ola_diff <= 50) ? "PASS" : "FAIL");

    /* ------------------------------------------------------------------------------------------------- */
    /* 4. Complete Fused MP3 Subband Engine: Windowed IMDCT-36 + Overlap-Add */
    /* ------------------------------------------------------------------------------------------------- */
    printf("=================================================================================================================================\n");
    printf(" Part 4: Fused MP3 Subband Engine (Windowed IMDCT-36 + Sine Window + Overlap-Add)\n");
    printf("---------------------------------------------------------------------------------------------------------------------------------\n");

    /* Reset prev buffers */
    for (int k = 0; k < MP3_N; k++) {
        prev_c[k] = (int16_t)(2000 * sin(2.0 * 3.14159265 * k / 9.0));
        prev_m[k] = prev_c[k];
    }

    /* Warmup */
    imdct36_windowed_overlap_add_c(q_in, prev_c, pcm_c);
    imdct36_windowed_overlap_add_ammx(q_in, prev_m, pcm_m);

    t0 = get_ccc();
    for (int i = 0; i < NUM_RUNS; i++) {
        imdct36_windowed_overlap_add_c(q_in, prev_c, pcm_c);
    }
    t1 = get_ccc();
    uint32_t cyc_fused_c = (t1 - t0) / NUM_RUNS;

    t0 = get_ccc();
    for (int i = 0; i < NUM_RUNS; i++) {
        imdct36_windowed_overlap_add_ammx(q_in, prev_m, pcm_m);
    }
    t1 = get_ccc();
    uint32_t cyc_fused_m = (t1 - t0) / NUM_RUNS;

    int max_fused_diff = 0;
    int max_fused_idx = 0;
    int max_prev_diff = 0;
    int max_prev_idx = 0;
    for (int k = 0; k < MP3_N; k++) {
        int d = abs(pcm_c[k] - pcm_m[k]);
        if (d > max_fused_diff) { max_fused_diff = d; max_fused_idx = k; }
        int pd = abs(prev_c[k] - prev_m[k]);
        if (pd > max_prev_diff) { max_prev_diff = pd; max_prev_idx = k; }
    }
    printf("   Fused pcm k=0..3: c=[%d, %d, %d, %d], m=[%d, %d, %d, %d]\n",
           pcm_c[0], pcm_c[1], pcm_c[2], pcm_c[3], pcm_m[0], pcm_m[1], pcm_m[2], pcm_m[3]);
    printf("   Max Fused diff at %d: c=%d, m=%d\n", max_fused_idx, pcm_c[max_fused_idx], pcm_m[max_fused_idx]);
    printf("   Max Prev diff at %d: c=%d, m=%d\n", max_prev_idx, prev_c[max_prev_idx], prev_m[max_prev_idx]);

    printf(" Complete MP3 Engine (C-Ref):          %6lu cycles\n", (unsigned long)cyc_fused_c);
    printf(" Complete MP3 Engine (Fused AMMX):     %6lu cycles (%.2fx vs C) [diff=%d LSB, prev_diff=%d LSB, %s]\n",
           (unsigned long)cyc_fused_m, (double)cyc_fused_c / cyc_fused_m,
           max_fused_diff, max_prev_diff, (max_fused_diff <= 50 && max_prev_diff <= 50) ? "PASS" : "FAIL");

    /* Calculate Real-Time MP3 Audio Decoding Budget on Vampire V4 (44.1 kHz Stereo) */
    /* 1 Second of Stereo MP3 = 44,100 / 1152 frames = 38.28 frames/s */
    /* Each frame has 2 granules * 2 channels * 32 subbands = 128 IMDCTs */
    /* Total IMDCTs per second = 38.28 * 128 = 4,900 IMDCTs/sec */
    double imdct_per_sec = (44100.0 / 1152.0) * 128.0;
    double mhz_v4 = 85.0; /* ~85 MHz effective 68080 clock */
    double cyc_per_sec_c = cyc_fused_c * imdct_per_sec;
    double cyc_per_sec_m = cyc_fused_m * imdct_per_sec;
    double cpu_usage_c = (cyc_per_sec_c / (mhz_v4 * 1000000.0)) * 100.0;
    double cpu_usage_m = (cyc_per_sec_m / (mhz_v4 * 1000000.0)) * 100.0;

    printf("\n---------------------------------------------------------------------------------------------------------------------------------\n");
    printf(" Real-Time 44.1 kHz Stereo MP3 Audio Load Estimation (4,900 IMDCTs/sec):\n");
    printf("   C-Ref CPU Load:       ~%.2f %% CPU\n", cpu_usage_c);
    printf("   Fused AMMX CPU Load:  ~%.2f %% CPU (Virtually ZERO overhead!)\n", cpu_usage_m);
    printf("=================================================================================================================================\n");

    free(raw_f_in);
    free(raw_f_out_c);
    free(raw_f_out_asm);
    free(raw_q_in);
    free(raw_q_out_c);
    free(raw_q_out_m);
    free(raw_prev_c);
    free(raw_prev_m);
    free(raw_pcm_c);
    free(raw_pcm_m);

    return 0;
}
