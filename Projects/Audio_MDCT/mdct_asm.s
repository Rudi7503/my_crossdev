; ======================================================================
; Apollo 68080 Audio MDCT / IMDCT Optimizations
; MP3 36-Point Windowed IMDCT & Overlap-Add Engine
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
        xdef    _imdct36_float_asm
        xdef    _imdct36_fixed_ammx
        xdef    _overlap_add_fixed_ammx
        xdef    _imdct36_windowed_overlap_add_ammx

        xref    _imdct_cos_float
        xref    _imdct_cos_q15_padded
        xref    _imdct_win_cos_q15_padded

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
; void imdct36_float_asm(const float *in18, float *out36)
; 32-bit Float IMDCT-36 using 68080 FPU
; ======================================================================
_imdct36_float_asm:
        movem.l a2-a3,-(sp)

        move.l  12(sp),a0               ; in18
        move.l  16(sp),a1               ; out36
        lea     _imdct_cos_float,a2     ; matrix[36][18]

        moveq   #36-1,d0                ; 36 rows
.f_row_loop:
        fmove.s #0.0,fp0                ; accumulator
        move.l  a0,a3                   ; in18 ptr
        moveq   #18-1,d1                ; 18 columns
.f_col_loop:
        fmove.s (a3)+,fp1
        fmul.s  (a2)+,fp1
        fadd.x  fp1,fp0
        dbra    d1,.f_col_loop

        fmove.s fp0,(a1)+
        dbra    d0,.f_row_loop

        movem.l (sp)+,a2-a3
        rts

; ======================================================================
; void imdct36_fixed_ammx(const int16_t *in18, int16_t *out36)
; 16-bit Q1.15 Fixed-Point IMDCT-36 via AMMX SIMD
; Inputs:
;   in18:  18 frequency subband coefficients
;   out36: 36 time-domain samples
; ======================================================================
_imdct36_fixed_ammx:
        movem.l d2/a2-a3,-(sp)
        move.l  16(sp),a0               ; in18
        move.l  20(sp),a1               ; out36
        lea     _imdct_cos_q15_padded,a2 ; padded to 20 words (40 bytes) per row

        ; Preload in18 into registers e16..e20 (ONCE for all 36 rows!)
        load    (a0),e16                ; in[0..3]
        load    8(a0),e17               ; in[4..7]
        load    16(a0),e18              ; in[8..11]
        load    24(a0),e19              ; in[12..15]
        move.l  32(a0),d0               ; in[16..17]
        subq.l  #8,sp
        move.l  d0,(sp)
        clr.l   4(sp)
        load    (sp),e20                ; in[16..17, 0, 0]
        addq.l  #8,sp

        moveq   #18-1,d2                ; 18 row pairs = 36 rows
.fix_row_loop:
        ; Row 2j:
        load    (a2),e0
        load    8(a2),e2
        load    16(a2),e4
        load    24(a2),e6
        load    32(a2),e8

        pmulh   e16,e0,e1
        pmulh   e17,e2,e3
        pmulh   e18,e4,e5
        pmulh   e19,e6,e7
        pmulh   e20,e8,e9

        paddw   e3,e1,e1
        paddw   e7,e5,e5
        paddw   e9,e1,e1
        paddw   e5,e1,e1                ; e1 = row 2j [l0, l1, l2, l3]

        ; Row 2j+1:
        load    40(a2),e0
        load    48(a2),e2
        load    56(a2),e4
        load    64(a2),e6
        load    72(a2),e8

        pmulh   e16,e0,e10
        pmulh   e17,e2,e3
        pmulh   e18,e4,e5
        pmulh   e19,e6,e7
        pmulh   e20,e8,e9

        paddw   e3,e10,e10
        paddw   e7,e5,e5
        paddw   e9,e10,e10
        paddw   e5,e10,e10              ; e10 = row 2j+1 [l0, l1, l2, l3]

        ; Combined horizontal reduction of e1 (row 2j) and e10 (row 2j+1)
        vperm   #$018945cd,e1,e10,e11   ; [a0, b0, a2, b2]
        vperm   #$23ab67ef,e1,e10,e12   ; [a1, b1, a3, b3]
        paddw   e12,e11,e11             ; [a0+a1, b0+b1, a2+a3, b2+b3]
        vperm   #$45670123,e11,e11,e13  ; [a2+a3, b2+b3, a0+a1, b0+b1]
        paddw   e13,e11,e11             ; [sum_a, sum_b, sum_a, sum_b]

        paddw   e11,e11,e11             ; * 2 for Q1.15
        paddw   e11,e11,e11             ; * 2 for 0.5x scaling restoration

        move.l  e11,d0                  ; d0 = [sum_a, sum_b]
        move.l  d0,(a1)+                ; store 2 samples in 1 write!

        lea     80(a2),a2               ; 2 rows = 80 bytes
        dbra    d2,.fix_row_loop

        movem.l (sp)+,d2/a2-a3
        rts

; ======================================================================
; void overlap_add_fixed_ammx(const int16_t *curr18, int16_t *prev18, int16_t *out18)
; Fast 18-sample overlap-add
; ======================================================================
_overlap_add_fixed_ammx:
        move.l  a2,-(sp)
        move.l  8(sp),a0                ; curr18
        move.l  12(sp),a1               ; prev18
        move.l  16(sp),a2               ; out18

        ; Words 0..3
        load    (a0),e0
        load    (a1),e1
        paddw   e1,e0,e2
        store   e2,(a2)

        ; Words 4..7
        load    8(a0),e0
        load    8(a1),e1
        paddw   e1,e0,e2
        store   e2,8(a2)

        ; Words 8..11
        load    16(a0),e0
        load    16(a1),e1
        paddw   e1,e0,e2
        store   e2,16(a2)

        ; Words 12..15
        load    24(a0),e0
        load    24(a1),e1
        paddw   e1,e0,e2
        store   e2,24(a2)

        ; Words 16..17
        move.w  32(a0),d0
        add.w   32(a1),d0
        move.w  d0,32(a2)

        move.w  34(a0),d0
        add.w   34(a1),d0
        move.w  d0,34(a2)

        move.l  (sp)+,a2
        rts

