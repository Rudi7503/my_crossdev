#ifndef TILED_H
#define TILED_H

#include <stdint.h>
#include <stddef.h>

#define BG_WIDTH   1920
#define BG_HEIGHT  1080

uint32_t get_ccc(void);

/* C Reference Tiled Copy */
void tile_screen_c(const uint8_t *sprite, int sprite_w, int sprite_h, int bpp,
                   uint8_t *dst_bg, int start_x, int start_y);

/* Implementation 1: Discrete Tile Grid via move16 */
void tile_screen_discrete_move16(const uint8_t *sprite, int sprite_w, int sprite_h, int bpp,
                                uint8_t *dst_bg, int start_x, int start_y);

/* Implementation 2: Optimized Scanline Tiled move16 */
void tile_screen_scanline_move16(const uint8_t *sprite, int sprite_w, int sprite_h, int bpp,
                                uint8_t *dst_bg, int start_x, int start_y);

/* Implementation 3: Optimized Alignment-Peeled move16 (Aligns dst to 256-bit cacheline) */
void tile_screen_peeled_move16(const uint8_t *sprite, int sprite_w, int sprite_h, int bpp,
                              uint8_t *dst_bg, int start_x, int start_y);

#endif /* TILED_H */
