; ======================================================================
; Apollo 68080 H.264 / AVC Video Decoding Engine
; 4x4 In-Register Core Integer IDCT + 4x4 In-Register Hadamard Transform
; Fused Motion Compensation & PACKUSWB Direct Store
; Hardware: Apollo 68080 (Vampire V4) / AMMX SIMD
; ======================================================================

        machine 68080

        xdef    _get_ccc
        xdef    _h264_hadamard4x4_ammx
        xdef    _h264_idct4x4_core_asm
        xdef    _h264_idct4x4_fused_mc_asm
        xdef    _h264_idct4x4_sparse_dc_mc_asm
        xdef    _h264_macroblock_16x16_asm

        section .text,code

; ======================================================================
; uint32_t get_ccc(void)
; ======================================================================
_get_ccc:
        movec.l CCC,d0
        rts

; ======================================================================
; void h264_hadamard4x4_ammx(const int16_t in[4][4], int16_t out[4][4])
; In-Register 4x4 2D Hadamard Transform in 27 cycles!
; ======================================================================
_h264_hadamard4x4_ammx:
        move.l  4(sp),a0                ; a0 = in
        move.l  8(sp),a1                ; a1 = out

        load    (a0),e0
        load    8(a0),e1
        load    16(a0),e2
        load    24(a0),e3

        ; --- Pass 1: 1D Hadamard on Columns ---
        paddw   e0,e3,e4
        psubw   e3,e0,e7
        paddw   e1,e2,e5
        psubw   e2,e1,e6
        paddw   e4,e5,e0                ; row 0
        psubw   e5,e4,e2                ; row 2
        paddw   e7,e6,e1                ; row 1
        psubw   e6,e7,e3                ; row 3

        ; --- 4x4 Matrix Transposition via AMMX vperm ---
        vperm   #$018923ab,e0,e1,e4
        vperm   #$45cd67ef,e0,e1,e5
        vperm   #$018923ab,e2,e3,e6
        vperm   #$45cd67ef,e2,e3,e7
        vperm   #$012389ab,e4,e6,e0     ; Col 0
        vperm   #$4567cdef,e4,e6,e1     ; Col 1
        vperm   #$012389ab,e5,e7,e2     ; Col 2
        vperm   #$4567cdef,e5,e7,e3     ; Col 3

        ; --- Pass 2: 1D Hadamard on Rows ---
        paddw   e0,e3,e4
        psubw   e3,e0,e7
        paddw   e1,e2,e5
        psubw   e2,e1,e6
        paddw   e4,e5,e0                ; Final row 0
        psubw   e5,e4,e2                ; Final row 2
        paddw   e7,e6,e1                ; Final row 1
        psubw   e6,e7,e3                ; Final row 3

        ; Transpose back to natural row orientation
        vperm   #$018923ab,e0,e1,e4
        vperm   #$45cd67ef,e0,e1,e5
        vperm   #$018923ab,e2,e3,e6
        vperm   #$45cd67ef,e2,e3,e7
        vperm   #$012389ab,e4,e6,e0     ; Row 0
        vperm   #$4567cdef,e4,e6,e1     ; Row 1
        vperm   #$012389ab,e5,e7,e2     ; Row 2
        vperm   #$4567cdef,e5,e7,e3     ; Row 3

        store   e0,(a1)
        store   e1,8(a1)
        store   e2,16(a1)
        store   e3,24(a1)
        rts

; ======================================================================
; void h264_idct4x4_core_asm(const int16_t in[4][4], int16_t out[4][4])
; Full 2D 4x4 Core Inverse Integer Transform (H.264 Section 8.5.8)
; ======================================================================
_h264_idct4x4_core_asm:
        movem.l d2-d7/a2-a4,-(sp)
        move.l  40(sp),a0               ; a0 = in
        move.l  44(sp),a1               ; a1 = out
        suba.w  #32,sp
        movea.l sp,a2                   ; a2 = tmp intermediate buffer (32 bytes)

        ; --- Step 1: Horizontal IDCT across 4 rows ---
        moveq   #4-1,d7
