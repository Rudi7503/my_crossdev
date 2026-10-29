; ======================================================================
; Apollo 68080 MPEG-1/2 Video Decoding Pipeline
; Fused 2D IDCT + Motion Compensation Residual Addition + PACKUSWB Direct Store
; Sparse Sub-Block Exits (DC-Only & 4x4 Sparse)
; Hardware: Apollo 68080 (Vampire V4) / AMMX SIMD
; ======================================================================

        machine 68080

        xdef    _get_ccc
        xdef    _mpeg_mc_residual_add_ammx
        xdef    _mpeg_idct8x8_fused_mc_ammx
        xdef    _mpeg_idct8x8_sparse_dc_mc_ammx
        xdef    _mpeg_idct8x8_sparse4x4_mc_ammx
        xdef    _mpeg_macroblock_mc_ammx

        section .text,code

; ======================================================================
; uint32_t get_ccc(void)
; ======================================================================
_get_ccc:
        movec.l CCC,d0
        rts

; ======================================================================
; Constants for Loeffler Fast IDCT (Q1.15 Fixed-Point)
; ======================================================================
        cnop    0,8
_fast_c4:
        dc.w    11585,11585,11585,11585         ; c4 = 0.35355 = 1/sqrt(8)
_fast_c2_s2:
        dc.w    15137,6270,15137,6270           ; c2 = 0.4619, s2 = 0.1913
_fast_s2_mc2:
        dc.w    6270,-15137,6270,-15137
_fast_odd1:
        dc.w    16069,13623,9102,3196           ; [c1, c3, c5, c7]
_fast_odd3:
        dc.w    13623,-3196,-16069,-9102        ; [c3, -c7, -c1, -c5]
_fast_odd5:
        dc.w    9102,-16069,3196,13623          ; [c5, -c1, c7, c3]
_fast_odd7:
        dc.w    3196,-9102,13623,-16069         ; [c7, -c5, c3, -c1]
_zero_vec:
        dc.w    0,0,0,0

; ======================================================================
; void mpeg_mc_residual_add_ammx(const uint8_t *ref, const int16_t *residual,
;                               uint8_t *dst, int stride)
; High-Speed Isolated Motion Compensation Residual Addition Kernel
; ======================================================================
_mpeg_mc_residual_add_ammx:
        movem.l d2-d3/a2-a4,-(sp)
        move.l  24(sp),a0               ; a0 = ref
        move.l  28(sp),a1               ; a1 = residual
        move.l  32(sp),a2               ; a2 = dst
        move.l  36(sp),d3               ; d3 = stride

        lea     _zero_vec,a3
        load    (a3),e23                ; e23 = [0, 0, 0, 0]

        moveq   #8-1,d2
.mc_loop:
        load    (a0),e0                 ; 8 reference prediction bytes
        vperm   #$08090a0b,e23,e0,e1    ; unpack ref[0..3]
        vperm   #$0c0d0e0f,e23,e0,e2    ; unpack ref[4..7]

        load    (a1)+,e8                ; residual[0..3]
        load    (a1)+,e12               ; residual[4..7]

        paddw   e1,e8,e8                ; ref[0..3] + res[0..3]
        paddw   e2,e12,e12              ; ref[4..7] + res[4..7]

        packuswb e8,e12,e0              ; clamp [0..255] and pack to 8 bytes
        store   e0,(a2)

        adda.l  d3,a0
        adda.l  d3,a2
        dbra    d2,.mc_loop

        movem.l (sp)+,d2-d3/a2-a4
        rts

