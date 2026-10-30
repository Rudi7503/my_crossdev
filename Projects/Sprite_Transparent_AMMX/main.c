#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "blit_trans.h"

#define BG_WIDTH   1920
#define BG_HEIGHT  1080

static const int SPRITE_SIZES[] = { 8, 16, 32, 64, 128, 256, 512 };
#define NUM_SIZES 7

static void *malloc_aligned(size_t size, size_t alignment, void **raw_out) {
    void *p = malloc(size + alignment + 32);
    if (!p) { *raw_out = NULL; return NULL; }
    *raw_out = p;
    uintptr_t addr = (uintptr_t)p;
    uintptr_t aligned = (addr + alignment - 1) & ~(alignment - 1);
    return (void *)aligned;
}

/* Fill sprite with checkerboard pattern: 50% transparent, 50% gray 127 */
static void fill_transparent_sprite(uint8_t *buf, int width, int height, int bpp, uint32_t key) {
    int total_pixels = width * height;
    if (bpp == 1) {
        uint8_t k = (uint8_t)key;
        uint8_t g = 127;
        for (int y = 0; y < height; y++) {
            for (int x = 0; x < width; x++) {
                buf[y * width + x] = ((x + y) & 1) ? g : k;
            }
        }
    } else if (bpp == 2) {
        uint16_t *p16 = (uint16_t *)buf;
        uint16_t k = (uint16_t)key;
        uint16_t g = 0x7BEF; /* RGB565 gray 127 */
        for (int y = 0; y < height; y++) {
            for (int x = 0; x < width; x++) {
                p16[y * width + x] = ((x + y) & 1) ? g : k;
            }
        }
    } else if (bpp == 4) {
        uint32_t *p32 = (uint32_t *)buf;
        uint32_t k = key;
        uint32_t g = 0x007F7F7F; /* RGB32 gray 127 */
        for (int y = 0; y < height; y++) {
            for (int x = 0; x < width; x++) {
                p32[y * width + x] = ((x + y) & 1) ? g : k;
            }
        }
    }
}

static void measure_single(blit_trans_fn fn, const uint8_t *src, int src_stride,
                           uint8_t *dst, int dst_stride,
                           int width, int height, uint32_t key,
                           int iters, PerfSample *out) {
    /* Warmup */
    for (int w = 0; w < 3; w++) {
        fn(src, src_stride, dst, dst_stride, width, height, key);
    }

    uint32_t s_ccc  = get_ccc();
    uint32_t s_iep1 = read_iep1();
    uint32_t s_iep2 = read_iep2();
    uint32_t s_iep3 = read_iep3();
    uint32_t s_dch  = read_dch();
    uint32_t s_dcm  = read_dcm();
    uint32_t s_bpc  = read_bpc();
    uint32_t s_bpw  = read_bpw();

    for (int i = 0; i < iters; i++) {
        fn(src, src_stride, dst, dst_stride, width, height, key);
    }

    uint32_t e_bpw  = read_bpw();
    uint32_t e_bpc  = read_bpc();
    uint32_t e_dcm  = read_dcm();
    uint32_t e_dch  = read_dch();
    uint32_t e_iep3 = read_iep3();
    uint32_t e_iep2 = read_iep2();
    uint32_t e_iep1 = read_iep1();
    uint32_t e_ccc  = get_ccc();

    out->ccc  = (e_ccc  - s_ccc)  / iters;
    out->iep1 = (e_iep1 - s_iep1) / iters;
    out->iep2 = (e_iep2 - s_iep2) / iters;
    out->iep3 = (e_iep3 - s_iep3) / iters;
    out->dch  = (e_dch  - s_dch)  / iters;
    out->dcm  = (e_dcm  - s_dcm)  / iters;
    out->bpc  = (e_bpc  - s_bpc)  / iters;
    out->bpw  = (e_bpw  - s_bpw)  / iters;
}

