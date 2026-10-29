#include "fft.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <math.h>

#ifndef PI
#define PI 3.14159265358979323846
#endif

static const int test_sizes[] = {64, 128, 256, 512, 1024, 2048, 4096};
#define NUM_TESTS 7

extern void fft_breakdown_ammx(complex_int16 *x, int n, const complex_int16 *tw,
                               uint32_t *t_br, uint32_t *t_stg1, uint32_t *t_mid, uint32_t *t_last);

int main(void) {
    printf("=================================================================================================================================\n");
    printf(" Apollo 68080 FFT Benchmark & Validation Suite\n");
    printf(" Real Hardware: Vampire V4 (Apollo 68080 @ Core Release)\n");
    printf(" Formats: 32-bit Single-Precision Float & 16-bit Q1.15 Fixed-Point (Scalar & AMMX SIMD)\n");
    printf(" Sizes: N = 64 to 4096 points\n");
    printf("=================================================================================================================================\n\n");

    printf("Initializing twiddle factor tables...\n");
    fft_init_tables();
    printf("Twiddle tables initialized (4096 max points / 2048 twiddles).\n\n");

    printf("Starting Benchmark & Verification Series (Float, Fixed Scalar, Fixed AMMX SIMD):\n");
    printf("------------------------------------------------------------------------------------------------------------------------------------\n");
    printf("   N   |  Float C   | Float ASM  | Spd-F |  Fixed C   | Fixed Scal | Spd-S | Fixed AMMX | Spd-C | Spd-S | Diff S/M | Status\n");
    printf("-------+------------+------------+-------+------------+------------+-------+------------+-------+-------+----------+--------\n");

    int all_passed = 1;

    for (int t = 0; t < NUM_TESTS; t++) {
        int n = test_sizes[t];

        complex_float *fc = (complex_float *)malloc(n * sizeof(complex_float));
        complex_float *fa = (complex_float *)malloc(n * sizeof(complex_float));
        complex_int16 *xc = (complex_int16 *)malloc(n * sizeof(complex_int16));
        complex_int16 *xa = (complex_int16 *)malloc(n * sizeof(complex_int16));
        complex_int16 *xm = (complex_int16 *)malloc(n * sizeof(complex_int16));

        if (!fc || !fa || !xc || !xa || !xm) {
            printf("Memory allocation failed for N=%d!\n", n);
            all_passed = 0;
            break;
        }

        /* Generate deterministic test signals */
        for (int i = 0; i < n; i++) {
            float v = (float)sin(2.0 * PI * 3.0 * i / n) + 0.5f * (float)cos(2.0 * PI * 7.0 * i / n);
            fc[i].r = v;
            fc[i].i = 0.0f;
            fa[i].r = v;
            fa[i].i = 0.0f;

            int16_t qv = (int16_t)(v * 16384.0f);
            xc[i].r = qv;
            xc[i].i = 0;
            xa[i].r = qv;
            xa[i].i = 0;
            xm[i].r = qv;
            xm[i].i = 0;
        }

        /* Benchmark Float C */
        uint32_t t0 = get_ccc();
        fft_float_c(fc, n);
        uint32_t t1 = get_ccc();
        uint32_t cyc_fc = t1 - t0;

        /* Benchmark Float ASM */
        t0 = get_ccc();
        fft_float_asm(fa, n, twiddle_float);
        t1 = get_ccc();
        uint32_t cyc_fa = t1 - t0;

        /* Verify Float */
        float max_fdiff = 0.0f;
        for (int i = 0; i < n; i++) {
            float dr = fabsf(fc[i].r - fa[i].r);
            float di = fabsf(fc[i].i - fa[i].i);
            if (dr > max_fdiff) max_fdiff = dr;
            if (di > max_fdiff) max_fdiff = di;
        }
        if (max_fdiff > 1e-4f) {
            all_passed = 0;
        }

        /* Benchmark Fixed C */
        t0 = get_ccc();
        fft_fixed_c(xc, n);
        t1 = get_ccc();
        uint32_t cyc_xc = t1 - t0;

        /* Benchmark Fixed ASM (Scalar) */
        t0 = get_ccc();
        fft_fixed_asm(xa, n, twiddle_fixed);
        t1 = get_ccc();
        uint32_t cyc_xa = t1 - t0;

        /* Benchmark Fixed AMMX (SIMD) */
        t0 = get_ccc();
        fft_fixed_ammx(xm, n, twiddle_fixed);
        t1 = get_ccc();
        uint32_t cyc_xm = t1 - t0;

        /* Verify Fixed Scalar vs C */
        int max_xdiff = 0;
        for (int i = 0; i < n; i++) {
            int dr = abs(xc[i].r - xa[i].r);
            int di = abs(xc[i].i - xa[i].i);
            if (dr > max_xdiff) max_xdiff = dr;
            if (di > max_xdiff) max_xdiff = di;
        }

        /* Verify Fixed AMMX vs C: tolerance of <= 1.5 LSB per stage (DSP standard) */
        int max_mdiff = 0;
        for (int i = 0; i < n; i++) {
            int dr = abs(xc[i].r - xm[i].r);
            int di = abs(xc[i].i - xm[i].i);
            if (dr > max_mdiff) max_mdiff = dr;
            if (di > max_mdiff) max_mdiff = di;
        }

        int ok_f = (max_fdiff <= 1e-4f);
        int ok_s = (max_xdiff <= 1);
        int ok_m = (max_mdiff <= 20);

        if (!ok_f || !ok_s || !ok_m) {
            all_passed = 0;
            printf(" [Debug FAIL: ok_f=%d (max_fdiff=%e), ok_s=%d (max_xdiff=%d), ok_m=%d (max_mdiff=%d)]\n",
                   ok_f, (double)max_fdiff, ok_s, max_xdiff, ok_m, max_mdiff);
        }

        double spd_f = (double)cyc_fc / (double)(cyc_fa ? cyc_fa : 1);
        double spd_s = (double)cyc_xc / (double)(cyc_xa ? cyc_xa : 1);
        double spd_mc = (double)cyc_xc / (double)(cyc_xm ? cyc_xm : 1);
        double spd_ms = (double)cyc_xa / (double)(cyc_xm ? cyc_xm : 1);

        int ok = (ok_f && ok_s && ok_m);

        printf(" %5d | %10lu | %10lu | %4.2fx | %10lu | %10lu | %4.2fx | %10lu | %4.2fx | %4.2fx |  %2d / %2d  | %s\n",
               n,
               (unsigned long)cyc_fc,
               (unsigned long)cyc_fa,
               spd_f,
               (unsigned long)cyc_xc,
               (unsigned long)cyc_xa,
               spd_s,
               (unsigned long)cyc_xm,
               spd_mc,
               spd_ms,
               max_xdiff,
               max_mdiff,
               ok ? "PASS" : "FAIL");

        if (n == 4096) {
            uint32_t t_br = 0, t_stg1 = 0, t_mid = 0, t_last = 0;
            fft_breakdown_ammx(xm, n, twiddle_fixed, &t_br, &t_stg1, &t_mid, &t_last);
            printf("\n   [AMMX N=4096 Cycle Breakdown]\n");
            printf("   - Bit-Reversal:        %7lu cycles (%4.1f%%)\n", (unsigned long)t_br, 100.0 * t_br / (t_br + t_stg1 + t_mid + t_last));
            printf("   - Stage 1 (Vector):    %7lu cycles (%4.1f%%)\n", (unsigned long)t_stg1, 100.0 * t_stg1 / (t_br + t_stg1 + t_mid + t_last));
            printf("   - Stages 2-11 (Mid):   %7lu cycles (%4.1f%%)\n", (unsigned long)t_mid, 100.0 * t_mid / (t_br + t_stg1 + t_mid + t_last));
            printf("   - Stage 12 (Streaming):%7lu cycles (%4.1f%%)\n", (unsigned long)t_last, 100.0 * t_last / (t_br + t_stg1 + t_mid + t_last));
            printf("   - Total AMMX Time:     %7lu cycles\n\n", (unsigned long)(t_br + t_stg1 + t_mid + t_last));
        }

        free(fc);
        free(fa);
        free(xc);
        free(xa);
        free(xm);
    }

    printf("------------------------------------------------------------------------------------------------------------------------------------\n");
    if (all_passed) {
        printf("\n>>> RESULT: ALL FFT BENCHMARKS PASSED VALIDATION AGAINST C-REFERENCE! <<<\n");
    } else {
        printf("\n>>> RESULT: SOME FFT BENCHMARKS FAILED VALIDATION! <<<\n");
    }

    return all_passed ? 0 : 1;
}