; ======================================================================
; void mpeg_idct8x8_sparse_dc_mc_ammx(const int16_t *src_raster,
;                                    const uint8_t *ref, uint8_t *dst, int stride)
; DC-Only Motion Compensation Fast-Path (~45 cycles for entire 8x8 block!)
; ======================================================================
_mpeg_idct8x8_sparse_dc_mc_ammx:
        movem.l d2-d4/a2-a3,-(sp)
        move.l  24(sp),a0               ; a0 = src_raster
        move.l  28(sp),a1               ; a1 = ref
        move.l  32(sp),a2               ; a2 = dst
        move.l  36(sp),d3               ; d3 = stride

        ; Calculate scalar DC residual: round(DC / 8)
        move.w  (a0),d0
        addq.w  #4,d0
        asr.w   #3,d0                   ; d0 = residual word

        ; Replicate d0 across 64-bit vector in e16 (move.l writes to lower 32-bit: bytes 4..7)
        move.w  d0,d1
        swap    d0
        move.w  d1,d0                   ; d0 = [res, res]
        move.l  d0,e16
        vperm   #$45674567,e16,e16,e16  ; e16 = [res, res, res, res]

        lea     _zero_vec,a3
        load    (a3),e23

        moveq   #8-1,d2
.dc_mc_loop:
        load    (a1),e0
        vperm   #$08090a0b,e23,e0,e1    ; ref[0..3]
        vperm   #$0c0d0e0f,e23,e0,e2    ; ref[4..7]

        paddw   e16,e1,e1               ; ref[0..3] + DC_residual
        paddw   e16,e2,e2               ; ref[4..7] + DC_residual

        packuswb e1,e2,e0               ; clamp [0..255]
        store   e0,(a2)

        adda.l  d3,a1
        adda.l  d3,a2
        dbra    d2,.dc_mc_loop

        movem.l (sp)+,d2-d4/a2-a3
        rts

; ======================================================================
; void mpeg_idct8x8_fused_mc_ammx(const int16_t *src_raster,
;                                 const uint8_t *ref, uint8_t *dst, int stride)
; Fused 2D 8x8 IDCT + Motion Compensation Residual Addition + PACKUSWB Direct Store
; ======================================================================
_mpeg_idct8x8_fused_mc_ammx:
        movem.l d2-d4/a2-a5,-(sp)
        move.l  32(sp),a2               ; a2 = src_raster
        move.l  36(sp),a3               ; a3 = ref
        move.l  40(sp),a4               ; a4 = dst
        move.l  44(sp),d3               ; d3 = stride

        suba.w  #264,sp
        move.l  sp,d0
        addq.l  #7,d0
        and.w   #$fff8,d0
        movea.l d0,a5                   ; a5 = 8-byte aligned tmp buffer (tmp1: 128 bytes, tmp2: 128 bytes)

        ; Preload IDCT constants
        lea     _fast_c4,a0
        load    (a0),e16                ; _fast_c4
        load    8(a0),e17               ; _fast_c2_s2
        load    16(a0),e18              ; _fast_s2_mc2
        load    24(a0),e19              ; _fast_odd1
        load    32(a0),e20              ; _fast_odd3
        load    40(a0),e21              ; _fast_odd5
        load    48(a0),e22              ; _fast_odd7
        lea     _zero_vec,a0
        load    (a0),e23                ; e23 = zero vector

        ; --- Step 1: Inlined Row IDCTs (scaled 0.5x, stored to tmp1) ---
        moveq   #8-1,d2
