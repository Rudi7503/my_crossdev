#ifndef DCT_H
#define DCT_H

#include <stdint.h>
#include <stddef.h>

#define DCT_SIZE 8
#define DCT_BLOCK_SIZE (DCT_SIZE * DCT_SIZE) /* 64 elements */

/* 1D Float DCT / IDCT (8 points) */
void dct1d_float_c(const float *src, float *dst);
void idct1d_float_c(const float *src, float *dst);
void dct1d_float_asm(const float *src, float *dst);
void idct1d_float_asm(const float *src, float *dst);
void dct1d_float_fast_c(const float *src, float *dst);
void idct1d_float_fast_c(const float *src, float *dst);
void dct1d_float_fast_asm(const float *src, float *dst);
void idct1d_float_fast_asm(const float *src, float *dst);

/* 1D 16-bit Fixed-Point DCT / IDCT (8 points, Q1.15) */
void dct1d_fixed_c(const int16_t *src, int16_t *dst);
void idct1d_fixed_c(const int16_t *src, int16_t *dst);
void dct1d_fixed_scalar_asm(const int16_t *src, int16_t *dst);
void idct1d_fixed_scalar_asm(const int16_t *src, int16_t *dst);
void dct1d_fixed_ammx(const int16_t *src, int16_t *dst);
void idct1d_fixed_ammx(const int16_t *src, int16_t *dst);
void dct1d_fixed_fast_ammx(const int16_t *src, int16_t *dst);
void idct1d_fixed_fast_ammx(const int16_t *src, int16_t *dst);

/* 2D 8x8 Float DCT / IDCT */
void dct2d_8x8_float_c(const float *src, float *dst);
void idct2d_8x8_float_c(const float *src, float *dst);
void dct2d_8x8_float_asm(const float *src, float *dst);
void idct2d_8x8_float_asm(const float *src, float *dst);
void dct2d_8x8_float_fast_asm(const float *src, float *dst);
void idct2d_8x8_float_fast_asm(const float *src, float *dst);

/* 2D 8x8 16-bit Fixed-Point DCT / IDCT */
void dct2d_8x8_fixed_c(const int16_t *src, int16_t *dst);
void idct2d_8x8_fixed_c(const int16_t *src, int16_t *dst);
void dct2d_8x8_fixed_scalar_asm(const int16_t *src, int16_t *dst);
void idct2d_8x8_fixed_scalar_asm(const int16_t *src, int16_t *dst);
void dct2d_8x8_fixed_ammx(const int16_t *src, int16_t *dst);
void idct2d_8x8_fixed_ammx(const int16_t *src, int16_t *dst);
void dct2d_8x8_fixed_fast_ammx(const int16_t *src, int16_t *dst);
void idct2d_8x8_fixed_fast_ammx(const int16_t *src, int16_t *dst);

/* Hardware cycle counter */
uint32_t get_ccc(void);

void transpose8x8_float(const float *src, float *dst);
void transpose8x8_fixed(const int16_t *src, int16_t *dst);

/* Tables initialization */
void dct_init_tables(void);

#endif /* DCT_H */
