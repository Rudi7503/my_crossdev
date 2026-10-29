        section .text,code
        xdef    _get_ccc
        xdef    _h264_idct4x4_asm
        xdef    _h264_idct4x4_unrolled
        xdef    _mc_halfpel_h_ammx
        xdef    _mc_halfpel_v_ammx
        xdef    _mc_halfpel_hv_ammx
        xdef    _yuv420_to_rgb32_ammx
        xdef    _mc_block_noprefetch
        xdef    _mc_block_prefetch

_get_ccc:
        movec.l CCC,d0
        rts

; ======================================================================
; 1. H.264 Core 4x4 IDCT Baseline ASM (Loops with Stack Frame)
; ======================================================================
_h264_idct4x4_asm:
        movem.l d2-d7/a2-a4,-(sp)
        move.l  40(sp),a0               ; a0 = in
        move.l  44(sp),a1               ; a1 = out
        suba.w  #32,sp
        movea.l sp,a2                   ; tmp buffer

        moveq   #4-1,d7
.row_loop:
        move.w  (a0)+,d0
        move.w  (a0)+,d1
        move.w  (a0)+,d2
        move.w  (a0)+,d3

        move.w  d0,d4
        add.w   d2,d4                   ; s0 = z0 + z2
        sub.w   d2,d0                   ; s1 = z0 - z2

        move.w  d1,d5
        asr.w   #1,d5
        sub.w   d3,d5                   ; s2 = (z1 >> 1) - z3

        move.w  d3,d6
        asr.w   #1,d6
        add.w   d1,d6                   ; s3 = z1 + (z3 >> 1)

        move.w  d4,d1
        add.w   d6,d1                   ; t0 = s0 + s3
        sub.w   d6,d4                   ; t3 = s0 - s3

        move.w  d0,d2
        add.w   d5,d2                   ; t1 = s1 + s2
        sub.w   d5,d0                   ; t2 = s1 - s2

        move.w  d1,(a2)+
        move.w  d2,(a2)+
        move.w  d0,(a2)+
        move.w  d4,(a2)+
        dbra    d7,.row_loop

        suba.w  #32,a2
        moveq   #0,d7
.col_loop:
        move.w  (a2,d7.w),d0
        move.w  8(a2,d7.w),d1
        move.w  16(a2,d7.w),d2
        move.w  24(a2,d7.w),d3

        move.w  d0,d4
        add.w   d2,d4
        sub.w   d2,d0

        move.w  d1,d5
        asr.w   #1,d5
        sub.w   d3,d5

        move.w  d3,d6
        asr.w   #1,d6
        add.w   d1,d6

        move.w  d4,d1
        add.w   d6,d1
        add.w   #32,d1
        asr.w   #6,d1                   ; out[0][c]

        sub.w   d6,d4
        add.w   #32,d4
        asr.w   #6,d4                   ; out[3][c]

        move.w  d0,d2
        add.w   d5,d2
        add.w   #32,d2
        asr.w   #6,d2                   ; out[1][c]

        sub.w   d5,d0
        add.w   #32,d0
        asr.w   #6,d0                   ; out[2][c]

        move.w  d1,(a1,d7.w)
        move.w  d2,8(a1,d7.w)
        move.w  d0,16(a1,d7.w)
        move.w  d4,24(a1,d7.w)

        addq.w  #2,d7
        cmpi.w  #8,d7
        blt.s   .col_loop

        adda.w  #32,sp
        movem.l (sp)+,d2-d7/a2-a4
        rts

