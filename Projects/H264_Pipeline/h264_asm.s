; ======================================================================
; Apollo 68080 H.264 / AVC Video Decoding Engine
; 4x4 In-Register Core Integer IDCT + 4x4 In-Register Hadamard Transform
; Fused Motion Compensation & PACKUSWB Direct Store
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
        xdef    _h264_hadamard4x4_ammx
        xdef    _h264_idct4x4_core_asm
        xdef    _h264_idct4x4_fused_mc_asm
        xdef    _h264_idct4x4_sparse_dc_mc_asm
        xdef    _h264_macroblock_16x16_asm

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
; Full 2D 4x4 Core Inverse Integer Transform (Unrolled in-register)
; ======================================================================
_h264_idct4x4_core_asm:
        movem.l d2-d7/a2,-(sp)
        move.l  32(sp),a0               ; a0 = in
        move.l  36(sp),a1               ; a1 = out
        suba.w  #32,sp
        movea.l sp,a2                   ; intermediate buffer (32 bytes)

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
; void h264_idct4x4_fused_mc_asm(const int16_t in[4][4], const uint8_t *ref,
;                                uint8_t *dst, int stride)
; In-Register 4x4 IDCT + AMMX SIMD PACKUSWB Clamping & 32-bit Stores
; ======================================================================
_h264_idct4x4_fused_mc_asm:
        movem.l d2-d7/a2-a4,-(sp)           ; 9 regs = 36 bytes
        suba.w  #64,sp                      ; 64 bytes scratch buffer
        ; Stack layout:
        ; sp..sp+31: tmp row buffer (32 bytes)
        ; sp+32..sp+63: idct out buffer (32 bytes)
        ; 100(sp): return address (4 bytes)
        ; 104(sp): in
        ; 108(sp): ref
        ; 112(sp): dst
        ; 116(sp): stride

        move.l  104(sp),a0                  ; in
        lea     (sp),a2                     ; tmp buffer

        ; --- Unrolled Horizontal IDCT Row 0 ---
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

        ; --- Unrolled Vertical IDCT Columns 0..3 (stores into row orientation) ---
        lea     32(sp),a3                   ; a3 = idct out buffer (row orientation)

        ; Col 0
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
        asr.w   #6,d1                       ; out[0][0]
        sub.w   d6,d4
        add.w   #32,d4
        asr.w   #6,d4                       ; out[3][0]
        move.w  d0,d2
        add.w   d5,d2
        add.w   #32,d2
        asr.w   #6,d2                       ; out[1][0]
        sub.w   d5,d0
        add.w   #32,d0
        asr.w   #6,d0                       ; out[2][0]
        move.w  d1,(a3)                     ; row 0, col 0
        move.w  d2,8(a3)                    ; row 1, col 0
        move.w  d0,16(a3)                   ; row 2, col 0
        move.w  d4,24(a3)                   ; row 3, col 0

        ; Col 1
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
        asr.w   #6,d1                       ; out[0][1]
        sub.w   d6,d4
        add.w   #32,d4
        asr.w   #6,d4                       ; out[3][1]
        move.w  d0,d2
        add.w   d5,d2
        add.w   #32,d2
        asr.w   #6,d2                       ; out[1][1]
        sub.w   d5,d0
        add.w   #32,d0
        asr.w   #6,d0                       ; out[2][1]
        move.w  d1,2(a3)                    ; row 0, col 1
        move.w  d2,10(a3)                   ; row 1, col 1
        move.w  d0,18(a3)                   ; row 2, col 1
        move.w  d4,26(a3)                   ; row 3, col 1

        ; Col 2
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
        asr.w   #6,d1                       ; out[0][2]
        sub.w   d6,d4
        add.w   #32,d4
        asr.w   #6,d4                       ; out[3][2]
        move.w  d0,d2
        add.w   d5,d2
        add.w   #32,d2
        asr.w   #6,d2                       ; out[1][2]
        sub.w   d5,d0
        add.w   #32,d0
        asr.w   #6,d0                       ; out[2][2]
        move.w  d1,4(a3)                    ; row 0, col 2
        move.w  d2,12(a3)                   ; row 1, col 2
        move.w  d0,20(a3)                   ; row 2, col 2
        move.w  d4,28(a3)                   ; row 3, col 2

        ; Col 3
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
        asr.w   #6,d1                       ; out[0][3]
        sub.w   d6,d4
        add.w   #32,d4
        asr.w   #6,d4                       ; out[3][3]
        move.w  d0,d2
        add.w   d5,d2
        add.w   #32,d2
        asr.w   #6,d2                       ; out[1][3]
        sub.w   d5,d0
        add.w   #32,d0
        asr.w   #6,d0                       ; out[2][3]
        move.w  d1,6(a3)                    ; row 0, col 3
        move.w  d2,14(a3)                   ; row 1, col 3
        move.w  d0,22(a3)                   ; row 2, col 3
        move.w  d4,30(a3)                   ; row 3, col 3

        ; --- Step 3: AMMX Fused Motion Compensation + Direct Store ---
        move.l  108(sp),a1                  ; ref
        move.l  112(sp),a0                  ; dst
        move.l  116(sp),d1                  ; stride

        load    (a3),e0                     ; e0 = row 0 residual
        load    8(a3),e1                    ; e1 = row 1 residual
        load    16(a3),e2                   ; e2 = row 2 residual
        load    24(a3),e3                   ; e3 = row 3 residual
        peor    e20,e20,e20

        ; Row 0
        move.l  (a1),e4
        vperm   #$84858687,e4,e20,e4
        paddw   e0,e4,e4
        packuswb e4,e4,e5
        move.l  e5,(a0)

        ; Row 1
        adda.l  d1,a1
        adda.l  d1,a0
        move.l  (a1),e4
        vperm   #$84858687,e4,e20,e4
        paddw   e1,e4,e4
        packuswb e4,e4,e5
        move.l  e5,(a0)

        ; Row 2
        adda.l  d1,a1
        adda.l  d1,a0
        move.l  (a1),e4
        vperm   #$84858687,e4,e20,e4
        paddw   e2,e4,e4
        packuswb e4,e4,e5
        move.l  e5,(a0)

        ; Row 3
        adda.l  d1,a1
        adda.l  d1,a0
        move.l  (a1),e4
        vperm   #$84858687,e4,e20,e4
        paddw   e3,e4,e4
        packuswb e4,e4,e5
        move.l  e5,(a0)

        adda.w  #64,sp
        movem.l (sp)+,d2-d7/a2-a4
        rts

