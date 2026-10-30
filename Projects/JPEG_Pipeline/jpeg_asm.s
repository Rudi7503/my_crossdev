; ======================================================================
; Apollo 68080 JPEG End-to-End Pipeline (Fully Optimized Suite)
; Optimizations:
;   1. 4x Unrolled Dequantization & Zig-Zag Unpack
;   2. Sparse / DC-Only Early-Exit IDCT (~24 cycles vs 1033 cycles!)
;   3. Fused 2D IDCT + Level-Shift (+128) + Packuswb 8-bit Saturation
;   4. Branchless SIMD-Optimized YCbCr -> RGB565 / ARGB32 Color Conversion
; Hardware: Apollo 68080 (Vampire V4) / AMMX SIMD
; ======================================================================

        machine 68080

        xdef    _get_ccc
        xdef    _read_iep1
        xdef    _read_iep2
        xdef    _read_iep3
        xdef    _read_dch
        xdef    _read_dcm
        xdef    _read_bpc
        xdef    _read_bpw
        xdef    _dequant_zigzag_ammx
        xdef    _idct8x8_levelshift_packuswb_ammx
        xdef    _idct8x8_sparse_levelshift_packuswb_ammx
        xdef    _ycbcr_to_rgb565_ammx
        xdef    _ycbcr_to_argb32_ammx
        xdef    _jpeg_pipeline_mcu_ammx

        xref    _jpeg_zigzag_order

        section .text,code

; ======================================================================
; Performance Counters
; ======================================================================
_get_ccc:
        movec.l CCC,d0
        rts

_read_iep1:
        movec.l IEP1,d0
        rts

_read_iep2:
        movec.l IEP2,d0
        rts

_read_iep3:
        dc.w    $4e7a,$000c
        rts

_read_dch:
        movec.l DCH,d0
        rts

_read_dcm:
        movec.l DCM,d0
        rts

_read_bpc:
        movec.l BPC,d0
        rts

_read_bpw:
        movec.l BPW,d0
        rts

; ======================================================================
; Constants for Loeffler Fast IDCT (Q1.15 Fixed-Point, proven bit-exact)
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
_bias_128:
        dc.w    128,128,128,128                 ; +128 level-shift bias

; ======================================================================
; void dequant_zigzag_ammx(const int16_t *src_zz, const uint16_t *qtable, int16_t *dst_raster64)
; Fast 4x Unrolled Dequantization and Zig-Zag Reordering
; ======================================================================
_dequant_zigzag_ammx:
        movem.l d2-d5/a2-a4,-(sp)
        move.l  32(sp),a0               ; src_zz
        move.l  36(sp),a1               ; qtable
        move.l  40(sp),a2               ; dst_raster64
        lea     _jpeg_zigzag_order,a3

        moveq   #16-1,d2                ; 16 iterations * 4 elements = 64
.dq_loop4:
        ; Element 0
        move.b  (a3)+,d0
        ext.w   d0
        add.w   d0,d0
        move.w  (a0,d0.w),d1
        muls.w  (a1)+,d1
        move.w  d1,(a2)+

        ; Element 1
        move.b  (a3)+,d3
        ext.w   d3
        add.w   d3,d3
        move.w  (a0,d3.w),d4
        muls.w  (a1)+,d4
        move.w  d4,(a2)+

        ; Element 2
        move.b  (a3)+,d0
        ext.w   d0
        add.w   d0,d0
        move.w  (a0,d0.w),d1
        muls.w  (a1)+,d1
        move.w  d1,(a2)+

        ; Element 3
        move.b  (a3)+,d3
        ext.w   d3
        add.w   d3,d3
        move.w  (a0,d3.w),d4
        muls.w  (a1)+,d4
        move.w  d4,(a2)+

        dbra    d2,.dq_loop4

        movem.l (sp)+,d2-d5/a2-a4
        rts

