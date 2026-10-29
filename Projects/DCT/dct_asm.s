; ======================================================================
; dct_asm.s - Apollo 68080 Discrete Cosine Transform Suite
; Targets: Apollo 68080 (Vampire V4) with AMMX SIMD & 68881/68080 FPU
; ======================================================================

        section .text,code
        xdef    _get_ccc
        xdef    _dct1d_float_asm
        xdef    _idct1d_float_asm
        xdef    _dct1d_fixed_scalar_asm
        xdef    _idct1d_fixed_scalar_asm
        xdef    _dct1d_fixed_ammx
        xdef    _idct1d_fixed_ammx
        xdef    _dct2d_8x8_float_asm
        xdef    _idct2d_8x8_float_asm
        xdef    _dct2d_8x8_fixed_scalar_asm
        xdef    _idct2d_8x8_fixed_scalar_asm
        xdef    _dct2d_8x8_fixed_ammx
        xdef    _idct2d_8x8_fixed_ammx
        xdef    _transpose8x8_ammx

        xref    _dct_matrix_f
        xref    _idct_matrix_f
        xref    _dct_matrix_q15
        xref    _idct_matrix_q15

_get_ccc:
        movec.l CCC,d0
        rts

; ======================================================================
; void dct1d_float_asm(const float *src, float *dst)
; ======================================================================
_dct1d_float_asm:
        move.l  a2,-(sp)
        move.l  8(sp),a0                ; a0 = src
        move.l  12(sp),a1               ; a1 = dst
        lea     _dct_matrix_f,a2        ; a2 = &dct_matrix_f[0][0]
        moveq   #8-1,d1                 ; 8 output points

.flt_loop_k:
        fmove.s (a0),fp0
        fmul.s  (a2)+,fp0

        fmove.s 4(a0),fp1
        fmul.s  (a2)+,fp1
        fadd.x  fp1,fp0

        fmove.s 8(a0),fp1
        fmul.s  (a2)+,fp1
        fadd.x  fp1,fp0

        fmove.s 12(a0),fp1
        fmul.s  (a2)+,fp1
        fadd.x  fp1,fp0

        fmove.s 16(a0),fp1
        fmul.s  (a2)+,fp1
        fadd.x  fp1,fp0

        fmove.s 20(a0),fp1
        fmul.s  (a2)+,fp1
        fadd.x  fp1,fp0

        fmove.s 24(a0),fp1
        fmul.s  (a2)+,fp1
        fadd.x  fp1,fp0

        fmove.s 28(a0),fp1
        fmul.s  (a2)+,fp1
        fadd.x  fp1,fp0

        fmove.s fp0,(a1)+
        dbra    d1,.flt_loop_k

        move.l  (sp)+,a2
        rts

; ======================================================================
; void idct1d_float_asm(const float *src, float *dst)
; ======================================================================
_idct1d_float_asm:
        move.l  a2,-(sp)
        move.l  8(sp),a0                ; a0 = src
        move.l  12(sp),a1               ; a1 = dst
        lea     _idct_matrix_f,a2
        moveq   #8-1,d1

.iflt_loop_k:
        fmove.s (a0),fp0
        fmul.s  (a2)+,fp0

        fmove.s 4(a0),fp1
        fmul.s  (a2)+,fp1
        fadd.x  fp1,fp0

        fmove.s 8(a0),fp1
        fmul.s  (a2)+,fp1
        fadd.x  fp1,fp0

        fmove.s 12(a0),fp1
        fmul.s  (a2)+,fp1
        fadd.x  fp1,fp0

        fmove.s 16(a0),fp1
        fmul.s  (a2)+,fp1
        fadd.x  fp1,fp0

        fmove.s 20(a0),fp1
        fmul.s  (a2)+,fp1
        fadd.x  fp1,fp0

        fmove.s 24(a0),fp1
        fmul.s  (a2)+,fp1
        fadd.x  fp1,fp0

        fmove.s 28(a0),fp1
        fmul.s  (a2)+,fp1
        fadd.x  fp1,fp0

        fmove.s fp0,(a1)+
        dbra    d1,.iflt_loop_k

        move.l  (sp)+,a2
        rts