.row_idct:
        load    (a2),e0
        load    8(a2),e1

        vperm   #$014589cd,e0,e1,e2
        vperm   #$2367abef,e0,e1,e3

        pmulh   e2,e16,e4
        vperm   #$01010101,e4,e4,e5
        vperm   #$45454545,e4,e4,e6
        paddw   e6,e5,e7
        psubw   e6,e5,e8
        vperm   #$01890189,e7,e8,e9

        vperm   #$23232323,e2,e2,e14
        vperm   #$67676767,e2,e2,e15
        pmulh   e14,e17,e4
        pmulh   e15,e18,e5
        paddw   e5,e4,e4

        vperm   #$01010101,e9,e9,e5
        vperm   #$23232323,e9,e9,e6
        vperm   #$01010101,e4,e4,e7
        vperm   #$23232323,e4,e4,e8

        paddw   e7,e5,e10
        psubw   e7,e5,e11
        paddw   e8,e6,e12
        psubw   e8,e6,e13

        vperm   #$01890189,e10,e12,e14
        vperm   #$01890189,e13,e11,e15
        vperm   #$012389ab,e14,e15,e6   ; e6 = [s0/2, s1/2, s2/2, s3/2]

        pmulh   e3,e19,e8
        pmulh   e3,e20,e9
        pmulh   e3,e21,e10
        pmulh   e3,e22,e11

        vperm   #$018945cd,e8,e9,e12
        vperm   #$23ab67ef,e8,e9,e13
        paddw   e13,e12,e12
        vperm   #$45670123,e12,e12,e13
        paddw   e13,e12,e12

        vperm   #$018945cd,e10,e11,e13
        vperm   #$23ab67ef,e10,e11,e15
        paddw   e15,e13,e13
        vperm   #$45670123,e13,e13,e15
        paddw   e15,e13,e13

        vperm   #$012389ab,e12,e13,e7   ; e7 = [d0/2, d1/2, d2/2, d3/2]

        paddw   e7,e6,e0
        psubw   e7,e6,e1
        vperm   #$67452301,e1,e1,e1

        store   e0,(a5)
        store   e1,8(a5)
        lea     16(a2),a2
        lea     16(a5),a5
        dbra    d2,.row_idct

        ; --- Step 2: Inlined Transpose tmp1 -> tmp2 ---
        suba.w  #128,a5                 ; a5 = tmp1
        movea.l a5,a0                   ; a0 = tmp1
        lea     128(a5),a1              ; a1 = tmp2
        load    (a0),e0
        load    16(a0),e1
        load    32(a0),e2
        load    48(a0),e3
        vperm   #$018923ab,e0,e1,e4
        vperm   #$45cd67ef,e0,e1,e5
        vperm   #$018923ab,e2,e3,e6
        vperm   #$45cd67ef,e2,e3,e7
        vperm   #$012389ab,e4,e6,e8
        vperm   #$4567cdef,e4,e6,e9
        vperm   #$012389ab,e5,e7,e10
        vperm   #$4567cdef,e5,e7,e11
        load    64(a0),e0
        load    80(a0),e1
        load    96(a0),e2
        load    112(a0),e3
        vperm   #$018923ab,e0,e1,e4
        vperm   #$45cd67ef,e0,e1,e5
        vperm   #$018923ab,e2,e3,e6
        vperm   #$45cd67ef,e2,e3,e7
        vperm   #$012389ab,e4,e6,e12
        vperm   #$4567cdef,e4,e6,e13
        vperm   #$012389ab,e5,e7,e14
        vperm   #$4567cdef,e5,e7,e15
        store   e8,(a1)
        store   e12,8(a1)
        store   e9,16(a1)
        store   e13,24(a1)
        store   e10,32(a1)
        store   e14,40(a1)
        store   e11,48(a1)
        store   e15,56(a1)
        load    8(a0),e0
        load    24(a0),e1
        load    40(a0),e2
        load    56(a0),e3
        vperm   #$018923ab,e0,e1,e4
        vperm   #$45cd67ef,e0,e1,e5
        vperm   #$018923ab,e2,e3,e6
        vperm   #$45cd67ef,e2,e3,e7
        vperm   #$012389ab,e4,e6,e8
        vperm   #$4567cdef,e4,e6,e9
        vperm   #$012389ab,e5,e7,e10
        vperm   #$4567cdef,e5,e7,e11
        load    72(a0),e0
        load    88(a0),e1
        load    104(a0),e2
        load    120(a0),e3
        vperm   #$018923ab,e0,e1,e4
        vperm   #$45cd67ef,e0,e1,e5
        vperm   #$018923ab,e2,e3,e6
        vperm   #$45cd67ef,e2,e3,e7
        vperm   #$012389ab,e4,e6,e12
        vperm   #$4567cdef,e4,e6,e13
        vperm   #$012389ab,e5,e7,e14
        vperm   #$4567cdef,e5,e7,e15
        store   e8,64(a1)
        store   e12,72(a1)
        store   e9,80(a1)
        store   e13,88(a1)
        store   e10,96(a1)
        store   e14,104(a1)
        store   e11,112(a1)
        store   e15,120(a1)

        ; --- Step 3: Inlined Column IDCTs tmp2 -> tmp1 (normal scale) ---
        lea     128(a5),a2              ; tmp2
        movea.l a5,a0                   ; tmp1
        moveq   #8-1,d2
