#include "dct.h"
#include <math.h>
#include <string.h>

#ifndef PI
#define PI 3.14159265358979323846
#endif

/* Global transformation matrices */
float dct_matrix_f[DCT_SIZE][DCT_SIZE];
float idct_matrix_f[DCT_SIZE][DCT_SIZE];

int16_t dct_matrix_q15[DCT_SIZE][DCT_SIZE];
int16_t idct_matrix_q15[DCT_SIZE][DCT_SIZE];

void dct_init_tables(void) {
    for (int k = 0; k < DCT_SIZE; k++) {
        float alpha = (k == 0) ? (float)sqrt(1.0 / (double)DCT_SIZE) : (float)sqrt(2.0 / (double)DCT_SIZE);
        for (int n = 0; n < DCT_SIZE; n++) {
            float val = alpha * (float)cos((2.0 * n + 1.0) * k * PI / (2.0 * (double)DCT_SIZE));
            dct_matrix_f[k][n] = val;
            idct_matrix_f[n][k] = val; /* Transpose */

            int q = (int)floor(val * 32767.0f + 0.5f);
            if (q > 32767) q = 32767;
            if (q < -32768) q = -32768;
            dct_matrix_q15[k][n] = (int16_t)q;
            idct_matrix_q15[n][k] = (int16_t)q;
        }
    }
}

/* 1D Float DCT */
void dct1d_float_c(const float *src, float *dst) {
    for (int k = 0; k < DCT_SIZE; k++) {
        float sum = 0.0f;
        for (int n = 0; n < DCT_SIZE; n++) {
            sum += dct_matrix_f[k][n] * src[n];
        }
        dst[k] = sum;
    }
}

/* 1D Float IDCT */
void idct1d_float_c(const float *src, float *dst) {
    for (int n = 0; n < DCT_SIZE; n++) {
        float sum = 0.0f;
        for (int k = 0; k < DCT_SIZE; k++) {
            sum += idct_matrix_f[n][k] * src[k];
        }
        dst[n] = sum;
    }
}

/* 1D 16-bit Fixed-Point DCT (Q1.15) */
void dct1d_fixed_c(const int16_t *src, int16_t *dst) {
    for (int k = 0; k < DCT_SIZE; k++) {
        int32_t sum = 16384; /* Rounding bias */
        for (int n = 0; n < DCT_SIZE; n++) {
            sum += (int32_t)src[n] * (int32_t)dct_matrix_q15[k][n];
        }
        int32_t res = sum >> 15;
        if (res > 32767) res = 32767;
        if (res < -32768) res = -32768;
        dst[k] = (int16_t)res;
    }
}

/* 1D 16-bit Fixed-Point IDCT (Q1.15) */
void idct1d_fixed_c(const int16_t *src, int16_t *dst) {
    for (int n = 0; n < DCT_SIZE; n++) {
        int32_t sum = 16384; /* Rounding bias */
        for (int k = 0; k < DCT_SIZE; k++) {
            sum += (int32_t)src[k] * (int32_t)idct_matrix_q15[n][k];
        }
        int32_t res = sum >> 15;
        if (res > 32767) res = 32767;
        if (res < -32768) res = -32768;
        dst[n] = (int16_t)res;
    }
}

void transpose8x8_float(const float *src, float *dst) {
    for (int r = 0; r < DCT_SIZE; r++) {
        for (int c = 0; c < DCT_SIZE; c++) {
            dst[c * DCT_SIZE + r] = src[r * DCT_SIZE + c];
        }
    }
}

void transpose8x8_fixed(const int16_t *src, int16_t *dst) {
    for (int r = 0; r < DCT_SIZE; r++) {
        for (int c = 0; c < DCT_SIZE; c++) {
            dst[c * DCT_SIZE + r] = src[r * DCT_SIZE + c];
        }
    }
}

/* 2D 8x8 Float DCT */
void dct2d_8x8_float_c(const float *src, float *dst) {
    float tmp[DCT_BLOCK_SIZE];

    /* Step 1: Transform rows */
    for (int r = 0; r < DCT_SIZE; r++) {
        dct1d_float_c(&src[r * DCT_SIZE], &tmp[r * DCT_SIZE]);
    }

    /* Step 2: Transform columns */
    for (int c = 0; c < DCT_SIZE; c++) {
        for (int k = 0; k < DCT_SIZE; k++) {
            float sum = 0.0f;
            for (int r = 0; r < DCT_SIZE; r++) {
                sum += dct_matrix_f[k][r] * tmp[r * DCT_SIZE + c];
            }
            dst[k * DCT_SIZE + c] = sum;
        }
    }
}

/* 2D 8x8 Float IDCT */
void idct2d_8x8_float_c(const float *src, float *dst) {
    float tmp[DCT_BLOCK_SIZE];

    /* Step 1: Inverse transform columns */
    for (int c = 0; c < DCT_SIZE; c++) {
        for (int r = 0; r < DCT_SIZE; r++) {
            float sum = 0.0f;
            for (int k = 0; k < DCT_SIZE; k++) {
                sum += idct_matrix_f[r][k] * src[k * DCT_SIZE + c];
            }
            tmp[r * DCT_SIZE + c] = sum;
        }
    }

    /* Step 2: Inverse transform rows */
    for (int r = 0; r < DCT_SIZE; r++) {
        idct1d_float_c(&tmp[r * DCT_SIZE], &dst[r * DCT_SIZE]);
    }
}

/* 2D 8x8 16-bit Fixed-Point DCT */
void dct2d_8x8_fixed_c(const int16_t *src, int16_t *dst) {
    int16_t tmp[DCT_BLOCK_SIZE];

    /* Step 1: Transform rows */
    for (int r = 0; r < DCT_SIZE; r++) {
        dct1d_fixed_c(&src[r * DCT_SIZE], &tmp[r * DCT_SIZE]);
    }

    /* Step 2: Transform columns */
    for (int c = 0; c < DCT_SIZE; c++) {
        for (int k = 0; k < DCT_SIZE; k++) {
            int32_t sum = 16384;
            for (int r = 0; r < DCT_SIZE; r++) {
                sum += (int32_t)tmp[r * DCT_SIZE + c] * (int32_t)dct_matrix_q15[k][r];
            }
            int32_t res = sum >> 15;
            if (res > 32767) res = 32767;
            if (res < -32768) res = -32768;
            dst[k * DCT_SIZE + c] = (int16_t)res;
        }
    }
}

/* 2D 8x8 16-bit Fixed-Point IDCT */
void idct2d_8x8_fixed_c(const int16_t *src, int16_t *dst) {
    int16_t tmp[DCT_BLOCK_SIZE];

    /* Step 1: Inverse transform columns */
    for (int c = 0; c < DCT_SIZE; c++) {
        for (int r = 0; r < DCT_SIZE; r++) {
            int32_t sum = 16384;
            for (int k = 0; k < DCT_SIZE; k++) {
                sum += (int32_t)src[k * DCT_SIZE + c] * (int32_t)idct_matrix_q15[r][k];
            }
            int32_t res = sum >> 15;
            if (res > 32767) res = 32767;
            if (res < -32768) res = -32768;
            tmp[r * DCT_SIZE + c] = (int16_t)res;
        }
    }

    /* Step 2: Inverse transform rows */
    for (int r = 0; r < DCT_SIZE; r++) {
        idct1d_fixed_c(&tmp[r * DCT_SIZE], &dst[r * DCT_SIZE]);
    }
}
