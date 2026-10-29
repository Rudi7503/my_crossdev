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

/* Fast 1D Float DCT (Loeffler) */
void dct1d_float_fast_c(const float *src, float *dst) {
    float s0 = src[0] + src[7];
    float s1 = src[1] + src[6];
    float s2 = src[2] + src[5];
    float s3 = src[3] + src[4];

    float d0 = src[0] - src[7];
    float d1 = src[1] - src[6];
    float d2 = src[2] - src[5];
    float d3 = src[3] - src[4];

    float t0 = s0 + s3;
    float t1 = s1 + s2;
    float t2 = s1 - s2;
    float t3 = s0 - s3;

    const float c4 = 0.35355339059327376f; /* 1/sqrt(8) */
    const float c2 = 0.46193976625564337f; /* 0.5 * cos(pi/8) */
    const float s2_coeff = 0.1913417161825449f; /* 0.5 * sin(pi/8) */

    const float c1 = 0.4903926402016152f;  /* 0.5 * cos(1*pi/16) */
    const float c3 = 0.4157348061512726f;  /* 0.5 * cos(3*pi/16) */
    const float c5 = 0.27778511650980114f; /* 0.5 * cos(5*pi/16) */
    const float c7 = 0.09754516100806413f; /* 0.5 * cos(7*pi/16) */

    dst[0] = c4 * (t0 + t1);
    dst[4] = c4 * (t0 - t1);
    dst[2] = c2 * t3 + s2_coeff * t2;
    dst[6] = s2_coeff * t3 - c2 * t2;

    dst[1] = d0 * c1 + d1 * c3 + d2 * c5 + d3 * c7;
    dst[3] = d0 * c3 - d1 * c7 - d2 * c1 - d3 * c5;
    dst[5] = d0 * c5 - d1 * c1 + d2 * c7 + d3 * c3;
    dst[7] = d0 * c7 - d1 * c5 + d2 * c3 - d3 * c1;
}

/* Fast 1D Float IDCT (Loeffler) */
void idct1d_float_fast_c(const float *src, float *dst) {
    const float c4 = 0.35355339059327376f;
    const float c2 = 0.46193976625564337f;
    const float s2_coeff = 0.1913417161825449f;

    const float c1 = 0.4903926402016152f;
    const float c3 = 0.4157348061512726f;
    const float c5 = 0.27778511650980114f;
    const float c7 = 0.09754516100806413f;

    float e0 = c4 * (src[0] + src[4]);
    float e1 = c4 * (src[0] - src[4]);
    float e2 = c2 * src[2] + s2_coeff * src[6];
    float e3 = s2_coeff * src[2] - c2 * src[6];

    float s0 = e0 + e2;
    float s1 = e1 + e3;
    float s2 = e1 - e3;
    float s3 = e0 - e2;

    float d0 = src[1] * c1 + src[3] * c3 + src[5] * c5 + src[7] * c7;
    float d1 = src[1] * c3 - src[3] * c7 - src[5] * c1 - src[7] * c5;
    float d2 = src[1] * c5 - src[3] * c1 + src[5] * c7 + src[7] * c3;
    float d3 = src[1] * c7 - src[3] * c5 + src[5] * c3 - src[7] * c1;

    dst[0] = s0 + d0;
    dst[7] = s0 - d0;
    dst[1] = s1 + d1;
    dst[6] = s1 - d1;
    dst[2] = s2 + d2;
    dst[5] = s2 - d2;
    dst[3] = s3 + d3;
    dst[4] = s3 - d3;
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

/* 8x8 Block Quantization (C Reference) */
void quantize8x8_c(const int16_t *src, int16_t *dst, const uint16_t *qmatrix) {
    for (int i = 0; i < DCT_BLOCK_SIZE; i++) {
        int32_t val = src[i];
        int32_t q = qmatrix[i];
        if (q == 0) q = 1;
        if (val >= 0) {
            dst[i] = (int16_t)((val + (q >> 1)) / q);
        } else {
            dst[i] = (int16_t)((val - (q >> 1)) / q);
        }
    }
}

/* Fused AAN DCT + Quantization (C Reference) */
void dct2d_8x8_aan_quant_c(const int16_t *src, int16_t *dst, const uint16_t *qmatrix_aan) {
    int16_t tmp[DCT_BLOCK_SIZE];
    dct2d_8x8_fixed_c(src, tmp);
    quantize8x8_c(tmp, dst, qmatrix_aan);
}
