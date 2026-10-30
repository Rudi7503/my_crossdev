#ifndef BLIT_H
#define BLIT_H

#include <stdint.h>
#include <stddef.h>

/* Cycle Counter on Apollo 68080 */
uint32_t get_ccc(void);

/* C Reference Blitter */
void blit_rect_c(const uint8_t *src, int src_stride,
                 uint8_t *dst, int dst_stride,
                 int width_bytes, int height_lines);

/* 68080 move16 (256-bit Burst: 2x move16 per 32 bytes) */
void blit_rect_move16(const uint8_t *src, int src_stride,
                      uint8_t *dst, int dst_stride,
                      int width_bytes, int height_lines);

/* Classical 68080 ASM (movem.l / move.l) */
void blit_rect_m68k(const uint8_t *src, int src_stride,
                    uint8_t *dst, int dst_stride,
                    int width_bytes, int height_lines);

/* AMMX 256-bit Vector SIMD (load / store e0-e3) */
void blit_rect_ammx(const uint8_t *src, int src_stride,
                    uint8_t *dst, int dst_stride,
                    int width_bytes, int height_lines);

#endif /* BLIT_H */
