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
        xdef    _dct1d_float_fast_asm
        xdef    _idct1d_float_fast_asm
        xdef    _dct1d_fixed_fast_ammx
        xdef    _idct1d_fixed_fast_ammx
        xdef    _dct2d_8x8_float_asm
        xdef    _idct2d_8x8_float_asm
        xdef    _dct2d_8x8_float_fast_asm
        xdef    _idct2d_8x8_float_fast_asm
        xdef    _dct2d_8x8_fixed_scalar_asm
        xdef    _idct2d_8x8_fixed_scalar_asm
        xdef    _dct2d_8x8_fixed_ammx
        xdef    _idct2d_8x8_fixed_ammx
        xdef    _dct2d_8x8_fixed_fast_ammx
        xdef    _idct2d_8x8_fixed_fast_ammx
        xdef    _dct2d_8x8_fixed_fast_inreg_ammx
        xdef    _idct2d_8x8_fixed_fast_inreg_ammx
        xdef    _idct2d_8x8_sparse_ammx
        xdef    _quantize8x8_ammx
        xdef    _dct2d_8x8_aan_quant_ammx
        xdef    _transpose8x8_ammx
        xdef    _transpose8x8_float_ammx

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

; ======================================================================
; void transpose8x8_float_ammx(const float *src, float *dst)
; 8x8 float matrix transpose (256 bytes) using AMMX vperm in ~50 cycles
; ======================================================================
_transpose8x8_float_ammx:
        move.l  4(sp),a0                ; src
        move.l  8(sp),a1                ; dst

        ; --- Pair 0: columns 0 and 1 ---
        load    (a0),e0
        load    32(a0),e1
        vperm   #$012389ab,e0,e1,e2     ; [r0.0, r1.0]
        vperm   #$4567cdef,e0,e1,e3     ; [r0.1, r1.1]

        load    64(a0),e0
        load    96(a0),e1
        vperm   #$012389ab,e0,e1,e4     ; [r2.0, r3.0]
        vperm   #$4567cdef,e0,e1,e5     ; [r2.1, r3.1]

        load    128(a0),e0
        load    160(a0),e1
        vperm   #$012389ab,e0,e1,e6     ; [r4.0, r5.0]
        vperm   #$4567cdef,e0,e1,e7     ; [r4.1, r5.1]

        load    192(a0),e0
        load    224(a0),e1
        vperm   #$012389ab,e0,e1,e8     ; [r6.0, r7.0]
        vperm   #$4567cdef,e0,e1,e9     ; [r6.1, r7.1]

        store   e2,(a1)
        store   e4,8(a1)
        store   e6,16(a1)
        store   e8,24(a1)

        store   e3,32(a1)
        store   e5,40(a1)
        store   e7,48(a1)
        store   e9,56(a1)

        ; --- Pair 1: columns 2 and 3 ---
        load    8(a0),e0
        load    40(a0),e1
        vperm   #$012389ab,e0,e1,e2
        vperm   #$4567cdef,e0,e1,e3

        load    72(a0),e0
        load    104(a0),e1
        vperm   #$012389ab,e0,e1,e4
        vperm   #$4567cdef,e0,e1,e5

        load    136(a0),e0
        load    168(a0),e1
        vperm   #$012389ab,e0,e1,e6
        vperm   #$4567cdef,e0,e1,e7

        load    200(a0),e0
        load    232(a0),e1
        vperm   #$012389ab,e0,e1,e8
        vperm   #$4567cdef,e0,e1,e9

        store   e2,64(a1)
        store   e4,72(a1)
        store   e6,80(a1)
        store   e8,88(a1)

        store   e3,96(a1)
        store   e5,104(a1)
        store   e7,112(a1)
        store   e9,120(a1)

        ; --- Pair 2: columns 4 and 5 ---
        load    16(a0),e0
        load    48(a0),e1
        vperm   #$012389ab,e0,e1,e2
        vperm   #$4567cdef,e0,e1,e3

        load    80(a0),e0
        load    112(a0),e1
        vperm   #$012389ab,e0,e1,e4
        vperm   #$4567cdef,e0,e1,e5

        load    144(a0),e0
        load    176(a0),e1
        vperm   #$012389ab,e0,e1,e6
        vperm   #$4567cdef,e0,e1,e7

        load    208(a0),e0
        load    240(a0),e1
        vperm   #$012389ab,e0,e1,e8
        vperm   #$4567cdef,e0,e1,e9

        store   e2,128(a1)
        store   e4,136(a1)
        store   e6,144(a1)
        store   e8,152(a1)

        store   e3,160(a1)
        store   e5,168(a1)
        store   e7,176(a1)
        store   e9,184(a1)

        ; --- Pair 3: columns 6 and 7 ---
        load    24(a0),e0
        load    56(a0),e1
        vperm   #$012389ab,e0,e1,e2
        vperm   #$4567cdef,e0,e1,e3

        load    88(a0),e0
        load    120(a0),e1
        vperm   #$012389ab,e0,e1,e4
        vperm   #$4567cdef,e0,e1,e5

        load    152(a0),e0
        load    184(a0),e1
        vperm   #$012389ab,e0,e1,e6
        vperm   #$4567cdef,e0,e1,e7

        load    216(a0),e0
        load    248(a0),e1
        vperm   #$012389ab,e0,e1,e8
        vperm   #$4567cdef,e0,e1,e9

        store   e2,192(a1)
        store   e4,200(a1)
        store   e6,208(a1)
        store   e8,216(a1)

        store   e3,224(a1)
        store   e5,232(a1)
        store   e7,240(a1)
        store   e9,248(a1)

        rts

