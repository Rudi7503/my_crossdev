#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

extern uint32_t get_ccc(void);
extern uint32_t read_iep1(void);
extern uint32_t read_iep2(void);
extern uint32_t read_iep3(void);
extern uint32_t read_dch(void);
extern uint32_t read_dcm(void);
extern uint32_t read_bpc(void);
extern uint32_t read_bpw(void);

extern void blit_rect_move16(const uint8_t *src, int src_stride,
                             uint8_t *dst, int dst_stride,
                             int width_bytes, int height_lines);

extern void blit_rect_m68k(const uint8_t *src, int src_stride,
                           uint8_t *dst, int dst_stride,
                           int width_bytes, int height_lines);

extern void blit_rect_ammx(const uint8_t *src, int src_stride,
                           uint8_t *dst, int dst_stride,
                           int width_bytes, int height_lines);

typedef void (*blit_func_t)(const uint8_t *src, int src_stride,
                            uint8_t *dst, int dst_stride,
                            int width_bytes, int height_lines);

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

static void *malloc_aligned(size_t size, size_t alignment, void **raw_out) {
    void *p = malloc(size + alignment + 32);
    if (!p) { *raw_out = NULL; return NULL; }
    *raw_out = p;
    uintptr_t addr = (uintptr_t)p;
    uintptr_t aligned = (addr + alignment - 1) & ~(alignment - 1);
    return (void *)aligned;
}

static void measure_routine(blit_func_t fn, const uint8_t *src, int src_stride,
                            uint8_t *dst, int dst_stride,
                            int width_bytes, int height_lines, int iters,
                            PerfSample *out) {
    /* Warmup */
    for (int w = 0; w < 5; w++) {
        fn(src, src_stride, dst, dst_stride, width_bytes, height_lines);
    }

    uint32_t s_ccc = get_ccc();
    uint32_t s_iep1 = read_iep1();
    uint32_t s_iep2 = read_iep2();
    uint32_t s_iep3 = read_iep3();
    uint32_t s_dch  = read_dch();
    uint32_t s_dcm  = read_dcm();
    uint32_t s_bpc  = read_bpc();
    uint32_t s_bpw  = read_bpw();

    for (int i = 0; i < iters; i++) {
        fn(src, src_stride, dst, dst_stride, width_bytes, height_lines);
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

static void print_perf_row(const char *name, const PerfSample *p) {
    uint32_t total_ins = p->iep1 + p->iep2 + p->iep3;
    double p1_pct = (total_ins > 0) ? (100.0 * (double)p->iep1 / (double)total_ins) : 0.0;
    double p2_pct = (total_ins > 0) ? (100.0 * (double)p->iep2 / (double)total_ins) : 0.0;
    double p3_pct = (total_ins > 0) ? (100.0 * (double)p->iep3 / (double)total_ins) : 0.0;
    double ipc = (p->ccc > 0) ? ((double)total_ins / (double)p->ccc) : 0.0;
    uint32_t total_dc = p->dch + p->dcm;
    double dch_pct = (total_dc > 0) ? (100.0 * (double)p->dch / (double)total_dc) : 100.0;

    printf(" %-12s | %7lu | %7lu | %5.2f | %5.1f%% | %5.1f%% | %5.1f%% | %5.1f%% (%lu/%lu)\n",
           name, (unsigned long)p->ccc, (unsigned long)total_ins, ipc,
           p1_pct, p2_pct, p3_pct, dch_pct, (unsigned long)p->dch, (unsigned long)p->dcm);
    fflush(stdout);
}

int main(void) {
    printf("========================================================================================================\n");
    printf(" Apollo 68080 Superscalar Pipeline & Cache Profiler (Harness v5 Methodology)\n");
    printf(" Core: Apollo 68080 (Vampire V4 @ 85 MHz) | Framebuffer: 1920x1080\n");
    printf(" Performance Counters: IEP1 (Anton), IEP2 (Bruno), IEP3 (Cesar), DCH, DCM\n");
    printf("========================================================================================================\n\n");

    /* Enable Apollo bit */
    __asm__ __volatile__("ori.w #0x0800,%%sr" ::: "cc");

    const int test_sizes[] = { 8, 32, 128, 512 };
    const int num_sizes = 4;
    const int test_depths[] = { 8, 32 };
    const int num_depths = 2;

    for (int d = 0; d < num_depths; d++) {
        int depth = test_depths[d];
        int bpp = depth / 8;
        int dst_stride = 1920 * bpp;
        size_t bg_bytes = (size_t)dst_stride * 1080;

        void *raw_bg;
        uint8_t *bg = malloc_aligned(bg_bytes, 32, &raw_bg);
        memset(bg, 0, bg_bytes);

        printf("--------------------------------------------------------------------------------------------------------\n");
        printf(" COLOR DEPTH: %d-BIT (%d Bpp) | Framebuffer Stride: %d Bytes\n", depth, bpp, dst_stride);
        printf("--------------------------------------------------------------------------------------------------------\n");

        for (int s = 0; s < num_sizes; s++) {
            int sz = test_sizes[s];
            int width_bytes = sz * bpp;
            int height_lines = sz;
            size_t sprite_bytes = (size_t)width_bytes * height_lines;

            void *raw_sprite;
            uint8_t *sprite = malloc_aligned(sprite_bytes, 32, &raw_sprite);
            memset(sprite, 127, sprite_bytes);

            int iters = (sz <= 32) ? 100 : ((sz <= 128) ? 20 : 5);

            uint8_t *dst_ptr = bg + 128 * dst_stride + 128 * bpp;

            PerfSample p_m16, p_ammx, p_m68k;
            measure_routine(blit_rect_move16, sprite, width_bytes, dst_ptr, dst_stride, width_bytes, height_lines, iters, &p_m16);
            measure_routine(blit_rect_ammx,   sprite, width_bytes, dst_ptr, dst_stride, width_bytes, height_lines, iters, &p_ammx);
            measure_routine(blit_rect_m68k,   sprite, width_bytes, dst_ptr, dst_stride, width_bytes, height_lines, iters, &p_m68k);

            printf("\n[Size: %dx%d %d-Bit (%d bytes/line x %d lines, %d iters)]\n", sz, sz, depth, width_bytes, height_lines, iters);
            printf(" Routine      | Cycles  | Insns   | IPC   | Anton  | Bruno  | Cesar  | D-Cache Hit Rate\n");
            printf("--------------+---------+---------+-------+--------+--------+--------+--------------------------\n");
            print_perf_row("move16", &p_m16);
            print_perf_row("AMMX",   &p_ammx);
            print_perf_row("m68k",   &p_m68k);

            free(raw_sprite);
        }

        free(raw_bg);
        printf("\n");
    }

    printf("========================================================================================================\n");
    printf(" PROFILING COMPLETED.\n");
    printf("========================================================================================================\n");
    return 0;
}