; ======================================================================
; void dct1d_fixed_scalar_asm(const int16_t *src, int16_t *dst)
; ======================================================================
_dct1d_fixed_scalar_asm:
        movem.l d2-d5/a2,-(sp)          ; 5 registers = 20 bytes
        move.l  24(sp),a0               ; src (20 + 4 = 24)
        move.l  28(sp),a1               ; dst
        lea     _dct_matrix_q15,a2
        moveq   #8-1,d5

.scl_loop_k:
        move.l  #16384,d0

        move.w  (a0),d1
        move.w  (a2)+,d2
        muls.w  d1,d2
        add.l   d2,d0

        move.w  2(a0),d1
        move.w  (a2)+,d2
        muls.w  d1,d2
        add.l   d2,d0

        move.w  4(a0),d1
        move.w  (a2)+,d2
        muls.w  d1,d2
        add.l   d2,d0

        move.w  6(a0),d1
        move.w  (a2)+,d2
        muls.w  d1,d2
        add.l   d2,d0

        move.w  8(a0),d1
        move.w  (a2)+,d2
        muls.w  d1,d2
        add.l   d2,d0

        move.w  10(a0),d1
        move.w  (a2)+,d2
        muls.w  d1,d2
        add.l   d2,d0

        move.w  12(a0),d1
        move.w  (a2)+,d2
        muls.w  d1,d2
        add.l   d2,d0

        move.w  14(a0),d1
        move.w  (a2)+,d2
        muls.w  d1,d2
        add.l   d2,d0

        asr.l   #8,d0
        asr.l   #7,d0
        move.w  d0,(a1)+
        dbra    d5,.scl_loop_k

        movem.l (sp)+,d2-d5/a2
        rts

; ======================================================================
; void idct1d_fixed_scalar_asm(const int16_t *src, int16_t *dst)
; ======================================================================
_idct1d_fixed_scalar_asm:
        movem.l d2-d5/a2,-(sp)
        move.l  24(sp),a0
        move.l  28(sp),a1
        lea     _idct_matrix_q15,a2
        moveq   #8-1,d5

.iscl_loop_k:
        move.l  #16384,d0

        move.w  (a0),d1
        move.w  (a2)+,d2
        muls.w  d1,d2
        add.l   d2,d0

        move.w  2(a0),d1
        move.w  (a2)+,d2
        muls.w  d1,d2
        add.l   d2,d0

        move.w  4(a0),d1
        move.w  (a2)+,d2
        muls.w  d1,d2
        add.l   d2,d0

        move.w  6(a0),d1
        move.w  (a2)+,d2
        muls.w  d1,d2
        add.l   d2,d0

        move.w  8(a0),d1
        move.w  (a2)+,d2
        muls.w  d1,d2
        add.l   d2,d0

        move.w  10(a0),d1
        move.w  (a2)+,d2
        muls.w  d1,d2
        add.l   d2,d0

        move.w  12(a0),d1
        move.w  (a2)+,d2
        muls.w  d1,d2
        add.l   d2,d0

        move.w  14(a0),d1
        move.w  (a2)+,d2
        muls.w  d1,d2
        add.l   d2,d0

        asr.l   #8,d0
        asr.l   #7,d0
        move.w  d0,(a1)+
        dbra    d5,.iscl_loop_k

        movem.l (sp)+,d2-d5/a2
        rts

; ======================================================================
; void dct1d_fixed_ammx(const int16_t *src, int16_t *dst)
; 2-way parallel vector reduction with 32-bit aligned stores
; ======================================================================
_dct1d_fixed_ammx:
        move.l  4(sp),a0                ; a0 = src
        move.l  8(sp),a1                ; a1 = dst

        load    (a0),e0                 ; e0 = [x0, x1, x2, x3]
        load    8(a0),e1                ; e1 = [x4, x5, x6, x7]

        lea     _dct_matrix_q15,a0      ; a0 = &dct_matrix_q15[0][0]
        moveq   #4-1,d0                 ; 4 iterations (2 points per iteration)

