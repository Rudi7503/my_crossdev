#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <math.h>
#include <string.h>
#include "jpeg.h"

#define PI 3.14159265358979323846

/* Standard JPEG Zig-Zag Scan Order */
const uint8_t jpeg_zigzag_order[64] = {
     0,  1,  8, 16,  9,  2,  3, 10,
    17, 24, 32, 25, 18, 11,  4,  5,
    12, 19, 26, 33, 40, 48, 41, 34,
    27, 20, 13,  6,  7, 14, 21, 28,
    35, 42, 49, 56, 57, 50, 43, 36,
    29, 22, 15, 23, 30, 37, 44, 51,
    58, 59, 52, 45, 38, 31, 39, 46,
    53, 60, 61, 54, 47, 55, 62, 63
};

/* Standard JPEG Luminance Quantization Table (ITU-T T.81 / ISO/IEC 10918-1) */
const uint16_t jpeg_luma_quant[64] = {
    16, 11, 10, 16, 24, 40, 51, 61,
    12, 12, 14, 19, 26, 58, 60, 55,
    14, 13, 16, 24, 40, 57, 69, 56,
    14, 17, 22, 29, 51, 87, 80, 62,
    18, 22, 37, 56, 68, 109, 103, 77,
    24, 35, 55, 64, 81, 104, 113, 92,
    49, 64, 78, 87, 103, 121, 120, 101,
    72, 92, 95, 98, 112, 100, 103, 99
};

/* Standard JPEG Chrominance Quantization Table */
const uint16_t jpeg_chroma_quant[64] = {
    17, 18, 24, 47, 99, 99, 99, 99,
    18, 21, 26, 66, 99, 99, 99, 99,
    24, 26, 56, 99, 99, 99, 99, 99,
    47, 66, 99, 99, 99, 99, 99, 99,
    99, 99, 99, 99, 99, 99, 99, 99,
    99, 99, 99, 99, 99, 99, 99, 99,
    99, 99, 99, 99, 99, 99, 99, 99,
    99, 99, 99, 99, 99, 99, 99, 99
};

/* Precomputed Q1.15 IDCT Transform Matrix */
static int16_t idct_matrix_q15[8][8];

void jpeg_init_tables(void) {
    for (int k = 0; k < 8; k++) {
        float alpha = (k == 0) ? (float)sqrt(1.0 / 8.0) : (float)sqrt(2.0 / 8.0);
        for (int n = 0; n < 8; n++) {
            float val = alpha * (float)cos((2.0 * n + 1.0) * k * PI / 16.0);
            int q = (int)floor(val * 32767.0f + 0.5f);
            if (q > 32767) q = 32767;
            if (q < -32768) q = -32768;
            idct_matrix_q15[n][k] = (int16_t)q; /* Transpose for IDCT */
        }
    }
}

/* 1. Dequantization & Zig-Zag Reordering in C */
void dequant_zigzag_c(const int16_t *src_zz, const uint16_t *qtable, int16_t *dst_raster64) {
    for (int i = 0; i < JPEG_BLOCK_SIZE; i++) {
        uint8_t zz_pos = jpeg_zigzag_order[i];
        dst_raster64[i] = (int16_t)(src_zz[zz_pos] * qtable[i]);
    }
}

/* 1D 16-bit Fixed-Point IDCT (Q1.15) */
static void idct1d_fixed_c(const int16_t *src, int16_t *dst) {
    for (int n = 0; n < 8; n++) {
        int32_t sum = 16384; /* Rounding bias */
        for (int k = 0; k < 8; k++) {
            sum += (int32_t)src[k] * (int32_t)idct_matrix_q15[n][k];
        }
        int32_t res = sum >> 15;
        if (res > 32767) res = 32767;
        if (res < -32768) res = -32768;
        dst[n] = (int16_t)res;
    }
}