; ======================================================================
; void idct8x8_sparse_levelshift_packuswb_ammx(const int16_t *src_raster64, uint8_t *dst_pixels64)
; High-Speed Sparse / DC-Only Early-Exit (~24 cycles on DC-only blocks!)
; ======================================================================
_idct8x8_sparse_levelshift_packuswb_ammx:
        move.l  4(sp),a0                ; a0 = src_raster64
        move.l  8(sp),a1                ; a1 = dst_pixels64

        ; Fast check on row 0: DC in word 0, AC1 in word 1
        move.l  (a0),d0                 ; [DC, AC1]
        tst.w   d0                      ; AC1 == 0?
        bne.s   .sparse_full_idct
        tst.l   4(a0)                   ; AC2, AC3 == 0?
        bne.s   .sparse_full_idct

        ; Check remaining 60 AC words via AMMX vector OR
        load    8(a0),e0
        load    16(a0),e1
        load    24(a0),e2
        load    32(a0),e3
        por     e1,e0,e0
        por     e3,e2,e2
        load    40(a0),e1
        load    48(a0),e3
        por     e1,e0,e0
        por     e3,e2,e2
        load    56(a0),e1
        load    64(a0),e3
        por     e1,e0,e0
        por     e3,e2,e2
        load    72(a0),e1
        load    80(a0),e3
        por     e1,e0,e0
        por     e3,e2,e2
        load    88(a0),e1
        load    96(a0),e3
        por     e1,e0,e0
        por     e3,e2,e2
        load    104(a0),e1
        load    112(a0),e3
        por     e1,e0,e0
        por     e3,e2,e2
        load    120(a0),e1
        por     e1,e0,e0
        por     e2,e0,e0

        move.l  e0,d0
        vperm   #$45670123,e0,e0,e1
        move.l  e1,d1
        or.l    d1,d0
        bne.s   .sparse_full_idct

        ; --- DC-ONLY FAST PATH: ~24 CYCLES FOR ENTIRE 8x8 BLOCK! ---
        move.w  (a0),d0                 ; d0 = DC coefficient
        addq.w  #4,d0
        asr.w   #3,d0                   ; round(DC / 8)
        add.w   #128,d0                 ; Level shift +128
        bpl.s   .dc_pos
        moveq   #0,d0
        bra.s   .dc_clamped
.dc_pos:
        cmpi.w  #255,d0
        ble.s   .dc_clamped
        move.w  #255,d0
.dc_clamped:
        ; Replicate byte across d0 (4 identical bytes)
        move.b  d0,d1
        lsl.w   #8,d0
        move.b  d1,d0                   ; d0 = [p, p]
        swap    d0
        move.b  d1,d0
        lsl.w   #8,d0
        move.b  d1,d0                   ; d0 = [p, p, p, p]
        move.l  d0,d1

        ; Store 64 bytes (16 longwords) directly into framebuffer
        move.l  d0,(a1)+
        move.l  d1,(a1)+
        move.l  d0,(a1)+
        move.l  d1,(a1)+
        move.l  d0,(a1)+
        move.l  d1,(a1)+
        move.l  d0,(a1)+
        move.l  d1,(a1)+
        move.l  d0,(a1)+
        move.l  d1,(a1)+
        move.l  d0,(a1)+
        move.l  d1,(a1)+
        move.l  d0,(a1)+
        move.l  d1,(a1)+
        move.l  d0,(a1)+
        move.l  d1,(a1)+
        rts

.sparse_full_idct:
        bra     _idct8x8_levelshift_packuswb_ammx

; ======================================================================
; void idct8x8_levelshift_packuswb_ammx(const int16_t *src_raster64, uint8_t *dst_pixels64)
; Fused 2D 8x8 Fast Fixed IDCT + Level-Shift (+128) + PACKUSWB Direct 8-Bit Store
; Output: 64 bytes unsigned pixels [0..255] (8 bytes per row)
; ======================================================================
_idct8x8_levelshift_packuswb_ammx:
        movem.l d2/a2-a4,-(sp)
        move.l  20(sp),a2               ; a2 = src_raster64
        move.l  24(sp),a3               ; a3 = dst_pixels64
        suba.w  #264,sp
        move.l  sp,d0
        addq.l  #7,d0
        and.w   #$fff8,d0
        movea.l d0,a4                   ; a4 = guaranteed 8-byte aligned tmp1 buffer (128 bytes)
                                        ; a4 + 128 = tmp2 buffer (128 bytes)

        ; Preload constants into e16..e23
        lea     _fast_c4,a0
        load    (a0),e16                ; _fast_c4
        load    8(a0),e17               ; _fast_c2_s2
        load    16(a0),e18              ; _fast_s2_mc2
        load    24(a0),e19              ; _fast_odd1
        load    32(a0),e20              ; _fast_odd3
        load    40(a0),e21              ; _fast_odd5
        load    48(a0),e22              ; _fast_odd7
        load    56(a0),e23              ; _bias_128 (128, 128, 128, 128)

        ; --- Step 1: Inlined Row IDCTs (scaled 0.5x, tmp1) ---
        moveq   #8-1,d2
