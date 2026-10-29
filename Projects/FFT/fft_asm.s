; ======================================================================
; fft_asm.s - Apollo 68080 High-Performance FFT Library
; Formats: 32-bit Single-Precision Float & 16-bit Q1.15 Fixed-Point
; Targets: Apollo 68080 (Vampire V4) with AMMX SIMD & 68881/68080 FPU
; ======================================================================

        section .text,code
        xdef    _fft_float_asm
        xdef    _fft_fixed_asm
        xdef    _fft_fixed_ammx
        xdef    _fft_breakdown_ammx
        xdef    _bit_reverse_fixed_asm
        xdef    _bit_reverse_float_asm
        xdef    _get_ccc
        xref    _fft_swap_table
        xref    _fft_swap_count

_get_ccc:
        movec.l CCC,d0
        rts

; ======================================================================
; void bit_reverse_fixed_asm(complex_int16 *x, int n)
; Table-accelerated in-place bit reversal for 32-bit complex entries
; ======================================================================
_bit_reverse_fixed_asm:
        move.l  4(sp),a0                ; a0 = x
        move.l  8(sp),d0                ; d0 = n

        ; Table lookup for standard sizes:
        moveq   #0,d1
        cmp.l   #64,d0
        beq.s   .have_fix_table
        addq.l  #1,d1
        cmp.l   #128,d0
        beq.s   .have_fix_table
        addq.l  #1,d1
        cmp.l   #256,d0
        beq.s   .have_fix_table
        addq.l  #1,d1
        cmp.l   #512,d0
        beq.s   .have_fix_table
        addq.l  #1,d1
        cmp.l   #1024,d0
        beq.s   .have_fix_table
        addq.l  #1,d1
        cmp.l   #2048,d0
        beq.s   .have_fix_table
        addq.l  #1,d1
        cmp.l   #4096,d0
        beq.s   .have_fix_table
        bra.s   .br_fix_algo

.have_fix_table:
        movem.l d2-d4/a2,-(sp)
        lea     _fft_swap_table,a1
        move.l  (a1,d1.l*4),a1          ; a1 = pointer to swap_pair array
        lea     _fft_swap_count,a2
        move.w  (a2,d1.l*2),d0          ; d0 = swap count
        beq.s   .table_fix_done

.table_fix_loop:
        move.w  (a1)+,d1                ; i
        move.w  (a1)+,d2                ; j
        move.l  (a0,d1.w*4),d3
        move.l  (a0,d2.w*4),d4
        move.l  d4,(a0,d1.w*4)
        move.l  d3,(a0,d2.w*4)
        subq.w  #1,d0
        bne.s   .table_fix_loop

.table_fix_done:
        movem.l (sp)+,d2-d4/a2
        rts

.br_fix_algo:
        movem.l d2-d6,-(sp)
        move.l  d0,d4
        subq.l  #1,d4                   ; d4 = n - 1
        clr.l   d1                      ; d1 = i = 0
        clr.l   d2                      ; d2 = j = 0

.br_fix_loop:
        cmp.l   d2,d1                   ; compare i with j
        bge.s   .br_fix_noswap

        move.l  (a0,d1.l*4),d5
        move.l  (a0,d2.l*4),d6
        move.l  d6,(a0,d1.l*4)
        move.l  d5,(a0,d2.l*4)

.br_fix_noswap:
        move.l  d0,d3
        lsr.l   #1,d3                   ; k = n >> 1

.br_fix_kloop:
        cmp.l   d2,d3                   ; while (k <= j)
        bhi.s   .br_fix_kdone
        sub.l   d3,d2                   ; j -= k
        lsr.l   #1,d3                   ; k >>= 1
        bra.s   .br_fix_kloop

.br_fix_kdone:
        add.l   d3,d2                   ; j += k
        addq.l  #1,d1                   ; i++
        cmp.l   d4,d1                   ; i < n - 1 ?
        blt.s   .br_fix_loop

        movem.l (sp)+,d2-d6
        rts