; ======================================================================
; void dct1d_float_fast_asm(const float *src, float *dst)
; 1D 8-point Fast Float DCT (Loeffler)
; ======================================================================
_dct1d_float_fast_asm:
        move.l  4(sp),a0                ; a0 = src
        move.l  8(sp),a1                ; a1 = dst
        fmovem.x fp2-fp7,-(sp)

        ; --- Stage 1 Even: s0..s3 ---
        fmove.s (a0),fp0                ; x0
        fadd.s  28(a0),fp0              ; s0 = x0 + x7

        fmove.s 4(a0),fp1               ; x1
        fadd.s  24(a0),fp1              ; s1 = x1 + x6

        fmove.s 8(a0),fp2               ; x2
        fadd.s  20(a0),fp2              ; s2 = x2 + x5

        fmove.s 12(a0),fp3              ; x3
        fadd.s  16(a0),fp3              ; s3 = x3 + x4

        ; Stage 2 Even: t0..t3
        fmove.x fp0,fp4
        fadd.x  fp3,fp0                 ; t0 = s0 + s3
        fsub.x  fp3,fp4                 ; t3 = s0 - s3

        fmove.x fp1,fp5
        fadd.x  fp2,fp1                 ; t1 = s1 + s2
        fsub.x  fp2,fp5                 ; t2 = s1 - s2

        ; X0 = c4*(t0 + t1), X4 = c4*(t0 - t1)
        fmove.x fp0,fp2
        fadd.x  fp1,fp0                 ; t0 + t1
        fsub.x  fp1,fp2                 ; t0 - t1
        fmove.s _fast_c4_f,fp6
        fmul.x  fp6,fp0                 ; X0
        fmul.x  fp6,fp2                 ; X4
        fmove.s fp0,(a1)
        fmove.s fp2,16(a1)

        ; X2 = c2*t3 + s2*t2, X6 = s2*t3 - c2*t2
        fmove.s _fast_c2_f,fp6
        fmove.s _fast_s2_f,fp7

        fmove.x fp4,fp0
        fmul.x  fp6,fp0                 ; t3 * c2
        fmove.x fp5,fp1
        fmul.x  fp7,fp1                 ; t2 * s2
        fadd.x  fp1,fp0                 ; X2
        fmove.s fp0,8(a1)

        fmul.x  fp7,fp4                 ; t3 * s2
        fmul.x  fp6,fp5                 ; t2 * c2
        fsub.x  fp5,fp4                 ; X6
        fmove.s fp4,24(a1)

        ; --- Stage 1 Odd: d0..d3 ---
        fmove.s (a0),fp0
        fsub.s  28(a0),fp0              ; d0 = x0 - x7

        fmove.s 4(a0),fp1
        fsub.s  24(a0),fp1              ; d1 = x1 - x6

        fmove.s 8(a0),fp2
        fsub.s  20(a0),fp2              ; d2 = x2 - x5

        fmove.s 12(a0),fp3
        fsub.s  16(a0),fp3              ; d3 = x3 - x4

        ; X1 = d0*c1 + d1*c3 + d2*c5 + d3*c7
        fmove.x fp0,fp4
        fmul.s  _fast_c1_f,fp4
        fmove.x fp1,fp5
        fmul.s  _fast_c3_f,fp5
        fadd.x  fp5,fp4
        fmove.x fp2,fp5
        fmul.s  _fast_c5_f,fp5
        fadd.x  fp5,fp4
        fmove.x fp3,fp5
        fmul.s  _fast_c7_f,fp5
        fadd.x  fp5,fp4
        fmove.s fp4,4(a1)

        ; X3 = d0*c3 - d1*c7 - d2*c1 - d3*c5
        fmove.x fp0,fp4
        fmul.s  _fast_c3_f,fp4
        fmove.x fp1,fp5
        fmul.s  _fast_c7_f,fp5
        fsub.x  fp5,fp4
        fmove.x fp2,fp5
        fmul.s  _fast_c1_f,fp5
        fsub.x  fp5,fp4
        fmove.x fp3,fp5
        fmul.s  _fast_c5_f,fp5
        fsub.x  fp5,fp4
        fmove.s fp4,12(a1)

        ; X5 = d0*c5 - d1*c1 + d2*c7 + d3*c3
        fmove.x fp0,fp4
        fmul.s  _fast_c5_f,fp4
        fmove.x fp1,fp5
        fmul.s  _fast_c1_f,fp5
        fsub.x  fp5,fp4
        fmove.x fp2,fp5
        fmul.s  _fast_c7_f,fp5
        fadd.x  fp5,fp4
        fmove.x fp3,fp5
        fmul.s  _fast_c3_f,fp5
        fadd.x  fp5,fp4
        fmove.s fp4,20(a1)

        ; X7 = d0*c7 - d1*c5 + d2*c3 - d3*c1
        fmul.s  _fast_c7_f,fp0
        fmul.s  _fast_c5_f,fp1
        fsub.x  fp1,fp0
        fmul.s  _fast_c3_f,fp2
        fadd.x  fp2,fp0
        fmul.s  _fast_c1_f,fp3
        fsub.x  fp3,fp0
        fmove.s fp0,28(a1)

        fmovem.x (sp)+,fp2-fp7
        rts

; ======================================================================
; void idct1d_float_fast_asm(const float *src, float *dst)
; 1D 8-point Fast Float IDCT (Loeffler)
; ======================================================================
_idct1d_float_fast_asm:
        move.l  4(sp),a0                ; src
        move.l  8(sp),a1                ; dst
        fmovem.x fp2-fp7,-(sp)

        ; e0 = c4*(src[0] + src[4]), e1 = c4*(src[0] - src[4])
        fmove.s (a0),fp0                ; X0
        fmove.s 16(a0),fp1              ; X4
        fmove.x fp0,fp2
        fadd.x  fp1,fp0
        fsub.x  fp1,fp2
        fmove.s _fast_c4_f,fp3
        fmul.x  fp3,fp0                 ; e0
        fmul.x  fp3,fp2                 ; e1

        ; e2 = c2*X2 + s2*X6, e3 = s2*X2 - c2*X6
        fmove.s 8(a0),fp4               ; X2
        fmove.s 24(a0),fp5              ; X6
        fmove.s _fast_c2_f,fp6          ; c2
        fmove.s _fast_s2_f,fp7          ; s2

        fmove.x fp4,fp1
        fmul.x  fp6,fp1
        fmove.x fp5,fp3
        fmul.x  fp7,fp3
        fadd.x  fp3,fp1                 ; e2

        fmul.x  fp7,fp4
        fmul.x  fp6,fp5
        fsub.x  fp5,fp4                 ; e3

        ; s0 = e0 + e2, s3 = e0 - e2
        fmove.x fp0,fp3
        fadd.x  fp1,fp0                 ; s0
        fsub.x  fp1,fp3                 ; s3

        ; s1 = e1 + e3, s2 = e1 - e3
        fmove.x fp2,fp5
        fadd.x  fp4,fp2                 ; s1
        fsub.x  fp4,fp5                 ; s2

        ; d0 = X1*c1 + X3*c3 + X5*c5 + X7*c7
        fmove.s 4(a0),fp1               ; X1
        fmul.s  _fast_c1_f,fp1
        fmove.s 12(a0),fp4              ; X3
        fmul.s  _fast_c3_f,fp4
        fadd.x  fp4,fp1
        fmove.s 20(a0),fp4              ; X5
        fmul.s  _fast_c5_f,fp4
        fadd.x  fp4,fp1
        fmove.s 28(a0),fp4              ; X7
        fmul.s  _fast_c7_f,fp4
        fadd.x  fp4,fp1                 ; d0

        ; x0 = s0 + d0, x7 = s0 - d0
        fmove.x fp0,fp4
        fadd.x  fp1,fp0                 ; x0
        fsub.x  fp1,fp4                 ; x7
        fmove.s fp0,(a1)
        fmove.s fp4,28(a1)

        ; d1 = X1*c3 - X3*c7 - X5*c1 - X7*c5
        fmove.s 4(a0),fp1
        fmul.s  _fast_c3_f,fp1
        fmove.s 12(a0),fp4
        fmul.s  _fast_c7_f,fp4
        fsub.x  fp4,fp1
        fmove.s 20(a0),fp4
        fmul.s  _fast_c1_f,fp4
        fsub.x  fp4,fp1
        fmove.s 28(a0),fp4
        fmul.s  _fast_c5_f,fp4
        fsub.x  fp4,fp1                 ; d1

        ; x1 = s1 + d1, x6 = s1 - d1
        fmove.x fp2,fp4
        fadd.x  fp1,fp2                 ; x1
        fsub.x  fp1,fp4                 ; x6
        fmove.s fp2,4(a1)
        fmove.s fp4,24(a1)

        ; d2 = X1*c5 - X3*c1 + X5*c7 + X7*c3
        fmove.s 4(a0),fp1
        fmul.s  _fast_c5_f,fp1
        fmove.s 12(a0),fp4
        fmul.s  _fast_c1_f,fp4
        fsub.x  fp4,fp1
        fmove.s 20(a0),fp4
        fmul.s  _fast_c7_f,fp4
        fadd.x  fp4,fp1
        fmove.s 28(a0),fp4
        fmul.s  _fast_c3_f,fp4
        fadd.x  fp4,fp1                 ; d2

        ; x2 = s2 + d2, x5 = s2 - d2
        fmove.x fp5,fp4
        fadd.x  fp1,fp5                 ; x2
        fsub.x  fp1,fp4                 ; x5
        fmove.s fp5,8(a1)
        fmove.s fp4,20(a1)

        ; d3 = X1*c7 - X3*c5 + X5*c3 - X7*c1
        fmove.s 4(a0),fp1
        fmul.s  _fast_c7_f,fp1
        fmove.s 12(a0),fp4
        fmul.s  _fast_c5_f,fp4
        fsub.x  fp4,fp1
        fmove.s 20(a0),fp4
        fmul.s  _fast_c3_f,fp4
        fadd.x  fp4,fp1
        fmove.s 28(a0),fp4
        fmul.s  _fast_c1_f,fp4
        fsub.x  fp4,fp1                 ; d3

        ; x3 = s3 + d3, x4 = s3 - d3
        fmove.x fp3,fp4
        fadd.x  fp1,fp3                 ; x3
        fsub.x  fp1,fp4                 ; x4
        fmove.s fp3,12(a1)
        fmove.s fp4,16(a1)

        fmovem.x (sp)+,fp2-fp7
        rts

