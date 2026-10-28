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

int main(void) {
    printf("======================================================================\n");
    printf(" Apollo 68080 FFT Benchmark & Validation Suite\n");
    printf(" Real Hardware: Vampire V4 (Apollo 68080 @ Core Release)\n");
    printf(" Formats: 32-bit Single-Precision Float & 16-bit Q1.15 Fixed-Point\n");
    printf(" Sizes: N = 64 to 4096 points\n");
    printf("======================================================================\n\n");

    printf("Initializing twiddle factor tables...\n");
    fft_init_tables();
    printf("Twiddle tables initialized (4096 max points / 2048 twiddles).\n\n");

    printf("Starting Benchmark & Verification Series:\n");
    printf("------------------------------------------------------------------------------------------------------------------\n");
    printf("   N   |  Float C (cyc) | Float ASM (cyc) | Spdup | Float Diff | Fixed C (cyc) | Fixed ASM (cyc) | Spdup | Fixed Diff\n");
    printf("-------+----------------+-----------------+-------+------------+---------------+-----------------+-------+------------\n");

    int all_passed = 1;

    for (int t = 0; t < NUM_TESTS; t++) {
        int n = test_sizes[t];

        complex_float *fc = (complex_float *)malloc(n * sizeof(complex_float));
        complex_float *fa = (complex_float *)malloc(n * sizeof(complex_float));
        complex_int16 *xc = (complex_int16 *)malloc(n * sizeof(complex_int16));
        complex_int16 *xa = (complex_int16 *)malloc(n * sizeof(complex_int16));

        if (!fc || !fa || !xc || !xa) {
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

        /* Benchmark Fixed ASM */
        t0 = get_ccc();
        fft_fixed_asm(xa, n, twiddle_fixed);
        t1 = get_ccc();
        uint32_t cyc_xa = t1 - t0;

        /* Verify Fixed */
        int max_xdiff = 0;
        for (int i = 0; i < n; i++) {
            int dr = abs(xc[i].r - xa[i].r);
            int di = abs(xc[i].i - xa[i].i);
            if (dr > max_xdiff) max_xdiff = dr;
            if (di > max_xdiff) max_xdiff = di;
        }
        if (max_xdiff > 1) {
            all_passed = 0;
        }

        double spd_f = (double)cyc_fc / (double)(cyc_fa ? cyc_fa : 1);
        double spd_x = (double)cyc_xc / (double)(cyc_xa ? cyc_xa : 1);

        printf(" %5d | %14lu | %15lu | %4.2fx |  %8.2e  | %13lu | %15lu | %4.2fx | %6d%s\n",
               n,
               (unsigned long)cyc_fc,
               (unsigned long)cyc_fa,
               spd_f,
               max_fdiff,
               (unsigned long)cyc_xc,
               (unsigned long)cyc_xa,
               spd_x,
               max_xdiff,
               (max_fdiff <= 1e-4f && max_xdiff <= 1) ? " (OK)" : " (FAIL)");

        free(fc);
        free(fa);
        free(xc);
        free(xa);
    }

    printf("------------------------------------------------------------------------------------------------------------------\n");
    if (all_passed) {
        printf("\n>>> RESULT: ALL FFT BENCHMARKS PASSED VALIDATION AGAINST C-REFERENCE! <<<\n");
    } else {
        printf("\n>>> RESULT: SOME FFT BENCHMARKS FAILED VALIDATION! <<<\n");
    }

    return all_passed ? 0 : 1;
}