; ======================================================================
; void bit_reverse_float_asm(complex_float *x, int n)
; Table-accelerated in-place bit reversal for 64-bit complex float
; ======================================================================
_bit_reverse_float_asm:
        move.l  4(sp),a0                ; a0 = x
        move.l  8(sp),d0                ; d0 = n

        ; Table lookup for standard sizes:
        moveq   #0,d1
        cmp.l   #64,d0
        beq.s   .have_flt_table
        addq.l  #1,d1
        cmp.l   #128,d0
        beq.s   .have_flt_table
        addq.l  #1,d1
        cmp.l   #256,d0
        beq.s   .have_flt_table
        addq.l  #1,d1
        cmp.l   #512,d0
        beq.s   .have_flt_table
        addq.l  #1,d1
        cmp.l   #1024,d0
        beq.s   .have_flt_table
        addq.l  #1,d1
        cmp.l   #2048,d0
        beq.s   .have_flt_table
        addq.l  #1,d1
        cmp.l   #4096,d0
        beq.s   .have_flt_table
        bra.s   .br_flt_algo

.have_flt_table:
        movem.l d2/a2,-(sp)
        lea     _fft_swap_table,a1
        move.l  (a1,d1.l*4),a1
        lea     _fft_swap_count,a2
        move.w  (a2,d1.l*2),d0
        beq.s   .flt_table_done

.table_flt_loop:
        move.w  (a1)+,d1
        move.w  (a1)+,d2
        load    (a0,d1.w*8),e0
        load    (a0,d2.w*8),e1
        store   e1,(a0,d1.w*8)
        store   e0,(a0,d2.w*8)
        subq.w  #1,d0
        bne.s   .table_flt_loop

.flt_table_done:
        movem.l (sp)+,d2/a2
        rts

.br_flt_algo:
        movem.l d2-d5,-(sp)
        move.l  d0,d4
        subq.l  #1,d4                   ; d4 = n - 1
        clr.l   d1                      ; d1 = i = 0
        clr.l   d2                      ; d2 = j = 0

.br_flt_loop:
        cmp.l   d2,d1                   ; compare i with j
        bge.s   .br_flt_noswap

        load    (a0,d1.l*8),e0
        load    (a0,d2.l*8),e1
        store   e1,(a0,d1.l*8)
        store   e0,(a0,d2.l*8)

.br_flt_noswap:
        move.l  d0,d3
        lsr.l   #1,d3                   ; k = n >> 1

.br_flt_kloop:
        cmp.l   d2,d3                   ; while (k <= j)
        bhi.s   .br_flt_kdone
        sub.l   d3,d2                   ; j -= k
        lsr.l   #1,d3                   ; k >>= 1
        bra.s   .br_flt_kloop

.br_flt_kdone:
        add.l   d3,d2                   ; j += k
        addq.l  #1,d1                   ; i++
        cmp.l   d4,d1                   ; i < n - 1 ?
        blt.s   .br_flt_loop

        movem.l (sp)+,d2-d5
        rts


; ======================================================================
; void fft_float_asm(complex_float *x, int n, const complex_float *tw)
; 32-bit Float FFT optimized with register pointer arithmetic and
; interleaved FPU execution
; ======================================================================
_fft_float_asm:
        move.l  4(sp),a0                ; a0 = x
        move.l  8(sp),d0                ; d0 = n
        move.l  12(sp),a1               ; a1 = tw

        movem.l d2-d7/a2-a6,-(sp)
        fmovem.x fp2-fp7,-(sp)
        subq.l  #8,sp                   ; local storage: 0(sp) = x, 4(sp) = n

        move.l  a0,(sp)                 ; store x
        move.l  d0,4(sp)                ; store n
        move.l  a1,a3                   ; a3 = tw

        ; 1. Fast Bit-Reversal
        move.l  d0,-(sp)
        move.l  a0,-(sp)
        jsr     _bit_reverse_float_asm
        addq.l  #8,sp

        ; 2. Stages 1..log2(N)
        moveq   #2,d2                   ; d2 = len = 2
        moveq   #1,d3                   ; d3 = half = 1
        move.l  #16384,d5               ; d5 = tw_step = tw_stride * 8 bytes = (2048/1) * 8 = 16384
        move.l  4(sp),d6
        lsr.l   #1,d6                   ; d6 = num_blocks = n / 2
        moveq   #16,d7                  ; d7 = step = len * 8 = 16 bytes
        move.l  d7,a6                   ; a6 = step