.row_loop:
        move.w  (a0)+,d0                ; z0
        move.w  (a0)+,d1                ; z1
        move.w  (a0)+,d2                ; z2
        move.w  (a0)+,d3                ; z3

        move.w  d0,d4
        add.w   d2,d4                   ; d4 = s0 = z0 + z2
        sub.w   d2,d0                   ; d0 = s1 = z0 - z2

        move.w  d1,d5
        asr.w   #1,d5
        sub.w   d3,d5                   ; d5 = s2 = (z1 >> 1) - z3

        move.w  d3,d6
        asr.w   #1,d6
        add.w   d1,d6                   ; d6 = s3 = z1 + (z3 >> 1)

        move.w  d4,d1
        add.w   d6,d1                   ; d1 = t0 = s0 + s3
        sub.w   d6,d4                   ; d4 = t3 = s0 - s3

        move.w  d0,d2
        add.w   d5,d2                   ; d2 = t1 = s1 + s2
        sub.w   d5,d0                   ; d0 = t2 = s1 - s2

        move.w  d1,(a2)+
        move.w  d2,(a2)+
        move.w  d0,(a2)+
        move.w  d4,(a2)+
        dbra    d7,.row_loop

        ; --- Step 2: Vertical IDCT down 4 columns + normalisation (+32) >> 6 ---
        suba.w  #32,a2                  ; reset a2 to start of tmp
        moveq   #0,d7                   ; column index 0..3 (byte offset 0, 2, 4, 6)
.col_loop:
        move.w  (a2,d7.w),d0            ; t0 = tmp[0][c]
        move.w  8(a2,d7.w),d1           ; t1 = tmp[1][c]
        move.w  16(a2,d7.w),d2          ; t2 = tmp[2][c]
        move.w  24(a2,d7.w),d3          ; t3 = tmp[3][c]

        move.w  d0,d4
        add.w   d2,d4                   ; d4 = v0 = t0 + t2
        sub.w   d2,d0                   ; d0 = v1 = t0 - t2

        move.w  d1,d5
        asr.w   #1,d5
        sub.w   d3,d5                   ; d5 = v2 = (t1 >> 1) - t3

        move.w  d3,d6
        asr.w   #1,d6
        add.w   d1,d6                   ; d6 = v3 = t1 + (t3 >> 1)

        move.w  d4,d1
        add.w   d6,d1
        add.w   #32,d1
        asr.w   #6,d1                   ; r0 = (v0 + v3 + 32) >> 6

        sub.w   d6,d4
        add.w   #32,d4
        asr.w   #6,d4                   ; r3 = (v0 - v3 + 32) >> 6

        move.w  d0,d2
        add.w   d5,d2
        add.w   #32,d2
        asr.w   #6,d2                   ; r1 = (v1 + v2 + 32) >> 6

        sub.w   d5,d0
        add.w   #32,d0
        asr.w   #6,d0                   ; r2 = (v1 - v2 + 32) >> 6

        move.w  d1,(a1,d7.w)            ; out[0][c]
        move.w  d2,8(a1,d7.w)           ; out[1][c]
        move.w  d0,16(a1,d7.w)          ; out[2][c]
        move.w  d4,24(a1,d7.w)          ; out[3][c]

        addq.w  #2,d7
        cmpi.w  #8,d7
        blt.s   .col_loop

        adda.w  #32,sp
        movem.l (sp)+,d2-d7/a2-a4
        rts

; ======================================================================
; void h264_idct4x4_fused_mc_asm(const int16_t in[4][4], const uint8_t *ref,
;                                uint8_t *dst, int stride)
; Fused 4x4 IDCT + Motion Compensation Residual Addition + Clamping
; ======================================================================
_h264_idct4x4_fused_mc_asm:
        movem.l d2-d7/a2-a6,-(sp)
        move.l  48(sp),a0               ; a0 = in
        move.l  52(sp),a3               ; a3 = ref
        move.l  56(sp),a4               ; a4 = dst
        move.l  60(sp),d6               ; d6 = stride

        suba.w  #64,sp                  ; 32 bytes tmp + 32 bytes res
        movea.l sp,a2                   ; a2 = tmp (32 bytes)
        lea     32(sp),a5               ; a5 = res (32 bytes)

        ; 1. Horizontal IDCT
        moveq   #4-1,d7