/* 2. 8x8 2D IDCT with Fused Level-Shift (+128) and Saturation (0..255) in C */
void idct8x8_levelshift_clamp_c(const int16_t *src_raster64, uint8_t *dst_pixels64) {
    int16_t tmp[JPEG_BLOCK_SIZE];

    /* Step 1: Inverse transform columns */
    for (int c = 0; c < 8; c++) {
        for (int r = 0; r < 8; r++) {
            int32_t sum = 16384;
            for (int k = 0; k < 8; k++) {
                sum += (int32_t)src_raster64[k * 8 + c] * (int32_t)idct_matrix_q15[r][k];
            }
            int32_t res = sum >> 15;
            if (res > 32767) res = 32767;
            if (res < -32768) res = -32768;
            tmp[r * 8 + c] = (int16_t)res;
        }
    }

    /* Step 2: Inverse transform rows + Level Shift (+128) + Clamp to uint8_t */
    int16_t row_out[8];
    for (int r = 0; r < 8; r++) {
        idct1d_fixed_c(&tmp[r * 8], row_out);
        for (int c = 0; c < 8; c++) {
            int32_t val = row_out[c] + 128;
            if (val < 0) val = 0;
            if (val > 255) val = 255;
            dst_pixels64[r * 8 + c] = (uint8_t)val;
        }
    }
}

/* 3. YCbCr -> RGB565 Conversion in C */
void ycbcr_to_rgb565_c(const uint8_t *y64, const uint8_t *cb64, const uint8_t *cr64, uint16_t *rgb565_64) {
    for (int i = 0; i < JPEG_BLOCK_SIZE; i++) {
        int32_t y  = y64[i];
        int32_t cb = (int32_t)cb64[i] - 128;
        int32_t cr = (int32_t)cr64[i] - 128;

        /* ITU-R BT.601 color matrix (Fixed-point Q16) */
        int32_t r = y + ((91881 * cr) >> 16);
        int32_t g = y - ((22554 * cb + 46802 * cr) >> 16);
        int32_t b = y + ((116130 * cb) >> 16);

        if (r < 0) r = 0; else if (r > 255) r = 255;
        if (g < 0) g = 0; else if (g > 255) g = 255;
        if (b < 0) b = 0; else if (b > 255) b = 255;

        uint16_t r5 = (uint16_t)(r >> 3);
        uint16_t g6 = (uint16_t)(g >> 2);
        uint16_t b5 = (uint16_t)(b >> 3);

        rgb565_64[i] = (r5 << 11) | (g6 << 5) | b5;
    }
}

/* 4. YCbCr -> ARGB32 Conversion in C */
void ycbcr_to_argb32_c(const uint8_t *y64, const uint8_t *cb64, const uint8_t *cr64, uint32_t *argb32_64) {
    for (int i = 0; i < JPEG_BLOCK_SIZE; i++) {
        int32_t y  = y64[i];
        int32_t cb = (int32_t)cb64[i] - 128;
        int32_t cr = (int32_t)cr64[i] - 128;

        int32_t r = y + ((91881 * cr) >> 16);
        int32_t g = y - ((22554 * cb + 46802 * cr) >> 16);
        int32_t b = y + ((116130 * cb) >> 16);

        if (r < 0) r = 0; else if (r > 255) r = 255;
        if (g < 0) g = 0; else if (g > 255) g = 255;
        if (b < 0) b = 0; else if (b > 255) b = 255;

        argb32_64[i] = (0xFF000000) | ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b;
    }
}

/* 5. Complete End-to-End MCU Decoding Pipeline in C */
void jpeg_pipeline_mcu_c(const int16_t *y_zz, const int16_t *cb_zz, const int16_t *cr_zz,
                         const uint16_t *qy, const uint16_t *qc, uint16_t *rgb565_out) {
    int16_t y_raster[64], cb_raster[64], cr_raster[64];
    uint8_t y_pix[64], cb_pix[64], cr_pix[64];

    /* Dequant + Zigzag */
    dequant_zigzag_c(y_zz, qy, y_raster);
    dequant_zigzag_c(cb_zz, qc, cb_raster);
    dequant_zigzag_c(cr_zz, qc, cr_raster);

    /* IDCT + Level-Shift (+128) + Clamp */
    idct8x8_levelshift_clamp_c(y_raster, y_pix);
    idct8x8_levelshift_clamp_c(cb_raster, cb_pix);
    idct8x8_levelshift_clamp_c(cr_raster, cr_pix);

    /* Color space conversion to RGB565 */
    ycbcr_to_rgb565_c(y_pix, cb_pix, cr_pix, rgb565_out);
}
