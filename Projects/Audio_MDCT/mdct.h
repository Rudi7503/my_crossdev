#ifndef MDCT_H
#define MDCT_H

#include <stdint.h>
#include <stddef.h>

#define MP3_N        18
#define MP3_2N       36

/* Table initialization */
void mdct_init_tables(void);

/* C Reference Functions */
void imdct36_float_c(const float *in18, float *out36);
void imdct36_fixed_c(const int16_t *in18, int16_t *out36);
void window_overlap_add_fixed_c(const int16_t *in36, int16_t *prev18, int16_t *out18);
void imdct36_windowed_overlap_add_c(const int16_t *in18, int16_t *prev18, int16_t *out18);

/* Forward MDCT (36 time samples -> 18 frequency coefficients) */
void mdct36_float_c(const float *in36, float *out18);
void mdct36_fixed_c(const int16_t *in36, int16_t *out18);

/* 68080 Assembly & AMMX Implementations */
void imdct36_float_asm(const float *in18, float *out36);
void imdct36_fixed_ammx(const int16_t *in18, int16_t *out36);
void overlap_add_fixed_ammx(const int16_t *curr18, int16_t *prev18, int16_t *out18);
void imdct36_windowed_overlap_add_ammx(const int16_t *in18, int16_t *prev18, int16_t *out18);

/* Cycle Counter */
uint32_t get_ccc(void);

#endif /* MDCT_H */
