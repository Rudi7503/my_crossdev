#ifndef H264_H
#define H264_H

#include <stdint.h>
#include <stddef.h>

/* Cycle Counter on Apollo 68080 */
uint32_t get_ccc(void);

/* C Reference Implementations */
void h264_hadamard4x4_c(const int16_t in[4][4], int16_t out[4][4]);
void h264_idct4x4_core_c(const int16_t in[4][4], int16_t out[4][4]);
void h264_idct4x4_fused_mc_c(const int16_t in[4][4], const uint8_t *ref, uint8_t *dst, int stride);
void h264_macroblock_16x16_c(const int16_t blocks[16][16], const uint8_t *ref, uint8_t *dst, int stride);

/* 68080 Assembly & AMMX SIMD Implementations */
void h264_hadamard4x4_ammx(const int16_t in[4][4], int16_t out[4][4]);
void h264_idct4x4_core_asm(const int16_t in[4][4], int16_t out[4][4]);
void h264_idct4x4_fused_mc_asm(const int16_t in[4][4], const uint8_t *ref, uint8_t *dst, int stride);
void h264_idct4x4_sparse_dc_mc_asm(const int16_t in[4][4], const uint8_t *ref, uint8_t *dst, int stride);
void h264_macroblock_16x16_asm(const int16_t blocks[16][16], const uint8_t *ref, uint8_t *dst, int stride);

#endif /* H264_H */
