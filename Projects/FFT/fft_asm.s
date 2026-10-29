; fft_asm.s - Apollo 68080 Optimized FFT
; 32-bit Float FFT & 16-bit Q1.15 Fixed-Point FFT

        section .text,code
        xdef    _fft_float_asm
        xdef    _fft_fixed_asm
        xdef    _fft_fixed_ammx
        xdef    _get_ccc
        xref    _bit_reverse_float
        xref    _bit_reverse_fixed

_get_ccc:
        movec.l CCC,d0
        rts

; ======================================================================
; void fft_float_asm(complex_float *x, int n, const complex_float *tw)
; ======================================================================
_fft_float_asm:
        ; 1. Fetch parameters
        move.l  4(sp),a0                ; a0 = x
        move.l  8(sp),d0                ; d0 = n
        move.l  12(sp),a1               ; a1 = tw

        ; 2. Preserve callee-saved registers
        movem.l d2-d7/a2-a6,-(sp)
        fmovem.x fp2-fp7,-(sp)

        move.l  a0,a2                   ; a2 = x
        move.l  d0,d2                   ; d2 = n
        move.l  a1,a3                   ; a3 = tw

        ; 3. Bit-reversal
        move.l  d2,-(sp)
        move.l  a2,-(sp)
        jsr     _bit_reverse_float
        addq.l  #8,sp

        ; 4. Stage setup: initial len = 2
        moveq   #2,d3                   ; d3 = len = 2
        moveq   #1,d4                   ; d4 = half = 1
        move.l  #2048,d5                ; d5 = tw_stride = 2048
        move.l  d2,d0
        lsr.l   #1,d0                   ; num_blocks = n / 2
        move.l  d0,a0                   ; a0 = initial num_blocks
        moveq   #16,d7                  ; d7 = step = len * 8 = 16 bytes
        move.l  d7,a1                   ; a1 = step bytes

.flt_stage_loop:
        cmp.l   d2,d3                   ; len > n ?
        bhi     .flt_done

        ; Loop j = 0 .. half-1
        clr.l   d6                      ; d6 = j

.flt_j_loop:
        cmp.l   d4,d6
        bge     .flt_j_done

        ; tw_idx = j * tw_stride
        move.l  d6,d7
        mulu.w  d5,d7                   ; tw_idx
        lsl.l   #3,d7                   ; tw_idx * 8 bytes
        lea     (a3,d7.l),a4            ; a4 = &tw[tw_idx]

        fmove.s (a4),fp0                ; fp0 = wr
        fmove.s 4(a4),fp1               ; fp1 = wi

        ; Start pointers for u and v
        move.l  d6,d7
        lsl.l   #3,d7                   ; j * 8 bytes
        lea     (a2,d7.l),a5            ; a5 = u_ptr = &x[j]

        move.l  d4,d7
        lsl.l   #3,d7                   ; half * 8 bytes
        lea     (a5,d7.l),a6            ; a6 = v_ptr = &x[j + half]

        ; Number of blocks for this j
        move.l  a0,d0                   ; d0 = num_blocks
        move.l  a1,d7                   ; d7 = step bytes

        ; Check for j == 0 special case (no multiplications)
        tst.l   d6
        bne.s   .flt_general_loop

.flt_j0_loop:
        fmove.s (a6),fp2                ; vr
        fmove.s 4(a6),fp3               ; vi
        fmove.s (a5),fp4                ; ur
        fmove.s 4(a5),fp5               ; ui

        fadd    fp2,fp4,fp6             ; ur + vr
        fsub    fp2,fp4,fp2             ; ur - vr
        fadd    fp3,fp5,fp7             ; ui + vi
        fsub    fp3,fp5,fp3             ; ui - vi

        fmove.s fp6,(a5)
        fmove.s fp7,4(a5)
        fmove.s fp2,(a6)
        fmove.s fp3,4(a6)

        add.l   d7,a5
        add.l   d7,a6
        subq.l  #1,d0
        bne.s   .flt_j0_loop
        bra.s   .flt_next_j