.ammx_loop_2k:
        ; Point k:
        load    (a0),e2                 ; C_k[0..3]
        load    8(a0),e3                ; C_k[4..7]
        pmulh   e0,e2,e4
        pmulh   e1,e3,e5
        paddw   e5,e4,e4                ; e4 = [p0..p3] for k

        ; Point k+1:
        load    16(a0),e2               ; C_k+1[0..3]
        load    24(a0),e3               ; C_k+1[4..7]
        lea     32(a0),a0
        pmulh   e0,e2,e6
        pmulh   e1,e3,e7
        paddw   e7,e6,e6                ; e6 = [p0..p3] for k+1

        ; Combined horizontal reduction of e4 (k) and e6 (k+1):
        vperm   #$018945cd,e4,e6,e8     ; [a0, b0, a2, b2]
        vperm   #$23ab67ef,e4,e6,e9     ; [a1, b1, a3, b3]
        paddw   e9,e8,e8                ; [a0+a1, b0+b1, a2+a3, b2+b3]
        vperm   #$45670123,e8,e8,e10    ; [a2+a3, b2+b3, a0+a1, b0+b1]
        paddw   e10,e8,e8               ; [sum_a, sum_b, sum_a, sum_b]

        paddw   e8,e8,e8                ; * 2 for Q1.15

        move.l  e8,d1                   ; d1 = [sum_k (high 16), sum_k+1 (low 16)]
        move.l  d1,(a1)+                ; write both words!
        dbra    d0,.ammx_loop_2k
        rts

; ======================================================================
; void idct1d_fixed_ammx(const int16_t *src, int16_t *dst)
; 2-way parallel vector reduction with 32-bit aligned stores
; ======================================================================
_idct1d_fixed_ammx:
        move.l  4(sp),a0
        move.l  8(sp),a1

        load    (a0),e0
        load    8(a0),e1

        lea     _idct_matrix_q15,a0
        moveq   #4-1,d0

.iammx_loop_2k:
        ; Point k:
        load    (a0),e2
        load    8(a0),e3
        pmulh   e0,e2,e4
        pmulh   e1,e3,e5
        paddw   e5,e4,e4

        ; Point k+1:
        load    16(a0),e2
        load    24(a0),e3
        lea     32(a0),a0
        pmulh   e0,e2,e6
        pmulh   e1,e3,e7
        paddw   e7,e6,e6

        ; Combined horizontal reduction of e4 (k) and e6 (k+1):
        vperm   #$018945cd,e4,e6,e8
        vperm   #$23ab67ef,e4,e6,e9
        paddw   e9,e8,e8
        vperm   #$45670123,e8,e8,e10
        paddw   e10,e8,e8

        paddw   e8,e8,e8

        move.l  e8,d1
        move.l  d1,(a1)+
        dbra    d0,.iammx_loop_2k
        rts

; ======================================================================
; void dct2d_8x8_float_asm(const float *src, float *dst)
; ======================================================================
_dct2d_8x8_float_asm:
        movem.l d2-d3/a2-a4,-(sp)
        move.l  24(sp),a2               ; src
        move.l  28(sp),a3               ; dst
        suba.w  #256,sp                 ; tmp buffer (256 bytes)
        move.l  sp,a4

        ; Step 1: Row transform from src -> tmp
        moveq   #8-1,d2
.f2d_row1:
        move.l  a4,-(sp)
        move.l  a2,-(sp)
        jsr     _dct1d_float_asm
        addq.l  #8,sp
        lea     32(a2),a2
        lea     32(a4),a4
        dbra    d2,.f2d_row1

        ; Step 2: Transform columns directly: dst[k*8 + c] = sum_{r=0..7} D[k][r] * tmp[r*8 + c]
        moveq   #0,d2                   ; c = 0..7
.f2d_col_loop:
        lea     _dct_matrix_f,a0
        moveq   #8-1,d3                 ; k = 0..7

.f2d_k_loop:
        move.l  sp,a1
        move.w  d2,d0
        lsl.w   #2,d0                   ; c * 4
        adda.w  d0,a1                   ; a1 = &tmp[0*8 + c]

        fmove.s (a1),fp0
        fmul.s  (a0)+,fp0

        fmove.s 32(a1),fp1
        fmul.s  (a0)+,fp1
        fadd.x  fp1,fp0

        fmove.s 64(a1),fp1
        fmul.s  (a0)+,fp1
        fadd.x  fp1,fp0

        fmove.s 96(a1),fp1
        fmul.s  (a0)+,fp1
        fadd.x  fp1,fp0

        fmove.s 128(a1),fp1
        fmul.s  (a0)+,fp1
        fadd.x  fp1,fp0

        fmove.s 160(a1),fp1
        fmul.s  (a0)+,fp1
        fadd.x  fp1,fp0

        fmove.s 192(a1),fp1
        fmul.s  (a0)+,fp1
        fadd.x  fp1,fp0

        fmove.s 224(a1),fp1
        fmul.s  (a0)+,fp1
        fadd.x  fp1,fp0

        move.w  #7,d0
        sub.w   d3,d0                   ; k
        lsl.w   #3,d0                   ; k * 8
        add.w   d2,d0                   ; k * 8 + c
        lsl.w   #2,d0                   ; offset * 4
        fmove.s fp0,(a3,d0.w)

        dbra    d3,.f2d_k_loop

        addq.w  #1,d2
        cmp.w   #8,d2
        blt     .f2d_col_loop

        adda.w  #256,sp
        movem.l (sp)+,d2-d3/a2-a4
        rts

