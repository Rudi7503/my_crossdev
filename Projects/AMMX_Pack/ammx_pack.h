#ifndef AMMX_PACK_H
#define AMMX_PACK_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * C-Referenz: Sättigt 16-Bit-Worte auf den Bereich [-128, +127]
 */
void pack_signed_words_c(const int16_t *in, int8_t *out, uint32_t count);

/*
 * AMMX Baseline (korrigierter Bias $0080 / $8080, 8 Worte pro Durchlauf)
 * block_count = Anzahl der 8-Wort-Blöcke (8 Worte = 16 Bytes In -> 8 Bytes Out)
 */
void pack_signed_words_baseline_asm(const int16_t *in, int8_t *out, uint32_t block_count);

/*
 * AMMX Optimiert (2x Unrolled & Interleaved, 16 Worte pro Durchlauf)
 * block_count = Anzahl der 16-Wort-Blöcke (16 Worte = 32 Bytes In -> 16 Bytes Out)
 */
void pack_signed_words_opt_asm(const int16_t *in, int8_t *out, uint32_t block_count);

/*
 * 68080 Hardware Cycle Counter
 */
uint32_t get_ccc(void);

#ifdef __cplusplus
}
#endif

#endif /* AMMX_PACK_H */