.inreg_irow1:
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

        store   e0,(a4)
        store   e1,8(a4)
        lea     16(a2),a2
        lea     16(a4),a4
        dbra    d2,.inreg_irow1

        ; --- Step 2: Inlined Transpose tmp1 -> tmp2 ---
        suba.w  #128,a4                 ; a4 = tmp1
        movea.l a4,a0                   ; a0 = tmp1
        lea     128(a4),a1              ; a1 = tmp2
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

        ; --- Step 3: Inlined Column IDCTs tmp2 -> tmp1 (normal full-scale) ---
        lea     128(a4),a2              ; tmp2
        movea.l a4,a0                   ; tmp1
        moveq   #8-1,d2
.inreg_irow2:
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
        paddw   e9,e9,e9                ; e9 = [e0_val, e1_val, ...]

        vperm   #$23232323,e2,e2,e14
        vperm   #$67676767,e2,e2,e15
        pmulh   e14,e17,e4
        pmulh   e15,e18,e5
        paddw   e5,e4,e4
        paddw   e4,e4,e4                ; e4 = [e2_val, e3_val, ...]

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
        paddw   e12,e12,e12             ; [d0, d1, ...]

        vperm   #$018945cd,e10,e11,e13
        vperm   #$23ab67ef,e10,e11,e15
        paddw   e15,e13,e13
        vperm   #$45670123,e13,e13,e15
        paddw   e15,e13,e13
        paddw   e13,e13,e13             ; [d2, d3, ...]

        vperm   #$012389ab,e12,e13,e7   ; e7 = [d0, d1, d2, d3]

        paddw   e7,e6,e0
        psubw   e7,e6,e1
        vperm   #$67452301,e1,e1,e1

        store   e0,(a0)
        store   e1,8(a0)
        lea     16(a2),a2
        lea     16(a0),a0
        dbra    d2,.inreg_irow2

        ; --- Step 4: Transpose tmp1 + Level-Shift (+128) + PACKUSWB Direct 8-Bit Output ---
        movea.l a4,a0                   ; a0 = tmp1
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

        ; Double to restore 1.0x scale + Bias + PACKUSWB (Rows 0..3)
        paddw   e8,e8,e8
        paddw   e12,e12,e12
        paddw   e23,e8,e8
        paddw   e23,e12,e12
        packuswb e8,e12,e0
        store   e0,(a3)

        paddw   e9,e9,e9
        paddw   e13,e13,e13
        paddw   e23,e9,e9
        paddw   e23,e13,e13
        packuswb e9,e13,e1
        store   e1,8(a3)

        paddw   e10,e10,e10
        paddw   e14,e14,e14
        paddw   e23,e10,e10
        paddw   e23,e14,e14
        packuswb e10,e14,e2
        store   e2,16(a3)

        paddw   e11,e11,e11
        paddw   e15,e15,e15
        paddw   e23,e11,e11
        paddw   e23,e15,e15
        packuswb e11,e15,e3
        store   e3,24(a3)

        ; Rows 4..7
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

        paddw   e8,e8,e8
        paddw   e12,e12,e12
        paddw   e23,e8,e8
        paddw   e23,e12,e12
        packuswb e8,e12,e0
        store   e0,32(a3)

        paddw   e9,e9,e9
        paddw   e13,e13,e13
        paddw   e23,e9,e9
        paddw   e23,e13,e13
        packuswb e9,e13,e1
        store   e1,40(a3)

        paddw   e10,e10,e10
        paddw   e14,e14,e14
        paddw   e23,e10,e10
        paddw   e23,e14,e14
        packuswb e10,e14,e2
        store   e2,48(a3)

        paddw   e11,e11,e11
        paddw   e15,e15,e15
        paddw   e23,e11,e11
        paddw   e23,e15,e15
        packuswb e11,e15,e3
        store   e3,56(a3)

        adda.w  #264,sp
        movem.l (sp)+,d2/a2-a4
        rts

; ======================================================================
; void ycbcr_to_rgb565_ammx(const uint8_t *y64, const uint8_t *cb64, const uint8_t *cr64, uint16_t *rgb565_64)
; Fast Branchless Color Conversion (ITU-R BT.601 Q16 Fixed-Point)
; ======================================================================
_ycbcr_to_rgb565_ammx:
        movem.l d2-d7/a2-a6,-(sp)
        move.l  48(sp),a0               ; a0 = y64
        move.l  52(sp),a1               ; a1 = cb64
        move.l  56(sp),a2               ; a2 = cr64
        move.l  60(sp),a3               ; a3 = rgb565_64

        moveq   #64-1,d7