; ======================================================================
; 1b. H.264 Core 4x4 IDCT Fully Unrolled & In-Register (Fast-Path)
; ======================================================================
_h264_idct4x4_unrolled:
        movem.l d2-d7/a2,-(sp)
        move.l  32(sp),a0               ; in
        move.l  36(sp),a1               ; out
        suba.w  #32,sp
        movea.l sp,a2

        ; --- Row 0 ---
        move.w  (a0)+,d0
        move.w  (a0)+,d1
        move.w  (a0)+,d2
        move.w  (a0)+,d3
        move.w  d0,d4
        add.w   d2,d4
        sub.w   d2,d0
        move.w  d1,d5
        asr.w   #1,d5
        sub.w   d3,d5
        move.w  d3,d6
        asr.w   #1,d6
        add.w   d1,d6
        move.w  d4,d1
        add.w   d6,d1
        sub.w   d6,d4
        move.w  d0,d2
        add.w   d5,d2
        sub.w   d5,d0
        move.w  d1,(a2)
        move.w  d2,2(a2)
        move.w  d0,4(a2)
        move.w  d4,6(a2)

        ; --- Row 1 ---
        move.w  (a0)+,d0
        move.w  (a0)+,d1
        move.w  (a0)+,d2
        move.w  (a0)+,d3
        move.w  d0,d4
        add.w   d2,d4
        sub.w   d2,d0
        move.w  d1,d5
        asr.w   #1,d5
        sub.w   d3,d5
        move.w  d3,d6
        asr.w   #1,d6
        add.w   d1,d6
        move.w  d4,d1
        add.w   d6,d1
        sub.w   d6,d4
        move.w  d0,d2
        add.w   d5,d2
        sub.w   d5,d0
        move.w  d1,8(a2)
        move.w  d2,10(a2)
        move.w  d0,12(a2)
        move.w  d4,14(a2)

        ; --- Row 2 ---
        move.w  (a0)+,d0
        move.w  (a0)+,d1
        move.w  (a0)+,d2
        move.w  (a0)+,d3
        move.w  d0,d4
        add.w   d2,d4
        sub.w   d2,d0
        move.w  d1,d5
        asr.w   #1,d5
        sub.w   d3,d5
        move.w  d3,d6
        asr.w   #1,d6
        add.w   d1,d6
        move.w  d4,d1
        add.w   d6,d1
        sub.w   d6,d4
        move.w  d0,d2
        add.w   d5,d2
        sub.w   d5,d0
        move.w  d1,16(a2)
        move.w  d2,18(a2)
        move.w  d0,20(a2)
        move.w  d4,22(a2)

        ; --- Row 3 ---
        move.w  (a0)+,d0
        move.w  (a0)+,d1
        move.w  (a0)+,d2
        move.w  (a0)+,d3
        move.w  d0,d4
        add.w   d2,d4
        sub.w   d2,d0
        move.w  d1,d5
        asr.w   #1,d5
        sub.w   d3,d5
        move.w  d3,d6
        asr.w   #1,d6
        add.w   d1,d6
        move.w  d4,d1
        add.w   d6,d1
        sub.w   d6,d4
        move.w  d0,d2
        add.w   d5,d2
        sub.w   d5,d0
        move.w  d1,24(a2)
        move.w  d2,26(a2)
        move.w  d0,28(a2)
        move.w  d4,30(a2)

        ; --- Unrolled Vertical Columns 0..3 ---
        ; Column 0
        move.w  (a2),d0
        move.w  8(a2),d1
        move.w  16(a2),d2
        move.w  24(a2),d3
        move.w  d0,d4
        add.w   d2,d4
        sub.w   d2,d0
        move.w  d1,d5
        asr.w   #1,d5
        sub.w   d3,d5
        move.w  d3,d6
        asr.w   #1,d6
        add.w   d1,d6
        move.w  d4,d1
        add.w   d6,d1
        add.w   #32,d1
        asr.w   #6,d1
        sub.w   d6,d4
        add.w   #32,d4
        asr.w   #6,d4
        move.w  d0,d2
        add.w   d5,d2
        add.w   #32,d2
        asr.w   #6,d2
        sub.w   d5,d0
        add.w   #32,d0
        asr.w   #6,d0
        move.w  d1,(a1)
        move.w  d2,8(a1)
        move.w  d0,16(a1)
        move.w  d4,24(a1)

        ; Column 1
        move.w  2(a2),d0
        move.w  10(a2),d1
        move.w  18(a2),d2
        move.w  26(a2),d3
        move.w  d0,d4
        add.w   d2,d4
        sub.w   d2,d0
        move.w  d1,d5
        asr.w   #1,d5
        sub.w   d3,d5
        move.w  d3,d6
        asr.w   #1,d6
        add.w   d1,d6
        move.w  d4,d1
        add.w   d6,d1
        add.w   #32,d1
        asr.w   #6,d1
        sub.w   d6,d4
        add.w   #32,d4
        asr.w   #6,d4
        move.w  d0,d2
        add.w   d5,d2
        add.w   #32,d2
        asr.w   #6,d2
        sub.w   d5,d0
        add.w   #32,d0
        asr.w   #6,d0
        move.w  d1,2(a1)
        move.w  d2,10(a1)
        move.w  d0,18(a1)
        move.w  d4,26(a1)

        ; Column 2
        move.w  4(a2),d0
        move.w  12(a2),d1
        move.w  20(a2),d2
        move.w  28(a2),d3
        move.w  d0,d4
        add.w   d2,d4
        sub.w   d2,d0
        move.w  d1,d5
        asr.w   #1,d5
        sub.w   d3,d5
        move.w  d3,d6
        asr.w   #1,d6
        add.w   d1,d6
        move.w  d4,d1
        add.w   d6,d1
        add.w   #32,d1
        asr.w   #6,d1
        sub.w   d6,d4
        add.w   #32,d4
        asr.w   #6,d4
        move.w  d0,d2
        add.w   d5,d2
        add.w   #32,d2
        asr.w   #6,d2
        sub.w   d5,d0
        add.w   #32,d0
        asr.w   #6,d0
        move.w  d1,4(a1)
        move.w  d2,12(a1)
        move.w  d0,20(a1)
        move.w  d4,28(a1)

        ; Column 3
        move.w  6(a2),d0
        move.w  14(a2),d1
        move.w  22(a2),d2
        move.w  30(a2),d3
        move.w  d0,d4
        add.w   d2,d4
        sub.w   d2,d0
        move.w  d1,d5
        asr.w   #1,d5
        sub.w   d3,d5
        move.w  d3,d6
        asr.w   #1,d6
        add.w   d1,d6
        move.w  d4,d1
        add.w   d6,d1
        add.w   #32,d1
        asr.w   #6,d1
        sub.w   d6,d4
        add.w   #32,d4
        asr.w   #6,d4
        move.w  d0,d2
        add.w   d5,d2
        add.w   #32,d2
        asr.w   #6,d2
        sub.w   d5,d0
        add.w   #32,d0
        asr.w   #6,d0
        move.w  d1,6(a1)
        move.w  d2,14(a1)
        move.w  d0,22(a1)
        move.w  d4,30(a1)

        adda.w  #32,sp
        movem.l (sp)+,d2-d7/a2
        rts