.flt_general_loop:
        ; Load v
        fmove.s (a6),fp2                ; vr
        fmove.s 4(a6),fp3               ; vi

        ; Complex multiply: tr = wr*vr - wi*vi; ti = wr*vi + wi*vr
        fmul    fp0,fp2,fp4             ; wr * vr
        fmul    fp1,fp3,fp5             ; wi * vi
        fmul    fp0,fp3,fp6             ; wr * vi
        fmul    fp1,fp2,fp7             ; wi * vr

        fsub    fp5,fp4,fp4             ; tr = wr*vr - wi*vi
        fadd    fp7,fp6,fp6             ; ti = wr*vi + wi*vr

        ; Load u
        fmove.s (a5),fp2                ; ur
        fmove.s 4(a5),fp3               ; ui

        ; Butterfly additions and subtractions
        fadd    fp4,fp2,fp5             ; u'_r = ur + tr
        fsub    fp4,fp2,fp4             ; v'_r = ur - tr
        fadd    fp6,fp3,fp7             ; u'_i = ui + ti
        fsub    fp6,fp3,fp6             ; v'_i = ui - ti

        ; Store results
        fmove.s fp5,(a5)
        fmove.s fp7,4(a5)
        fmove.s fp4,(a6)
        fmove.s fp6,4(a6)

        add.l   d7,a5
        add.l   d7,a6
        subq.l  #1,d0
        bne.s   .flt_general_loop

.flt_next_j:
        addq.l  #1,d6
        bra     .flt_j_loop

.flt_j_done:
        lsl.l   #1,d3                   ; len <<= 1
        lsl.l   #1,d4                   ; half <<= 1
        lsr.l   #1,d5                   ; tw_stride >>= 1
        move.l  a0,d0
        lsr.l   #1,d0                   ; num_blocks >>= 1
        move.l  d0,a0
        move.l  a1,d7
        lsl.l   #1,d7                   ; step <<= 1
        move.l  d7,a1
        bra     .flt_stage_loop

.flt_done:
        fmovem.x (sp)+,fp2-fp7
        movem.l (sp)+,d2-d7/a2-a6
        rts


; ======================================================================
; void fft_fixed_asm(complex_int16 *x, int n, const complex_int16 *tw)
; ======================================================================
_fft_fixed_asm:
        move.l  4(sp),a0                ; a0 = x
        move.l  8(sp),d0                ; d0 = n
        move.l  12(sp),a1               ; a1 = tw

        movem.l d2-d7/a2-a5,-(sp)
        link.w  a6,#-40

        ; Frame layout:
        ; -4(a6)  : n
        ; -8(a6)  : len
        ; -12(a6) : half
        ; -16(a6) : tw_stride
        ; -20(a6) : j
        ; -24(a6) : num_blocks
        ; -28(a6) : step (bytes)
        ; -32(a6) : wr
        ; -36(a6) : wi

        move.l  a0,a2                   ; a2 = x
        move.l  d0,-4(a6)               ; n
        move.l  a1,a3                   ; a3 = tw

        ; 1. Bit-reversal
        move.l  d0,-(sp)
        move.l  a2,-(sp)
        jsr     _bit_reverse_fixed
        addq.l  #8,sp

        ; 2. Stage setup: initial len = 2
        move.l  #2,-8(a6)               ; len = 2
        move.l  #1,-12(a6)              ; half = 1
        move.l  #2048,-16(a6)           ; tw_stride = 2048
        move.l  -4(a6),d0
        lsr.l   #1,d0
        move.l  d0,-24(a6)              ; num_blocks = n / 2
        move.l  #8,-28(a6)              ; step = len * 4 = 8 bytes

.fix_stage_loop:
        move.l  -8(a6),d0               ; len
        cmp.l   -4(a6),d0               ; len > n ?
        bhi     .fix_done

        ; Loop j = 0 .. half-1
        clr.l   -20(a6)                 ; j = 0