.col_idct:
        load    (a2),e0
        load    8(a2),e1

        vperm   #$014589cd,e0,e1,e2
        vperm   #$2367abef,e0,e1,e3

        pmulh   e2,e16,e4
        vperm   #$01010101,e4,e4,e5
        vperm   #$45454545,e4,e4,e6
        paddw   e6,e5,e7
        psubw   e6,e5,e8
        vperm   #$01890189,e7,e8,e9
        paddw   e9,e9,e9

        vperm   #$23232323,e2,e2,e14
        vperm   #$67676767,e2,e2,e15
        pmulh   e14,e17,e4
        pmulh   e15,e18,e5
        paddw   e5,e4,e4
        paddw   e4,e4,e4

        vperm   #$01010101,e9,e9,e5
        vperm   #$23232323,e9,e9,e6
        vperm   #$01010101,e4,e4,e7
        vperm   #$23232323,e4,e4,e8

        paddw   e7,e5,e10
        psubw   e7,e5,e11
        paddw   e8,e6,e12
        psubw   e8,e6,e13

        vperm   #$01890189,e10,e12,e14
        vperm   #$01890189,e13,e11,e15
        vperm   #$012389ab,e14,e15,e6   ; e6 = [s0, s1, s2, s3]

        pmulh   e3,e19,e8
        pmulh   e3,e20,e9
        pmulh   e3,e21,e10
        pmulh   e3,e22,e11

        vperm   #$018945cd,e8,e9,e12
        vperm   #$23ab67ef,e8,e9,e13
        paddw   e13,e12,e12
        vperm   #$45670123,e12,e12,e13
        paddw   e13,e12,e12
        paddw   e12,e12,e12

        vperm   #$018945cd,e10,e11,e13
        vperm   #$23ab67ef,e10,e11,e15
        paddw   e15,e13,e13
        vperm   #$45670123,e13,e13,e15
        paddw   e15,e13,e13
        paddw   e13,e13,e13

        vperm   #$012389ab,e12,e13,e7   ; e7 = [d0, d1, d2, d3]

        paddw   e7,e6,e0
        psubw   e7,e6,e1
        vperm   #$67452301,e1,e1,e1

        store   e0,(a0)
        store   e1,8(a0)
        lea     16(a2),a2
        lea     16(a0),a0
        dbra    d2,.col_idct

        ; --- Step 4: Transpose tmp1 + Unpack Ref Prediction + Residual Addition + PACKUSWB Direct Store ---
        movea.l a5,a0                   ; a0 = tmp1
        lea     _zero_vec,a1
        load    (a1),e23                ; e23 = zero vector

        ; Top Half (Rows 0..3)
        load    (a0),e0
        load    16(a0),e1
        load    32(a0),e2
        load    48(a0),e3
        vperm   #$018923ab,e0,e1,e4
        vperm   #$45cd67ef,e0,e1,e5
        vperm   #$018923ab,e2,e3,e6
        vperm   #$45cd67ef,e2,e3,e7
        vperm   #$012389ab,e4,e6,e8     ; Row 0 [0..3]
        vperm   #$4567cdef,e4,e6,e9     ; Row 1 [0..3]
        vperm   #$012389ab,e5,e7,e10    ; Row 2 [0..3]
        vperm   #$4567cdef,e5,e7,e11    ; Row 3 [0..3]

        load    64(a0),e0
        load    80(a0),e1
        load    96(a0),e2
        load    112(a0),e3
        vperm   #$018923ab,e0,e1,e4
        vperm   #$45cd67ef,e0,e1,e5
        vperm   #$018923ab,e2,e3,e6
        vperm   #$45cd67ef,e2,e3,e7
        vperm   #$012389ab,e4,e6,e12    ; Row 0 [4..7]
        vperm   #$4567cdef,e4,e6,e13    ; Row 1 [4..7]
        vperm   #$012389ab,e5,e7,e14    ; Row 2 [4..7]
        vperm   #$4567cdef,e5,e7,e15    ; Row 3 [4..7]

        ; Row 0
        paddw   e8,e8,e8
        paddw   e12,e12,e12
        load    (a3),e0
        vperm   #$08090a0b,e23,e0,e1
        vperm   #$0c0d0e0f,e23,e0,e2
        paddw   e1,e8,e8
        paddw   e2,e12,e12
        packuswb e8,e12,e0
        store   e0,(a4)
        adda.l  d3,a3
        adda.l  d3,a4

        ; Row 1
        paddw   e9,e9,e9
        paddw   e13,e13,e13
        load    (a3),e0
        vperm   #$08090a0b,e23,e0,e1
        vperm   #$0c0d0e0f,e23,e0,e2
        paddw   e1,e9,e9
        paddw   e2,e13,e13
        packuswb e9,e13,e1
        store   e1,(a4)
        adda.l  d3,a3
        adda.l  d3,a4

        ; Row 2
        paddw   e10,e10,e10
        paddw   e14,e14,e14
        load    (a3),e0
        vperm   #$08090a0b,e23,e0,e1
        vperm   #$0c0d0e0f,e23,e0,e2
        paddw   e1,e10,e10
        paddw   e2,e14,e14
        packuswb e10,e14,e2
        store   e2,(a4)
        adda.l  d3,a3
        adda.l  d3,a4

        ; Row 3
        paddw   e11,e11,e11
        paddw   e15,e15,e15
        load    (a3),e0
        vperm   #$08090a0b,e23,e0,e1
        vperm   #$0c0d0e0f,e23,e0,e2
        paddw   e1,e11,e11
        paddw   e2,e15,e15
        packuswb e11,e15,e3
        store   e3,(a4)
        adda.l  d3,a3
        adda.l  d3,a4

        ; Bottom Half (Rows 4..7)
        load    8(a0),e0
        load    24(a0),e1
        load    40(a0),e2
        load    56(a0),e3
        vperm   #$018923ab,e0,e1,e4
        vperm   #$45cd67ef,e0,e1,e5
        vperm   #$018923ab,e2,e3,e6
        vperm   #$45cd67ef,e2,e3,e7
        vperm   #$012389ab,e4,e6,e8     ; Row 4 [0..3]
        vperm   #$4567cdef,e4,e6,e9     ; Row 5 [0..3]
        vperm   #$012389ab,e5,e7,e10    ; Row 6 [0..3]
        vperm   #$4567cdef,e5,e7,e11    ; Row 7 [0..3]

        load    72(a0),e0
        load    88(a0),e1
        load    104(a0),e2
        load    120(a0),e3
        vperm   #$018923ab,e0,e1,e4
        vperm   #$45cd67ef,e0,e1,e5
        vperm   #$018923ab,e2,e3,e6
        vperm   #$45cd67ef,e2,e3,e7
        vperm   #$012389ab,e4,e6,e12    ; Row 4 [4..7]
        vperm   #$4567cdef,e4,e6,e13    ; Row 5 [4..7]
        vperm   #$012389ab,e5,e7,e14    ; Row 6 [4..7]
        vperm   #$4567cdef,e5,e7,e15    ; Row 7 [4..7]

        ; Row 4
        paddw   e8,e8,e8
        paddw   e12,e12,e12
        load    (a3),e0
        vperm   #$08090a0b,e23,e0,e1
        vperm   #$0c0d0e0f,e23,e0,e2
        paddw   e1,e8,e8
        paddw   e2,e12,e12
        packuswb e8,e12,e0
        store   e0,(a4)
        adda.l  d3,a3
        adda.l  d3,a4

        ; Row 5
        paddw   e9,e9,e9
        paddw   e13,e13,e13
        load    (a3),e0
        vperm   #$08090a0b,e23,e0,e1
        vperm   #$0c0d0e0f,e23,e0,e2
        paddw   e1,e9,e9
        paddw   e2,e13,e13
        packuswb e9,e13,e1
        store   e1,(a4)
        adda.l  d3,a3
        adda.l  d3,a4

        ; Row 6
        paddw   e10,e10,e10
        paddw   e14,e14,e14
        load    (a3),e0
        vperm   #$08090a0b,e23,e0,e1
        vperm   #$0c0d0e0f,e23,e0,e2
        paddw   e1,e10,e10
        paddw   e2,e14,e14
        packuswb e10,e14,e2
        store   e2,(a4)
        adda.l  d3,a3
        adda.l  d3,a4

        ; Row 7
        paddw   e11,e11,e11
        paddw   e15,e15,e15
        load    (a3),e0
        vperm   #$08090a0b,e23,e0,e1
        vperm   #$0c0d0e0f,e23,e0,e2
        paddw   e1,e11,e11
        paddw   e2,e15,e15
        packuswb e11,e15,e3
        store   e3,(a4)

        lea     264(sp),sp
        movem.l (sp)+,d2-d4/a2-a5
        rts