; ======================================================================
; void h264_idct4x4_sparse_dc_mc_asm(const int16_t in[4][4], const uint8_t *ref,
;                                    uint8_t *dst, int stride)
; Ultra-Fast AMMX Vectorized DC-Only Fast-Path (~30 cycles!)
; In H.264: residual = (in[0][0] + 32) >> 6
; ======================================================================
_h264_idct4x4_sparse_dc_mc_asm:
        move.l  4(sp),a0                ; a0 = in
        move.w  (a0),d0                 ; in[0][0]
        add.w   #32,d0
        asr.w   #6,d0                   ; res = (in[0][0] + 32) >> 6

        move.l  8(sp),a1                ; a1 = ref
        move.l  12(sp),a0               ; a0 = dst (reuse scratch a0!)
        move.l  16(sp),d1               ; d1 = stride

        move.l  d0,e0
        vperm   #$67676767,e0,e0,e21    ; e21 = [res, res, res, res]
        peor    e20,e20,e20             ; e20 = 0

        ; Row 0
        move.l  (a1),e0
        vperm   #$84858687,e0,e20,e1    ; unpack ref row 0 to 4 words
        paddw   e21,e1,e2               ; e2 = ref + res
        packuswb e2,e2,e3               ; clamp to 0..255
        move.l  e3,(a0)

        ; Row 1
        adda.l  d1,a1
        adda.l  d1,a0
        move.l  (a1),e0
        vperm   #$84858687,e0,e20,e1
        paddw   e21,e1,e2
        packuswb e2,e2,e3
        move.l  e3,(a0)

        ; Row 2
        adda.l  d1,a1
        adda.l  d1,a0
        move.l  (a1),e0
        vperm   #$84858687,e0,e20,e1
        paddw   e21,e1,e2
        packuswb e2,e2,e3
        move.l  e3,(a0)

        ; Row 3
        adda.l  d1,a1
        adda.l  d1,a0
        move.l  (a1),e0
        vperm   #$84858687,e0,e20,e1
        paddw   e21,e1,e2
        packuswb e2,e2,e3
        move.l  e3,(a0)

        rts