.fix_j_loop:
        move.l  -20(a6),d0              ; j
        cmp.l   -12(a6),d0              ; j >= half ?
        bge     .fix_j_done

        ; tw_idx = j * tw_stride
        move.l  -16(a6),d1
        mulu.w  d0,d1                   ; tw_idx = tw_stride * j
        lsl.l   #2,d1                   ; tw_idx * 4 bytes
        lea     (a3,d1.l),a4            ; &tw[tw_idx]

        move.w  (a4),d2
        ext.l   d2
        move.l  d2,-32(a6)              ; wr
        move.w  2(a4),d2
        ext.l   d2
        move.l  d2,-36(a6)              ; wi

        ; Start u_ptr = &x[j]
        move.l  d0,d1
        lsl.l   #2,d1                   ; j * 4 bytes
        lea     (a2,d1.l),a4            ; a4 = u_ptr = &x[j]

        ; Start v_ptr = &x[j + half]
        move.l  -12(a6),d1              ; half
        lsl.l   #2,d1                   ; half * 4 bytes
        lea     (a4,d1.l),a5            ; a5 = v_ptr = &x[j + half]

        move.l  -24(a6),d0              ; d0 = num_blocks
        move.l  -28(a6),a1              ; a1 = step bytes

        ; Check if j == 0
        tst.l   -20(a6)
        bne.s   .fix_general_loop

.fix_j0_loop:
        move.w  (a4),d1                 ; ur
        ext.l   d1
        move.w  2(a4),d2                ; ui
        ext.l   d2
        move.w  (a5),d3                 ; vr
        ext.l   d3
        move.w  2(a5),d4                ; vi
        ext.l   d4

        ; Real: (ur + vr) >> 1, (ur - vr) >> 1
        move.l  d1,d5
        add.l   d3,d1
        sub.l   d3,d5
        asr.l   #1,d1
        asr.l   #1,d5

        ; Imag: (ui + vi) >> 1, (ui - vi) >> 1
        move.l  d2,d6
        add.l   d4,d2
        sub.l   d4,d6
        asr.l   #1,d2
        asr.l   #1,d6

        ; Store
        move.w  d1,(a4)
        move.w  d2,2(a4)
        move.w  d5,(a5)
        move.w  d6,2(a5)

        add.l   a1,a4                   ; u_ptr += step
        add.l   a1,a5                   ; v_ptr += step
        subq.l  #1,d0
        bne.s   .fix_j0_loop
        bra.s   .fix_next_j

.fix_general_loop:
        ; Preload wr into d2, wi into d3
        move.l  -32(a6),d2              ; wr
        move.l  -36(a6),d3              ; wi

.fix_gen_blk:
        ; Load v
        move.w  (a5),d1                 ; vr
        move.w  2(a5),d4                ; vi

        ; wr * vr
        move.l  d2,d5
        muls.w  d1,d5                   ; d5 = wr * vr

        ; wi * vi
        move.l  d3,d6
        muls.w  d4,d6                   ; d6 = wi * vi

        ; tr = (wr * vr - wi * vi) >> 15
        sub.l   d6,d5
        asr.l   #8,d5
        asr.l   #7,d5                   ; d5 = tr

        ; wr * vi
        move.l  d2,d6
        muls.w  d4,d6                   ; d6 = wr * vi

        ; wi * vr
        move.l  d3,d7
        muls.w  d1,d7                   ; d7 = wi * vr

        ; ti = (wr * vi + wi * vr) >> 15
        add.l   d7,d6
        asr.l   #8,d6
        asr.l   #7,d6                   ; d6 = ti

        ; Load u
        move.w  (a4),d1                 ; ur
        ext.l   d1
        move.w  2(a4),d4                ; ui
        ext.l   d4

        ; ur + tr, ur - tr, >> 1 (in 32-bit to prevent overflow!)
        move.l  d1,d7
        add.l   d5,d1
        sub.l   d5,d7
        asr.l   #1,d1                   ; u'_r
        asr.l   #1,d7                   ; v'_r

        ; ui + ti, ui - ti, >> 1 (in 32-bit to prevent overflow!)
        move.l  d4,d5
        add.l   d6,d4
        sub.l   d6,d5
        asr.l   #1,d4                   ; u'_i
        asr.l   #1,d5                   ; v'_i

        ; Store
        move.w  d1,(a4)
        move.w  d4,2(a4)
        move.w  d7,(a5)
        move.w  d5,2(a5)

        add.l   a1,a4
        add.l   a1,a5
        subq.l  #1,d0
        bne.s   .fix_gen_blk

.fix_next_j:
        addq.l  #1,-20(a6)              ; j++
        bra     .fix_j_loop