; ======================================================================
; void idct2d_8x8_float_asm(const float *src, float *dst)
; ======================================================================
_idct2d_8x8_float_asm:
        movem.l d2-d3/a2-a4,-(sp)
        move.l  24(sp),a2
        move.l  28(sp),a3
        suba.w  #256,sp
        move.l  sp,a4

        ; Step 1: Inverse transform columns: tmp[r*8 + c] = sum_{k=0..7} D^T[r][k] * src[k*8 + c]
        moveq   #0,d2
.if2d_col_loop:
        lea     _idct_matrix_f,a0
        moveq   #8-1,d3

.if2d_r_loop:
        move.l  a2,a1
        move.w  d2,d0
        lsl.w   #2,d0
        adda.w  d0,a1

        fmove.s (a1),fp0
        fmul.s  (a0)+,fp0

        fmove.s 32(a1),fp1
        fmul.s  (a0)+,fp1
        fadd.x  fp1,fp0

        fmove.s 64(a1),fp1
        fmul.s  (a0)+,fp1
        fadd.x  fp1,fp0

        fmove.s 96(a1),fp1
        fmul.s  (a0)+,fp1
        fadd.x  fp1,fp0

        fmove.s 128(a1),fp1
        fmul.s  (a0)+,fp1
        fadd.x  fp1,fp0

        fmove.s 160(a1),fp1
        fmul.s  (a0)+,fp1
        fadd.x  fp1,fp0

        fmove.s 192(a1),fp1
        fmul.s  (a0)+,fp1
        fadd.x  fp1,fp0

        fmove.s 224(a1),fp1
        fmul.s  (a0)+,fp1
        fadd.x  fp1,fp0

        move.w  #7,d0
        sub.w   d3,d0                   ; r
        lsl.w   #3,d0                   ; r * 8
        add.w   d2,d0                   ; r * 8 + c
        lsl.w   #2,d0
        fmove.s fp0,(a4,d0.w)

        dbra    d3,.if2d_r_loop

        addq.w  #1,d2
        cmp.w   #8,d2
        blt     .if2d_col_loop

        ; Step 2: Inverse transform rows from tmp -> dst
        moveq   #8-1,d2
.if2d_row_loop:
        move.l  a3,-(sp)
        move.l  a4,-(sp)
        jsr     _idct1d_float_asm
        addq.l  #8,sp
        lea     32(a4),a4
        lea     32(a3),a3
        dbra    d2,.if2d_row_loop

        adda.w  #256,sp
        movem.l (sp)+,d2-d3/a2-a4
        rts

; ======================================================================
; void dct2d_8x8_fixed_scalar_asm(const int16_t *src, int16_t *dst)
; ======================================================================
_dct2d_8x8_fixed_scalar_asm:
        movem.l d2-d4/a2-a4,-(sp)       ; 6 registers = 24 bytes
        move.l  28(sp),a2               ; src (24 + 4 = 28)
        move.l  32(sp),a3               ; dst (24 + 8 = 32)
        suba.w  #128,sp                 ; 128 bytes (64 words) for tmp
        move.l  sp,a4

        ; Step 1: Transform rows
        moveq   #8-1,d2
.qs2d_row_loop:
        move.l  a4,-(sp)
        move.l  a2,-(sp)
        jsr     _dct1d_fixed_scalar_asm
        addq.l  #8,sp
        lea     16(a2),a2
        lea     16(a4),a4
        dbra    d2,.qs2d_row_loop

        ; Step 2: Transform columns
        moveq   #0,d2                   ; c = 0..7
.qs2d_col_loop:
        lea     _dct_matrix_q15,a0
        moveq   #8-1,d3                 ; k = 0..7