.flt_stage_loop:
        cmp.l   4(sp),d2                ; len > n ?
        bhi     .flt_done

        ; Initialize j-loop pointers
        move.l  a3,a2                   ; a2 = tw_ptr (starts at &tw[0])
        move.l  (sp),a4                 ; a4 = u_base = x
        move.l  d3,d0                   ; half
        lsl.l   #3,d0                   ; half * 8 bytes
        lea     (a4,d0.l),a5            ; a5 = v_base = &x[half]

        move.l  d3,d4                   ; d4 = j_count = half

.flt_j_loop:
        ; Load twiddle factor: fp0 = wr, fp1 = wi
        fmove.s (a2),fp0
        fmove.s 4(a2),fp1

        move.l  a4,a0                   ; a0 = u_ptr
        move.l  a5,a1                   ; a1 = v_ptr
        move.l  d6,d7                   ; d7 = num_blocks

        ; Check if twiddle is (1.0, 0.0)
        cmp.l   a3,a2
        bne.s   .flt_gen_loop

.flt_j0_loop:
        fmove.s (a1),fp2                ; vr
        fmove.s 4(a1),fp3               ; vi
        fmove.s (a0),fp4                ; ur
        fmove.s 4(a0),fp5               ; ui

        fadd    fp2,fp4,fp6             ; ur + vr
        fsub    fp2,fp4,fp2             ; ur - vr
        fadd    fp3,fp5,fp7             ; ui + vi
        fsub    fp3,fp5,fp3             ; ui - vi

        fmove.s fp6,(a0)
        fmove.s fp7,4(a0)
        fmove.s fp2,(a1)
        fmove.s fp3,4(a1)

        add.l   a6,a0
        add.l   a6,a1
        subq.l  #1,d7
        bne.s   .flt_j0_loop
        bra.s   .flt_next_j

.flt_gen_loop:
        fmove.s (a1),fp2                ; vr
        fmove.s 4(a1),fp3               ; vi

        ; Complex multiplication:
        ; tr = wr*vr - wi*vi
        ; ti = wr*vi + wi*vr
        fmul    fp0,fp2,fp6             ; wr * vr
        fmul    fp1,fp3,fp7             ; wi * vi
        fmul    fp0,fp3,fp4             ; wr * vi
        fmul    fp1,fp2,fp5             ; wi * vr

        fsub    fp7,fp6,fp6             ; tr = wr*vr - wi*vi
        fadd    fp5,fp4,fp4             ; ti = wr*vi + wi*vr

        ; Load u
        fmove.s (a0),fp2                ; ur
        fmove.s 4(a0),fp3               ; ui

        ; Butterfly additions
        fadd    fp6,fp2,fp7             ; u'_r = ur + tr
        fsub    fp6,fp2,fp6             ; v'_r = ur - tr
        fadd    fp4,fp3,fp5             ; u'_i = ui + ti
        fsub    fp4,fp3,fp4             ; v'_i = ui - ti

        fmove.s fp7,(a0)
        fmove.s fp5,4(a0)
        fmove.s fp6,(a1)
        fmove.s fp4,4(a1)

        add.l   a6,a0
        add.l   a6,a1
        subq.l  #1,d7
        bne.s   .flt_gen_loop

.flt_next_j:
        add.l   d5,a2                   ; tw_ptr += tw_step
        addq.l  #8,a4                   ; u_base += 8
        addq.l  #8,a5                   ; v_base += 8
        subq.l  #1,d4
        bne     .flt_j_loop

        ; Advance stage:
        lsl.l   #1,d2                   ; len <<= 1
        lsl.l   #1,d3                   ; half <<= 1
        lsr.l   #1,d5                   ; tw_step >>= 1
        lsr.l   #1,d6                   ; num_blocks >>= 1
        add.l   a6,a6                   ; step = step * 2
        bra     .flt_stage_loop

.flt_done:
        addq.l  #8,sp
        fmovem.x (sp)+,fp2-fp7
        movem.l (sp)+,d2-d7/a2-a6
        rts