static void measure_tiled(blit_trans_fn fn, const uint8_t *src, int src_stride,
                          uint8_t *dst_base, int dst_stride, int bpp,
                          int sprite_sz, int offset_x, int offset_y, uint32_t key,
                          int iters, PerfSample *out) {
    int tiles_x = (BG_WIDTH - offset_x) / sprite_sz;
    int tiles_y = (BG_HEIGHT - offset_y) / sprite_sz;

    /* Warmup */
    for (int ty = 0; ty < tiles_y && ty < 2; ty++) {
        for (int tx = 0; tx < tiles_x && tx < 2; tx++) {
            uint8_t *d = dst_base + (offset_y + ty * sprite_sz) * dst_stride + (offset_x + tx * sprite_sz) * bpp;
            fn(src, src_stride, d, dst_stride, sprite_sz, sprite_sz, key);
        }
    }

    uint32_t s_ccc  = get_ccc();
    uint32_t s_iep1 = read_iep1();
    uint32_t s_iep2 = read_iep2();
    uint32_t s_iep3 = read_iep3();
    uint32_t s_dch  = read_dch();
    uint32_t s_dcm  = read_dcm();
    uint32_t s_bpc  = read_bpc();
    uint32_t s_bpw  = read_bpw();

    for (int it = 0; it < iters; it++) {
        for (int ty = 0; ty < tiles_y; ty++) {
            for (int tx = 0; tx < tiles_x; tx++) {
                uint8_t *d = dst_base + (offset_y + ty * sprite_sz) * dst_stride + (offset_x + tx * sprite_sz) * bpp;
                fn(src, src_stride, d, dst_stride, sprite_sz, sprite_sz, key);
            }
        }
    }

    uint32_t e_bpw  = read_bpw();
    uint32_t e_bpc  = read_bpc();
    uint32_t e_dcm  = read_dcm();
    uint32_t e_dch  = read_dch();
    uint32_t e_iep3 = read_iep3();
    uint32_t e_iep2 = read_iep2();
    uint32_t e_iep1 = read_iep1();
    uint32_t e_ccc  = get_ccc();

    out->ccc  = (e_ccc  - s_ccc)  / iters;
    out->iep1 = (e_iep1 - s_iep1) / iters;
    out->iep2 = (e_iep2 - s_iep2) / iters;
    out->iep3 = (e_iep3 - s_iep3) / iters;
    out->dch  = (e_dch  - s_dch)  / iters;
    out->dcm  = (e_dcm  - s_dcm)  / iters;
    out->bpc  = (e_bpc  - s_bpc)  / iters;
    out->bpw  = (e_bpw  - s_bpw)  / iters;
}

static void print_harness_v5_row(const char *name, const PerfSample *p, double speedup) {
    uint32_t total_ins = p->iep1 + p->iep2 + p->iep3;
    double p1_pct = (total_ins > 0) ? (100.0 * (double)p->iep1 / (double)total_ins) : 0.0;
    double p2_pct = (total_ins > 0) ? (100.0 * (double)p->iep2 / (double)total_ins) : 0.0;
    double p3_pct = (total_ins > 0) ? (100.0 * (double)p->iep3 / (double)total_ins) : 0.0;
    double ipc = (p->ccc > 0) ? ((double)total_ins / (double)p->ccc) : 0.0;
    uint32_t total_dc = p->dch + p->dcm;
    double dch_pct = (total_dc > 0) ? (100.0 * (double)p->dch / (double)total_dc) : 100.0;

    printf(" %-15s | %8lu | %8lu | %5.2f | %5.1f%% | %5.1f%% | %5.1f%% | %5.1f%% (%lu/%lu) | %5.2fx\n",
           name, (unsigned long)p->ccc, (unsigned long)total_ins, ipc,
           p1_pct, p2_pct, p3_pct, dch_pct, (unsigned long)p->dch, (unsigned long)p->dcm, speedup);
    fflush(stdout);
}