; ======================================================================
; void dct2d_8x8_float_fast_asm(const float *src, float *dst)
; 2D 8x8 Fast Float DCT (Loeffler + AMMX float transpose)
; ======================================================================
_dct2d_8x8_float_fast_asm:
        movem.l d2/a2-a4,-(sp)
        move.l  20(sp),a2               ; src
        move.l  24(sp),a3               ; dst
        suba.w  #512,sp                 ; tmp1 (256B) and tmp2 (256B)
        lea     (sp),a4                 ; tmp1

        ; Step 1: Transform rows from src -> tmp1
        moveq   #8-1,d2
.ff2d_row1:
        move.l  a4,-(sp)
        move.l  a2,-(sp)
        jsr     _dct1d_float_fast_asm
        addq.l  #8,sp
        lea     32(a2),a2
        lea     32(a4),a4
        dbra    d2,.ff2d_row1

        ; Step 2: Transpose tmp1 -> tmp2 via AMMX float transpose
        pea     256(sp)                 ; tmp2
        pea     4(sp)                   ; tmp1
        jsr     _transpose8x8_float_ammx
        addq.l  #8,sp

        ; Step 3: Transform rows from tmp2 -> tmp1
        lea     256(sp),a2              ; tmp2
        lea     (sp),a4                 ; tmp1
        moveq   #8-1,d2
.ff2d_row2:
        move.l  a4,-(sp)
        move.l  a2,-(sp)
        jsr     _dct1d_float_fast_asm
        addq.l  #8,sp
        lea     32(a2),a2
        lea     32(a4),a4
        dbra    d2,.ff2d_row2

        ; Step 4: Transpose tmp1 -> dst via AMMX float transpose
        move.l  a3,-(sp)                ; dst
        pea     4(sp)                   ; tmp1
        jsr     _transpose8x8_float_ammx
        addq.l  #8,sp

        adda.w  #512,sp
        movem.l (sp)+,d2/a2-a4
        rts

; ======================================================================
; void idct2d_8x8_float_fast_asm(const float *src, float *dst)
; 2D 8x8 Fast Float IDCT (Loeffler + AMMX float transpose)
; ======================================================================
_idct2d_8x8_float_fast_asm:
        movem.l d2/a2-a4,-(sp)
        move.l  20(sp),a2
        move.l  24(sp),a3
        suba.w  #512,sp
        lea     (sp),a4

        ; Step 1: Inverse transform rows from src -> tmp1
        moveq   #8-1,d2
.iff2d_row1:
        move.l  a4,-(sp)
        move.l  a2,-(sp)
        jsr     _idct1d_float_fast_asm
        addq.l  #8,sp
        lea     32(a2),a2
        lea     32(a4),a4
        dbra    d2,.iff2d_row1

        ; Step 2: Transpose tmp1 -> tmp2
        pea     256(sp)
        pea     4(sp)
        jsr     _transpose8x8_float_ammx
        addq.l  #8,sp

        ; Step 3: Inverse transform rows from tmp2 -> tmp1
        lea     256(sp),a2
        lea     (sp),a4
        moveq   #8-1,d2
.iff2d_row2:
        move.l  a4,-(sp)
        move.l  a2,-(sp)
        jsr     _idct1d_float_fast_asm
        addq.l  #8,sp
        lea     32(a2),a2
        lea     32(a4),a4
        dbra    d2,.iff2d_row2

        ; Step 4: Transpose tmp1 -> dst
        move.l  a3,-(sp)
        pea     4(sp)
        jsr     _transpose8x8_float_ammx
        addq.l  #8,sp

        adda.w  #512,sp
        movem.l (sp)+,d2/a2-a4
        rts