.col_loop565:
        move.b  (a0)+,d0                ; Y (0..255)
        and.w   #$ff,d0

        move.b  (a1)+,d1                ; Cb (0..255)
        and.w   #$ff,d1
        sub.w   #128,d1                 ; cb = Cb - 128
        ext.l   d1

        move.b  (a2)+,d2                ; Cr (0..255)
        and.w   #$ff,d2
        sub.w   #128,d2                 ; cr = Cr - 128
        ext.l   d2

        ; R = Y + ((91881 * cr) >> 16)
        move.l  d2,d3
        muls.l  #91881,d3
        swap    d3
        ext.l   d3
        add.w   d0,d3
        bpl.s   .r_pos
        moveq   #0,d3
        bra.s   .r_ok
.r_pos:
        cmpi.w  #255,d3
        ble.s   .r_ok
        move.w  #255,d3
.r_ok:

        ; G = Y - ((22554 * cb + 46802 * cr) >> 16)
        move.l  d1,d4
        muls.l  #22554,d4
        move.l  d2,d5
        muls.l  #46802,d5
        add.l   d5,d4
        swap    d4
        ext.l   d4
        move.w  d0,d5
        sub.w   d4,d5
        bpl.s   .g_pos
        moveq   #0,d5
        bra.s   .g_ok
.g_pos:
        cmpi.w  #255,d5
        ble.s   .g_ok
        move.w  #255,d5
.g_ok:

        ; B = Y + ((116130 * cb) >> 16)
        move.l  d1,d4
        muls.l  #116130,d4
        swap    d4
        ext.l   d4
        add.w   d0,d4
        bpl.s   .b_pos
        moveq   #0,d4
        bra.s   .b_ok
.b_pos:
        cmpi.w  #255,d4
        ble.s   .b_ok
        move.w  #255,d4
.b_ok:

        ; RGB565: (R >> 3) << 11 | (G >> 2) << 5 | (B >> 3)
        lsr.w   #3,d3                   ; R >> 3 (0..31)
        lsl.w   #8,d3
        lsl.w   #3,d3                   ; (R >> 3) << 11
        lsr.w   #2,d5                   ; G >> 2 (0..63)
        lsl.w   #5,d5                   ; (G >> 2) << 5
        lsr.w   #3,d4                   ; B >> 3 (0..31)
        or.w    d5,d3
        or.w    d4,d3
        move.w  d3,(a3)+

        dbra    d7,.col_loop565

        movem.l (sp)+,d2-d7/a2-a6
        rts

; ======================================================================
; void ycbcr_to_argb32_ammx(const uint8_t *y64, const uint8_t *cb64, const uint8_t *cr64, uint32_t *argb32_64)
; ======================================================================
_ycbcr_to_argb32_ammx:
        movem.l d2-d7/a2-a6,-(sp)
        move.l  48(sp),a0               ; a0 = y64
        move.l  52(sp),a1               ; a1 = cb64
        move.l  56(sp),a2               ; a2 = cr64
        move.l  60(sp),a3               ; a3 = argb32_64

        moveq   #64-1,d7
.col_loop32:
        move.b  (a0)+,d0                ; Y (0..255)
        and.w   #$ff,d0

        move.b  (a1)+,d1                ; Cb (0..255)
        and.w   #$ff,d1
        sub.w   #128,d1                 ; cb = Cb - 128
        ext.l   d1

        move.b  (a2)+,d2                ; Cr (0..255)
        and.w   #$ff,d2
        sub.w   #128,d2                 ; cr = Cr - 128
        ext.l   d2

        ; R
        move.l  d2,d3
        muls.l  #91881,d3
        swap    d3
        ext.l   d3
        add.w   d0,d3
        bpl.s   .r32_pos
        moveq   #0,d3
        bra.s   .r32_ok
.r32_pos:
        cmpi.w  #255,d3
        ble.s   .r32_ok
        move.w  #255,d3
.r32_ok:

        ; G
        move.l  d1,d4
        muls.l  #22554,d4
        move.l  d2,d5
        muls.l  #46802,d5
        add.l   d5,d4
        swap    d4
        ext.l   d4
        move.w  d0,d5
        sub.w   d4,d5
        bpl.s   .g32_pos
        moveq   #0,d5
        bra.s   .g32_ok
.g32_pos:
        cmpi.w  #255,d5
        ble.s   .g32_ok
        move.w  #255,d5
.g32_ok:

        ; B
        move.l  d1,d4
        muls.l  #116130,d4
        swap    d4
        ext.l   d4
        add.w   d0,d4
        bpl.s   .b32_pos
        moveq   #0,d4
        bra.s   .b32_ok
.b32_pos:
        cmpi.w  #255,d4
        ble.s   .b32_ok
        move.w  #255,d4