; ======================================================================
; void fft_fixed_asm(complex_int16 *x, int n, const complex_int16 *tw)
; 16-bit Q1.15 Fixed-Point FFT optimized with pure register variables
; ======================================================================
_fft_fixed_asm:
        move.l  4(sp),a0                ; a0 = x
        move.l  8(sp),d0                ; d0 = n
        move.l  12(sp),a1               ; a1 = tw

        movem.l d2-d7/a2-a6,-(sp)
        sub.l   #32,sp                  ; local storage:
                                        ;  0(sp) = x
                                        ;  4(sp) = n
                                        ;  8(sp) = len
                                        ; 12(sp) = half
                                        ; 16(sp) = tw_step
                                        ; 20(sp) = num_blocks
                                        ; 24(sp) = wr
                                        ; 28(sp) = wi

        move.l  a0,(sp)
        move.l  d0,4(sp)
        move.l  a1,a3                   ; a3 = tw_base

        ; 1. Fast Bit-Reversal
        move.l  d0,-(sp)
        move.l  a0,-(sp)
        jsr     _bit_reverse_fixed_asm
        addq.l  #8,sp

        ; 2. Stages 1..log2(N)
        move.l  #2,8(sp)                ; len = 2
        move.l  #1,12(sp)               ; half = 1
        move.l  #8192,16(sp)            ; tw_step = (2048/1) * 4 = 8192
        move.l  4(sp),d0
        lsr.l   #1,d0
        move.l  d0,20(sp)               ; num_blocks = n / 2
        moveq   #8,d0
        move.l  d0,a6                   ; a6 = step = 8 bytes

.fix_stage_loop:
        move.l  8(sp),d0
        cmp.l   4(sp),d0                ; len > n ?
        bhi     .fix_done

        ; Initialize j-loop pointers
        move.l  a3,a2                   ; a2 = tw_ptr
        move.l  (sp),a4                 ; a4 = u_base = x
        move.l  12(sp),d0               ; half
        lsl.l   #2,d0                   ; half * 4 bytes
        lea     (a4,d0.l),a5            ; a5 = v_base = &x[half]

        move.l  12(sp),d7               ; d7 = j_count = half

.fix_j_loop:
        move.w  (a2),d0                 ; wr
        ext.l   d0
        move.l  d0,24(sp)
        move.w  2(a2),d1                ; wi
        ext.l   d1
        move.l  d1,28(sp)

        move.l  a4,a0                   ; a0 = u_ptr
        move.l  a5,a1                   ; a1 = v_ptr
        move.l  20(sp),d6               ; d6 = num_blocks

        cmp.l   a3,a2                   ; is twiddle (1.0, 0.0)?
        bne.s   .fix_gen_loop

.fix_j0_loop:
        move.w  (a0),d0                 ; ur
        ext.l   d0
        move.w  2(a0),d1                ; ui
        ext.l   d1
        move.w  (a1),d2                 ; vr
        ext.l   d2
        move.w  2(a1),d3                ; vi
        ext.l   d3

        ; Real: (ur + vr) >> 1, (ur - vr) >> 1
        move.l  d0,d4
        add.l   d2,d0
        sub.l   d2,d4
        asr.l   #1,d0
        asr.l   #1,d4

        ; Imag: (ui + vi) >> 1, (ui - vi) >> 1
        move.l  d1,d5
        add.l   d3,d1
        sub.l   d3,d5
        asr.l   #1,d1
        asr.l   #1,d5

        move.w  d0,(a0)
        move.w  d1,2(a0)
        move.w  d4,(a1)
        move.w  d5,2(a1)

        add.l   a6,a0
        add.l   a6,a1
        subq.l  #1,d6
        bne.s   .fix_j0_loop
        bra.s   .fix_next_j

.fix_gen_loop:
        move.w  (a1),d2                 ; vr
        ext.l   d2
        move.w  2(a1),d3                ; vi
        ext.l   d3

        ; tr = (wr * vr - wi * vi) >> 15
        move.l  24(sp),d0               ; wr
        muls.w  d2,d0                   ; wr * vr
        move.l  28(sp),d1               ; wi
        muls.w  d3,d1                   ; wi * vi
        sub.l   d1,d0
        asr.l   #8,d0
        asr.l   #7,d0                   ; d0 = tr

        ; ti = (wr * vi + wi * vr) >> 15
        move.l  24(sp),d1               ; wr
        muls.w  d3,d1                   ; wr * vi
        move.l  28(sp),d3               ; wi
        muls.w  d2,d3                   ; wi * vr
        add.l   d3,d1
        asr.l   #8,d1
        asr.l   #7,d1                   ; d1 = ti

        ; Butterfly additions with (>> 1) scaling:
        move.w  (a0),d2                 ; ur
        ext.l   d2
        move.w  2(a0),d3                ; ui
        ext.l   d3

        move.l  d2,d4                   ; ur
        add.l   d0,d2                   ; ur + tr
        sub.l   d0,d4                   ; ur - tr
        asr.l   #1,d2
        asr.l   #1,d4

        move.l  d3,d5                   ; ui
        add.l   d1,d3                   ; ui + ti
        sub.l   d1,d5                   ; ui - ti
        asr.l   #1,d3
        asr.l   #1,d5

        move.w  d2,(a0)
        move.w  d3,2(a0)
        move.w  d4,(a1)
        move.w  d5,2(a1)

        add.l   a6,a0
        add.l   a6,a1
        subq.l  #1,d6
        bne.s   .fix_gen_loop

