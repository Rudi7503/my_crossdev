#include "opt.h"

/* ========================================================================= */
/* 1. H.264 Core 4x4 IDCT C Reference                                      */
/* ========================================================================= */
void h264_idct4x4_c(const int16_t in[4][4], int16_t out[4][4]) {
    int16_t tmp[4][4];
    int i;

    /* Horizontal inverse transform */
    for (i = 0; i < 4; i++) {
        int z0 = in[i][0];
        int z1 = in[i][1];
        int z2 = in[i][2];
        int z3 = in[i][3];

        int s0 = z0 + z2;
        int s1 = z0 - z2;
        int s2 = (z1 >> 1) - z3;
        int s3 = z1 + (z3 >> 1);

        tmp[i][0] = s0 + s3;
        tmp[i][1] = s1 + s2;
        tmp[i][2] = s1 - s2;
        tmp[i][3] = s0 - s3;
    }

    /* Vertical inverse transform */
    for (i = 0; i < 4; i++) {
        int t0 = tmp[0][i];
        int t1 = tmp[1][i];
        int t2 = tmp[2][i];
        int t3 = tmp[3][i];

        int v0 = t0 + t2;
        int v1 = t0 - t2;
        int v2 = (t1 >> 1) - t3;
        int v3 = t1 + (t3 >> 1);

        out[0][i] = (v0 + v3 + 32) >> 6;
        out[1][i] = (v1 + v2 + 32) >> 6;
        out[2][i] = (v1 - v2 + 32) >> 6;
        out[3][i] = (v0 - v3 + 32) >> 6;
    }
}

/* ========================================================================= */
/* 2. MPEG / H.264 Half-Pel Motion Compensation C Reference                */
/* ========================================================================= */
void mc_halfpel_h_c(const uint8_t *ref, int stride, uint8_t *dst) {
    int y, x;
    for (y = 0; y < 8; y++) {
        const uint8_t *r = ref + y * stride;
        uint8_t *d = dst + y * 8;
        for (x = 0; x < 8; x++) {
            d[x] = (uint8_t)((r[x] + r[x + 1] + 1) >> 1);
        }
    }
}

void mc_halfpel_v_c(const uint8_t *ref, int stride, uint8_t *dst) {
    int y, x;
    for (y = 0; y < 8; y++) {
        const uint8_t *r0 = ref + y * stride;
        const uint8_t *r1 = ref + (y + 1) * stride;
        uint8_t *d = dst + y * 8;
        for (x = 0; x < 8; x++) {
            d[x] = (uint8_t)((r0[x] + r1[x] + 1) >> 1);
        }
    }
}

void mc_halfpel_hv_c(const uint8_t *ref, int stride, uint8_t *dst) {
    int y, x;
    for (y = 0; y < 8; y++) {
        const uint8_t *r0 = ref + y * stride;
        const uint8_t *r1 = ref + (y + 1) * stride;
        uint8_t *d = dst + y * 8;
        for (x = 0; x < 8; x++) {
            d[x] = (uint8_t)((r0[x] + r0[x + 1] + r1[x] + r1[x + 1] + 2) >> 2);
        }
    }
}

/* ========================================================================= */
/* 3. Color Space Conversion (YCbCr 4:2:0 -> 32-Bit ARGB) C Reference       */
/* ========================================================================= */
static inline uint8_t clamp8(int val) {
    if (val < 0) return 0;
    if (val > 255) return 255;
    return (uint8_t)val;
}

void yuv420_to_rgb32_c(const uint8_t *y_plane, const uint8_t *u_plane, const uint8_t *v_plane,
                       int stride_y, int stride_uv, uint32_t *dst, int dst_stride) {
    int y, x;
    for (y = 0; y < 8; y++) {
        const uint8_t *yp = y_plane + y * stride_y;
        const uint8_t *up = u_plane + (y >> 1) * stride_uv;
        const uint8_t *vp = v_plane + (y >> 1) * stride_uv;
        uint32_t *dp = dst + y * dst_stride;

        for (x = 0; x < 8; x++) {
            int y_val = yp[x];
            int u_val = up[x >> 1] - 128;
            int v_val = vp[x >> 1] - 128;

            /* Fixed-point coefficients (scaled by 256 for fast scalar integer arithmetic) */
            int r = y_val + ((359 * v_val) >> 8);
            int g = y_val - ((88 * u_val + 183 * v_val) >> 8);
            int b = y_val + ((454 * u_val) >> 8);

            dp[x] = (0xFF000000) | (clamp8(r) << 16) | (clamp8(g) << 8) | clamp8(b);
        }
    }
}

/* ========================================================================= */
/* 4. SAGA Native Direct YUV Planar Store (Zero CPU Color Conversion Cost!)  */
/* ========================================================================= */
void yuv420_saga_direct_store(const uint8_t *y_plane, const uint8_t *u_plane, const uint8_t *v_plane,
                              int stride_y, int stride_uv, uint8_t *dst_y, uint8_t *dst_u, uint8_t *dst_v,
                              int dst_stride_y, int dst_stride_uv) {
    int y, x;
    /* Direct copy of Y plane */
    for (y = 0; y < 8; y++) {
        const uint8_t *sy = y_plane + y * stride_y;
        uint8_t *dy = dst_y + y * dst_stride_y;
        for (x = 0; x < 8; x++) {
            dy[x] = sy[x];
        }
    }
    /* Direct copy of subsampled U/V planes (4x4) */
    for (y = 0; y < 4; y++) {
        const uint8_t *su = u_plane + y * stride_uv;
        const uint8_t *sv = v_plane + y * stride_uv;
        uint8_t *du = dst_u + y * dst_stride_uv;
        uint8_t *dv = dst_v + y * dst_stride_uv;
        for (x = 0; x < 4; x++) {
            du[x] = su[x];
            dv[x] = sv[x];
        }
    }
}