; ======================================================================
; void mpeg_idct8x8_sparse4x4_mc_ammx(const int16_t *src_raster,
;                                    const uint8_t *ref, uint8_t *dst, int stride)
; Fast 4x4 Sparse Exit: If coefficients outside top-left 4x4 are zero,
; computes only 4 rows and 4 columns; else falls back to full IDCT.
; ======================================================================
_mpeg_idct8x8_sparse4x4_mc_ammx:
        move.l  4(sp),a0                ; a0 = src_raster
        ; Check if rows 4..7 are completely zero
        load    64(a0),e0
        load    72(a0),e1
        load    80(a0),e2
        load    88(a0),e3
        por     e1,e0,e0
        por     e3,e2,e2
        load    96(a0),e1
        load    104(a0),e3
        por     e1,e0,e0
        por     e3,e2,e2
        load    112(a0),e1
        load    120(a0),e3
        por     e1,e0,e0
        por     e3,e2,e2
        por     e2,e0,e0

        ; Check if cols 4..7 in rows 0..3 are zero
        load    8(a0),e1
        load    24(a0),e2
        load    40(a0),e3
        load    56(a0),e4
        por     e1,e0,e0
        por     e3,e2,e2
        por     e4,e0,e0
        por     e2,e0,e0

        move.l  e0,d0
        vperm   #$45670123,e0,e0,e1
        move.l  e1,d1
        or.l    d1,d0
        bne.s   .sparse_full_idct

        ; Check DC-only condition on remaining 4x4
        move.l  (a0),d0                 ; [DC, AC1]
        tst.w   d0
        bne.s   .run_full_or_sparse4x4
        tst.l   4(a0)
        bne.s   .run_full_or_sparse4x4
        load    16(a0),e0
        load    32(a0),e1
        load    48(a0),e2
        por     e1,e0,e0
        por     e2,e0,e0
        move.l  e0,d0
        vperm   #$45670123,e0,e0,e1
        move.l  e1,d1
        or.l    d1,d0
        bne.s   .run_full_or_sparse4x4

        ; DC-Only exit!
        bra     _mpeg_idct8x8_sparse_dc_mc_ammx

