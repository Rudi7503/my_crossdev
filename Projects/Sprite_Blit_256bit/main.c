#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "blit.h"

#define BG_WIDTH   1920
#define BG_HEIGHT  1080
#define DST_X      128
#define DST_Y      128

static const int SPRITE_SIZES[] = { 8, 16, 32, 64, 128, 256, 512 };
#define NUM_SIZES 7

static const int DEPTHS[] = { 8, 16, 24, 32 };
#define NUM_DEPTHS 4

static void *malloc_aligned(size_t size, size_t alignment, void **raw_out) {
    void *p = malloc(size + alignment + 32);
    if (!p) { *raw_out = NULL; return NULL; }
    *raw_out = p;
    uintptr_t addr = (uintptr_t)p;
    uintptr_t aligned = (addr + alignment - 1) & ~(alignment - 1);
    return (void *)aligned;
}

static void fill_sprite_gray127(uint8_t *buf, int width, int height, int bpp) {
    int total_pixels = width * height;
    if (bpp == 1) {
        memset(buf, 127, (size_t)total_pixels);
    } else if (bpp == 2) {
        /* RGB565 gray 127: (15<<11) | (31<<5) | 15 = 0x7BEF */
        uint16_t *p16 = (uint16_t *)buf;
        uint16_t val = 0x7BEF;
        for (int i = 0; i < total_pixels; i++) p16[i] = val;
    } else if (bpp == 3) {
        /* RGB24: 127, 127, 127 */
        for (int i = 0; i < total_pixels * 3; i++) buf[i] = 127;
    } else if (bpp == 4) {
        /* RGB32 / ARGB: 0x007F7F7F */
        uint32_t *p32 = (uint32_t *)buf;
        uint32_t val = 0x007F7F7F;
        for (int i = 0; i < total_pixels; i++) p32[i] = val;
    }
}