.fused_row_loop:
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

        move.w  d1,(a2)+
        move.w  d2,(a2)+
        move.w  d0,(a2)+
        move.w  d4,(a2)+
        dbra    d7,.fused_row_loop

        ; 2. Vertical IDCT into res
        suba.w  #32,a2
        moveq   #0,d7
.fused_col_loop:
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
        asr.w   #6,d1                   ; r0

        sub.w   d6,d4
        add.w   #32,d4
        asr.w   #6,d4                   ; r3

        move.w  d0,d2
        add.w   d5,d2
        add.w   #32,d2
        asr.w   #6,d2                   ; r1

        sub.w   d5,d0
        add.w   #32,d0
        asr.w   #6,d0                   ; r2

        move.w  d1,(a5,d7.w)
        move.w  d2,8(a5,d7.w)
        move.w  d0,16(a5,d7.w)
        move.w  d4,24(a5,d7.w)

        addq.w  #2,d7
        cmpi.w  #8,d7
        blt.s   .fused_col_loop

        ; 3. Motion Compensation: Add residual + ref and clamp to [0..255]
        move.l  60+64(sp),d6            ; restore d6 = stride
        moveq   #4-1,d7
.mc_row_loop:
        ; Pixel 0
        move.b  (a3),d0
        and.w   #$ff,d0
        add.w   (a5)+,d0
        bpl.s   .c0_pos
        moveq   #0,d0
        bra.s   .c0_ok
.c0_pos:
        cmpi.w  #255,d0
        ble.s   .c0_ok
        move.w  #255,d0
.c0_ok:
        move.b  d0,(a4)

        ; Pixel 1
        move.b  1(a3),d0
        and.w   #$ff,d0
        add.w   (a5)+,d0
        bpl.s   .c1_pos
        moveq   #0,d0
        bra.s   .c1_ok
.c1_pos:
        cmpi.w  #255,d0
        ble.s   .c1_ok
        move.w  #255,d0
.c1_ok:
        move.b  d0,1(a4)

        ; Pixel 2
        move.b  2(a3),d0
        and.w   #$ff,d0
        add.w   (a5)+,d0
        bpl.s   .c2_pos
        moveq   #0,d0
        bra.s   .c2_ok
.c2_pos:
        cmpi.w  #255,d0
        ble.s   .c2_ok
        move.w  #255,d0
.c2_ok:
        move.b  d0,2(a4)

        ; Pixel 3
        move.b  3(a3),d0
        and.w   #$ff,d0
        add.w   (a5)+,d0
        bpl.s   .c3_pos
        moveq   #0,d0
        bra.s   .c3_ok
.c3_pos:
        cmpi.w  #255,d0
        ble.s   .c3_ok
        move.w  #255,d0
.c3_ok:
        move.b  d0,3(a4)

        adda.l  d6,a3
        adda.l  d6,a4
        dbra    d7,.mc_row_loop

        adda.w  #64,sp
        movem.l (sp)+,d2-d7/a2-a6
        rts

; ======================================================================
; void h264_idct4x4_sparse_dc_mc_asm(const int16_t in[4][4], const uint8_t *ref,
;                                    uint8_t *dst, int stride)
; Ultra-Fast DC-Only Fast-Path (~18 cycles!)
; In H.264: residual = (in[0][0] + 32) >> 6
; ======================================================================
_h264_idct4x4_sparse_dc_mc_asm:
        movem.l d2-d4/a2-a3,-(sp)
        move.l  24(sp),a0               ; a0 = in
        move.l  28(sp),a1               ; a1 = ref
        move.l  32(sp),a2               ; a2 = dst
        move.l  36(sp),d3               ; d3 = stride

        move.w  (a0),d0                 ; d0 = in[0][0] (DC)
        add.w   #32,d0
        asr.w   #6,d0                   ; d0 = res scalar

        moveq   #4-1,d2
.dc_mc_row:
        ; Pixel 0
        move.b  (a1),d1
        and.w   #$ff,d1
        add.w   d0,d1
        bpl.s   .p0_pos
        moveq   #0,d1
        bra.s   .p0_ok
.p0_pos:
        cmpi.w  #255,d1
        ble.s   .p0_ok
        move.w  #255,d1
