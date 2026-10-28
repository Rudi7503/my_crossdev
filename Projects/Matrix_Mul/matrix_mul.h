#ifndef MATRIX_MUL_H
#define MATRIX_MUL_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * C-Referenz-Implementierungen (Row-Major: C[i][j] = Summe_k A[i][k] * B[k][j])
 */
void matmul_2x2_c(const float *a, const float *b, float *out);
void matmul_3x3_c(const float *a, const float *b, float *out);
void matmul_4x4_c(const float *a, const float *b, float *out);

/*
 * Optimierte 68080-Assembler-Implementierungen (Hardware-FPU, Pipelined)
 */
void matmul_2x2_asm(const float *a, const float *b, float *out);
void matmul_3x3_asm(const float *a, const float *b, float *out);
void matmul_4x4_asm(const float *a, const float *b, float *out);

/*
 * Hilfsfunktion: 68080 Hardware Cycle Counter (CCC)
 */
uint32_t get_ccc(void);

#ifdef __cplusplus
}
#endif

#endif /* MATRIX_MUL_H */