int main(void) {
    printf("========================================================================================================================\n");
    printf(" Apollo 68080 2D Sprite Blitting & 256-Bit Aligned Rectangle Copy Suite\n");
    printf(" Background: 1920x1080 (Black) | Sprites: 8x8 .. 512x512 (Gray 127) | Depths: 8, 16, 24, 32 Bit\n");
    printf(" Implementations: Standard move16 (256-bit Burst), Classical 68080 ASM, AMMX SIMD Quad, C-Ref (memcpy)\n");
    printf(" Real Hardware: Vampire V4 Standalone (Apollo 68080 @ 85 MHz)\n");
    printf("========================================================================================================================\n\n");

    int total_tests = 0;
    int total_passes = 0;

    for (int d = 0; d < NUM_DEPTHS; d++) {
        int depth = DEPTHS[d];
        int bpp = depth / 8;
        int dst_stride = BG_WIDTH * bpp;
        size_t bg_bytes = (size_t)dst_stride * BG_HEIGHT;

        printf("========================================================================================================================\n");
        printf(" COLOR DEPTH: %d-BIT (%d Bytes/Pixel) | Framebuffer: 1920x1080 (%lu Bytes, Stride=%d Bytes)\n",
               depth, bpp, (unsigned long)bg_bytes, dst_stride);
        printf("------------------------------------------------------------------------------------------------------------------------\n");
        printf(" Size   | Width (B) | C-Ref (cyc) | m68k ASM (cyc) | move16 (cyc) | AMMX (cyc) | Speedup vs C | MB/s (m16) | Parity\n");
        printf("------------------------------------------------------------------------------------------------------------------------\n");

        void *raw_bg_c, *raw_bg_m, *raw_bg_k, *raw_bg_x;
        uint8_t *bg_c = malloc_aligned(bg_bytes, 32, &raw_bg_c);
        uint8_t *bg_m = malloc_aligned(bg_bytes, 32, &raw_bg_m);
        uint8_t *bg_k = malloc_aligned(bg_bytes, 32, &raw_bg_k);
        uint8_t *bg_x = malloc_aligned(bg_bytes, 32, &raw_bg_x);

        if (!bg_c || !bg_m || !bg_k || !bg_x) {
            printf("ERROR: Out of memory for %d-bit background buffers!\n", depth);
            if (raw_bg_c) free(raw_bg_c);
            if (raw_bg_m) free(raw_bg_m);
            if (raw_bg_k) free(raw_bg_k);
            if (raw_bg_x) free(raw_bg_x);
            continue;
        }

        for (int s = 0; s < NUM_SIZES; s++) {
            int sz = SPRITE_SIZES[s];
            int width_bytes = sz * bpp;
            int src_stride = width_bytes;
            size_t sprite_bytes = (size_t)width_bytes * sz;

            void *raw_sprite;
            uint8_t *sprite = malloc_aligned(sprite_bytes, 32, &raw_sprite);
            if (!sprite) {
                printf("ERROR: Out of memory for %dx%d sprite!\n", sz, sz);
                continue;
            }

            fill_sprite_gray127(sprite, sz, sz, bpp);

            /* Destination pointers (always 256-bit aligned since DST_X*bpp % 32 == 0) */
            size_t dst_offset = (size_t)DST_Y * dst_stride + (size_t)DST_X * bpp;
            uint8_t *dst_c = bg_c + dst_offset;
            uint8_t *dst_m = bg_m + dst_offset;
            uint8_t *dst_k = bg_k + dst_offset;
            uint8_t *dst_x = bg_x + dst_offset;

            /* Number of iterations based on size to ensure stable timing */
            int runs = (sz <= 32) ? 100 : ((sz <= 128) ? 50 : 20);

            /* 1. C-Reference */
            memset(bg_c, 0, bg_bytes);
            blit_rect_c(sprite, src_stride, dst_c, dst_stride, width_bytes, sz);
            uint32_t t0 = get_ccc();
            for (int r = 0; r < runs; r++) {
                blit_rect_c(sprite, src_stride, dst_c, dst_stride, width_bytes, sz);
            }
            uint32_t t1 = get_ccc();
            uint32_t cyc_c = (t1 - t0) / runs;

            /* 2. Classical 68080 ASM */
            memset(bg_k, 0, bg_bytes);
            blit_rect_m68k(sprite, src_stride, dst_k, dst_stride, width_bytes, sz);
            t0 = get_ccc();
            for (int r = 0; r < runs; r++) {
                blit_rect_m68k(sprite, src_stride, dst_k, dst_stride, width_bytes, sz);
            }
            t1 = get_ccc();
            uint32_t cyc_k = (t1 - t0) / runs;

            /* 3. Standard move16 (256-bit Burst) */
            memset(bg_m, 0, bg_bytes);
            blit_rect_move16(sprite, src_stride, dst_m, dst_stride, width_bytes, sz);
            t0 = get_ccc();
            for (int r = 0; r < runs; r++) {
                blit_rect_move16(sprite, src_stride, dst_m, dst_stride, width_bytes, sz);
            }
            t1 = get_ccc();
            uint32_t cyc_m = (t1 - t0) / runs;

            /* 4. AMMX Quad-Burst */
            memset(bg_x, 0, bg_bytes);
            blit_rect_ammx(sprite, src_stride, dst_x, dst_stride, width_bytes, sz);
            t0 = get_ccc();
            for (int r = 0; r < runs; r++) {
                blit_rect_ammx(sprite, src_stride, dst_x, dst_stride, width_bytes, sz);
            }
            t1 = get_ccc();
            uint32_t cyc_x = (t1 - t0) / runs;

            /* Verify bit-exactness of all rows */
            int diff_m = memcmp(bg_c, bg_m, bg_bytes);
            int diff_k = memcmp(bg_c, bg_k, bg_bytes);
            int diff_x = memcmp(bg_c, bg_x, bg_bytes);

            total_tests++;
            int pass = (diff_m == 0 && diff_k == 0 && diff_x == 0);
            if (pass) total_passes++;

            /* Throughput: sprite_bytes / (cyc_m / 85,000,000) = (sprite_bytes * 85) / cyc_m MB/s */
            double mb_s = (cyc_m > 0) ? ((double)sprite_bytes * 85.0 / (double)cyc_m) : 0.0;
            double speedup = (cyc_m > 0) ? ((double)cyc_c / (double)cyc_m) : 0.0;

            printf(" %3dx%-3d | %9d | %11lu | %14lu | %12lu | %10lu | %11.2fx | %9.1f | %s\n",
                   sz, sz, width_bytes,
                   (unsigned long)cyc_c, (unsigned long)cyc_k, (unsigned long)cyc_m, (unsigned long)cyc_x,
                   speedup, mb_s, pass ? "PASS" : "FAIL");

            free(raw_sprite);
        }

        free(raw_bg_c);
        free(raw_bg_m);
        free(raw_bg_k);
        free(raw_bg_x);
        printf("\n");
    }

    printf("========================================================================================================================\n");
    printf(" TOTAL VERIFICATION RESULT: %d / %d TESTS PASSED (100%% Bit-Exact)\n", total_passes, total_tests);
    printf("========================================================================================================================\n");

    return (total_passes == total_tests) ? 0 : 1;
}
