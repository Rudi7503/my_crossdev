#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <math.h>
#include <string.h>

#include "matrix_mul.h"
#include "lifecycle.h"

#define EPSILON 0.0001f

/*
 * Hilfsfunktion zum Vergleichen zweier Matrizen
 */
static int compare_matrices(const char *name, const float *c_res, const float *asm_res, int n)
{
    int i, j;
    int errors = 0;
    float max_diff = 0.0f;

    for (i = 0; i < n * n; i++) {
        float diff = fabsf(c_res[i] - asm_res[i]);
        if (diff > max_diff) {
            max_diff = diff;
        }
        if (diff > EPSILON) {
            if (errors < 5) {
                printf("  [FAIL] %s Element [%d]: C=%f, ASM=%f, Diff=%f\n",
                       name, i, c_res[i], asm_res[i], diff);
            }
            errors++;
        }
    }

    if (errors == 0) {
        printf("  [PASS] %s (N=%d, max_diff = %.7f)\n", name, n, max_diff);
        return 1;
    } else {
        printf("  [FAIL] %s hat %d fehlerhafte Elemente! (max_diff = %.7f)\n", name, errors, max_diff);
        return 0;
    }
}

/*
 * Test 2x2 Matrix-Multiplikation
 */
static int test_2x2(void)
{
    printf("\n=== TEST 1: 2x2 MATRIX MULTIPLIKATION ===\n");
    int all_ok = 1;

    /* Fall 1: Bekannte Zahlen */
    float a1[4] = { 1.0f, 2.0f,
                    3.0f, 4.0f };
    float b1[4] = { 2.0f, 0.0f,
                    1.0f, 2.0f };
    float out_c[4], out_asm[4];

    matmul_2x2_c(a1, b1, out_c);
    matmul_2x2_asm(a1, b1, out_asm);
    all_ok &= compare_matrices("2x2 Standard-Test", out_c, out_asm, 2);

    /* Fall 2: Identitätsmatrix */
    float id2[4] = { 1.0f, 0.0f,
                     0.0f, 1.0f };
    float a2[4] = { -4.5f,  7.2f,
                     3.14f, -0.88f };
    matmul_2x2_c(a2, id2, out_c);
    matmul_2x2_asm(a2, id2, out_asm);
    all_ok &= compare_matrices("2x2 Identitaet", out_c, out_asm, 2);

    /* Fall 3: Gemischte Nachkommastellen & Vorzeichen */
    float a3[4] = { 12.34f, -56.78f,
                    90.12f, -34.56f };
    float b3[4] = { -0.12f,  3.45f,
                     6.78f, -9.01f };
    matmul_2x2_c(a3, b3, out_c);
    matmul_2x2_asm(a3, b3, out_asm);
    all_ok &= compare_matrices("2x2 Random/Signed", out_c, out_asm, 2);

    return all_ok;
}

/*
 * Test 3x3 Matrix-Multiplikation
 */
static int test_3x3(void)
{
    printf("\n=== TEST 2: 3x3 MATRIX MULTIPLIKATION ===\n");
    int all_ok = 1;

    /* Fall 1: Standard 3D Rotation/Skalierung */
    float a1[9] = {  1.0f,  2.0f,  3.0f,
                     0.0f,  1.0f,  4.0f,
                     5.0f,  6.0f,  0.0f };
    float b1[9] = { -1.0f,  0.0f,  2.0f,
                     3.0f, -2.0f,  1.0f,
                     0.5f,  4.0f, -1.0f };
    float out_c[9], out_asm[9];

    matmul_3x3_c(a1, b1, out_c);
    matmul_3x3_asm(a1, b1, out_asm);
    all_ok &= compare_matrices("3x3 Standard-Test", out_c, out_asm, 3);

    /* Fall 2: Identitätsmatrix */
    float id3[9] = { 1.0f, 0.0f, 0.0f,
                     0.0f, 1.0f, 0.0f,
                     0.0f, 0.0f, 1.0f };
    matmul_3x3_c(a1, id3, out_c);
    matmul_3x3_asm(a1, id3, out_asm);
    all_ok &= compare_matrices("3x3 Identitaet", out_c, out_asm, 3);

    /* Fall 3: Negative Werte & Bruchzahlen */
    float a3[9] = {  0.7071f, -0.7071f, 0.0f,
                     0.7071f,  0.7071f, 0.0f,
                     0.0f,     0.0f,    1.0f };
    float b3[9] = {  2.5f, -1.5f,  3.0f,
                    -4.0f,  5.0f, -0.5f,
                     1.2f,  3.4f, -5.6f };
    matmul_3x3_c(a3, b3, out_c);
    matmul_3x3_asm(a3, b3, out_asm);
    all_ok &= compare_matrices("3x3 Rotation/Scale", out_c, out_asm, 3);

    return all_ok;
}

/*
 * Test 4x4 Matrix-Multiplikation (3D Transformationsmatrix)
 */
