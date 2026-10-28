; fft_asm.s - Apollo 68080 Optimized FFT
; 32-bit Float FFT & 16-bit Q1.15 Fixed-Point FFT

        section .text,code
        xdef    _fft_float_asm
        xdef    _fft_fixed_asm
        xref    _bit_reverse_float
        xref    _bit_reverse_fixed

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

        ; 4. Stage loop: len = 2, 4, 8, ..., n
        moveq   #2,d3                   ; d3 = len

.flt_stage_loop:
        cmp.l   d2,d3
        bhi     .flt_done

        move.l  d3,d4
        lsr.l   #1,d4                   ; d4 = half = len / 2

        move.l  #4096,d5
        divu.w  d3,d5                   ; d5 = tw_stride = 4096 / len
        and.l   #$ffff,d5

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

        move.l  d3,d7
        lsl.l   #3,d7                   ; d7 = step = len * 8 bytes

        ; Number of blocks = n / len
        move.l  d2,d0
        divu.w  d3,d0
        and.l   #$ffff,d0               ; d0 = num_blocks

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
        link.w  a6,#-32

        move.l  a0,a2                   ; a2 = x
        move.l  d0,-4(a6)               ; n
        move.l  a1,a3                   ; a3 = tw

        ; 1. Bit-reversal
        move.l  d0,-(sp)
        move.l  a2,-(sp)
        jsr     _bit_reverse_fixed
        addq.l  #8,sp

        ; 2. Stage loop: len = 2, 4, ..., n
        move.l  #2,-8(a6)               ; len = 2

.fix_stage_loop:
        move.l  -8(a6),d0               ; len
        cmp.l   -4(a6),d0               ; cmp n, len
        bhi     .fix_done

        move.l  d0,d1
        lsr.l   #1,d1                   ; half = len / 2
        move.l  d1,-12(a6)              ; save half

        move.l  #4096,d2
        divu.w  d0,d2
        and.l   #$ffff,d2
        move.l  d2,-16(a6)              ; save tw_stride = 4096 / len

        move.l  d0,d1
        lsl.l   #2,d1                   ; step = len * 4 bytes
        move.l  d1,-24(a6)              ; save step

        ; Loop j = 0 .. half-1
        clr.l   -20(a6)                 ; j = 0

.fix_j_loop:
        move.l  -20(a6),d0              ; j
        cmp.l   -12(a6),d0              ; cmp half, j
        bge     .fix_j_done

        ; tw_idx = j * tw_stride
        move.l  d0,d1
        mulu.w  -16(a6),d1              ; tw_idx
        lsl.l   #2,d1                   ; tw_idx * 4 bytes
        lea     (a3,d1.l),a4            ; &tw[tw_idx]

        move.w  (a4),d2
        ext.l   d2
        move.l  d2,-28(a6)              ; wr
        move.w  2(a4),d2
        ext.l   d2
        move.l  d2,-32(a6)              ; wi

        ; Start u_ptr = &x[j]
        move.l  d0,d1
        lsl.l   #2,d1                   ; j * 4 bytes
        lea     (a2,d1.l),a4            ; a4 = u_ptr = &x[j]

        ; Start v_ptr = &x[j + half]
        move.l  -12(a6),d1              ; half
        lsl.l   #2,d1                   ; half * 4 bytes
        lea     (a4,d1.l),a5            ; a5 = v_ptr = &x[j + half]

        ; Number of blocks = n / len
        move.l  -4(a6),d0               ; n
        divu.w  -8(a6),d0               ; n / len
        and.l   #$ffff,d0               ; d0 = num_blocks

        move.l  -24(a6),a1              ; a1 = step bytes

        ; Check if j == 0
        tst.l   -20(a6)
        bne.s   .fix_general_loop

.fix_j0_loop:
        move.w  (a4),d1                 ; ur
        move.w  2(a4),d2                ; ui
        move.w  (a5),d3                 ; vr
        move.w  2(a5),d4                ; vi

        ; Real: (ur + vr) >> 1, (ur - vr) >> 1
        move.w  d1,d5
        add.w   d3,d1
        sub.w   d3,d5
        asr.w   #1,d1
        asr.w   #1,d5

        ; Imag: (ui + vi) >> 1, (ui - vi) >> 1
        move.w  d2,d6
        add.w   d4,d2
        sub.w   d4,d6
        asr.w   #1,d2
        asr.w   #1,d6

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
        move.l  -28(a6),d2              ; wr
        move.l  -32(a6),d3              ; wi

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
        move.w  2(a4),d4                ; ui

        ; ur + tr, ur - tr, >> 1
        move.w  d1,d7
        add.w   d5,d1
        sub.w   d5,d7
        asr.w   #1,d1                   ; u'_r
        asr.w   #1,d7                   ; v'_r

        ; ui + ti, ui - ti, >> 1
        move.w  d4,d5
        add.w   d6,d4
        sub.w   d6,d5
        asr.w   #1,d4                   ; u'_i
        asr.w   #1,d5                   ; v'_i

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
        bra     .fix_stage_loop

.fix_done:
        unlk    a6
        movem.l (sp)+,d2-d7/a2-a5
        rts