; ======================================================================
; void imdct36_windowed_overlap_add_ammx(const int16_t *in18, int16_t *prev18, int16_t *out18)
; Fully Fused MP3 Subband Engine:
;   - Preloads in18 into e16..e20
;   - Computes 36 windowed IMDCT samples directly using fused table
;   - First 18 samples: adds prev18 -> writes directly to out18
;   - Second 18 samples: writes directly to prev18 (ready for next block)
; Zero fast-RAM intermediate buffer!
; ======================================================================
_imdct36_windowed_overlap_add_ammx:
        movem.l d2-d3/a2-a4,-(sp)
        move.l  24(sp),a0               ; in18
        move.l  28(sp),a3               ; prev18
        move.l  32(sp),a4               ; out18
        lea     _imdct_win_cos_q15_padded,a2

        ; Preload in18 into e16..e20
        load    (a0),e16
        load    8(a0),e17
        load    16(a0),e18
        load    24(a0),e19
        move.l  32(a0),d0
        subq.l  #8,sp
        move.l  d0,(sp)
        clr.l   4(sp)
        load    (sp),e20
        addq.l  #8,sp

        ; --- Pass 1: 9 row pairs (18 samples) -> y_win[0..17] + prev[0..17] -> out18 ---
        moveq   #9-1,d2
.fused_pass1:
        load    (a2),e0
        load    8(a2),e2
        load    16(a2),e4
        load    24(a2),e6
        load    32(a2),e8

        pmulh   e16,e0,e1
        pmulh   e17,e2,e3
        pmulh   e18,e4,e5
        pmulh   e19,e6,e7
        pmulh   e20,e8,e9

        paddw   e3,e1,e1
        paddw   e7,e5,e5
        paddw   e9,e1,e1
        paddw   e5,e1,e1

        load    40(a2),e0
        load    48(a2),e2
        load    56(a2),e4
        load    64(a2),e6
        load    72(a2),e8

        pmulh   e16,e0,e10
        pmulh   e17,e2,e3
        pmulh   e18,e4,e5
        pmulh   e19,e6,e7
        pmulh   e20,e8,e9

        paddw   e3,e10,e10
        paddw   e7,e5,e5
        paddw   e9,e10,e10
        paddw   e5,e10,e10

        vperm   #$018945cd,e1,e10,e11
        vperm   #$23ab67ef,e1,e10,e12
        paddw   e12,e11,e11
        vperm   #$45670123,e11,e11,e13
        paddw   e13,e11,e11

        paddw   e11,e11,e11             ; * 2
        paddw   e11,e11,e11             ; * 2

        ; Load 2 samples from prev18, add, write to out18
        move.l  e11,d0                  ; d0 = [sum_a, sum_b]
        move.l  (a3)+,d1                ; d1 = [prev_a, prev_b]
        move.w  d0,d3
        add.w   d1,d3                   ; d3.w = sum_b + prev_b
        swap    d0
        swap    d1
        add.w   d1,d0                   ; d0.w = sum_a + prev_a
        swap    d0
        move.w  d3,d0                   ; d0 = [sum_a+prev_a, sum_b+prev_b]
        move.l  d0,(a4)+                ; write to out18!

        lea     80(a2),a2
        dbra    d2,.fused_pass1

        ; Reset a3 to start of prev18 for storing the second half
        move.l  28(sp),a3

        ; --- Pass 2: 9 row pairs (18 samples) -> y_win[18..35] -> prev18 ---
        moveq   #9-1,d2
.fused_pass2:
        load    (a2),e0
        load    8(a2),e2
        load    16(a2),e4
        load    24(a2),e6
        load    32(a2),e8

        pmulh   e16,e0,e1
        pmulh   e17,e2,e3
        pmulh   e18,e4,e5
        pmulh   e19,e6,e7
        pmulh   e20,e8,e9

        paddw   e3,e1,e1
        paddw   e7,e5,e5
        paddw   e9,e1,e1
        paddw   e5,e1,e1

        load    40(a2),e0
        load    48(a2),e2
        load    56(a2),e4
        load    64(a2),e6
        load    72(a2),e8

        pmulh   e16,e0,e10
        pmulh   e17,e2,e3
        pmulh   e18,e4,e5
        pmulh   e19,e6,e7
        pmulh   e20,e8,e9

        paddw   e3,e10,e10
        paddw   e7,e5,e5
        paddw   e9,e10,e10
        paddw   e5,e10,e10

        vperm   #$018945cd,e1,e10,e11
        vperm   #$23ab67ef,e1,e10,e12
        paddw   e12,e11,e11
        vperm   #$45670123,e11,e11,e13
        paddw   e13,e11,e11

        paddw   e11,e11,e11             ; * 2
        paddw   e11,e11,e11             ; * 2

        move.l  e11,d0
        move.l  d0,(a3)+

        lea     80(a2),a2
        dbra    d2,.fused_pass2

        movem.l (sp)+,d2-d3/a2-a4
        rts