; ======================================================================
; void dct1d_fixed_fast_ammx(const int16_t *src, int16_t *dst)
; 1D 8-point Fast Fixed DCT (Loeffler AMMX SIMD, unrolled)
; ======================================================================
_dct1d_fixed_fast_ammx:
        move.l  4(sp),a0                ; a0 = src
        move.l  8(sp),a1                ; a1 = dst

        load    (a0),e0                 ; [x0, x1, x2, x3]
        load    8(a0),e1                ; [x4, x5, x6, x7]

        ; --- Stage 1: Mirror & Butterflies ---
        vperm   #$67452301,e1,e1,e2     ; e2 = [x7, x6, x5, x4]
        paddw   e2,e0,e3                ; e3 = [s0, s1, s2, s3] (Even inputs)
        psubw   e2,e0,e4                ; e4 = [d0, d1, d2, d3] (Odd inputs)

        ; --- Stage 2 Even ---
        vperm   #$67452301,e3,e3,e5     ; e5 = [s3, s2, s1, s0]
        paddw   e5,e3,e6                ; e6 = [t0, t1, t1, t0]
        psubw   e5,e3,e7                ; e7 = [t3, t2, -t2, -t3]

        ; X0 = (t0 + t1)*c4, X4 = (t0 - t1)*c4
        load    _fast_c4,e12            ; [c4, c4, c4, c4]
        pmulh   e6,e12,e8               ; e8 = [t0*c4, t1*c4, t1*c4, t0*c4]
        vperm   #$23016745,e8,e8,e9     ; e9 = [t1*c4, t0*c4, t0*c4, t1*c4]
        paddw   e9,e8,e10               ; e10[0] = (t0+t1)*c4 = X0/2
        psubw   e9,e8,e11               ; e11[0] = (t0-t1)*c4 = X4/2
        vperm   #$01890189,e10,e11,e13  ; e13 = [X0/2, X4/2, X0/2, X4/2]
        paddw   e13,e13,e13             ; [X0, X4, X0, X4]

        ; X2 = t3*c2 + t2*s2, X6 = t3*s2 - t2*c2
        vperm   #$01010101,e7,e7,e14    ; [t3, t3, t3, t3]
        vperm   #$23232323,e7,e7,e15    ; [t2, t2, t2, t2]
        load    _fast_c2_s2,e16         ; [c2, s2, c2, s2]
        load    _fast_s2_mc2,e17        ; [s2, -c2, s2, -c2]
        pmulh   e14,e16,e18             ; [t3*c2, t3*s2, ...]
        pmulh   e15,e17,e19             ; [t2*s2, -t2*c2, ...]
        paddw   e19,e18,e18             ; [X2/2, X6/2, ...]
        paddw   e18,e18,e18             ; [X2, X6, X2, X6]

        ; Combine Even outputs: [X0, X2, X4, X6]
        vperm   #$018923ab,e13,e18,e20  ; e20 = [X0, X2, X4, X6]

        ; --- Stage 2 Odd ---
        load    _fast_odd1,e12          ; [c1, c3, c5, c7]
        load    _fast_odd3,e13          ; [c3, -c7, -c1, -c5]
        pmulh   e4,e12,e8               ; p1
        pmulh   e4,e13,e9               ; p3

        load    _fast_odd5,e12          ; [c5, -c1, c7, c3]
        load    _fast_odd7,e13          ; [c7, -c5, c3, -c1]
        pmulh   e4,e12,e10              ; p5
        pmulh   e4,e13,e11              ; p7

        ; 2-way horizontal reduction on (p1, p3):
        vperm   #$018945cd,e8,e9,e16
        vperm   #$23ab67ef,e8,e9,e17
        paddw   e17,e16,e16
        vperm   #$45670123,e16,e16,e18
        paddw   e18,e16,e16
        paddw   e16,e16,e16             ; [X1, X3, X1, X3]

        ; 2-way horizontal reduction on (p5, p7):
        vperm   #$018945cd,e10,e11,e17
        vperm   #$23ab67ef,e10,e11,e18
        paddw   e18,e17,e17
        vperm   #$45670123,e17,e17,e19
        paddw   e19,e17,e17
        paddw   e17,e17,e17             ; [X5, X7, X5, X7]

        ; Combine Odd outputs: [X1, X3, X5, X7]
        vperm   #$012389ab,e16,e17,e21  ; e21 = [X1, X3, X5, X7]

        ; Interleave Even & Odd into [X0, X1, X2, X3] and [X4, X5, X6, X7]
        vperm   #$018923ab,e20,e21,e0
        vperm   #$45cd67ef,e20,e21,e1

        store   e0,(a1)
        store   e1,8(a1)
        rts

; ======================================================================
; void idct1d_fixed_fast_ammx(const int16_t *src, int16_t *dst)
; 1D 8-point Fast Fixed IDCT (Loeffler AMMX SIMD, unrolled)
; ======================================================================
_idct1d_fixed_fast_ammx:
        move.l  4(sp),a0
        move.l  8(sp),a1

        load    (a0),e0                 ; [X0, X1, X2, X3]
        load    8(a0),e1                ; [X4, X5, X6, X7]

        ; Separate Even and Odd
        vperm   #$014589cd,e0,e1,e2     ; e2 = [X0, X2, X4, X6]
        vperm   #$2367abef,e0,e1,e3     ; e3 = [X1, X3, X5, X7]

        ; Even:
        ; e0_val = c4*(X0 + X4) = c4*X0 + c4*X4
        ; e1_val = c4*(X0 - X4) = c4*X0 - c4*X4
        load    _fast_c4,e12
        pmulh   e2,e12,e4               ; e4 = [X0*c4, X2*c4, X4*c4, X6*c4]
        vperm   #$01010101,e4,e4,e5     ; [X0*c4, X0*c4, X0*c4, X0*c4]
        vperm   #$45454545,e4,e4,e6     ; [X4*c4, X4*c4, X4*c4, X4*c4]
        paddw   e6,e5,e7                ; e7 = [e0_val/2, ...]
        psubw   e6,e5,e8                ; e8 = [e1_val/2, ...]
        vperm   #$01890189,e7,e8,e9     ; e9 = [e0_val/2, e1_val/2, ...]
        paddw   e9,e9,e9                ; e9 = [e0_val, e1_val, ...]

        ; e2_val = c2*X2 + s2*X6, e3_val = s2*X2 - c2*X6
        vperm   #$23232323,e2,e2,e14    ; [X2, X2, X2, X2]
        vperm   #$67676767,e2,e2,e15    ; [X6, X6, X6, X6]
        load    _fast_c2_s2,e16
        load    _fast_s2_mc2,e17
        pmulh   e14,e16,e18
        pmulh   e15,e17,e19
        paddw   e19,e18,e18
        paddw   e18,e18,e18             ; e18 = [e2_val, e3_val, ...]

        ; s0 = e0 + e2, s3 = e0 - e2
        ; s1 = e1 + e3, s2 = e1 - e3
        vperm   #$01010101,e9,e9,e4     ; [e0, e0, e0, e0]
        vperm   #$23232323,e9,e9,e5     ; [e1, e1, e1, e1]
        vperm   #$01010101,e18,e18,e6   ; [e2, e2, e2, e2]
        vperm   #$23232323,e18,e18,e7   ; [e3, e3, e3, e3]

        paddw   e6,e4,e8                ; s0 = e0 + e2
        psubw   e6,e4,e9                ; s3 = e0 - e2
        paddw   e7,e5,e10               ; s1 = e1 + e3
        psubw   e7,e5,e11               ; s2 = e1 - e3

        ; Combine s = [s0, s1, s2, s3]
        vperm   #$01890189,e8,e10,e12   ; [s0, s1, ...]
        vperm   #$01890189,e11,e9,e13   ; [s2, s3, ...]
        vperm   #$012389ab,e12,e13,e20  ; e20 = [s0, s1, s2, s3]

        ; Odd: d0..d3 with symmetric Dodd
        load    _fast_odd1,e12
        load    _fast_odd3,e13
        pmulh   e3,e12,e8
        pmulh   e3,e13,e9

        load    _fast_odd5,e12
        load    _fast_odd7,e13
        pmulh   e3,e12,e10
        pmulh   e3,e13,e11

        vperm   #$018945cd,e8,e9,e16
        vperm   #$23ab67ef,e8,e9,e17
        paddw   e17,e16,e16
        vperm   #$45670123,e16,e16,e18
        paddw   e18,e16,e16
        paddw   e16,e16,e16             ; [d0, d1, d0, d1]

        vperm   #$018945cd,e10,e11,e17
        vperm   #$23ab67ef,e10,e11,e18
        paddw   e18,e17,e17
        vperm   #$45670123,e17,e17,e19
        paddw   e19,e17,e17
        paddw   e17,e17,e17             ; [d2, d3, d2, d3]

        vperm   #$012389ab,e16,e17,e21  ; e21 = [d0, d1, d2, d3]

        ; Final butterflies:
        ; x0..x3 = s + d
        ; x7..x4 = s - d
        paddw   e21,e20,e0              ; e0 = [x0, x1, x2, x3]
        psubw   e21,e20,e1              ; e1 = [x7, x6, x5, x4]
        vperm   #$67452301,e1,e1,e1     ; e1 = [x4, x5, x6, x7]

        store   e0,(a1)
        store   e1,8(a1)
        rts

