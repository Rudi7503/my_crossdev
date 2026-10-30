#include "blit_trans.h"

void blit_trans_8_c(const uint8_t *src, int src_stride,
                    uint8_t *dst, int dst_stride,
                    int width, int height,
                    uint32_t key) {
    uint8_t k = (uint8_t)key;
    for (int y = 0; y < height; y++) {
        const uint8_t *s = src + y * src_stride;
        uint8_t *d = dst + y * dst_stride;
        for (int x = 0; x < width; x++) {
            uint8_t val = s[x];
            if (val != k) {
                d[x] = val;
            }
        }
    }
}

void blit_trans_16_c(const uint8_t *src, int src_stride,
                     uint8_t *dst, int dst_stride,
                     int width, int height,
                     uint32_t key) {
    uint16_t k = (uint16_t)key;
    for (int y = 0; y < height; y++) {
        const uint16_t *s = (const uint16_t *)(src + y * src_stride);
        uint16_t *d = (uint16_t *)(dst + y * dst_stride);
        for (int x = 0; x < width; x++) {
            uint16_t val = s[x];
            if (val != k) {
                d[x] = val;
            }
        }
    }
}

void blit_trans_32_c(const uint8_t *src, int src_stride,
                     uint8_t *dst, int dst_stride,
                     int width, int height,
                     uint32_t key) {
    uint32_t k = key;
    for (int y = 0; y < height; y++) {
        const uint32_t *s = (const uint32_t *)(src + y * src_stride);
        uint32_t *d = (uint32_t *)(dst + y * dst_stride);
        for (int x = 0; x < width; x++) {
            uint32_t val = s[x];
            if (val != k) {
                d[x] = val;
            }
        }
    }
}