.fix_next_j:
        add.l   16(sp),a2               ; tw_ptr += tw_step
        addq.l  #4,a4                   ; u_base += 4 bytes (1 complex_int16)
        addq.l  #4,a5                   ; v_base += 4 bytes (1 complex_int16)
        subq.l  #1,d7
        bne     .fix_j_loop

        ; Advance stage:
        move.l  8(sp),d0
        lsl.l   #1,d0
        move.l  d0,8(sp)                ; len <<= 1

        move.l  12(sp),d0
        lsl.l   #1,d0
        move.l  d0,12(sp)               ; half <<= 1

        move.l  16(sp),d0
        lsr.l   #1,d0
        move.l  d0,16(sp)               ; tw_step >>= 1

        move.l  20(sp),d0
        lsr.l   #1,d0
        move.l  d0,20(sp)               ; num_blocks >>= 1

        add.l   a6,a6                   ; step = step * 2
        bra     .fix_stage_loop

.fix_done:
        add.l   #32,sp
        movem.l (sp)+,d2-d7/a2-a6
        rts


; ======================================================================
; void fft_fixed_ammx(complex_int16 *x, int n, const complex_int16 *tw)
; 16-bit Q1.15 Fixed-Point AMMX SIMD Implementation
; - Stage 1: Vectorized with bflyw (2 butterflies / iter, 0 twiddles)
; - Stages 2..log2(N)-1: Register twiddle generation + interleaved bflyw
; - Stage log2(N): Specialized continuous streaming with (An)+ post-increment
; ======================================================================
_fft_fixed_ammx:
        move.l  4(sp),a0                ; a0 = x
        move.l  8(sp),d0                ; d0 = n
        move.l  12(sp),a1               ; a1 = tw

        movem.l d2-d7/a2-a6,-(sp)
        sub.l   #16,sp                  ; local storage:
                                        ;  0(sp)..7(sp) = scratch buffer for twiddles
                                        ;  8(sp) = x
                                        ; 12(sp) = n

        move.l  a0,8(sp)                ; store x
        move.l  d0,12(sp)               ; store n
        move.l  a1,a3                   ; a3 = tw_base

        ; 1. Fast Bit-Reversal
        move.l  d0,-(sp)
        move.l  a0,-(sp)
        jsr     _bit_reverse_fixed_asm
        addq.l  #8,sp

        ; 2. Preload AMMX permanent constants
        load.w  #$8000,e10              ; e10 = $8000 for >> 1 scaling
        peor    e11,e11,e11             ; e11 = 0

        ; 3. Stage 1: len = 2, half = 1 (Vectorized with AMMX, 2 butterflies per iter)
        move.l  12(sp),d0               ; n
        lsr.l   #2,d0                   ; num_quads = n / 4
        move.l  8(sp),a0                ; a0 = &x[0]

