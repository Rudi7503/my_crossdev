#include <stdint.h>
#include <string.h>
#include "blit.h"

void blit_rect_c(const uint8_t *src, int src_stride,
                 uint8_t *dst, int dst_stride,
                 int width_bytes, int height_lines) {
    for (int y = 0; y < height_lines; y++) {
        memcpy(dst, src, (size_t)width_bytes);
        src += src_stride;
        dst += dst_stride;
    }
}
