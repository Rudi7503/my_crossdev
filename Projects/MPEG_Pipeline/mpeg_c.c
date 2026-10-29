#include <stdio.h>
#include <stdint.h>
#include <math.h>
#include "mpeg.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static int16_t idct_matrix_q15[8][8];

void mpeg_init_tables(void) {
    for (int r = 0; r < 8; r++) {
        for (int k = 0; k < 8; k++) {
            double alpha = (k == 0) ? (1.0 / sqrt(8.0)) : 0.5;
            double val = alpha * cos((2.0 * r + 1.0) * k * M_PI / 16.0);
            idct_matrix_q15[r][k] = (int16_t)round(val * 32768.0);
        }
    }
}

static void idct1d_fixed_c(const int16_t in[8], int16_t out[8]) {
    for (int r = 0; r < 8; r++) {
        int32_t sum = 16384;
        for (int k = 0; k < 8; k++) {
            sum += (int32_t)in[k] * (int32_t)idct_matrix_q15[r][k];
        }
        int32_t res = sum >> 15;
        if (res > 32767) res = 32767;
        if (res < -32768) res = -32768;
        out[r] = (int16_t)res;
    }
}

void mpeg_idct8x8_c(const int16_t *src_raster, int16_t *dst_residual) {
    int16_t tmp[MPEG_BLOCK_SIZE];

    /* Step 1: Column IDCT */
    for (int c = 0; c < 8; c++) {
        for (int r = 0; r < 8; r++) {
            int32_t sum = 16384;
            for (int k = 0; k < 8; k++) {
                sum += (int32_t)src_raster[k * 8 + c] * (int32_t)idct_matrix_q15[r][k];
            }
            int32_t res = sum >> 15;
            if (res > 32767) res = 32767;
            if (res < -32768) res = -32768;
            tmp[r * 8 + c] = (int16_t)res;
        }
    }

    /* Step 2: Row IDCT -> signed 16-bit residuals */
    for (int r = 0; r < 8; r++) {
        idct1d_fixed_c(&tmp[r * 8], &dst_residual[r * 8]);
    }
}

void mpeg_mc_residual_add_c(const uint8_t *ref, const int16_t *residual, uint8_t *dst, int stride) {
    for (int r = 0; r < 8; r++) {
        for (int c = 0; c < 8; c++) {
            int32_t val = (int32_t)ref[r * stride + c] + (int32_t)residual[r * 8 + c];
            if (val < 0) val = 0;
            if (val > 255) val = 255;
            dst[r * stride + c] = (uint8_t)val;
        }
    }
}

void mpeg_idct8x8_fused_mc_c(const int16_t *src_raster, const uint8_t *ref, uint8_t *dst, int stride) {
    int16_t residual[MPEG_BLOCK_SIZE];
    mpeg_idct8x8_c(src_raster, residual);
    mpeg_mc_residual_add_c(ref, residual, dst, stride);
}

void mpeg_macroblock_mc_c(const int16_t block_residuals[6][64],
                          const uint8_t *ref_y, const uint8_t *ref_cb, const uint8_t *ref_cr,
                          uint8_t *dst_y, uint8_t *dst_cb, uint8_t *dst_cr,
                          int luma_stride, int chroma_stride) {
    /* Luma block 0: Top-Left (0, 0) */
    mpeg_idct8x8_fused_mc_c(block_residuals[0], ref_y, dst_y, luma_stride);

    /* Luma block 1: Top-Right (8, 0) */
    mpeg_idct8x8_fused_mc_c(block_residuals[1], ref_y + 8, dst_y + 8, luma_stride);

    /* Luma block 2: Bottom-Left (0, 8) */
    mpeg_idct8x8_fused_mc_c(block_residuals[2], ref_y + 8 * luma_stride, dst_y + 8 * luma_stride, luma_stride);

    /* Luma block 3: Bottom-Right (8, 8) */
    mpeg_idct8x8_fused_mc_c(block_residuals[3], ref_y + 8 * luma_stride + 8, dst_y + 8 * luma_stride + 8, luma_stride);

    /* Chroma Cb (8x8) */
    mpeg_idct8x8_fused_mc_c(block_residuals[4], ref_cb, dst_cb, chroma_stride);

    /* Chroma Cr (8x8) */
    mpeg_idct8x8_fused_mc_c(block_residuals[5], ref_cr, dst_cr, chroma_stride);
}