.ammx_stg1_vec:
        load    (a0)+,e0                ; [u0, v0]
        load    (a0)+,e1                ; [u1, v1]

        vperm   #$012389ab,e0,e1,e2     ; e2 = [u0, u1]
        vperm   #$4567cdef,e0,e1,e3     ; e3 = [v0, v1]

        pmulh   e10,e2,e4
        psubw   e4,e11,e2               ; e2 = u_half

        pmulh   e10,e3,e5
        psubw   e5,e11,e3               ; e3 = v_half

        bflyw   e3,e2,e12:e13           ; e12 = u', e13 = v'

        vperm   #$012389ab,e12,e13,e0   ; e0 = [u0', v0']
        vperm   #$4567cdef,e12,e13,e1   ; e1 = [u1', v1']

        store   e0,-16(a0)
        store   e1,-8(a0)

        subq.l  #1,d0
        bne.s   .ammx_stg1_vec

        ; 4. Stages 2..log2(N): len = 4, 8, ..., n
        moveq   #4,d2                   ; d2 = len = 4
        moveq   #2,d3                   ; d3 = half = 2
        move.l  #4096,d4                ; d4 = tw_offset1 = tw_stride * 4 (1024 * 4 = 4096)
        move.l  #8192,d5                ; d5 = tw_step = tw_stride * 8 = 8192
        move.l  12(sp),d6               ; n
        lsr.l   #2,d6                   ; d6 = num_blocks = n / 4
        moveq   #16,d0                  ; step = len * 4 = 16 bytes
        move.l  d0,a6                   ; a6 = step

.ammx_stage_loop:
        cmp.l   12(sp),d2               ; len > n ?
        bhi     .ammx_done

        ; Check if this is the final stage (num_blocks == 1)
        cmp.l   #1,d6
        beq     .ammx_last_stage

        ; Start j-loop pointers
        move.l  a3,a2                   ; a2 = tw_ptr (starts at &tw[0])
        move.l  8(sp),a4                ; a4 = u_base = x
        move.l  d3,d0                   ; half
        lsl.l   #2,d0                   ; half * 4 bytes
        lea     (a4,d0.l),a5            ; a5 = v_base = &x[half]

        move.l  d3,d7
        lsr.l   #1,d7                   ; d7 = j_count = half / 2

.ammx_j_loop:
        ; Load W0 and W1 safely and synthesize e8, e9 in AMMX
        move.l  (a2),d0                 ; d0 = W0 = [wr0, wi0]
        move.l  (a2,d4.l),d1            ; d1 = W1 = [wr1, wi1]
        move.l  d0,(sp)
        move.l  d1,4(sp)
        load    (sp),e8                 ; e8 = [wr0, wi0, wr1, wi1]

        ; Derive e9 = [-wi0, wr0, -wi1, wr1] in 3 AMMX ops
        vperm   #$23016745,e8,e8,e9     ; [wi0, wr0, wi1, wr1]
        psubw   e9,e11,e2               ; [-wi0, -wr0, -wi1, -wr1]
        vperm   #$01ab45ef,e2,e9,e9     ; [-wi0, wr0, -wi1, wr1]

        ; Inner block loop
        move.l  a4,a0                   ; a0 = u_ptr
        move.l  a5,a1                   ; a1 = v_ptr
        move.l  d6,d0                   ; d0 = num_blocks

.ammx_blk_loop:
        load    (a0),e6                 ; load u = [u0, u1]
        load    (a1),e0                 ; load v = [v0, v1]

        pmulh   e10,e6,e7               ; start u >> 1
        vperm   #$01014545,e0,e0,e1     ; e1 = V1 = [v0.r, v0.r, v1.r, v1.r]
        vperm   #$23236767,e0,e0,e2     ; e2 = V2 = [v0.i, v0.i, v1.i, v1.i]

        pmulh   e8,e1,e3                ; e3 = W1 * V1
        psubw   e7,e11,e6               ; e6 = u_half (e7 ready!)
        pmulh   e9,e2,e4                ; e4 = W2 * V2

        paddw   e4,e3,e5                ; e5 = t_half
        bflyw   e5,e6,e12:e13           ; e12 = u', e13 = v'

        store   e12,(a0)
        store   e13,(a1)

        add.l   a6,a0                   ; u_ptr += step
        add.l   a6,a1                   ; v_ptr += step
        subq.l  #1,d0
        bne.s   .ammx_blk_loop

        ; Advance j-loop pointers
        add.l   d5,a2                   ; tw_ptr += tw_step
        addq.l  #8,a4                   ; u_base += 8 bytes
        addq.l  #8,a5                   ; v_base += 8 bytes
        subq.l  #1,d7
        bne     .ammx_j_loop

        ; Advance stage:
        lsl.l   #1,d2                   ; len <<= 1
        lsl.l   #1,d3                   ; half <<= 1
        lsr.l   #1,d4                   ; tw_offset1 >>= 1
        lsr.l   #1,d5                   ; tw_step >>= 1
        lsr.l   #1,d6                   ; num_blocks >>= 1
        add.l   a6,a6                   ; step = step * 2
        bra     .ammx_stage_loop