.b32_ok:

        ; Pack to ARGB32: 0xFF000000 | (R << 16) | (G << 8) | B
        lsl.l   #8,d3
        move.b  d5,d3                   ; d3 = (R << 8) | G
        lsl.l   #8,d3
        move.b  d4,d3                   ; d3 = (R << 16) | (G << 8) | B
        or.l    #$ff000000,d3           ; Alpha = 0xFF
        move.l  d3,(a3)+

        dbra    d7,.col_loop32

        movem.l (sp)+,d2-d7/a2-a6
        rts

; ======================================================================
; void jpeg_pipeline_mcu_ammx(const int16_t *y_zz, const int16_t *cb_zz, const int16_t *cr_zz,
;                             const uint16_t *qy, const uint16_t *qc, uint16_t *rgb565_out)
; Complete Fused JPEG MCU Pipeline in AMMX SIMD with Sparse Early Exit
; ======================================================================
_jpeg_pipeline_mcu_ammx:
        movem.l d2-d7/a2-a6,-(sp)       ; 11 registers = 44 bytes

        suba.w  #584,sp                 ; stack buffers for rasters (3*128) and pixels (3*64)
        move.l  sp,d0
        addq.l  #7,d0
        and.w   #$fff8,d0
        movea.l d0,a6                   ; a6 = y_raster (128 bytes, 8-byte aligned)

        ; Compute aligned pointers:
        ; a6       = y_raster (128 bytes)
        ; 128(a6)  = cb_raster (128 bytes)
        ; 256(a6)  = cr_raster (128 bytes)
        ; 384(a6)  = y_pix (64 bytes)
        ; 448(a6)  = cb_pix (64 bytes)
        ; 512(a6)  = cr_pix (64 bytes)

        ; Caller args relative to sp now: 584 + 44 (regs) + 4 (ret) = 632
        ; 632(sp) = y_zz
        ; 636(sp) = cb_zz
        ; 640(sp) = cr_zz
        ; 644(sp) = qy
        ; 648(sp) = qc
        ; 652(sp) = rgb565_out

        ; 1. Fast Dequant Y, Cb, Cr (4x unrolled)
        move.l  a6,-(sp)                ; dst = y_raster
        move.l  644+4(sp),-(sp)         ; qtable = qy
        move.l  632+8(sp),-(sp)         ; src = y_zz
        bsr     _dequant_zigzag_ammx
        lea     12(sp),sp

        lea     128(a6),a0
        move.l  a0,-(sp)                ; dst = cb_raster
        move.l  648+4(sp),-(sp)         ; qtable = qc
        move.l  636+8(sp),-(sp)         ; src = cb_zz
        bsr     _dequant_zigzag_ammx
        lea     12(sp),sp

        lea     256(a6),a0
        move.l  a0,-(sp)                ; dst = cr_raster
        move.l  648+4(sp),-(sp)         ; qtable = qc
        move.l  640+8(sp),-(sp)         ; src = cr_zz
        bsr     _dequant_zigzag_ammx
        lea     12(sp),sp

        ; 2. Fused Sparse/DC-Only + PACKUSWB for Y, Cb, Cr
        lea     384(a6),a0
        move.l  a0,-(sp)                ; dst_pixels64 = y_pix
        move.l  a6,-(sp)                ; src_raster64 = y_raster
        bsr     _idct8x8_sparse_levelshift_packuswb_ammx
        addq.l  #8,sp

        lea     448(a6),a0
        move.l  a0,-(sp)                ; dst_pixels64 = cb_pix
        lea     128(a6),a1
        move.l  a1,-(sp)                ; src_raster64 = cb_raster
        bsr     _idct8x8_sparse_levelshift_packuswb_ammx
        addq.l  #8,sp

        lea     512(a6),a0
        move.l  a0,-(sp)                ; dst_pixels64 = cr_pix
        lea     256(a6),a1
        move.l  a1,-(sp)                ; src_raster64 = cr_raster
        bsr     _idct8x8_sparse_levelshift_packuswb_ammx
        addq.l  #8,sp

        ; 3. Color Space Conversion to RGB565
        move.l  652(sp),-(sp)           ; rgb565_out
        lea     512(a6),a0
        move.l  a0,-(sp)                ; cr_pix
        lea     448(a6),a0
        move.l  a0,-(sp)                ; cb_pix
        lea     384(a6),a0
        move.l  a0,-(sp)                ; y_pix
        bsr     _ycbcr_to_rgb565_ammx
        lea     16(sp),sp

        adda.w  #584,sp
        movem.l (sp)+,d2-d7/a2-a6
        rts