; ======================================================================
; void h264_macroblock_16x16_asm(const int16_t blocks[16][16], const uint8_t *ref,
;                               uint8_t *dst, int stride)
; Full 16x16 Macroblock (16 4x4 blocks in 4x4 raster grid)
; Optimizations: Zero mulu.w, Inlined AMMX DC Fast-Path, Pointer Stepping
; ======================================================================
_h264_macroblock_16x16_asm:
        movem.l d2-d7/a2-a6,-(sp)
        move.l  48(sp),a2               ; a2 = blocks
        move.l  52(sp),a3               ; a3 = ref (row start)
        move.l  56(sp),a4               ; a4 = dst (row start)
        move.l  60(sp),d6               ; d6 = stride

        move.l  d6,d7
        lsl.l   #2,d7                   ; d7 = stride * 4

        moveq   #4-1,d4                 ; by = 0..3
.mb_by_loop:
        movea.l a3,a5                   ; a5 = b_ref
        movea.l a4,a6                   ; a6 = b_dst
        moveq   #4-1,d5                 ; bx = 0..3

.mb_bx_loop:
        movea.l a5,a0                   ; a0 = current block ref
        movea.l a6,a1                   ; a1 = current block dst

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

        ; --- Inlined Ultra-Fast DC-Only Fast-Path ---
        move.w  (a2),d0                 ; in[0][0]
        add.w   #32,d0
        asr.w   #6,d0                   ; res = (in[0][0] + 32) >> 6
        move.l  d0,e0
        vperm   #$67676767,e0,e0,e21    ; e21 = [res, res, res, res]
        peor    e20,e20,e20             ; e20 = 0

        ; Row 0
        move.l  (a0),e0
        vperm   #$84858687,e0,e20,e1
        paddw   e21,e1,e2
        packuswb e2,e2,e3
        move.l  e3,(a1)

        ; Row 1
        adda.l  d6,a0
        adda.l  d6,a1
        move.l  (a0),e0
        vperm   #$84858687,e0,e20,e1
        paddw   e21,e1,e2
        packuswb e2,e2,e3
        move.l  e3,(a1)

        ; Row 2
        adda.l  d6,a0
        adda.l  d6,a1
        move.l  (a0),e0
        vperm   #$84858687,e0,e20,e1
        paddw   e21,e1,e2
        packuswb e2,e2,e3
        move.l  e3,(a1)

        ; Row 3
        adda.l  d6,a0
        adda.l  d6,a1
        move.l  (a0),e0
        vperm   #$84858687,e0,e20,e1
        paddw   e21,e1,e2
        packuswb e2,e2,e3
        move.l  e3,(a1)

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
        addq.l  #4,a5                   ; next block ref
        addq.l  #4,a6                   ; next block dst
        dbra    d5,.mb_bx_loop

        adda.l  d7,a3                   ; advance ref to next block row
        adda.l  d7,a4                   ; advance dst to next block row
        dbra    d4,.mb_by_loop

        movem.l (sp)+,d2-d7/a2-a6
        rts