; ======================================================================
; void dct2d_8x8_fixed_fast_ammx(const int16_t *src, int16_t *dst)
; 2D 8x8 Fast Fixed DCT (Loeffler + AMMX 8x8 transpose)
; ======================================================================
_dct2d_8x8_fixed_fast_ammx:
        movem.l d2/a2-a4,-(sp)
        move.l  20(sp),a2               ; src
        move.l  24(sp),a3               ; dst
        suba.w  #256,sp                 ; tmp1 (128B) and tmp2 (128B)
        lea     (sp),a4                 ; tmp1

        ; Step 1: Transform rows from src -> tmp1 (scaled 0.5x)
        moveq   #8-1,d2
.ffix2d_row1:
        move.l  a4,-(sp)
        move.l  a2,-(sp)
        jsr     _dct1d_fixed_fast_ammx_half
        addq.l  #8,sp
        lea     16(a2),a2
        lea     16(a4),a4
        dbra    d2,.ffix2d_row1

        ; Step 2: Transpose tmp1 -> tmp2 via AMMX
        pea     128(sp)                 ; tmp2
        pea     4(sp)                   ; tmp1
        jsr     _transpose8x8_ammx
        addq.l  #8,sp

        ; Step 3: Transform rows from tmp2 -> tmp1 (normal full-scale)
        lea     128(sp),a2              ; tmp2
        lea     (sp),a4                 ; tmp1
        moveq   #8-1,d2
.ffix2d_row2:
        move.l  a4,-(sp)
        move.l  a2,-(sp)
        jsr     _dct1d_fixed_fast_ammx
        addq.l  #8,sp
        lea     16(a2),a2
        lea     16(a4),a4
        dbra    d2,.ffix2d_row2

        ; Step 4: Transpose tmp1 -> dst via AMMX
        move.l  a3,-(sp)                ; dst
        pea     4(sp)                   ; tmp1
        jsr     _transpose8x8_ammx
        addq.l  #8,sp

        ; Step 5: Double dst (* 2) to restore 1.0x scale
        move.l  a3,a0
        moveq   #8-1,d0
.ffix2d_scale2:
        load    (a0),e0
        load    8(a0),e1
        paddw   e0,e0,e0
        paddw   e1,e1,e1
        store   e0,(a0)
        store   e1,8(a0)
        lea     16(a0),a0
        dbra    d0,.ffix2d_scale2

        adda.w  #256,sp
        movem.l (sp)+,d2/a2-a4
        rts

; ======================================================================
; void dct1d_fixed_fast_ammx_half(const int16_t *src, int16_t *dst)
; Same as dct1d_fixed_fast_ammx, but outputs scaled by 0.5 (no doubling)
; ======================================================================
_dct1d_fixed_fast_ammx_half:
        move.l  4(sp),a0
        move.l  8(sp),a1

        load    (a0),e0
        load    8(a0),e1

        vperm   #$67452301,e1,e1,e2
        paddw   e2,e0,e3
        psubw   e2,e0,e4

        vperm   #$67452301,e3,e3,e5
        paddw   e5,e3,e6
        psubw   e5,e3,e7

        load    _fast_c4,e12            ; [c4, c4, c4, c4]
        pmulh   e6,e12,e8               ; e8 = [t0*c4, t1*c4, t1*c4, t0*c4]
        vperm   #$23016745,e8,e8,e9     ; e9 = [t1*c4, t0*c4, t0*c4, t1*c4]
        paddw   e9,e8,e10               ; e10[0] = (t0+t1)*c4 = X0/2
        psubw   e9,e8,e11               ; e11[0] = (t0-t1)*c4 = X4/2
        vperm   #$01890189,e10,e11,e13  ; e13 = [X0/2, X4/2, X0/2, X4/2]

        vperm   #$01010101,e7,e7,e14
        vperm   #$23232323,e7,e7,e15
        load    _fast_c2_s2,e16
        load    _fast_s2_mc2,e17
        pmulh   e14,e16,e18
        pmulh   e15,e17,e19
        paddw   e19,e18,e18             ; [X2/2, X6/2, ...]

        vperm   #$018923ab,e13,e18,e20  ; e20 = [X0/2, X2/2, X4/2, X6/2]

        load    _fast_odd1,e12
        load    _fast_odd3,e13
        pmulh   e4,e12,e8
        pmulh   e4,e13,e9

        load    _fast_odd5,e12
        load    _fast_odd7,e13
        pmulh   e4,e12,e10
        pmulh   e4,e13,e11

        vperm   #$018945cd,e8,e9,e16
        vperm   #$23ab67ef,e8,e9,e17
        paddw   e17,e16,e16
        vperm   #$45670123,e16,e16,e18
        paddw   e18,e16,e16             ; [X1/2, X3/2, ...]

        vperm   #$018945cd,e10,e11,e17
        vperm   #$23ab67ef,e10,e11,e18
        paddw   e18,e17,e17
        vperm   #$45670123,e17,e17,e19
        paddw   e19,e17,e17             ; [X5/2, X7/2, ...]

        vperm   #$012389ab,e16,e17,e21  ; e21 = [X1/2, X3/2, X5/2, X7/2]

        vperm   #$018923ab,e20,e21,e0
        vperm   #$45cd67ef,e20,e21,e1

        store   e0,(a1)
        store   e1,8(a1)
        rts

