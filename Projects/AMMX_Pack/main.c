#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "ammx_pack.h"

#define NUM_SAMPLES 65536

static int16_t in_buf[NUM_SAMPLES] __attribute__((aligned(16)));
static int8_t  ref_out[NUM_SAMPLES] __attribute__((aligned(16)));
static int8_t  base_out[NUM_SAMPLES] __attribute__((aligned(16)));
static int8_t  opt_out[NUM_SAMPLES] __attribute__((aligned(16)));

static int verify_against_ref(const char *name, const int8_t *test_out, uint32_t count)
{
    uint32_t i;
    int errors = 0;

    for (i = 0; i < count; i++) {
        if (test_out[i] != ref_out[i]) {
            if (errors < 5) {
                printf("  [FAIL] %s Index %u: Input=%d, Ref=%d, ASM=%d\n",
                       name, i, in_buf[i], ref_out[i], test_out[i]);
            }
            errors++;
        }
    }

    if (errors == 0) {
        printf("  [PASS] %s (%u Samples bit-exakt identisch zur C-Referenz!)\n", name, count);
        return 1;
    } else {
        printf("  [FAIL] %s: %d Fehler gefunden!\n", name, errors);
        return 0;
    }
}

int main(int argc, char **argv)
{
    printf("================================================================\n");
    printf("  APOLLO 68080 AMMX PACKSSWB (Word zu Signed Byte mit Saturation)\n");
    printf("  Verifikation ASM gegen C-Referenz ueber alle 65536 Werte\n");
    printf("================================================================\n");

    /* 1. Testpuffer mit allen 16-Bit-Zweierkomplement-Werten füllen (-32768 bis +32767) */
    uint32_t i;
    for (i = 0; i < NUM_SAMPLES; i++) {
        in_buf[i] = (int16_t)(i - 32768);
    }

    /* 2. C-Referenz berechnen */
    pack_signed_words_c(in_buf, ref_out, NUM_SAMPLES);

    /* 3. ASM Baseline ausführen */
    memset(base_out, 0x55, sizeof(base_out));
    pack_signed_words_baseline_asm(in_buf, base_out, NUM_SAMPLES / 8);

    /* 4. ASM Opt ausführen */
    memset(opt_out, 0x55, sizeof(opt_out));
    pack_signed_words_opt_asm(in_buf, opt_out, NUM_SAMPLES / 16);

    /* 5. Verifikation */
    int ok = 1;
    ok &= verify_against_ref("Baseline ASM (8 Worte/Loop)", base_out, NUM_SAMPLES);
    ok &= verify_against_ref("Optimized ASM (16 Worte/Loop)", opt_out, NUM_SAMPLES);

    if (ok) {
        printf("\n>>> ERGEBNIS: AMMX PACKSSWB 100%% BIT-EXAKT PASS! <<<\n");
    } else {
        printf("\n>>> FEHLER IN DER AMMX PACKSSWB IMPLEMENTIERUNG! <<<\n");
    }

    /* 6. Performance-Benchmark */
    printf("\n=== PERFORMANCE-BENCHMARK (65536 Samples) ===\n");
    uint32_t t0, t1;
    uint32_t c_cyc, base_cyc, opt_cyc;

    /* C-Referenz */
    t0 = get_ccc();
    pack_signed_words_c(in_buf, ref_out, NUM_SAMPLES);
    t1 = get_ccc();
    c_cyc = t1 - t0;

    /* Baseline ASM */
    t0 = get_ccc();
    pack_signed_words_baseline_asm(in_buf, base_out, NUM_SAMPLES / 8);
    t1 = get_ccc();
    base_cyc = t1 - t0;

    /* Optimized ASM */
    t0 = get_ccc();
    pack_signed_words_opt_asm(in_buf, opt_out, NUM_SAMPLES / 16);
    t1 = get_ccc();
    opt_cyc = t1 - t0;

    printf("C-Referenz:    %u Cycles (%.2f cyc/Sample)\n", c_cyc, (double)c_cyc / NUM_SAMPLES);
    printf("ASM Baseline:  %u Cycles (%.2f cyc/Sample)\n", base_cyc, (double)base_cyc / NUM_SAMPLES);
    printf("ASM Optimized: %u Cycles (%.2f cyc/Sample)\n", opt_cyc, (double)opt_cyc / NUM_SAMPLES);

    if (opt_cyc > 0) {
        printf("Speedup Opt vs C:        %.2fx schneller\n", (double)c_cyc / (double)opt_cyc);
        printf("Speedup Opt vs Baseline: %.2fx schneller\n", (double)base_cyc / (double)opt_cyc);
    }

    return ok ? 0 : 20;
}