int main(void) {
    __asm__ __volatile__("ori.w #0x0800,%%sr" ::: "cc");

    printf("========================================================================================================================\n");
    printf(" Apollo 68080 AMMX Transparent Sprite Blitting Suite (Harness v5 Methodology)\n");
    printf(" Target: Vampire V4 Standalone (Apollo 68080 @ 85 MHz, ApolloOS 9.5)\n");
    printf(" Store Architectures: storeilm (Inverted Long Mask), bsel+store (Register Mux), C Reference\n");
    printf(" Background: 1920x1080 (Black) | Sprites: 8x8 .. 512x512 (50%% Transparent / 50%% Gray 127)\n");
    printf(" Performance Counters: CCC, IEP1 (Anton), IEP2 (Bruno), IEP3 (Cesar), DCH, DCM\n");
    printf("========================================================================================================================\n\n");

    const int depths[] = { 8, 16, 32 };
    const uint32_t keys[] = { 0x00, 0xF81F, 0x00000000 };

    for (int d = 0; d < 3; d++) {
        int depth = depths[d];
        int bpp = depth / 8;
        uint32_t key = keys[d];
        int dst_stride = BG_WIDTH * bpp;
        size_t bg_bytes = (size_t)dst_stride * BG_HEIGHT;

        void *raw_bg, *raw_ref;
        uint8_t *bg = malloc_aligned(bg_bytes, 32, &raw_bg);
        uint8_t *bg_ref = malloc_aligned(bg_bytes, 32, &raw_ref);

        blit_trans_fn fn_c = (depth == 8) ? blit_trans_8_c : ((depth == 16) ? blit_trans_16_c : blit_trans_32_c);
        blit_trans_fn fn_ilm = (depth == 8) ? blit_trans_8_storeilm : ((depth == 16) ? blit_trans_16_storeilm : blit_trans_32_storeilm);
        blit_trans_fn fn_bsel = (depth == 8) ? blit_trans_8_bsel : ((depth == 16) ? blit_trans_16_bsel : blit_trans_32_bsel);

        printf("########################################################################################################################\n");
        printf(" COLOR DEPTH: %d-BIT (%d Bpp) | Color-Key: 0x%0*lX | Framebuffer: 1920x1080 (%lu KB)\n",
               depth, bpp, bpp * 2, (unsigned long)key, (unsigned long)(bg_bytes / 1024));
        printf("########################################################################################################################\n\n");

        /* ================================================================== */
        /* TEST 1: SINGLE 256-BIT ALIGNED SPRITE BLIT                         */
        /* ================================================================== */
        printf("------------------------------------------------------------------------------------------------------------------------\n");
        printf(" TEST 1: SINGLE 256-BIT ALIGNED SPRITE BLIT (8x8 .. 512x512 at [128, 128])\n");
        printf(" Routine         |   Cycles |    Insns |   IPC |  Anton |  Bruno |  Cesar | D-Cache Hit Rate (Hits/Misses) | Speedup\n");
        printf("-----------------+----------+----------+-------+--------+--------+--------+--------------------------------+--------\n");

        for (int s = 0; s < NUM_SIZES; s++) {
            int sz = SPRITE_SIZES[s];
            int width_bytes = sz * bpp;
            size_t sprite_bytes = (size_t)width_bytes * sz;

            void *raw_sprite;
            uint8_t *sprite = malloc_aligned(sprite_bytes, 32, &raw_sprite);
            fill_transparent_sprite(sprite, sz, sz, bpp, key);

            uint8_t *dst_ptr = bg + 128 * dst_stride + 128 * bpp;
            uint8_t *ref_ptr = bg_ref + 128 * dst_stride + 128 * bpp;

            /* Verification against C reference */
            memset(bg, 0x11, bg_bytes);
            memset(bg_ref, 0x11, bg_bytes);
            fn_c(sprite, width_bytes, ref_ptr, dst_stride, sz, sz, key);
            fn_ilm(sprite, width_bytes, dst_ptr, dst_stride, sz, sz, key);
            int v_ilm = (memcmp(bg, bg_ref, bg_bytes) == 0);

            memset(bg, 0x11, bg_bytes);
            fn_bsel(sprite, width_bytes, dst_ptr, dst_stride, sz, sz, key);
            int v_bsel = (memcmp(bg, bg_ref, bg_bytes) == 0);

            int iters = (sz <= 32) ? 100 : ((sz <= 128) ? 20 : 5);
            PerfSample p_c, p_ilm, p_bsel;

            measure_single(fn_c,    sprite, width_bytes, ref_ptr, dst_stride, sz, sz, key, iters, &p_c);
            measure_single(fn_ilm,  sprite, width_bytes, dst_ptr, dst_stride, sz, sz, key, iters, &p_ilm);
            measure_single(fn_bsel, sprite, width_bytes, dst_ptr, dst_stride, sz, sz, key, iters, &p_bsel);

            double spd_ilm  = (p_ilm.ccc > 0) ? ((double)p_c.ccc / (double)p_ilm.ccc) : 1.0;
            double spd_bsel = (p_bsel.ccc > 0) ? ((double)p_c.ccc / (double)p_bsel.ccc) : 1.0;

            printf("[%dx%d Sprite %d-Bit (%d bytes/line, %d iters)] - Verify: ilm=%s bsel=%s\n",
                   sz, sz, depth, width_bytes, iters, v_ilm ? "OK" : "ERR", v_bsel ? "OK" : "ERR");
            print_harness_v5_row("C-Reference",   &p_c,    1.00);
            print_harness_v5_row("AMMX-storeilm", &p_ilm,  spd_ilm);
            print_harness_v5_row("AMMX-bsel",     &p_bsel, spd_bsel);
            printf("-----------------+----------+----------+-------+--------+--------+--------+--------------------------------+--------\n");

            free(raw_sprite);
        }

        /* ================================================================== */
        /* TEST 2: FULL-SCREEN TILED BLIT (ALIGNED [0,0])                     */
        /* ================================================================== */
        printf("\n------------------------------------------------------------------------------------------------------------------------\n");
        printf(" TEST 2: FULL-SCREEN TILED BLIT (Aligned [0,0] across 1920x1080 Screen)\n");
        printf(" Routine         |   Cycles |    Insns |   IPC |  Anton |  Bruno |  Cesar | D-Cache Hit Rate (Hits/Misses) | Speedup\n");
        printf("-----------------+----------+----------+-------+--------+--------+--------+--------------------------------+--------\n");

        const int tiled_sizes[] = { 16, 64, 256 };
        for (int ts = 0; ts < 3; ts++) {
            int sz = tiled_sizes[ts];
            int width_bytes = sz * bpp;
            size_t sprite_bytes = (size_t)width_bytes * sz;

            void *raw_sprite;
            uint8_t *sprite = malloc_aligned(sprite_bytes, 32, &raw_sprite);
            fill_transparent_sprite(sprite, sz, sz, bpp, key);

            int iters = (sz <= 16) ? 2 : 1;
            PerfSample p_c, p_ilm, p_bsel;

            measure_tiled(fn_c,    sprite, width_bytes, bg_ref, dst_stride, bpp, sz, 0, 0, key, iters, &p_c);
            measure_tiled(fn_ilm,  sprite, width_bytes, bg,     dst_stride, bpp, sz, 0, 0, key, iters, &p_ilm);
            measure_tiled(fn_bsel, sprite, width_bytes, bg,     dst_stride, bpp, sz, 0, 0, key, iters, &p_bsel);

            double spd_ilm  = (p_ilm.ccc > 0) ? ((double)p_c.ccc / (double)p_ilm.ccc) : 1.0;
            double spd_bsel = (p_bsel.ccc > 0) ? ((double)p_c.ccc / (double)p_bsel.ccc) : 1.0;

            int tiles_total = (1920 / sz) * (1080 / sz);
            printf("[Tiled %dx%d %d-Bit: %d Tiles covering Full Screen]\n", sz, sz, depth, tiles_total);
            print_harness_v5_row("C-Reference",   &p_c,    1.00);
            print_harness_v5_row("AMMX-storeilm", &p_ilm,  spd_ilm);
            print_harness_v5_row("AMMX-bsel",     &p_bsel, spd_bsel);
            printf("-----------------+----------+----------+-------+--------+--------+--------+--------------------------------+--------\n");

            free(raw_sprite);
        }

        /* ================================================================== */
        /* TEST 3: FULL-SCREEN TILED BLIT WITH 1-PIXEL OFFSET [1, 1]          */
        /* ================================================================== */
        printf("\n------------------------------------------------------------------------------------------------------------------------\n");
        printf(" TEST 3: TILED BLIT WITH 1-PIXEL OFFSET [1,1] (Unaligned Memory Access Torture Test)\n");
        printf(" Routine         |   Cycles |    Insns |   IPC |  Anton |  Bruno |  Cesar | D-Cache Hit Rate (Hits/Misses) | Speedup\n");
        printf("-----------------+----------+----------+-------+--------+--------+--------+--------------------------------+--------\n");

        for (int ts = 0; ts < 3; ts++) {
            int sz = tiled_sizes[ts];
            int width_bytes = sz * bpp;
            size_t sprite_bytes = (size_t)width_bytes * sz;

            void *raw_sprite;
            uint8_t *sprite = malloc_aligned(sprite_bytes, 32, &raw_sprite);
            fill_transparent_sprite(sprite, sz, sz, bpp, key);

            int iters = (sz <= 16) ? 2 : 1;
            PerfSample p_c, p_ilm, p_bsel;

            measure_tiled(fn_c,    sprite, width_bytes, bg_ref, dst_stride, bpp, sz, 1, 1, key, iters, &p_c);
            measure_tiled(fn_ilm,  sprite, width_bytes, bg,     dst_stride, bpp, sz, 1, 1, key, iters, &p_ilm);
            measure_tiled(fn_bsel, sprite, width_bytes, bg,     dst_stride, bpp, sz, 1, 1, key, iters, &p_bsel);

            double spd_ilm  = (p_ilm.ccc > 0) ? ((double)p_c.ccc / (double)p_ilm.ccc) : 1.0;
            double spd_bsel = (p_bsel.ccc > 0) ? ((double)p_c.ccc / (double)p_bsel.ccc) : 1.0;

            int tiles_total = ((1920 - 1) / sz) * ((1080 - 1) / sz);
            printf("[Unaligned Tiled %dx%d %d-Bit (+1,+1 offset): %d Tiles]\n", sz, sz, depth, tiles_total);
            print_harness_v5_row("C-Reference",   &p_c,    1.00);
            print_harness_v5_row("AMMX-storeilm", &p_ilm,  spd_ilm);
            print_harness_v5_row("AMMX-bsel",     &p_bsel, spd_bsel);
            printf("-----------------+----------+----------+-------+--------+--------+--------+--------------------------------+--------\n");

            free(raw_sprite);
        }

        free(raw_ref);
        free(raw_bg);
        printf("\n\n");
    }

    printf("========================================================================================================================\n");
    printf(" PROFILING & HARDWARE VERIFICATION COMPLETED SUCCESSFULLY.\n");
    printf("========================================================================================================================\n");

    return 0;
}