; ======================================================================
; 2. Half-Pel Motion Compensation with AMMX PAVGB
; ======================================================================
; void mc_halfpel_h_ammx(const uint8_t *ref, int stride, uint8_t *dst)
_mc_halfpel_h_ammx:
        move.l  4(sp),a0                ; ref
        move.l  8(sp),d0                ; stride
        move.l  12(sp),a1               ; dst

        moveq   #8-1,d1
.h_loop:
        load    (a0),e0                 ; ref[y][0..7]
        load    1(a0),e1                ; ref[y][1..8] (unaligned load)
        pavgb   e1,e0,e2                ; 8-byte parallel half-pel average in 1 cycle!
        store   e2,(a1)
        adda.l  d0,a0
        addq.w  #8,a1
        dbra    d1,.h_loop
        rts

; void mc_halfpel_v_ammx(const uint8_t *ref, int stride, uint8_t *dst)
_mc_halfpel_v_ammx:
        move.l  4(sp),a0                ; ref
        move.l  8(sp),d0                ; stride
        move.l  12(sp),a1               ; dst

        moveq   #8-1,d1
.v_loop:
        load    (a0),e0                 ; ref[y][0..7]
        load    (a0,d0.l),e1            ; ref[y+1][0..7]
        pavgb   e1,e0,e2                ; 8-byte parallel vertical average in 1 cycle!
        store   e2,(a1)
        adda.l  d0,a0
        addq.w  #8,a1
        dbra    d1,.v_loop
        rts

; void mc_halfpel_hv_ammx(const uint8_t *ref, int stride, uint8_t *dst)
_mc_halfpel_hv_ammx:
        move.l  4(sp),a0                ; ref
        move.l  8(sp),d0                ; stride
        move.l  12(sp),a1               ; dst

        moveq   #8-1,d1
.hv_loop:
        ; Horizontal average row y
        load    (a0),e0
        load    1(a0),e1
        pavgb   e1,e0,e2                ; H0 = avg(ref[y], ref[y]+1)

        ; Horizontal average row y+1
        load    (a0,d0.l),e3
        load    1(a0,d0.l),e4
        pavgb   e4,e3,e5                ; H1 = avg(ref[y+1], ref[y+1]+1)

        ; Vertical average between H0 and H1
        pavgb   e5,e2,e6                ; HV = avg(H0, H1)
        store   e6,(a1)

        adda.l  d0,a0
        addq.w  #8,a1
        dbra    d1,.hv_loop
        rts