.run_full_or_sparse4x4:
        ; Fallback to full fused IDCT
        bra     _mpeg_idct8x8_fused_mc_ammx

.sparse_full_idct:
        bra     _mpeg_idct8x8_fused_mc_ammx

; ======================================================================
; void mpeg_macroblock_mc_ammx(const int16_t block_residuals[6][64],
;                             const uint8_t *ref_y, const uint8_t *ref_cb, const uint8_t *ref_cr,
;                             uint8_t *dst_y, uint8_t *dst_cb, uint8_t *dst_cr,
;                             int luma_stride, int chroma_stride)
; Full 16x16 Macroblock Motion Compensation (4 Y blocks, 1 Cb, 1 Cr)
; ======================================================================
_mpeg_macroblock_mc_ammx:
        movem.l d2-d7/a2-a6,-(sp)       ; 11 registers = 44 bytes + 4 = 48 bytes offset
        ; 48(sp)  = block_residuals
        ; 52(sp)  = ref_y
        ; 56(sp)  = ref_cb
        ; 60(sp)  = ref_cr
        ; 64(sp)  = dst_y
        ; 68(sp)  = dst_cb
        ; 72(sp)  = dst_cr
        ; 76(sp)  = luma_stride
        ; 80(sp)  = chroma_stride

        move.l  48(sp),a2               ; a2 = block_residuals pointer
        move.l  52(sp),a3               ; a3 = ref_y
        move.l  64(sp),a4               ; a4 = dst_y
        move.l  76(sp),d6               ; d6 = luma_stride
        move.l  80(sp),d7               ; d7 = chroma_stride

        ; Block 0: Top-Left (0, 0)
        move.l  d6,-(sp)
        move.l  a4,-(sp)
        move.l  a3,-(sp)
        move.l  a2,-(sp)
        bsr     _mpeg_idct8x8_sparse4x4_mc_ammx
        lea     16(sp),sp

        ; Block 1: Top-Right (8, 0)
        lea     128(a2),a2
        lea     8(a4),a0
        lea     8(a3),a1
        move.l  d6,-(sp)
        move.l  a0,-(sp)
        move.l  a1,-(sp)
        move.l  a2,-(sp)
        bsr     _mpeg_idct8x8_sparse4x4_mc_ammx
        lea     16(sp),sp

        ; Block 2: Bottom-Left (0, 8)
        lea     128(a2),a2
        move.l  d6,d0
        lsl.l   #3,d0                   ; 8 * luma_stride
        lea     (a4,d0.l),a0
        lea     (a3,d0.l),a1
        move.l  d6,-(sp)
        move.l  a0,-(sp)
        move.l  a1,-(sp)
        move.l  a2,-(sp)
        bsr     _mpeg_idct8x8_sparse4x4_mc_ammx
        lea     16(sp),sp

        ; Block 3: Bottom-Right (8, 8)
        lea     128(a2),a2
        move.l  d6,d0
        lsl.l   #3,d0
        addq.l  #8,d0                   ; 8 * luma_stride + 8
        lea     (a4,d0.l),a0
        lea     (a3,d0.l),a1
        move.l  d6,-(sp)
        move.l  a0,-(sp)
        move.l  a1,-(sp)
        move.l  a2,-(sp)
        bsr     _mpeg_idct8x8_sparse4x4_mc_ammx
        lea     16(sp),sp

        ; Block 4: Cb (8x8)
        lea     128(a2),a2
        move.l  68(sp),a0               ; dst_cb
        move.l  56(sp),a1               ; ref_cb
        move.l  d7,-(sp)
        move.l  a0,-(sp)
        move.l  a1,-(sp)
        move.l  a2,-(sp)
        bsr     _mpeg_idct8x8_sparse4x4_mc_ammx
        lea     16(sp),sp

        ; Block 5: Cr (8x8)
        lea     128(a2),a2
        move.l  72(sp),a0               ; dst_cr
        move.l  60(sp),a1               ; ref_cr
        move.l  d7,-(sp)
        move.l  a0,-(sp)
        move.l  a1,-(sp)
        move.l  a2,-(sp)
        bsr     _mpeg_idct8x8_sparse4x4_mc_ammx
        lea     16(sp),sp

        movem.l (sp)+,d2-d7/a2-a6
        rts
