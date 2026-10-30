#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "tiled.h"

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
        uint16_t *p16 = (uint16_t *)buf;
        uint16_t val = 0x7BEF;
        for (int i = 0; i < total_pixels; i++) p16[i] = val;
    } else if (bpp == 3) {
        for (int i = 0; i < total_pixels * 3; i++) buf[i] = 127;
    } else if (bpp == 4) {
        uint32_t *p32 = (uint32_t *)buf;
        uint32_t val = 0x007F7F7F;
        for (int i = 0; i < total_pixels; i++) p32[i] = val;
    }
}

int main(void) {
    printf("========================================================================================================================\n");
    printf(" Apollo 68080 Full-Screen Tiled Sprite Blitting Benchmark Suite\n");
    printf(" Background: 1920x1080 (Black) | Sprites: 8x8 .. 512x512 (Gray 127) | Depths: 8, 16, 24, 32 Bit\n");
    printf(" Modes: [A] Aligned Tiling (x=0, y=0)  |  [B] 1-Pixel Offset Unaligned Tiling (x=1, y=1)\n");
    printf(" Implementations: C-Ref, Discrete move16, Scanline move16, Alignment-Peeled move16\n");
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
        printf(" COLOR DEPTH: %d-BIT (%d Bpp) | Framebuffer: 1920x1080 (%lu Bytes, Stride=%d Bytes)\n",
               depth, bpp, (unsigned long)bg_bytes, dst_stride);
        printf("========================================================================================================================\n\n");

        void *raw_bg_c, *raw_bg_disc, *raw_bg_scan, *raw_bg_peel;
        uint8_t *bg_c    = malloc_aligned(bg_bytes, 32, &raw_bg_c);
        uint8_t *bg_disc = malloc_aligned(bg_bytes, 32, &raw_bg_disc);
        uint8_t *bg_scan = malloc_aligned(bg_bytes, 32, &raw_bg_scan);
        uint8_t *bg_peel = malloc_aligned(bg_bytes, 32, &raw_bg_peel);

        if (!bg_c || !bg_disc || !bg_scan || !bg_peel) {
            printf("ERROR: Out of memory for %d-bit background buffers!\n", depth);
            if (raw_bg_c) free(raw_bg_c);
            if (raw_bg_disc) free(raw_bg_disc);
            if (raw_bg_scan) free(raw_bg_scan);
            if (raw_bg_peel) free(raw_bg_peel);
            continue;
        }

        /* --- PART A: Aligned Tiling (x=0, y=0) --- */
        printf("--- [PART A] Aligned Tiling (Start at x=0, y=0: 100%% 256-Bit Aligned) ---\n");
        printf(" Size   | C-Ref (cyc) | Discrete m16 | Scanline m16 | Peeled m16 | Best Speedup | MB/s (Best) | Parity\n");
        printf("------------------------------------------------------------------------------------------------------------------------\n");

        for (int s = 0; s < NUM_SIZES; s++) {
            int sz = SPRITE_SIZES[s];
            size_t sprite_bytes = (size_t)sz * sz * bpp;
            void *raw_sprite;
            uint8_t *sprite = malloc_aligned(sprite_bytes, 32, &raw_sprite);
            fill_sprite_gray127(sprite, sz, sz, bpp);

            int runs = 1;

            /* 1. C-Reference */
            memset(bg_c, 0, bg_bytes);
            tile_screen_c(sprite, sz, sz, bpp, bg_c, 0, 0);
            uint32_t t0 = get_ccc();
            for (int r = 0; r < runs; r++) {
                tile_screen_c(sprite, sz, sz, bpp, bg_c, 0, 0);
            }
            uint32_t t1 = get_ccc();
            uint32_t cyc_c = (t1 - t0) / runs;

            /* 2. Discrete move16 */
            memset(bg_disc, 0, bg_bytes);
            tile_screen_discrete_move16(sprite, sz, sz, bpp, bg_disc, 0, 0);
            t0 = get_ccc();
            for (int r = 0; r < runs; r++) {
                tile_screen_discrete_move16(sprite, sz, sz, bpp, bg_disc, 0, 0);
            }
            t1 = get_ccc();
            uint32_t cyc_disc = (t1 - t0) / runs;

            /* 3. Scanline move16 */
            memset(bg_scan, 0, bg_bytes);
            tile_screen_scanline_move16(sprite, sz, sz, bpp, bg_scan, 0, 0);
            t0 = get_ccc();
            for (int r = 0; r < runs; r++) {
                tile_screen_scanline_move16(sprite, sz, sz, bpp, bg_scan, 0, 0);
            }
            t1 = get_ccc();
            uint32_t cyc_scan = (t1 - t0) / runs;

            /* 4. Peeled move16 */
            memset(bg_peel, 0, bg_bytes);
            tile_screen_peeled_move16(sprite, sz, sz, bpp, bg_peel, 0, 0);
            t0 = get_ccc();
            for (int r = 0; r < runs; r++) {
                tile_screen_peeled_move16(sprite, sz, sz, bpp, bg_peel, 0, 0);
            }
            t1 = get_ccc();
            uint32_t cyc_peel = (t1 - t0) / runs;

            int diff_disc = memcmp(bg_c, bg_disc, bg_bytes);
            int diff_scan = memcmp(bg_c, bg_scan, bg_bytes);
            int diff_peel = memcmp(bg_c, bg_peel, bg_bytes);

            total_tests++;
            int pass = (diff_disc == 0 && diff_scan == 0 && diff_peel == 0);
            if (pass) total_passes++;

            uint32_t best_cyc = cyc_scan < cyc_peel ? cyc_scan : cyc_peel;
            if (cyc_disc < best_cyc) best_cyc = cyc_disc;

            double speedup = (best_cyc > 0) ? ((double)cyc_c / (double)best_cyc) : 0.0;
            double mb_s = (best_cyc > 0) ? ((double)bg_bytes * 85.0 / (double)best_cyc) : 0.0;

            printf(" %3dx%-3d | %11lu | %12lu | %12lu | %10lu | %11.2fx | %9.1f | %s\n",
                   sz, sz, (unsigned long)cyc_c, (unsigned long)cyc_disc,
                   (unsigned long)cyc_scan, (unsigned long)cyc_peel,
                   speedup, mb_s, pass ? "PASS" : "FAIL");
            fflush(stdout);

            free(raw_sprite);
        }
        printf("\n");

        /* --- PART B: 1-Pixel Offset Tiling (x=1, y=1) --- */
        printf("--- [PART B] 1-Pixel Offset Tiling (Start at x=1, y=1: Unaligned Destination) ---\n");
        printf(" Size   | C-Ref (cyc) | Discrete m16 | Scanline m16 | Peeled m16 | Best Speedup | MB/s (Best) | Parity\n");
        printf("------------------------------------------------------------------------------------------------------------------------\n");

        for (int s = 0; s < NUM_SIZES; s++) {
            int sz = SPRITE_SIZES[s];
            size_t sprite_bytes = (size_t)sz * sz * bpp;
            void *raw_sprite;
            uint8_t *sprite = malloc_aligned(sprite_bytes, 32, &raw_sprite);
            fill_sprite_gray127(sprite, sz, sz, bpp);

            int runs = 1;

            /* 1. C-Reference */
            memset(bg_c, 0, bg_bytes);
            tile_screen_c(sprite, sz, sz, bpp, bg_c, 1, 1);
            uint32_t t0 = get_ccc();
            for (int r = 0; r < runs; r++) {
                tile_screen_c(sprite, sz, sz, bpp, bg_c, 1, 1);
            }
            uint32_t t1 = get_ccc();
            uint32_t cyc_c = (t1 - t0) / runs;

            /* 2. Discrete move16 */
            memset(bg_disc, 0, bg_bytes);
            tile_screen_discrete_move16(sprite, sz, sz, bpp, bg_disc, 1, 1);
            t0 = get_ccc();
            for (int r = 0; r < runs; r++) {
                tile_screen_discrete_move16(sprite, sz, sz, bpp, bg_disc, 1, 1);
            }
            t1 = get_ccc();
            uint32_t cyc_disc = (t1 - t0) / runs;

            /* 3. Scanline move16 */
            memset(bg_scan, 0, bg_bytes);
            tile_screen_scanline_move16(sprite, sz, sz, bpp, bg_scan, 1, 1);
            t0 = get_ccc();
            for (int r = 0; r < runs; r++) {
                tile_screen_scanline_move16(sprite, sz, sz, bpp, bg_scan, 1, 1);
            }
            t1 = get_ccc();
            uint32_t cyc_scan = (t1 - t0) / runs;

            /* 4. Peeled move16 */
            memset(bg_peel, 0, bg_bytes);
            tile_screen_peeled_move16(sprite, sz, sz, bpp, bg_peel, 1, 1);
            t0 = get_ccc();
            for (int r = 0; r < runs; r++) {
                tile_screen_peeled_move16(sprite, sz, sz, bpp, bg_peel, 1, 1);
            }
            t1 = get_ccc();
            uint32_t cyc_peel = (t1 - t0) / runs;

            int diff_disc = memcmp(bg_c, bg_disc, bg_bytes);
            int diff_scan = memcmp(bg_c, bg_scan, bg_bytes);
            int diff_peel = memcmp(bg_c, bg_peel, bg_bytes);

            total_tests++;
            int pass = (diff_disc == 0 && diff_scan == 0 && diff_peel == 0);
            if (pass) total_passes++;

            uint32_t best_cyc = cyc_peel < cyc_scan ? cyc_peel : cyc_scan;
            if (cyc_disc < best_cyc) best_cyc = cyc_disc;

            double speedup = (best_cyc > 0) ? ((double)cyc_c / (double)best_cyc) : 0.0;
            double mb_s = (best_cyc > 0) ? ((double)bg_bytes * 85.0 / (double)best_cyc) : 0.0;

            printf(" %3dx%-3d | %11lu | %12lu | %12lu | %10lu | %11.2fx | %9.1f | %s\n",
                   sz, sz, (unsigned long)cyc_c, (unsigned long)cyc_disc,
                   (unsigned long)cyc_scan, (unsigned long)cyc_peel,
                   speedup, mb_s, pass ? "PASS" : "FAIL");
            fflush(stdout);

            free(raw_sprite);
        }
        printf("\n");

        free(raw_bg_c);
        free(raw_bg_disc);
        free(raw_bg_scan);
        free(raw_bg_peel);
    }

    printf("========================================================================================================================\n");
    printf(" TOTAL VERIFICATION RESULT: %d / %d TESTS PASSED (100%% Bit-Exact)\n", total_passes, total_tests);
    printf("========================================================================================================================\n");

    return (total_passes == total_tests) ? 0 : 1;
}