; ======================================================================
; void idct1d_fixed_fast_ammx_half(const int16_t *src, int16_t *dst)
; Same as idct1d_fixed_fast_ammx, but outputs scaled by 0.5 (no doubling)
; ======================================================================
_idct1d_fixed_fast_ammx_half:
        move.l  4(sp),a0
        move.l  8(sp),a1

        load    (a0),e0
        load    8(a0),e1

        vperm   #$014589cd,e0,e1,e2
        vperm   #$2367abef,e0,e1,e3

        load    _fast_c4,e12
        pmulh   e2,e12,e4               ; e4 = [X0*c4, X2*c4, X4*c4, X6*c4]
        vperm   #$01010101,e4,e4,e5     ; [X0*c4, X0*c4, X0*c4, X0*c4]
        vperm   #$45454545,e4,e4,e6     ; [X4*c4, X4*c4, X4*c4, X4*c4]
        paddw   e6,e5,e7                ; e7 = [e0_val/2, ...]
        psubw   e6,e5,e8                ; e8 = [e1_val/2, ...]
        vperm   #$01890189,e7,e8,e9     ; e9 = [e0_val/2, e1_val/2, ...]

        vperm   #$23232323,e2,e2,e14
        vperm   #$67676767,e2,e2,e15
        load    _fast_c2_s2,e16
        load    _fast_s2_mc2,e17
        pmulh   e14,e16,e18
        pmulh   e15,e17,e19
        paddw   e19,e18,e18             ; [e2_val/2, e3_val/2, ...]

        vperm   #$01010101,e9,e9,e4
        vperm   #$23232323,e9,e9,e5
        vperm   #$01010101,e18,e18,e6
        vperm   #$23232323,e18,e18,e7

        paddw   e6,e4,e8
        psubw   e6,e4,e9
        paddw   e7,e5,e10
        psubw   e7,e5,e11

        vperm   #$01890189,e8,e10,e12
        vperm   #$01890189,e11,e9,e13
        vperm   #$012389ab,e12,e13,e20  ; e20 = [s0/2, s1/2, s2/2, s3/2]

        load    _fast_odd1,e12
        load    _fast_odd3,e13
        pmulh   e3,e12,e8
        pmulh   e3,e13,e9

        load    _fast_odd5,e12
        load    _fast_odd7,e13
        pmulh   e3,e12,e10
        pmulh   e3,e13,e11

        vperm   #$018945cd,e8,e9,e16
        vperm   #$23ab67ef,e8,e9,e17
        paddw   e17,e16,e16
        vperm   #$45670123,e16,e16,e18
        paddw   e18,e16,e16             ; [d0/2, d1/2, ...]

        vperm   #$018945cd,e10,e11,e17
        vperm   #$23ab67ef,e10,e11,e18
        paddw   e18,e17,e17
        vperm   #$45670123,e17,e17,e19
        paddw   e19,e17,e17             ; [d2/2, d3/2, ...]

        vperm   #$012389ab,e16,e17,e21  ; e21 = [d0/2, d1/2, d2/2, d3/2]

        paddw   e21,e20,e0
        psubw   e21,e20,e1
        vperm   #$67452301,e1,e1,e1

        store   e0,(a1)
        store   e1,8(a1)
        rts

; ======================================================================
; void idct2d_8x8_fixed_fast_ammx(const int16_t *src, int16_t *dst)
; 2D 8x8 Fast Fixed IDCT (Loeffler + AMMX 8x8 transpose)
; ======================================================================
_idct2d_8x8_fixed_fast_ammx:
        movem.l d2/a2-a4,-(sp)
        move.l  20(sp),a2
        move.l  24(sp),a3
        suba.w  #256,sp
        lea     (sp),a4

        ; Step 1: Inverse transform rows from src -> tmp1 (scaled 0.5x)
        moveq   #8-1,d2
.iffix2d_row1:
        move.l  a4,-(sp)
        move.l  a2,-(sp)
        jsr     _idct1d_fixed_fast_ammx_half
        addq.l  #8,sp
        lea     16(a2),a2
        lea     16(a4),a4
        dbra    d2,.iffix2d_row1

        ; Step 2: Transpose tmp1 -> tmp2
        pea     128(sp)
        pea     4(sp)
        jsr     _transpose8x8_ammx
        addq.l  #8,sp

        ; Step 3: Inverse transform rows from tmp2 -> tmp1 (normal full-scale)
        lea     128(sp),a2
        lea     (sp),a4
        moveq   #8-1,d2
.iffix2d_row2:
        move.l  a4,-(sp)
        move.l  a2,-(sp)
        jsr     _idct1d_fixed_fast_ammx
        addq.l  #8,sp
        lea     16(a2),a2
        lea     16(a4),a4
        dbra    d2,.iffix2d_row2

        ; Step 4: Transpose tmp1 -> dst
        move.l  a3,-(sp)
        pea     4(sp)
        jsr     _transpose8x8_ammx
        addq.l  #8,sp

        ; Step 5: Double dst (* 2) to restore 1.0x scale
        move.l  a3,a0
        moveq   #8-1,d0
.iffix2d_scale2:
        load    (a0),e0
        load    8(a0),e1
        paddw   e0,e0,e0
        paddw   e1,e1,e1
        store   e0,(a0)
        store   e1,8(a0)
        lea     16(a0),a0
        dbra    d0,.iffix2d_scale2

        adda.w  #256,sp
        movem.l (sp)+,d2/a2-a4
        rts

; ======================================================================
; void dct2d_8x8_fixed_fast_inreg_ammx(const int16_t *src, int16_t *dst)
; 2D 8x8 Fast Fixed DCT with inlined 1D transforms & preloaded constants
; ======================================================================
_dct2d_8x8_fixed_fast_inreg_ammx:
        movem.l d2/a2-a4,-(sp)
        move.l  20(sp),a2               ; src
        move.l  24(sp),a3               ; dst
        suba.w  #256,sp                 ; tmp1 (128B) and tmp2 (128B)
        lea     (sp),a4                 ; tmp1

        ; Preload constants into e16..e22 (alive for entire function!)
        lea     _fast_c4,a0
        load    (a0),e16                ; _fast_c4
        load    8(a0),e17               ; _fast_c2_s2
        load    16(a0),e18              ; _fast_s2_mc2
        load    24(a0),e19              ; _fast_odd1
        load    32(a0),e20              ; _fast_odd3
        load    40(a0),e21              ; _fast_odd5
        load    48(a0),e22              ; _fast_odd7

        ; --- Step 1: Inlined Row Transforms (scaled 0.5x) ---
        moveq   #8-1,d2