.qs2d_k_loop:
        move.l  sp,a1
        lsl.w   #1,d2
        adda.w  d2,a1
        lsr.w   #1,d2

        move.l  #16384,d0

        move.w  (a1),d1
        move.w  (a0)+,d4
        muls.w  d1,d4
        add.l   d4,d0

        move.w  16(a1),d1
        move.w  (a0)+,d4
        muls.w  d1,d4
        add.l   d4,d0

        move.w  32(a1),d1
        move.w  (a0)+,d4
        muls.w  d1,d4
        add.l   d4,d0

        move.w  48(a1),d1
        move.w  (a0)+,d4
        muls.w  d1,d4
        add.l   d4,d0

        move.w  64(a1),d1
        move.w  (a0)+,d4
        muls.w  d1,d4
        add.l   d4,d0

        move.w  80(a1),d1
        move.w  (a0)+,d4
        muls.w  d1,d4
        add.l   d4,d0

        move.w  96(a1),d1
        move.w  (a0)+,d4
        muls.w  d1,d4
        add.l   d4,d0

        move.w  112(a1),d1
        move.w  (a0)+,d4
        muls.w  d1,d4
        add.l   d4,d0

        asr.l   #8,d0
        asr.l   #7,d0

        move.w  #7,d1
        sub.w   d3,d1                   ; k
        lsl.w   #3,d1                   ; k * 8
        add.w   d2,d1                   ; k * 8 + c
        lsl.w   #1,d1                   ; word offset
        move.w  d0,(a3,d1.w)

        dbra    d3,.qs2d_k_loop

        addq.w  #1,d2
        cmp.w   #8,d2
        blt     .qs2d_col_loop

        adda.w  #128,sp
        movem.l (sp)+,d2-d4/a2-a4
        rts

; ======================================================================
; void idct2d_8x8_fixed_scalar_asm(const int16_t *src, int16_t *dst)
; ======================================================================
_idct2d_8x8_fixed_scalar_asm:
        movem.l d2-d4/a2-a4,-(sp)       ; 6 registers = 24 bytes
        move.l  28(sp),a2               ; src (24 + 4 = 28)
        move.l  32(sp),a3               ; dst (24 + 8 = 32)
        suba.w  #128,sp                 ; 128 bytes (64 words) for tmp
        move.l  sp,a4

        ; Step 1: Inverse transform columns
        moveq   #0,d2
.iqs2d_col_loop:
        lea     _idct_matrix_q15,a0
        moveq   #8-1,d3

.iqs2d_r_loop:
        move.l  a2,a1
        lsl.w   #1,d2
        adda.w  d2,a1
        lsr.w   #1,d2

        move.l  #16384,d0

        move.w  (a1),d1
        move.w  (a0)+,d4
        muls.w  d1,d4
        add.l   d4,d0

        move.w  16(a1),d1
        move.w  (a0)+,d4
        muls.w  d1,d4
        add.l   d4,d0

        move.w  32(a1),d1
        move.w  (a0)+,d4
        muls.w  d1,d4
        add.l   d4,d0

        move.w  48(a1),d1
        move.w  (a0)+,d4
        muls.w  d1,d4
        add.l   d4,d0

        move.w  64(a1),d1
        move.w  (a0)+,d4
        muls.w  d1,d4
        add.l   d4,d0

        move.w  80(a1),d1
        move.w  (a0)+,d4
        muls.w  d1,d4
        add.l   d4,d0

        move.w  96(a1),d1
        move.w  (a0)+,d4
        muls.w  d1,d4
        add.l   d4,d0

        move.w  112(a1),d1
        move.w  (a0)+,d4
        muls.w  d1,d4
        add.l   d4,d0

        asr.l   #8,d0
        asr.l   #7,d0

        move.w  #7,d1
        sub.w   d3,d1
        lsl.w   #3,d1
        add.w   d2,d1
        lsl.w   #1,d1
        move.w  d0,(a4,d1.w)

        dbra    d3,.iqs2d_r_loop

        addq.w  #1,d2
        cmp.w   #8,d2
        blt     .iqs2d_col_loop

        ; Step 2: Inverse transform rows
        moveq   #8-1,d2