; ======================================================================
; Final Stage Specialization (num_blocks == 1):
; Data u and v are 100% contiguous memory!
; We stream directly with post-increment (a4)+ and (a5)+ !
; ======================================================================
.ammx_last_stage:
        move.l  a3,a2                   ; a2 = tw_ptr
        move.l  8(sp),a4                ; a4 = u_ptr = &x[0]
        move.l  d3,d0                   ; half
        lsl.l   #2,d0                   ; half * 4 bytes
        lea     (a4,d0.l),a5            ; a5 = v_ptr = &x[half]

        move.l  d3,d7
        lsr.l   #1,d7                   ; d7 = half / 2 iterations

.ammx_last_loop:
        ; Load W0 and W1 safely
        move.l  (a2),d0                 ; W0
        move.l  (a2,d4.l),d1            ; W1
        add.l   d5,a2                   ; tw_ptr += tw_step
        move.l  d0,(sp)
        move.l  d1,4(sp)
        load    (sp),e8                 ; e8 = [wr0, wi0, wr1, wi1]

        vperm   #$23016745,e8,e8,e9     ; [wi0, wr0, wi1, wr1]
        psubw   e9,e11,e2               ; [-wi0, -wr0, -wi1, -wr1]
        vperm   #$01ab45ef,e2,e9,e9     ; [-wi0, wr0, -wi1, wr1]

        load    (a4),e6                 ; load u
        load    (a5),e0                 ; load v

        pmulh   e10,e6,e7
        vperm   #$01014545,e0,e0,e1
        vperm   #$23236767,e0,e0,e2

        pmulh   e8,e1,e3
        psubw   e7,e11,e6
        pmulh   e9,e2,e4

        paddw   e4,e3,e5
        bflyw   e5,e6,e12:e13

        store   e12,(a4)+               ; stream store u' and advance!
        store   e13,(a5)+               ; stream store v' and advance!

        subq.l  #1,d7
        bne.s   .ammx_last_loop

.ammx_done:
        add.l   #16,sp
        movem.l (sp)+,d2-d7/a2-a6
        rts

; ======================================================================
; void fft_breakdown_ammx(complex_int16 *x, int n, const complex_int16 *tw,
;                         uint32_t *t_br, uint32_t *t_stg1, uint32_t *t_mid, uint32_t *t_last)
; ======================================================================
_fft_breakdown_ammx:
        move.l  4(sp),a0                ; x
        move.l  8(sp),d0                ; n
        move.l  12(sp),a1               ; tw
        move.l  16(sp),d1               ; t_br
        move.l  20(sp),d2               ; t_stg1
        move.l  24(sp),d3               ; t_mid
        move.l  28(sp),d4               ; t_last

        movem.l d2-d7/a2-a6,-(sp)
        sub.l   #40,sp                  ; storage

        move.l  a0,8(sp)                ; x
        move.l  d0,12(sp)               ; n
        move.l  a1,a3                   ; tw
        move.l  d1,16(sp)               ; ptr t_br
        move.l  d2,20(sp)               ; ptr t_stg1
        move.l  d3,24(sp)               ; ptr t_mid
        move.l  d4,28(sp)               ; ptr t_last

        ; 1. Bit-reversal timing
        movec.l CCC,d2                  ; start br
        move.l  12(sp),-(sp)
        move.l  12(sp),-(sp)
        jsr     _bit_reverse_fixed_asm
        addq.l  #8,sp
        movec.l CCC,d3                  ; end br
        sub.l   d2,d3
        move.l  16(sp),a0
        move.l  d3,(a0)                 ; store t_br

        ; AMMX constants
        load.w  #$8000,e10
        peor    e11,e11,e11

        ; 2. Stage 1 timing
        movec.l CCC,d2                  ; start stg1
        move.l  12(sp),d0
        lsr.l   #2,d0
        move.l  8(sp),a0

