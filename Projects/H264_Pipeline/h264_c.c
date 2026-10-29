#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "h264.h"

/* 4x4 2D Hadamard Transform (ITU-T H.264 Section 8.5.6 / 8.5.7) */
void h264_hadamard4x4_c(const int16_t in[4][4], int16_t out[4][4]) {
    int16_t tmp[4][4];

    /* Horizontal pass */
    for (int r = 0; r < 4; r++) {
        int32_t p0 = (int32_t)in[r][0] + in[r][3];
        int32_t p1 = (int32_t)in[r][1] + in[r][2];
        int32_t p2 = (int32_t)in[r][1] - in[r][2];
        int32_t p3 = (int32_t)in[r][0] - in[r][3];

        tmp[r][0] = (int16_t)(p0 + p1);
        tmp[r][1] = (int16_t)(p3 + p2);
        tmp[r][2] = (int16_t)(p0 - p1);
        tmp[r][3] = (int16_t)(p3 - p2);
    }

    /* Vertical pass */
    for (int c = 0; c < 4; c++) {
        int32_t p0 = (int32_t)tmp[0][c] + tmp[3][c];
        int32_t p1 = (int32_t)tmp[1][c] + tmp[2][c];
        int32_t p2 = (int32_t)tmp[1][c] - tmp[2][c];
        int32_t p3 = (int32_t)tmp[0][c] - tmp[3][c];

        out[0][c] = (int16_t)(p0 + p1);
        out[1][c] = (int16_t)(p3 + p2);
        out[2][c] = (int16_t)(p0 - p1);
        out[3][c] = (int16_t)(p3 - p2);
    }
}

/* 4x4 Core Inverse Integer Transform (ITU-T H.264 Section 8.5.8) */
void h264_idct4x4_core_c(const int16_t in[4][4], int16_t out[4][4]) {
    int32_t tmp[4][4];

    /* Step 1: Horizontal IDCT across rows */
    for (int r = 0; r < 4; r++) {
        int32_t z0 = in[r][0];
        int32_t z1 = in[r][1];
        int32_t z2 = in[r][2];
        int32_t z3 = in[r][3];

        int32_t s0 = z0 + z2;
        int32_t s1 = z0 - z2;
        int32_t s2 = (z1 >> 1) - z3;
        int32_t s3 = z1 + (z3 >> 1);

        tmp[r][0] = s0 + s3;
        tmp[r][1] = s1 + s2;
        tmp[r][2] = s1 - s2;
        tmp[r][3] = s0 - s3;
    }

    /* Step 2: Vertical IDCT down columns + normalisation (+32) >> 6 */
    for (int c = 0; c < 4; c++) {
        int32_t t0 = tmp[0][c];
        int32_t t1 = tmp[1][c];
        int32_t t2 = tmp[2][c];
        int32_t t3 = tmp[3][c];

        int32_t v0 = t0 + t2;
        int32_t v1 = t0 - t2;
        int32_t v2 = (t1 >> 1) - t3;
        int32_t v3 = t1 + (t3 >> 1);

        out[0][c] = (int16_t)((v0 + v3 + 32) >> 6);
        out[1][c] = (int16_t)((v1 + v2 + 32) >> 6);
        out[2][c] = (int16_t)((v1 - v2 + 32) >> 6);
        out[3][c] = (int16_t)((v0 - v3 + 32) >> 6);
    }
}

void h264_idct4x4_fused_mc_c(const int16_t in[4][4], const uint8_t *ref, uint8_t *dst, int stride) {
    int16_t res[4][4];
    h264_idct4x4_core_c(in, res);

    for (int r = 0; r < 4; r++) {
        for (int c = 0; c < 4; c++) {
            int32_t val = (int32_t)ref[r * stride + c] + (int32_t)res[r][c];
            if (val < 0) val = 0;
            if (val > 255) val = 255;
            dst[r * stride + c] = (uint8_t)val;
        }
    }
}

void h264_macroblock_16x16_c(const int16_t blocks[16][16], const uint8_t *ref, uint8_t *dst, int stride) {
    /* 16 4x4 blocks in 4x4 raster grid */
    for (int by = 0; by < 4; by++) {
        for (int bx = 0; bx < 4; bx++) {
            int b_idx = by * 4 + bx;
            const uint8_t *b_ref = ref + (by * 4) * stride + (bx * 4);
            uint8_t *b_dst = dst + (by * 4) * stride + (bx * 4);
            h264_idct4x4_fused_mc_c((const int16_t (*)[4])&blocks[b_idx][0], b_ref, b_dst, stride);
        }
    }
}
