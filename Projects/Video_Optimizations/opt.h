#ifndef OPT_H
#define OPT_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

uint32_t get_ccc(void);

/* ========================================================================= */
/* 1. H.264 Core IDCT (4x4) Benchmark                                      */
/* ========================================================================= */
void h264_idct4x4_c(const int16_t in[4][4], int16_t out[4][4]);
void h264_idct4x4_asm(const int16_t in[4][4], int16_t out[4][4]);
void h264_idct4x4_unrolled(const int16_t in[4][4], int16_t out[4][4]);

/* ========================================================================= */
/* 2. MPEG / H.264 Half-Pel Motion Compensation with PAVGB                 */
/* ========================================================================= */
/* Computes an 8x8 block prediction with half-pel subpixel interpolation     */
void mc_halfpel_h_c(const uint8_t *ref, int stride, uint8_t *dst);
void mc_halfpel_v_c(const uint8_t *ref, int stride, uint8_t *dst);
void mc_halfpel_hv_c(const uint8_t *ref, int stride, uint8_t *dst);

void mc_halfpel_h_ammx(const uint8_t *ref, int stride, uint8_t *dst);
void mc_halfpel_v_ammx(const uint8_t *ref, int stride, uint8_t *dst);
void mc_halfpel_hv_ammx(const uint8_t *ref, int stride, uint8_t *dst);

/* ========================================================================= */
/* 3. Color Space Conversion (YCbCr -> RGB32 / RGB16 vs SAGA Native YUV)    */
/* ========================================================================= */
/* Converts 64 pixels (8x8 block) of YCbCr 4:2:0 to 32-bit ARGB             */
void yuv420_to_rgb32_c(const uint8_t *y, const uint8_t *u, const uint8_t *v,
                       int stride_y, int stride_uv, uint32_t *dst, int dst_stride);
void yuv420_to_rgb32_ammx(const uint8_t *y, const uint8_t *u, const uint8_t *v,
                          int stride_y, int stride_uv, uint32_t *dst, int dst_stride);

/* SAGA Native Direct YUV Planar / Packed Store (Zero CPU conversion cost) */
void yuv420_saga_direct_store(const uint8_t *y, const uint8_t *u, const uint8_t *v,
                              int stride_y, int stride_uv, uint8_t *dst_y, uint8_t *dst_u, uint8_t *dst_v,
                              int dst_stride_y, int dst_stride_uv);

/* ========================================================================= */
/* 4. Sequential Prefetching (TOUCH)                                       */
/* ========================================================================= */
void mc_block_noprefetch(const uint8_t *ref, int stride, uint8_t *dst);
void mc_block_prefetch(const uint8_t *ref, int stride, uint8_t *dst, const uint8_t *next_ref);

#ifdef __cplusplus
}
#endif

#endif /* OPT_H */