; ======================================================================
; 3. AMMX SIMD YCbCr 4:2:0 to 32-Bit ARGB Color Converter
; ======================================================================
; void yuv420_to_rgb32_ammx(const uint8_t *y, const uint8_t *u, const uint8_t *v,
;                           int stride_y, int stride_uv, uint32_t *dst, int dst_stride)
_yuv420_to_rgb32_ammx:
        link.w  a6,#-16
        movem.l d2-d7/a2-a5,-(sp)
        move.l  8(a6),a0                ; y
        move.l  12(a6),a1               ; u
        move.l  16(a6),a2               ; v
        move.l  20(a6),d0               ; stride_y
        move.l  24(a6),d1               ; stride_uv
        move.l  28(a6),a3               ; dst
        move.l  32(a6),d2               ; dst_stride
        lsl.l   #2,d2                   ; dst_stride in bytes

        moveq   #8-1,d7
.row_loop:
        moveq   #0,d4
        move.b  (a1),d4
        subi.w  #128,d4                 ; u0
        moveq   #0,d5
        move.b  (a2),d5
        subi.w  #128,d5                 ; v0

        ; r_off = (359 * v0) >> 8
        move.w  d5,d6
        muls.w  #359,d6
        asr.w   #8,d6
        move.w  d6,-2(a6)               ; r_off

        ; g_off = (88 * u0 + 183 * v0) >> 8
        move.w  d4,d6
        muls.w  #88,d6
        move.w  d5,d3
        muls.w  #183,d3
        add.w   d3,d6
        asr.w   #8,d6
        move.w  d6,-4(a6)               ; g_off

        ; b_off = (454 * u0) >> 8
        move.w  d4,d6
        muls.w  #454,d6
        asr.w   #8,d6
        move.w  d6,-6(a6)               ; b_off

        ; Process 8 pixels in this row
        moveq   #8-1,d6
        movea.l a3,a4
.pix_loop:
        moveq   #0,d3
        move.b  (a0)+,d3                ; y

        ; R
        move.w  d3,d4
        add.w   -2(a6),d4
        spl     d5
        ext.w   d5
        and.w   d5,d4
        cmpi.w  #255,d4
        ble.s   .r_ok
        moveq   #255,d4
.r_ok:
        ; G
        move.w  d3,d5
        sub.w   -4(a6),d5
        spl     d1
        ext.w   d1
        and.w   d1,d5
        cmpi.w  #255,d5
        ble.s   .g_ok
        moveq   #255,d5
.g_ok:
        ; B
        add.w   -6(a6),d3
        spl     d1
        ext.w   d1
        and.w   d1,d3
        cmpi.w  #255,d3
        ble.s   .b_ok
        moveq   #255,d3
.b_ok:

        ; Pack ARGB
        lsl.l   #8,d4
        or.b    d5,d4
        lsl.l   #8,d4
        or.b    d3,d4
        ori.l   #$ff000000,d4
        move.l  d4,(a4)+

        dbra    d6,.pix_loop

        move.l  24(a6),d1               ; reload stride_uv
        suba.w  #8,a0
        adda.l  d0,a0
        btst    #0,d7
        bne.s   .skip_uv
        adda.l  d1,a1
        adda.l  d1,a2
.skip_uv:
        adda.l  d2,a3
        dbra    d7,.row_loop

        movem.l (sp)+,d2-d7/a2-a5
        unlk    a6
        rts

; ======================================================================
; 4. Prefetching Benchmark (TOUCH)
; ======================================================================
; void mc_block_noprefetch(const uint8_t *ref, int stride, uint8_t *dst)
_mc_block_noprefetch:
        move.l  4(sp),a0
        move.l  8(sp),d0
        move.l  12(sp),a1

        moveq   #8-1,d1
.np_loop:
        load    (a0),e0
        store   e0,(a1)
        adda.l  d0,a0
        addq.w  #8,a1
        dbra    d1,.np_loop
        rts

; void mc_block_prefetch(const uint8_t *ref, int stride, uint8_t *dst, const uint8_t *next_ref)
_mc_block_prefetch:
        move.l  4(sp),a0                ; ref
        move.l  8(sp),d0                ; stride
        move.l  12(sp),a1               ; dst
        move.l  16(sp),a0               ; next_ref temporary in scratch a0

        moveq   #0,d1
        touch   (a0,d1.w)               ; prefetch next macroblock
        moveq   #32,d1
        touch   (a0,d1.w)

        move.l  4(sp),a0                ; reload ref
        moveq   #8-1,d1
.pf_loop:
        load    (a0),e0
        store   e0,(a1)
        adda.l  d0,a0
        addq.w  #8,a1
        dbra    d1,.pf_loop
        rts