.iqs2d_row_loop:
        move.l  a3,-(sp)
        move.l  a4,-(sp)
        jsr     _idct1d_fixed_scalar_asm
        addq.l  #8,sp
        lea     16(a4),a4
        lea     16(a3),a3
        dbra    d2,.iqs2d_row_loop

        adda.w  #128,sp
        movem.l (sp)+,d2-d4/a2-a4
        rts

; ======================================================================
; void transpose8x8_ammx(const int16_t *src, int16_t *dst)
; 8x8 matrix transpose using AMMX SIMD vperm
; ======================================================================
_transpose8x8_ammx:
        move.l  4(sp),a0                ; a0 = src
        move.l  8(sp),a1                ; a1 = dst

        ; --- Submatrix M00 (rows 0..3, cols 0..3) ---
        load    (a0),e0                 ; L0
        load    16(a0),e1               ; L1
        load    32(a0),e2               ; L2
        load    48(a0),e3               ; L3

        vperm   #$018923ab,e0,e1,e4     ; T0 = [L0.0, L1.0, L0.1, L1.1]
        vperm   #$45cd67ef,e0,e1,e5     ; T1 = [L0.2, L1.2, L0.3, L1.3]
        vperm   #$018923ab,e2,e3,e6     ; T2 = [L2.0, L3.0, L2.1, L3.1]
        vperm   #$45cd67ef,e2,e3,e7     ; T3 = [L2.2, L3.2, L2.3, L3.3]

        vperm   #$012389ab,e4,e6,e8     ; M00_T[0] = [L0.0, L1.0, L2.0, L3.0]
        vperm   #$4567cdef,e4,e6,e9     ; M00_T[1] = [L0.1, L1.1, L2.1, L3.1]
        vperm   #$012389ab,e5,e7,e10    ; M00_T[2] = [L0.2, L1.2, L2.2, L3.2]
        vperm   #$4567cdef,e5,e7,e11    ; M00_T[3] = [L0.3, L1.3, L2.3, L3.3]

        ; --- Submatrix M10 (rows 4..7, cols 0..3) ---
        load    64(a0),e0               ; L4
        load    80(a0),e1               ; L5
        load    96(a0),e2               ; L6
        load    112(a0),e3              ; L7

        vperm   #$018923ab,e0,e1,e4
        vperm   #$45cd67ef,e0,e1,e5
        vperm   #$018923ab,e2,e3,e6
        vperm   #$45cd67ef,e2,e3,e7

        vperm   #$012389ab,e4,e6,e12    ; M10_T[0]
        vperm   #$4567cdef,e4,e6,e13    ; M10_T[1]
        vperm   #$012389ab,e5,e7,e14    ; M10_T[2]
        vperm   #$4567cdef,e5,e7,e15    ; M10_T[3]

        ; Store upper 4 rows of transposed matrix (M00_T is low quadword, M10_T is high quadword)
        store   e8,(a1)
        store   e12,8(a1)
        store   e9,16(a1)
        store   e13,24(a1)
        store   e10,32(a1)
        store   e14,40(a1)
        store   e11,48(a1)
        store   e15,56(a1)

        ; --- Submatrix M01 (rows 0..3, cols 4..7) ---
        load    8(a0),e0                ; H0
        load    24(a0),e1               ; H1
        load    40(a0),e2               ; H2
        load    56(a0),e3               ; H3

        vperm   #$018923ab,e0,e1,e4
        vperm   #$45cd67ef,e0,e1,e5
        vperm   #$018923ab,e2,e3,e6
        vperm   #$45cd67ef,e2,e3,e7

        vperm   #$012389ab,e4,e6,e8     ; M01_T[0]
        vperm   #$4567cdef,e4,e6,e9     ; M01_T[1]
        vperm   #$012389ab,e5,e7,e10    ; M01_T[2]
        vperm   #$4567cdef,e5,e7,e11    ; M01_T[3]

        ; --- Submatrix M11 (rows 4..7, cols 4..7) ---
        load    72(a0),e0               ; H4
        load    88(a0),e1               ; H5
        load    104(a0),e2              ; H6
        load    120(a0),e3              ; H7

        vperm   #$018923ab,e0,e1,e4
        vperm   #$45cd67ef,e0,e1,e5
        vperm   #$018923ab,e2,e3,e6
        vperm   #$45cd67ef,e2,e3,e7

        vperm   #$012389ab,e4,e6,e12    ; M11_T[0]
        vperm   #$4567cdef,e4,e6,e13    ; M11_T[1]
        vperm   #$012389ab,e5,e7,e14    ; M11_T[2]
        vperm   #$4567cdef,e5,e7,e15    ; M11_T[3]

        ; Store lower 4 rows of transposed matrix (M01_T is low quadword, M11_T is high quadword)
        store   e8,64(a1)
        store   e12,72(a1)
        store   e9,80(a1)
        store   e13,88(a1)
        store   e10,96(a1)
        store   e14,104(a1)
        store   e11,112(a1)
        store   e15,120(a1)

        rts

