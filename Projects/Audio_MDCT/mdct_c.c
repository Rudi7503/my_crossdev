#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <math.h>
#include <string.h>
#include "mdct.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* Global transformation tables */
float imdct_cos_float[MP3_2N][MP3_N];
float imdct_win_cos_float[MP3_2N][MP3_N];
float mp3_sine_window_float[MP3_2N];

int16_t imdct_cos_q15[MP3_2N][MP3_N];
int16_t imdct_cos_q15_padded[MP3_2N][20];
int16_t imdct_win_cos_q15[MP3_2N][MP3_N];
/* Padded to 20 words per row for 5-quadword AMMX alignment */
int16_t imdct_win_cos_q15_padded[MP3_2N][20];
int16_t mp3_sine_window_q15[MP3_2N];

void mdct_init_tables(void) {
    /* 1. Calculate standard MP3 sine window: w[i] = sin(pi / 36 * (i + 0.5)) */
    for (int i = 0; i < MP3_2N; i++) {
        double w = sin(M_PI / 36.0 * ((double)i + 0.5));
        mp3_sine_window_float[i] = (float)w;
        mp3_sine_window_q15[i] = (int16_t)round(w * 32767.0);
    }

    /* 2. Calculate IMDCT-36 cosine basis & Fused Windowed Cosine basis */
    for (int i = 0; i < MP3_2N; i++) {
        for (int k = 0; k < MP3_N; k++) {
            double angle = (M_PI / 72.0) * ((double)(2 * i + 19)) * ((double)(2 * k + 1));
            double c = cos(angle);
            double wc = c * mp3_sine_window_float[i];

            imdct_cos_float[i][k] = (float)c;
            imdct_win_cos_float[i][k] = (float)wc;

            /* Scale by 0.5 (16383) to prevent 16-bit intermediate overflow */
            imdct_cos_q15[i][k] = (int16_t)round(c * 16383.0);
            imdct_cos_q15_padded[i][k] = (int16_t)round(c * 16383.0);
            imdct_win_cos_q15[i][k] = (int16_t)round(wc * 16383.0);
            imdct_win_cos_q15_padded[i][k] = (int16_t)round(wc * 16383.0);
        }
        imdct_cos_q15_padded[i][18] = 0;
        imdct_cos_q15_padded[i][19] = 0;
        imdct_win_cos_q15_padded[i][18] = 0;
        imdct_win_cos_q15_padded[i][19] = 0;
    }
}

/* 32-bit Float IMDCT-36 */
void imdct36_float_c(const float *in18, float *out36) {
    for (int i = 0; i < MP3_2N; i++) {
        float sum = 0.0f;
        for (int k = 0; k < MP3_N; k++) {
            sum += in18[k] * imdct_cos_float[i][k];
        }
        out36[i] = sum;
    }
}

/* 16-bit Q1.15 Fixed-Point IMDCT-36 */
void imdct36_fixed_c(const int16_t *in18, int16_t *out36) {
    for (int i = 0; i < MP3_2N; i++) {
        int32_t sum = 0;
        for (int k = 0; k < MP3_N; k++) {
            sum += ((int32_t)in18[k] * (int32_t)imdct_cos_q15[i][k]) >> 15;
        }
        sum <<= 1; /* *2 to restore full 1.0x scale */
        if (sum > 32767) sum = 32767;
        if (sum < -32768) sum = -32768;
        out36[i] = (int16_t)sum;
    }
}

/* Windowing + Overlap-Add in C */
void window_overlap_add_fixed_c(const int16_t *in36, int16_t *prev18, int16_t *out18) {
    for (int i = 0; i < MP3_N; i++) {
        /* Window first half */
        int32_t win0 = ((int32_t)in36[i] * (int32_t)mp3_sine_window_q15[i]) >> 15;
        int32_t s0 = win0 + (int32_t)prev18[i];
        if (s0 > 32767) s0 = 32767;
        if (s0 < -32768) s0 = -32768;
        out18[i] = (int16_t)s0;

        /* Window second half -> save to prev18 for next block */
        int32_t win1 = ((int32_t)in36[i + MP3_N] * (int32_t)mp3_sine_window_q15[i + MP3_N]) >> 15;
        if (win1 > 32767) win1 = 32767;
        if (win1 < -32768) win1 = -32768;
        prev18[i] = (int16_t)win1;
    }
}

/* Fused IMDCT-36 + Windowing + Overlap-Add in C */
void imdct36_windowed_overlap_add_c(const int16_t *in18, int16_t *prev18, int16_t *out18) {
    int16_t win_out[MP3_2N];

    /* Calculate windowed IMDCT directly using fused table */
    for (int i = 0; i < MP3_2N; i++) {
        int32_t sum = 0;
        for (int k = 0; k < MP3_N; k++) {
            sum += ((int32_t)in18[k] * (int32_t)imdct_win_cos_q15[i][k]) >> 15;
        }
        sum <<= 1; /* *2 to restore full 1.0x scale */
        if (sum > 32767) sum = 32767;
        if (sum < -32768) sum = -32768;
        win_out[i] = (int16_t)sum;
    }

    /* 50% Overlap-Add with prev */
    for (int i = 0; i < MP3_N; i++) {
        int32_t s = (int32_t)win_out[i] + (int32_t)prev18[i];
        if (s > 32767) s = 32767;
        if (s < -32768) s = -32768;
        out18[i] = (int16_t)s;
        prev18[i] = win_out[i + MP3_N];
    }
}

/* Forward MDCT (36 time samples -> 18 frequency coefficients) */
void mdct36_float_c(const float *in36, float *out18) {
    for (int k = 0; k < MP3_N; k++) {
        float sum = 0.0f;
        for (int i = 0; i < MP3_2N; i++) {
            sum += in36[i] * imdct_win_cos_float[i][k];
        }
        out18[k] = sum;
    }
}

void mdct36_fixed_c(const int16_t *in36, int16_t *out18) {
    for (int k = 0; k < MP3_N; k++) {
        int32_t sum = 16384;
        for (int i = 0; i < MP3_2N; i++) {
            sum += ((int32_t)in36[i] * (int32_t)imdct_win_cos_q15[i][k]) >> 15;
        }
        if (sum > 32767) sum = 32767;
        if (sum < -32768) sum = -32768;
        out18[k] = (int16_t)sum;
    }
}
