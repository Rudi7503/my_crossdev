#include "fft.h"
#include <math.h>
#include <stdlib.h>

#ifndef PI
#define PI 3.14159265358979323846
#endif

complex_float twiddle_float[NUM_TWIDDLES];
complex_int16 twiddle_fixed[NUM_TWIDDLES];

void fft_init_tables(void) {
    for (int k = 0; k < NUM_TWIDDLES; k++) {
        double angle = -2.0 * PI * (double)k / (double)MAX_FFT_SIZE;
        double cr = cos(angle);
        double si = sin(angle);

        twiddle_float[k].r = (float)cr;
        twiddle_float[k].i = (float)si;

        /* Q1.15 representation: 1.0 -> 32767, -1.0 -> -32768 */
        double qr = round(cr * 32767.0);
        double qi = round(si * 32767.0);
        if (qr > 32767.0) qr = 32767.0;
        if (qr < -32768.0) qr = -32768.0;
        if (qi > 32767.0) qi = 32767.0;
        if (qi < -32768.0) qi = -32768.0;

        twiddle_fixed[k].r = (int16_t)qr;
        twiddle_fixed[k].i = (int16_t)qi;
    }
}

void bit_reverse_float(complex_float *x, int n) {
    int j = 0;
    for (int i = 0; i < n - 1; i++) {
        if (i < j) {
            complex_float tmp = x[i];
            x[i] = x[j];
            x[j] = tmp;
        }
        int k = n >> 1;
        while (k <= j) {
            j -= k;
            k >>= 1;
        }
        j += k;
    }
}

void bit_reverse_fixed(complex_int16 *x, int n) {
    int j = 0;
    for (int i = 0; i < n - 1; i++) {
        if (i < j) {
            complex_int16 tmp = x[i];
            x[i] = x[j];
            x[j] = tmp;
        }
        int k = n >> 1;
        while (k <= j) {
            j -= k;
            k >>= 1;
        }
        j += k;
    }
}

void fft_float_c(complex_float *x, int n) {
    bit_reverse_float(x, n);

    for (int len = 2; len <= n; len <<= 1) {
        int half = len >> 1;
        int tw_stride = MAX_FFT_SIZE / len;

        for (int i = 0; i < n; i += len) {
            for (int j = 0; j < half; j++) {
                int tw_idx = j * tw_stride;
                complex_float w = twiddle_float[tw_idx];
                complex_float v = x[i + j + half];

                /* Complex multiplication: t = w * v */
                float tr = w.r * v.r - w.i * v.i;
                float ti = w.r * v.i + w.i * v.r;

                complex_float u = x[i + j];
                x[i + j].r = u.r + tr;
                x[i + j].i = u.i + ti;
                x[i + j + half].r = u.r - tr;
                x[i + j + half].i = u.i - ti;
            }
        }
    }
}

void fft_fixed_c(complex_int16 *x, int n) {
    bit_reverse_fixed(x, n);

    for (int len = 2; len <= n; len <<= 1) {
        int half = len >> 1;
        int tw_stride = MAX_FFT_SIZE / len;

        for (int i = 0; i < n; i += len) {
            for (int j = 0; j < half; j++) {
                int tw_idx = j * tw_stride;
                complex_int16 w = twiddle_fixed[tw_idx];
                complex_int16 v = x[i + j + half];

                /* Q1.15 multiplication: (w * v) >> 15 */
                int32_t vr = v.r;
                int32_t vi = v.i;
                int32_t wr = w.r;
                int32_t wi = w.i;

                int16_t tr = (int16_t)((wr * vr - wi * vi) >> 15);
                int16_t ti = (int16_t)((wr * vi + wi * vr) >> 15);

                complex_int16 u = x[i + j];

                /* Butterfly with 1-bit scale down (>> 1) */
                x[i + j].r = (int16_t)((u.r + tr) >> 1);
                x[i + j].i = (int16_t)((u.i + ti) >> 1);
                x[i + j + half].r = (int16_t)((u.r - tr) >> 1);
                x[i + j + half].i = (int16_t)((u.i - ti) >> 1);
            }
        }
    }
}
