#ifndef JPEG_H
#define JPEG_H

#include <stdint.h>
#include <stddef.h>

#define JPEG_BLOCK_SIZE  64

/* Standard JPEG Zig-Zag order mapping raster index [0..63] -> zig-zag position */
extern const uint8_t jpeg_zigzag_order[64];

/* Standard JPEG Luminance & Chrominance Quantization Tables */
extern const uint16_t jpeg_luma_quant[64];
extern const uint16_t jpeg_chroma_quant[64];

/* Table initialization & precomputed scaled reciprocal quantizers */
void jpeg_init_tables(void);

/* C Reference Implementations */
void dequant_zigzag_c(const int16_t *src_zz, const uint16_t *qtable, int16_t *dst_raster64);
void idct8x8_levelshift_clamp_c(const int16_t *src_raster64, uint8_t *dst_pixels64);
void ycbcr_to_rgb565_c(const uint8_t *y64, const uint8_t *cb64, const uint8_t *cr64, uint16_t *rgb565_64);
void ycbcr_to_argb32_c(const uint8_t *y64, const uint8_t *cb64, const uint8_t *cr64, uint32_t *argb32_64);
void jpeg_pipeline_mcu_c(const int16_t *y_zz, const int16_t *cb_zz, const int16_t *cr_zz,
                         const uint16_t *qy, const uint16_t *qc, uint16_t *rgb565_out);

/* 68080 Assembly & AMMX SIMD Implementations */
void dequant_zigzag_ammx(const int16_t *src_zz, const uint16_t *qtable, int16_t *dst_raster64);
void idct8x8_levelshift_packuswb_ammx(const int16_t *src_raster64, uint8_t *dst_pixels64);
void ycbcr_to_rgb565_ammx(const uint8_t *y64, const uint8_t *cb64, const uint8_t *cr64, uint16_t *rgb565_64);
void ycbcr_to_argb32_ammx(const uint8_t *y64, const uint8_t *cb64, const uint8_t *cr64, uint32_t *argb32_64);
void jpeg_pipeline_mcu_ammx(const int16_t *y_zz, const int16_t *cb_zz, const int16_t *cr_zz,
                            const uint16_t *qy, const uint16_t *qc, uint16_t *rgb565_out);

/* Hardware Cycle Counter */
uint32_t get_ccc(void);

#endif /* JPEG_H */