.inreg_row1:
        load    (a2),e0
        load    8(a2),e1

        vperm   #$67452301,e1,e1,e2
        paddw   e2,e0,e3
        psubw   e2,e0,e4

        vperm   #$67452301,e3,e3,e5
        paddw   e5,e3,e6
        psubw   e5,e3,e7

        pmulh   e6,e16,e8
        vperm   #$23016745,e8,e8,e9
        paddw   e9,e8,e10
        psubw   e9,e8,e11
        vperm   #$01890189,e10,e11,e13  ; [X0/2, X4/2, X0/2, X4/2]

        vperm   #$01010101,e7,e7,e14
        vperm   #$23232323,e7,e7,e15
        pmulh   e14,e17,e2
        pmulh   e15,e18,e3
        paddw   e3,e2,e2                ; [X2/2, X6/2, ...]

        vperm   #$018923ab,e13,e2,e14   ; e14 = [X0/2, X2/2, X4/2, X6/2]

        pmulh   e4,e19,e8
        pmulh   e4,e20,e9
        pmulh   e4,e21,e10
        pmulh   e4,e22,e11

        vperm   #$018945cd,e8,e9,e12
        vperm   #$23ab67ef,e8,e9,e13
        paddw   e13,e12,e12
        vperm   #$45670123,e12,e12,e13
        paddw   e13,e12,e12             ; [X1/2, X3/2, ...]

        vperm   #$018945cd,e10,e11,e13
        vperm   #$23ab67ef,e10,e11,e15
        paddw   e15,e13,e13
        vperm   #$45670123,e13,e13,e15
        paddw   e15,e13,e13             ; [X5/2, X7/2, ...]

        vperm   #$012389ab,e12,e13,e15  ; e15 = [X1/2, X3/2, X5/2, X7/2]

        vperm   #$018923ab,e14,e15,e0
        vperm   #$45cd67ef,e14,e15,e1

        store   e0,(a4)
        store   e1,8(a4)
        lea     16(a2),a2
        lea     16(a4),a4
        dbra    d2,.inreg_row1

        ; --- Step 2: Inlined Transpose tmp1 -> tmp2 ---
        lea     (sp),a0                 ; tmp1
        lea     128(sp),a1              ; tmp2
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

        ; --- Step 3: Inlined Column Transforms tmp2 -> tmp1 (normal full-scale) ---
        lea     128(sp),a2              ; tmp2
        lea     (sp),a4                 ; tmp1
        moveq   #8-1,d2
.inreg_row2:
        load    (a2),e0
        load    8(a2),e1

        vperm   #$67452301,e1,e1,e2
        paddw   e2,e0,e3
        psubw   e2,e0,e4

        vperm   #$67452301,e3,e3,e5
        paddw   e5,e3,e6
        psubw   e5,e3,e7

        pmulh   e6,e16,e8
        vperm   #$23016745,e8,e8,e9
        paddw   e9,e8,e10
        psubw   e9,e8,e11
        vperm   #$01890189,e10,e11,e13
        paddw   e13,e13,e13             ; [X0, X4, X0, X4]

        vperm   #$01010101,e7,e7,e14
        vperm   #$23232323,e7,e7,e15
        pmulh   e14,e17,e2
        pmulh   e15,e18,e3
        paddw   e3,e2,e2
        paddw   e2,e2,e2                ; [X2, X6, ...]

        vperm   #$018923ab,e13,e2,e14   ; e14 = [X0, X2, X4, X6]

        pmulh   e4,e19,e8
        pmulh   e4,e20,e9
        pmulh   e4,e21,e10
        pmulh   e4,e22,e11

        vperm   #$018945cd,e8,e9,e12
        vperm   #$23ab67ef,e8,e9,e13
        paddw   e13,e12,e12
        vperm   #$45670123,e12,e12,e13
        paddw   e13,e12,e12
        paddw   e12,e12,e12             ; [X1, X3, ...]

        vperm   #$018945cd,e10,e11,e13
        vperm   #$23ab67ef,e10,e11,e15
        paddw   e15,e13,e13
        vperm   #$45670123,e13,e13,e15
        paddw   e15,e13,e13
        paddw   e13,e13,e13             ; [X5, X7, ...]

        vperm   #$012389ab,e12,e13,e15  ; e15 = [X1, X3, X5, X7]

        vperm   #$018923ab,e14,e15,e0
        vperm   #$45cd67ef,e14,e15,e1

        store   e0,(a4)
        store   e1,8(a4)
        lea     16(a2),a2
        lea     16(a4),a4
        dbra    d2,.inreg_row2

        ; --- Step 4: Inlined Transpose tmp1 -> dst ---
        lea     (sp),a0                 ; tmp1
        move.l  a3,a1                   ; dst
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

        ; --- Step 5: Double dst to restore 1.0x scale ---
        move.l  a3,a0
        moveq   #8-1,d0
.inreg_scale:
        load    (a0),e0
        load    8(a0),e1
        paddw   e0,e0,e0
        paddw   e1,e1,e1
        store   e0,(a0)
        store   e1,8(a0)
        lea     16(a0),a0
        dbra    d0,.inreg_scale

        adda.w  #256,sp
        movem.l (sp)+,d2/a2-a4
        rts