.bd_stg1:
        load    (a0)+,e0
        load    (a0)+,e1
        vperm   #$012389ab,e0,e1,e2
        vperm   #$4567cdef,e0,e1,e3
        pmulh   e10,e2,e4
        psubw   e4,e11,e2
        pmulh   e10,e3,e5
        psubw   e5,e11,e3
        bflyw   e3,e2,e12:e13
        vperm   #$012389ab,e12,e13,e0
        vperm   #$4567cdef,e12,e13,e1
        store   e0,-16(a0)
        store   e1,-8(a0)
        subq.l  #1,d0
        bne.s   .bd_stg1

        movec.l CCC,d3                  ; end stg1
        sub.l   d2,d3
        move.l  20(sp),a0
        move.l  d3,(a0)                 ; store t_stg1

        ; 3. Stages 2..log2(N)-1 timing
        movec.l CCC,d0
        move.l  d0,32(sp)               ; start_mid

        moveq   #4,d2                   ; len = 4
        moveq   #2,d3                   ; half = 2
        move.l  #4096,d4                ; tw_offset1
        move.l  #8192,d5                ; tw_step
        move.l  12(sp),d6               ; n
        lsr.l   #2,d6                   ; num_blocks
        moveq   #16,d0
        move.l  d0,a6

.bd_stage_loop:
        cmp.l   #1,d6
        beq     .bd_last_stage

        move.l  a3,a2
        move.l  8(sp),a4
        move.l  d3,d0
        lsl.l   #2,d0
        lea     (a4,d0.l),a5
        move.l  d3,d7
        lsr.l   #1,d7

.bd_j_loop:
        move.l  (a2),d0
        move.l  (a2,d4.l),d1
        move.l  d0,(sp)
        move.l  d1,4(sp)
        load    (sp),e8
        vperm   #$23016745,e8,e8,e9
        psubw   e9,e11,e2
        vperm   #$01ab45ef,e2,e9,e9

        move.l  a4,a0
        move.l  a5,a1
        move.l  d6,d0

.bd_blk_loop:
        load    (a0),e6
        load    (a1),e0
        pmulh   e10,e6,e7
        vperm   #$01014545,e0,e0,e1
        vperm   #$23236767,e0,e0,e2
        pmulh   e8,e1,e3
        psubw   e7,e11,e6
        pmulh   e9,e2,e4
        paddw   e4,e3,e5
        bflyw   e5,e6,e12:e13
        store   e12,(a0)
        store   e13,(a1)
        add.l   a6,a0
        add.l   a6,a1
        subq.l  #1,d0
        bne.s   .bd_blk_loop

        add.l   d5,a2
        addq.l  #8,a4
        addq.l  #8,a5
        subq.l  #1,d7
        bne     .bd_j_loop

        lsl.l   #1,d2
        lsl.l   #1,d3
        lsr.l   #1,d4
        lsr.l   #1,d5
        lsr.l   #1,d6
        add.l   a6,a6
        bra     .bd_stage_loop

.bd_last_stage:
        movec.l CCC,d0                  ; end mid
        sub.l   32(sp),d0
        move.l  24(sp),a0
        move.l  d0,(a0)                 ; store t_mid

        ; 4. Final stage timing
        movec.l CCC,d2                  ; start last

        move.l  a3,a2
        move.l  8(sp),a4
        move.l  d3,d0
        lsl.l   #2,d0
        lea     (a4,d0.l),a5
        move.l  d3,d7
        lsr.l   #1,d7

.bd_last_loop:
        move.l  (a2),d0
        move.l  (a2,d4.l),d1
        add.l   d5,a2
        move.l  d0,(sp)
        move.l  d1,4(sp)
        load    (sp),e8
        vperm   #$23016745,e8,e8,e9
        psubw   e9,e11,e2
        vperm   #$01ab45ef,e2,e9,e9

        load    (a4),e6
        load    (a5),e0
        pmulh   e10,e6,e7
        vperm   #$01014545,e0,e0,e1
        vperm   #$23236767,e0,e0,e2
        pmulh   e8,e1,e3
        psubw   e7,e11,e6
        pmulh   e9,e2,e4
        paddw   e4,e3,e5
        bflyw   e5,e6,e12:e13
        store   e12,(a4)+
        store   e13,(a5)+
        subq.l  #1,d7
        bne.s   .bd_last_loop

        movec.l CCC,d3                  ; end last
        sub.l   d2,d3
        move.l  28(sp),a0
        move.l  d3,(a0)                 ; store t_last

        add.l   #40,sp
        movem.l (sp)+,d2-d7/a2-a6
        rts
