#ifndef BLIT_TRANS_H
#define BLIT_TRANS_H

#include <stdint.h>
#include <stddef.h>

/* Performance Counters */
uint32_t get_ccc(void);
uint32_t read_iep1(void);
uint32_t read_iep2(void);
uint32_t read_iep3(void);
uint32_t read_dch(void);
uint32_t read_dcm(void);
uint32_t read_bpc(void);
uint32_t read_bpw(void);

typedef struct {
    uint32_t ccc;
    uint32_t iep1;
    uint32_t iep2;
    uint32_t iep3;
    uint32_t dch;
    uint32_t dcm;
    uint32_t bpc;
    uint32_t bpw;
} PerfSample;

/* Sprite Blitting Function Types */
typedef void (*blit_trans_fn)(const uint8_t *src, int src_stride,
                              uint8_t *dst, int dst_stride,
                              int width_pixels, int height_lines,
                              uint32_t key);

/* C Reference */
void blit_trans_8_c(const uint8_t *src, int src_stride, uint8_t *dst, int dst_stride, int width, int height, uint32_t key);
void blit_trans_16_c(const uint8_t *src, int src_stride, uint8_t *dst, int dst_stride, int width, int height, uint32_t key);
void blit_trans_32_c(const uint8_t *src, int src_stride, uint8_t *dst, int dst_stride, int width, int height, uint32_t key);

/* AMMX Variant 1: storeilm (pcmpeq + storeilm) */
void blit_trans_8_storeilm(const uint8_t *src, int src_stride, uint8_t *dst, int dst_stride, int width, int height, uint32_t key);
void blit_trans_16_storeilm(const uint8_t *src, int src_stride, uint8_t *dst, int dst_stride, int width, int height, uint32_t key);
void blit_trans_32_storeilm(const uint8_t *src, int src_stride, uint8_t *dst, int dst_stride, int width, int height, uint32_t key);

/* AMMX Variant 2: bsel (pcmpeq + bsel + store) */
void blit_trans_8_bsel(const uint8_t *src, int src_stride, uint8_t *dst, int dst_stride, int width, int height, uint32_t key);
void blit_trans_16_bsel(const uint8_t *src, int src_stride, uint8_t *dst, int dst_stride, int width, int height, uint32_t key);
void blit_trans_32_bsel(const uint8_t *src, int src_stride, uint8_t *dst, int dst_stride, int width, int height, uint32_t key);

/* AMMX Variant 3: storem (byte mask storem) for 8-bit */
void blit_trans_8_storem(const uint8_t *src, int src_stride, uint8_t *dst, int dst_stride, int width, int height, uint32_t key);

#endif /* BLIT_TRANS_H */