.fix_j_done:
        move.l  -8(a6),d0
        lsl.l   #1,d0
        move.l  d0,-8(a6)               ; len <<= 1

        move.l  -12(a6),d0
        lsl.l   #1,d0
        move.l  d0,-12(a6)              ; half <<= 1

        move.l  -16(a6),d0
        lsr.l   #1,d0
        move.l  d0,-16(a6)              ; tw_stride >>= 1

        move.l  -24(a6),d0
        lsr.l   #1,d0
        move.l  d0,-24(a6)              ; num_blocks >>= 1

        move.l  -28(a6),d0
        lsl.l   #1,d0
        move.l  d0,-28(a6)              ; step <<= 1

        bra     .fix_stage_loop

.fix_done:
        unlk    a6
        movem.l (sp)+,d2-d7/a2-a5
        rts

; ======================================================================
; void fft_fixed_ammx(complex_int16 *x, int n, const complex_int16 *tw)
; ======================================================================
_fft_fixed_ammx:
        move.l  4(sp),a0                ; a0 = x
        move.l  8(sp),d0                ; d0 = n
        move.l  12(sp),a1               ; a1 = tw

        movem.l d2-d7/a2-a6,-(sp)
        link.w  a6,#-48

        move.l  a0,a2                   ; a2 = x
        move.l  d0,-4(a6)               ; n
        move.l  a1,a3                   ; a3 = tw

        ; 1. Bit-reversal
        move.l  -4(a6),-(sp)            ; push n
        move.l  a2,-(sp)                ; push x
        jsr     _bit_reverse_fixed
        addq.l  #8,sp

        ; 2. Preload AMMX permanent constants
        load.w  #$8000,e10              ; e10 = $8000 for >> 1 scaling
        peor    e11,e11,e11             ; e11 = 0

        ; 3. Stage 1: len = 2, half = 1 (all butterflies j = 0)
        move.l  -4(a6),d0
        lsr.l   #1,d0                   ; num_blocks = n / 2
        move.l  a2,a4                   ; a4 = u_ptr = &x[0]
        lea     4(a2),a5                ; a5 = v_ptr = &x[1]
        moveq   #8,d7                   ; step = 8 bytes

.ammx_stg1_loop:
        move.w  (a4),d1                 ; ur
        ext.l   d1
        move.w  2(a4),d2                ; ui
        ext.l   d2
        move.w  (a5),d3                 ; vr
        ext.l   d3
        move.w  2(a5),d4                ; vi
        ext.l   d4

        ; Real: (ur + vr) >> 1, (ur - vr) >> 1
        move.l  d1,d5
        add.l   d3,d1
        sub.l   d3,d5
        asr.l   #1,d1
        asr.l   #1,d5

        ; Imag: (ui + vi) >> 1, (ui - vi) >> 1
        move.l  d2,d6
        add.l   d4,d2
        sub.l   d4,d6
        asr.l   #1,d2
        asr.l   #1,d6

        ; Store
        move.w  d1,(a4)
        move.w  d2,2(a4)
        move.w  d5,(a5)
        move.w  d6,2(a5)

        add.l   d7,a4
        add.l   d7,a5
        subq.l  #1,d0
        bne.s   .ammx_stg1_loop

        ; 4. Stages 2..log2(N): len = 4, 8, ..., n
        move.l  #4,-8(a6)               ; len = 4
        move.l  #2,-12(a6)              ; half = 2
        move.l  #1024,-16(a6)           ; tw_stride = 1024
        move.l  -4(a6),d0
        lsr.l   #2,d0
        move.l  d0,-24(a6)              ; num_blocks = n / 4
        move.l  #16,-28(a6)             ; step = len * 4 = 16 bytes

.ammx_stage_loop:
        move.l  -8(a6),d0               ; len
        cmp.l   -4(a6),d0               ; len > n ?
        bhi     .ammx_done

        ; Loop j = 0, 2, 4, ..., half-2 (steps of 2 butterflies)
        clr.l   -20(a6)                 ; j = 0

