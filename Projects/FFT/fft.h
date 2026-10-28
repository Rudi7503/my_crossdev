#ifndef FFT_H
#define FFT_H

#include <stdint.h>
#include <stddef.h>

#define MAX_FFT_SIZE 4096
#define NUM_TWIDDLES (MAX_FFT_SIZE / 2)

typedef struct {
    float r;
    float i;
} complex_float;

typedef struct {
    int16_t r;
    int16_t i;
} complex_int16;

extern complex_float twiddle_float[NUM_TWIDDLES];
extern complex_int16 twiddle_fixed[NUM_TWIDDLES];

void fft_init_tables(void);

/* Bit reversal routines */
void bit_reverse_float(complex_float *x, int n);
void bit_reverse_fixed(complex_int16 *x, int n);

/* Canonical C references */
void fft_float_c(complex_float *x, int n);
void fft_fixed_c(complex_int16 *x, int n);

/* 68080 ASM implementations */
void fft_float_asm(complex_float *x, int n, const complex_float *tw);
void fft_fixed_asm(complex_int16 *x, int n, const complex_int16 *tw);

/* Hardware cycle counter on Apollo 68080 */
uint32_t get_ccc(void);

#endif /* FFT_H */