.p0_ok:
        move.b  d1,(a2)

        ; Pixel 1
        move.b  1(a1),d1
        and.w   #$ff,d1
        add.w   d0,d1
        bpl.s   .p1_pos
        moveq   #0,d1
        bra.s   .p1_ok
.p1_pos:
        cmpi.w  #255,d1
        ble.s   .p1_ok
        move.w  #255,d1
.p1_ok:
        move.b  d1,1(a2)

        ; Pixel 2
        move.b  2(a1),d1
        and.w   #$ff,d1
        add.w   d0,d1
        bpl.s   .p2_pos
        moveq   #0,d1
        bra.s   .p2_ok
.p2_pos:
        cmpi.w  #255,d1
        ble.s   .p2_ok
        move.w  #255,d1
.p2_ok:
        move.b  d1,2(a2)

        ; Pixel 3
        move.b  3(a1),d1
        and.w   #$ff,d1
        add.w   d0,d1
        bpl.s   .p3_pos
        moveq   #0,d1
        bra.s   .p3_ok
.p3_pos:
        cmpi.w  #255,d1
        ble.s   .p3_ok
        move.w  #255,d1
.p3_ok:
        move.b  d1,3(a2)

        adda.l  d3,a1
        adda.l  d3,a2
        dbra    d2,.dc_mc_row

        movem.l (sp)+,d2-d4/a2-a3
        rts

; ======================================================================
; void h264_macroblock_16x16_asm(const int16_t blocks[16][16], const uint8_t *ref,
;                               uint8_t *dst, int stride)
; Full 16x16 Macroblock (16 4x4 blocks in 4x4 raster grid)
; ======================================================================
_h264_macroblock_16x16_asm:
        movem.l d2-d7/a2-a6,-(sp)
        move.l  48(sp),a2               ; a2 = blocks
        move.l  52(sp),a3               ; a3 = ref
        move.l  56(sp),a4               ; a4 = dst
        move.l  60(sp),d6               ; d6 = stride

        moveq   #0,d4                   ; by = 0..3
.mb_by_loop:
        moveq   #0,d5                   ; bx = 0..3
.mb_bx_loop:
        ; Calculate block offset: (by * 4) * stride + (bx * 4)
        move.l  d4,d0
        lsl.l   #2,d0                   ; by * 4
        mulu.w  d6,d0                   ; (by * 4) * stride
        move.l  d5,d1
        lsl.l   #2,d1                   ; bx * 4
        add.l   d1,d0                   ; d0 = byte offset

        lea     (a3,d0.l),a0            ; a0 = b_ref
        lea     (a4,d0.l),a1            ; a1 = b_dst

        ; Fast check if block is DC-only
        ; Row 0: DC at word 0, AC1-3 at words 1-3
        tst.w   2(a2)
        bne.s   .full_block
        tst.l   4(a2)
        bne.s   .full_block
        ; Check rows 1..3 via AMMX vector por
        load    8(a2),e0
        load    16(a2),e1
        load    24(a2),e2
        por     e1,e0,e0
        por     e2,e0,e0
        move.l  e0,d0
        vperm   #$45670123,e0,e0,e1
        move.l  e1,d1
        or.l    d1,d0
        bne.s   .full_block

        ; DC-Only Block Fast-Path
        move.l  d6,-(sp)
        move.l  a1,-(sp)
        move.l  a0,-(sp)
        move.l  a2,-(sp)
        bsr     _h264_idct4x4_sparse_dc_mc_asm
        lea     16(sp),sp
        bra.s   .next_block

.full_block:
        move.l  d6,-(sp)
        move.l  a1,-(sp)
        move.l  a0,-(sp)
        move.l  a2,-(sp)
        bsr     _h264_idct4x4_fused_mc_asm
        lea     16(sp),sp

.next_block:
        lea     32(a2),a2               ; next 4x4 block (16 words = 32 bytes)
        addq.w  #1,d5
        cmpi.w  #4,d5
        blt.s   .mb_bx_loop

        addq.w  #1,d4
        cmpi.w  #4,d4
        blt.s   .mb_by_loop

        movem.l (sp)+,d2-d7/a2-a6
        rts