.ammx_j_loop:
        move.l  -20(a6),d0              ; j
        cmp.l   -12(a6),d0              ; j >= half ?
        bge     .ammx_j_done

        ; tw_idx0 = j * tw_stride
        ; tw_idx1 = tw_idx0 + tw_stride
        move.l  -16(a6),d1              ; tw_stride
        mulu.w  d0,d1                   ; tw_idx0 = tw_stride * j
        lsl.l   #2,d1                   ; tw_idx0 * 4 bytes
        lea     (a3,d1.l),a0            ; a0 = &tw[tw_idx0]

        move.l  -16(a6),d2              ; tw_stride
        lsl.l   #2,d2                   ; tw_stride * 4 bytes
        lea     (a0,d2.l),a1            ; a1 = &tw[tw_idx1]

        ; W0 = (wr0, wi0) from (a0)
        move.w  (a0),d0                 ; wr0
        move.w  2(a0),d1                ; wi0
        ; W1 = (wr1, wi1) from (a1)
        move.w  (a1),d2                 ; wr1
        move.w  2(a1),d3                ; wi1

        ; Build e8 = [wr0, wi0, wr1, wi1]
        move.w  d0,-36(a6)
        move.w  d1,-34(a6)
        move.w  d2,-32(a6)
        move.w  d3,-30(a6)
        load    -36(a6),e8

        ; Build e9 = [-wi0, wr0, -wi1, wr1]
        neg.w   d1                      ; -wi0
        neg.w   d3                      ; -wi1
        move.w  d1,-44(a6)
        move.w  d0,-42(a6)
        move.w  d3,-40(a6)
        move.w  d2,-38(a6)
        load    -44(a6),e9

        ; Start u_ptr = &x[j]
        move.l  -20(a6),d0              ; j
        lsl.l   #2,d0                   ; j * 4 bytes
        lea     (a2,d0.l),a4            ; a4 = u_ptr = &x[j]

        ; Start v_ptr = &x[j + half]
        move.l  -12(a6),d1              ; half
        lsl.l   #2,d1                   ; half * 4 bytes
        lea     (a4,d1.l),a5            ; a5 = v_ptr = &x[j + half]

        move.l  -24(a6),d0              ; d0 = num_blocks
        move.l  -28(a6),a1              ; a1 = step bytes

.ammx_blk_loop:
        load    (a5),e0                 ; load 2 v's = [v0.r, v0.i, v1.r, v1.i]
        vperm   #$01014545,e0,e0,e1     ; e1 = V1 = [v0.r, v0.r, v1.r, v1.r]
        vperm   #$23236767,e0,e0,e2     ; e2 = V2 = [v0.i, v0.i, v1.i, v1.i]

        pmulh   e8,e1,e3                ; e3 = W1 * V1
        pmulh   e9,e2,e4                ; e4 = W2 * V2
        paddw   e4,e3,e5                ; e5 = t_half

        load    (a4),e6                 ; load 2 u's = [u0.r, u0.i, u1.r, u1.i]
        pmulh   e10,e6,e7               ; e7 = -(u >> 1)
        psubw   e7,e11,e6               ; e6 = +(u >> 1) = u_half

        bflyw   e5,e6,e12:e13           ; e12 = u', e13 = v'

        store   e12,(a4)
        store   e13,(a5)

        add.l   a1,a4                   ; u_ptr += step
        add.l   a1,a5                   ; v_ptr += step
        subq.l  #1,d0
        bne.s   .ammx_blk_loop

        addq.l  #2,-20(a6)              ; j += 2
        bra     .ammx_j_loop

.ammx_j_done:
        move.l  -8(a6),d0
        lsl.l   #1,d0
        move.l  d0,-8(a6)               ; len <<= 1

        move.l  -12(a6),d0
        lsl.l   #1,d0
        move.l  d0,-12(a6)              ; half <<= 1

        move.l  -16(a6),d0
        lsr.l   #1,d0
        move.l  d0,-16(a6)              ; tw_stride >>= 1

        move.l  -24(a6),d0
        lsr.l   #1,d0
        move.l  d0,-24(a6)              ; num_blocks >>= 1

        move.l  -28(a6),d0
        lsl.l   #1,d0
        move.l  d0,-28(a6)              ; step <<= 1

        bra     .ammx_stage_loop

.ammx_done:
        unlk    a6
        movem.l (sp)+,d2-d7/a2-a6
        rts