; ======================================================================
; void dct2d_8x8_fixed_ammx(const int16_t *src, int16_t *dst)
; ======================================================================
_dct2d_8x8_fixed_ammx:
        movem.l d2/a2-a4,-(sp)
        move.l  20(sp),a2               ; src
        move.l  24(sp),a3               ; dst
        suba.w  #256,sp                 ; tmp1 (128B) and tmp2 (128B)
        lea     (sp),a4                 ; tmp1

        ; Step 1: Transform rows from src -> tmp1
        moveq   #8-1,d2
.ammx2d_row1:
        move.l  a4,-(sp)
        move.l  a2,-(sp)
        jsr     _dct1d_fixed_ammx
        addq.l  #8,sp
        lea     16(a2),a2
        lea     16(a4),a4
        dbra    d2,.ammx2d_row1

        ; Step 2: Transpose tmp1 -> tmp2 via AMMX
        pea     128(sp)                 ; tmp2 (dst)
        pea     4(sp)                   ; tmp1 (src)
        jsr     _transpose8x8_ammx
        addq.l  #8,sp

        ; Step 3: Transform rows from tmp2 -> tmp1
        lea     128(sp),a2              ; tmp2
        lea     (sp),a4                 ; tmp1
        moveq   #8-1,d2
.ammx2d_row2:
        move.l  a4,-(sp)
        move.l  a2,-(sp)
        jsr     _dct1d_fixed_ammx
        addq.l  #8,sp
        lea     16(a2),a2
        lea     16(a4),a4
        dbra    d2,.ammx2d_row2

        ; Step 4: Transpose tmp1 -> dst via AMMX
        move.l  a3,-(sp)                ; dst
        pea     4(sp)                   ; tmp1 (src)
        jsr     _transpose8x8_ammx
        addq.l  #8,sp

        adda.w  #256,sp
        movem.l (sp)+,d2/a2-a4
        rts

; ======================================================================
; void idct2d_8x8_fixed_ammx(const int16_t *src, int16_t *dst)
; ======================================================================
_idct2d_8x8_fixed_ammx:
        movem.l d2/a2-a4,-(sp)
        move.l  20(sp),a2               ; src
        move.l  24(sp),a3               ; dst
        suba.w  #256,sp                 ; tmp1 (128B) and tmp2 (128B)
        lea     (sp),a4                 ; tmp1

        ; Step 1: Inverse transform rows from src -> tmp1
        moveq   #8-1,d2
.iammx2d_row1:
        move.l  a4,-(sp)
        move.l  a2,-(sp)
        jsr     _idct1d_fixed_ammx
        addq.l  #8,sp
        lea     16(a2),a2
        lea     16(a4),a4
        dbra    d2,.iammx2d_row1

        ; Step 2: Transpose tmp1 -> tmp2 via AMMX
        pea     128(sp)                 ; tmp2
        pea     4(sp)                   ; tmp1
        jsr     _transpose8x8_ammx
        addq.l  #8,sp

        ; Step 3: Inverse transform rows from tmp2 -> tmp1
        lea     128(sp),a2              ; tmp2
        lea     (sp),a4                 ; tmp1
        moveq   #8-1,d2
.iammx2d_row2:
        move.l  a4,-(sp)
        move.l  a2,-(sp)
        jsr     _idct1d_fixed_ammx
        addq.l  #8,sp
        lea     16(a2),a2
        lea     16(a4),a4
        dbra    d2,.iammx2d_row2

        ; Step 4: Transpose tmp1 -> dst via AMMX
        move.l  a3,-(sp)                ; dst
        pea     4(sp)                   ; tmp1
        jsr     _transpose8x8_ammx
        addq.l  #8,sp

        adda.w  #256,sp
        movem.l (sp)+,d2/a2-a4
        rts