; ======================================================================
; void idct2d_8x8_fixed_fast_inreg_ammx(const int16_t *src, int16_t *dst)
; 2D 8x8 Fast Fixed IDCT with inlined 1D transforms & preloaded constants
; ======================================================================
_idct2d_8x8_fixed_fast_inreg_ammx:
        movem.l d2/a2-a4,-(sp)
        move.l  20(sp),a2               ; src
        move.l  24(sp),a3               ; dst
        suba.w  #256,sp
        lea     (sp),a4                 ; tmp1

        ; Preload constants into e16..e22
        lea     _fast_c4,a0
        load    (a0),e16                ; _fast_c4
        load    8(a0),e17               ; _fast_c2_s2
        load    16(a0),e18              ; _fast_s2_mc2
        load    24(a0),e19              ; _fast_odd1
        load    32(a0),e20              ; _fast_odd3
        load    40(a0),e21              ; _fast_odd5
        load    48(a0),e22              ; _fast_odd7

        ; --- Step 1: Inlined Row IDCTs (scaled 0.5x) ---
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
        vperm   #$01890189,e7,e8,e9     ; [e0_val/2, e1_val/2, ...]

        vperm   #$23232323,e2,e2,e14
        vperm   #$67676767,e2,e2,e15
        pmulh   e14,e17,e4
        pmulh   e15,e18,e5
        paddw   e5,e4,e4                ; [e2_val/2, e3_val/2, ...]

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
        paddw   e13,e12,e12             ; [d0/2, d1/2, ...]

        vperm   #$018945cd,e10,e11,e13
        vperm   #$23ab67ef,e10,e11,e15
        paddw   e15,e13,e13
        vperm   #$45670123,e13,e13,e15
        paddw   e15,e13,e13             ; [d2/2, d3/2, ...]

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
        lea     (sp),a0                 ; tmp1
        lea     128(sp),a1              ; tmp2
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
        lea     128(sp),a2              ; tmp2
        lea     (sp),a4                 ; tmp1
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

        store   e0,(a4)
        store   e1,8(a4)
        lea     16(a2),a2
        lea     16(a4),a4
        dbra    d2,.inreg_irow2

        ; --- Step 4: Inlined Transpose tmp1 -> dst ---
        lea     (sp),a0                 ; tmp1
        move.l  a3,a1                   ; dst
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

        ; --- Step 5: Double dst to restore 1.0x scale ---
        move.l  a3,a0
        moveq   #8-1,d0
.inreg_iscale:
        load    (a0),e0
        load    8(a0),e1
        paddw   e0,e0,e0
        paddw   e1,e1,e1
        store   e0,(a0)
        store   e1,8(a0)
        lea     16(a0),a0
        dbra    d0,.inreg_iscale

        adda.w  #256,sp
        movem.l (sp)+,d2/a2-a4
        rts

; ======================================================================
; void idct2d_8x8_sparse_ammx(const int16_t *src, int16_t *dst)
; High-performance Sparse / DC-Only IDCT Early Exit (~15-20 cycles)
; ======================================================================
_idct2d_8x8_sparse_ammx:
        move.l  4(sp),a0                ; src
        move.l  8(sp),a1                ; dst

        ; Check word 1 (AC1) and words 2, 3 (AC2, AC3) of row 0
        ; a0 points to src: word 0 is DC, word 1 is AC1
        move.l  (a0),d0                 ; d0 = [DC (high word), AC1 (low word)]
        tst.w   d0                      ; test low word (AC1)
        bne     .sparse_fallback
        tst.l   4(a0)                   ; test AC2, AC3
        bne     .sparse_fallback

        ; Check remaining 60 AC words via AMMX bitwise OR
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

        ; Test if e0 is zero
        move.l  e0,d0
        vperm   #$45670123,e0,e0,e1
        move.l  e1,d1
        or.l    d1,d0
        bne     .sparse_fallback

        ; --- FAST PATH: DC-ONLY BLOCK (~15-20 CYCLES!) ---
        ; For 2D Orthonormal IDCT: pixel = round(DC / 8)
        move.w  (a0),d0
        addq.w  #4,d0
        asr.w   #3,d0                   ; d0 = round(DC / 8)
        move.w  d0,d1
        swap    d1
        move.w  d0,d1                   ; d1 = [pixel, pixel]

        subq.l  #8,sp
        move.l  d1,(sp)
        move.l  d1,4(sp)
        load    (sp),e0                 ; e0 = [pixel, pixel, pixel, pixel]
        addq.l  #8,sp

        store   e0,(a1)
        store   e0,8(a1)
        store   e0,16(a1)
        store   e0,24(a1)
        store   e0,32(a1)
        store   e0,40(a1)
        store   e0,48(a1)
        store   e0,56(a1)
        store   e0,64(a1)
        store   e0,72(a1)
        store   e0,80(a1)
        store   e0,88(a1)
        store   e0,96(a1)
        store   e0,104(a1)
        store   e0,112(a1)
        store   e0,120(a1)
        rts

.sparse_fallback:
        move.l  a1,-(sp)
        move.l  a0,-(sp)
        jsr     _idct2d_8x8_fixed_fast_inreg_ammx
        addq.l  #8,sp
        rts

; ======================================================================
; void quantize8x8_ammx(const int16_t *src, int16_t *dst, const int16_t *recip_q)
; Fast 8x8 block quantization (16 quadwords) using pmulh
; ======================================================================
_quantize8x8_ammx:
        move.l  a2,-(sp)
        move.l  8(sp),a0                ; src
        move.l  12(sp),a1               ; dst
        move.l  16(sp),a2               ; recip_q
        moveq   #16-1,d0
.q_loop:
        load    (a0),e0
        load    (a2),e1
        pmulh   e0,e1,e2
        paddw   e2,e2,e2                ; * 2 for Q1.15
        store   e2,(a1)
        lea     8(a0),a0
        lea     8(a1),a1
        lea     8(a2),a2
        dbra    d0,.q_loop
        move.l  (sp)+,a2
        rts

; ======================================================================
; void dct2d_8x8_aan_quant_ammx(const int16_t *src, int16_t *dst, const int16_t *recip_q_aan)
; Fused AAN 2D DCT + Quantization in a single combined call
; ======================================================================
_dct2d_8x8_aan_quant_ammx:
        movem.l d2/a2-a4,-(sp)
        move.l  20(sp),a2               ; src
        move.l  24(sp),a3               ; dst
        move.l  28(sp),a4               ; recip_q_aan

        ; Run 2D Fast DCT into dst
        move.l  a3,-(sp)
        move.l  a2,-(sp)
        jsr     _dct2d_8x8_fixed_fast_inreg_ammx
        addq.l  #8,sp

        ; Run in-place quantization on dst
        move.l  a4,-(sp)                ; recip_q_aan
        move.l  a3,-(sp)                ; dst (out)
        move.l  a3,-(sp)                ; dst (in)
        jsr     _quantize8x8_ammx
        lea     12(sp),sp

        movem.l (sp)+,d2/a2-a4
        rts

; ======================================================================
; Data Section: Constants
; ======================================================================
        section .data,data
        align   8
_fast_c4:       dc.w    11585,11585,11585,11585
_fast_c2_s2:    dc.w    15137,6270,15137,6270
_fast_s2_mc2:   dc.w    6270,-15137,6270,-15137
_fast_odd1:     dc.w    16069,13623,9102,3196
_fast_odd3:     dc.w    13623,-3196,-16069,-9102
_fast_odd5:     dc.w    9102,-16069,3196,13623
_fast_odd7:     dc.w    3196,-9102,13623,-16069

        align   4
_fast_c4_f:     dc.s    0.35355339
_fast_c2_f:     dc.s    0.46193977
_fast_s2_f:     dc.s    0.19134172
_fast_c1_f:     dc.s    0.49039264
_fast_c3_f:     dc.s    0.41573481
_fast_c5_f:     dc.s    0.27778512
_fast_c7_f:     dc.s    0.09754516