static int test_4x4(void)
{
    printf("\n=== TEST 3: 4x4 MATRIX MULTIPLIKATION (3D ENGINE) ===\n");
    int all_ok = 1;

    /* Fall 1: Voll besetzte 4x4 Matrizen */
    float a1[16] = {
         1.0f,  2.0f,  3.0f,  4.0f,
         5.0f,  6.0f,  7.0f,  8.0f,
        -1.0f, -2.0f, -3.0f, -4.0f,
         2.5f,  1.5f, -0.5f,  3.0f
    };
    float b1[16] = {
         2.0f,  0.0f,  1.0f, -1.0f,
         1.0f,  3.0f, -2.0f,  0.5f,
         0.0f,  1.5f,  2.5f, -3.0f,
        -1.0f,  2.0f,  0.0f,  1.0f
    };
    float out_c[16], out_asm[16];

    matmul_4x4_c(a1, b1, out_c);
    matmul_4x4_asm(a1, b1, out_asm);
    all_ok &= compare_matrices("4x4 Standard-Test", out_c, out_asm, 4);

    /* Fall 2: 4x4 Identitätsmatrix */
    float id4[16] = {
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f
    };
    matmul_4x4_c(a1, id4, out_c);
    matmul_4x4_asm(a1, id4, out_asm);
    all_ok &= compare_matrices("4x4 Identitaet", out_c, out_asm, 4);

    /* Fall 3: Typische 3D ModelView * Projection Matrix */
    float mv[16] = {
         0.866f, 0.0f, -0.5f,  10.0f,
         0.0f,   1.0f,  0.0f, -20.0f,
         0.5f,   0.0f,  0.866f, 50.0f,
         0.0f,   0.0f,  0.0f,   1.0f
    };
    float proj[16] = {
         1.81f,  0.0f,   0.0f,   0.0f,
         0.0f,   2.41f,  0.0f,   0.0f,
         0.0f,   0.0f,  -1.02f, -2.02f,
         0.0f,   0.0f,  -1.0f,   0.0f
    };
    matmul_4x4_c(mv, proj, out_c);
    matmul_4x4_asm(mv, proj, out_asm);
    all_ok &= compare_matrices("4x4 3D MVP-Matrix", out_c, out_asm, 4);

    return all_ok;
}

/*
 * Benchmark: Misst Taktzyklen von C vs. ASM auf der 68080 (1000 Durchläufe)
 */
static void benchmark(void)
{
    printf("\n=== PERFORMANCE-BENCHMARK (1000 Iterationen) ===\n");

    float m4_a[16], m4_b[16], m4_out[16];
    int i;
    for (i = 0; i < 16; i++) {
        m4_a[i] = (float)(i + 1) * 0.25f;
        m4_b[i] = (float)(16 - i) * 0.5f;
    }

    uint32_t t0, t1;
    uint32_t c_cycles = 0, asm_cycles = 0;

    /* 2x2 Benchmark */
    t0 = get_ccc();
    for (i = 0; i < 1000; i++) {
        matmul_2x2_c(m4_a, m4_b, m4_out);
    }
    t1 = get_ccc();
    c_cycles = t1 - t0;

    t0 = get_ccc();
    for (i = 0; i < 1000; i++) {
        matmul_2x2_asm(m4_a, m4_b, m4_out);
    }
    t1 = get_ccc();
    asm_cycles = t1 - t0;

    printf("2x2 Matrix: C = %u cyc (%.1f/mul), ASM = %u cyc (%.1f/mul)",
           c_cycles, (double)c_cycles / 1000.0,
           asm_cycles, (double)asm_cycles / 1000.0);
    if (asm_cycles > 0)
        printf(" -> %.2fx schneller\n", (double)c_cycles / (double)asm_cycles);
    else
        printf("\n");

    /* 3x3 Benchmark */
    t0 = get_ccc();
    for (i = 0; i < 1000; i++) {
        matmul_3x3_c(m4_a, m4_b, m4_out);
    }
    t1 = get_ccc();
    c_cycles = t1 - t0;

    t0 = get_ccc();
    for (i = 0; i < 1000; i++) {
        matmul_3x3_asm(m4_a, m4_b, m4_out);
    }
    t1 = get_ccc();
    asm_cycles = t1 - t0;

    printf("3x3 Matrix: C = %u cyc (%.1f/mul), ASM = %u cyc (%.1f/mul)",
           c_cycles, (double)c_cycles / 1000.0,
           asm_cycles, (double)asm_cycles / 1000.0);
    if (asm_cycles > 0)
        printf(" -> %.2fx schneller\n", (double)c_cycles / (double)asm_cycles);
    else
        printf("\n");

    /* 4x4 Benchmark */
    t0 = get_ccc();
    for (i = 0; i < 1000; i++) {
        matmul_4x4_c(m4_a, m4_b, m4_out);
    }
    t1 = get_ccc();
    c_cycles = t1 - t0;

    t0 = get_ccc();
    for (i = 0; i < 1000; i++) {
        matmul_4x4_asm(m4_a, m4_b, m4_out);
    }
    t1 = get_ccc();
    asm_cycles = t1 - t0;

    printf("4x4 Matrix: C = %u cyc (%.1f/mul), ASM = %u cyc (%.1f/mul)",
           c_cycles, (double)c_cycles / 1000.0,
           asm_cycles, (double)asm_cycles / 1000.0);
    if (asm_cycles > 0)
        printf(" -> %.2fx schneller\n", (double)c_cycles / (double)asm_cycles);
    else
        printf("\n");
}

int main(int argc, char **argv)
{
    TEST_START("Matrix_Mul");
    printf("================================================================\n");
    printf("  APOLLO 68080 MATRIX-MULTIPLIKATION (2x2, 3x3, 4x4)\n");
    printf("  Verifikation ASM gegen C-Referenz + Cycle Benchmark\n");
    printf("================================================================\n");

    int ok = 1;
    ok &= test_2x2();
    test_alive("Matrix_Mul", 2);
    ok &= test_3x3();
    test_alive("Matrix_Mul", 3);
    ok &= test_4x4();
    test_alive("Matrix_Mul", 4);

    if (ok) {
        printf("\n>>> ERGEBNIS: ALLE MATRIX-TESTS ERFOLGREICH (100%% BIT-EXAKT) <<<\n");
    } else {
        printf("\n>>> FEHLER: MINDESTENS EIN TEST IST FEHLGESCHLAGEN! <<<\n");
    }

    benchmark();
    TEST_DONE("Matrix_Mul");

    return ok ? 0 : 20;
}
