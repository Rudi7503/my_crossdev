#include <stdint.h>
#include <string.h>
#include "tiled.h"

extern void blit_rect_m16(const uint8_t *src, int src_stride,
                          uint8_t *dst, int dst_stride,
                          int width_bytes, int height_lines);

extern void copy_line_m16(const uint8_t *src, uint8_t *dst, int count_bytes);
extern void copy_line_peeled_m16(const uint8_t *src, uint8_t *dst, int count_bytes);

/* C Reference */
void tile_screen_c(const uint8_t *sprite, int sprite_w, int sprite_h, int bpp,
                   uint8_t *dst_bg, int start_x, int start_y) {
    int sprite_stride = sprite_w * bpp;
    int dst_stride = BG_WIDTH * bpp;

    for (int y = start_y; y < BG_HEIGHT; y++) {
        int sy = (y - start_y) % sprite_h;
        const uint8_t *src_row = sprite + sy * sprite_stride;
        uint8_t *dst_row = dst_bg + y * dst_stride;

        int x = start_x;
        while (x < BG_WIDTH) {
            int sx = (x - start_x) % sprite_w;
            int copy_pixels = sprite_w - sx;
            if (x + copy_pixels > BG_WIDTH) {
                copy_pixels = BG_WIDTH - x;
            }
            memcpy(dst_row + x * bpp, src_row + sx * bpp, (size_t)(copy_pixels * bpp));
            x += copy_pixels;
        }
    }
}

/* Implementation 1: Discrete Tile Grid via move16 */
void tile_screen_discrete_move16(const uint8_t *sprite, int sprite_w, int sprite_h, int bpp,
                                uint8_t *dst_bg, int start_x, int start_y) {
    int sprite_stride = sprite_w * bpp;
    int dst_stride = BG_WIDTH * bpp;

    for (int ty = start_y; ty < BG_HEIGHT; ty += sprite_h) {
        int h = (ty + sprite_h <= BG_HEIGHT) ? sprite_h : (BG_HEIGHT - ty);
        for (int tx = start_x; tx < BG_WIDTH; tx += sprite_w) {
            int w = (tx + sprite_w <= BG_WIDTH) ? sprite_w : (BG_WIDTH - tx);
            uint8_t *dst_tile = dst_bg + ty * dst_stride + tx * bpp;
            blit_rect_m16(sprite, sprite_stride, dst_tile, dst_stride, w * bpp, h);
        }
    }
}

/* Implementation 2: Optimized Scanline Tiled move16 */
void tile_screen_scanline_move16(const uint8_t *sprite, int sprite_w, int sprite_h, int bpp,
                                uint8_t *dst_bg, int start_x, int start_y) {
    int sprite_stride = sprite_w * bpp;
    int dst_stride = BG_WIDTH * bpp;

    for (int y = start_y; y < BG_HEIGHT; y++) {
        int sy = (y - start_y) % sprite_h;
        const uint8_t *src_row = sprite + sy * sprite_stride;
        uint8_t *dst_row = dst_bg + y * dst_stride;

        int x = start_x;
        while (x < BG_WIDTH) {
            int sx = (x - start_x) % sprite_w;
            int copy_pixels = sprite_w - sx;
            if (x + copy_pixels > BG_WIDTH) {
                copy_pixels = BG_WIDTH - x;
            }
            copy_line_m16(src_row + sx * bpp, dst_row + x * bpp, copy_pixels * bpp);
            x += copy_pixels;
        }
    }
}

/* Implementation 3: Alignment-Peeled move16 (256-bit cacheline-aligned writes) */
void tile_screen_peeled_move16(const uint8_t *sprite, int sprite_w, int sprite_h, int bpp,
                              uint8_t *dst_bg, int start_x, int start_y) {
    int sprite_stride = sprite_w * bpp;
    int dst_stride = BG_WIDTH * bpp;

    for (int y = start_y; y < BG_HEIGHT; y++) {
        int sy = (y - start_y) % sprite_h;
        const uint8_t *src_row = sprite + sy * sprite_stride;
        uint8_t *dst_row = dst_bg + y * dst_stride;

        int x = start_x;
        while (x < BG_WIDTH) {
            int sx = (x - start_x) % sprite_w;
            int copy_pixels = sprite_w - sx;
            if (x + copy_pixels > BG_WIDTH) {
                copy_pixels = BG_WIDTH - x;
            }
            copy_line_peeled_m16(src_row + sx * bpp, dst_row + x * bpp, copy_pixels * bpp);
            x += copy_pixels;
        }
    }
}
