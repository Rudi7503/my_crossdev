#ifndef MPEG_H
#define MPEG_H

#include <stdint.h>
#include <stddef.h>

#define MPEG_BLOCK_SIZE 64      /* 8x8 block */
#define MPEG_MB_Y_BLOCKS 4      /* 16x16 luma: 4 8x8 blocks */
#define MPEG_MB_C_BLOCKS 2      /* 8x8 chroma: 1 Cb, 1 Cr */
#define MPEG_TOTAL_BLOCKS 6     /* 6 blocks per 4:2:0 Macroblock */

/* Cycle Counter on Apollo 68080 */
uint32_t get_ccc(void);

/* Table Initialization */
void mpeg_init_tables(void);

/* C Reference Implementations */
void mpeg_idct8x8_c(const int16_t *src_raster, int16_t *dst_residual);
void mpeg_mc_residual_add_c(const uint8_t *ref, const int16_t *residual, uint8_t *dst, int stride);
void mpeg_idct8x8_fused_mc_c(const int16_t *src_raster, const uint8_t *ref, uint8_t *dst, int stride);
void mpeg_macroblock_mc_c(const int16_t block_residuals[6][64],
                          const uint8_t *ref_y, const uint8_t *ref_cb, const uint8_t *ref_cr,
                          uint8_t *dst_y, uint8_t *dst_cb, uint8_t *dst_cr,
                          int luma_stride, int chroma_stride);

/* 68080 Assembly & AMMX SIMD Implementations */
void mpeg_mc_residual_add_ammx(const uint8_t *ref, const int16_t *residual, uint8_t *dst, int stride);
void mpeg_idct8x8_fused_mc_ammx(const int16_t *src_raster, const uint8_t *ref, uint8_t *dst, int stride);
void mpeg_idct8x8_sparse_dc_mc_ammx(const int16_t *src_raster, const uint8_t *ref, uint8_t *dst, int stride);
void mpeg_idct8x8_sparse4x4_mc_ammx(const int16_t *src_raster, const uint8_t *ref, uint8_t *dst, int stride);
void mpeg_macroblock_mc_ammx(const int16_t block_residuals[6][64],
                            const uint8_t *ref_y, const uint8_t *ref_cb, const uint8_t *ref_cr,
                            uint8_t *dst_y, uint8_t *dst_cb, uint8_t *dst_cr,
                            int luma_stride, int chroma_stride);

#endif /* MPEG_H */
