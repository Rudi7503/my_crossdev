; seq/FFT-002.i - 16-bit Q1.15 Fixed-Point Scalar FFT Butterfly (Radix-2)
; Inputs:  a0 = u (complex_int16 = 4 bytes), a1 = v, a2 = twiddle w
; Outputs: a3 = u', a4 = v'
; Uses:    d0, d1, d2, d3, d4, a0, a1, a2, a3, a4
; MUST NOT touch: d5, d6, d7, a5, a6, a7

        ; Load w: d2 = wr, d3 = wi
        move.w  (a2),d2
        ext.l   d2
        move.w  2(a2),d3
        ext.l   d3

        ; Load v: d1 = vr, d4 = vi
        move.w  (a1),d1                 ; vr
        move.w  2(a1),d4                ; vi

        ; wr * vr
        move.l  d2,d0
        muls.w  d1,d0                   ; d0 = wr * vr
        ; wi * vi
        move.l  d3,d1
        muls.w  d4,d1                   ; d1 = wi * vi
        sub.l   d1,d0                   ; wr*vr - wi*vi
        asr.l   #8,d0
        asr.l   #7,d0                   ; d0 = tr

        ; wr * vi
        move.l  d2,d1
        muls.w  d4,d1                   ; d1 = wr * vi
        ; wi * vr
        move.w  (a1),d4                 ; vr
        muls.w  d3,d4                   ; d4 = wi * vr
        add.l   d4,d1                   ; wr*vi + wi*vr
        asr.l   #8,d1
        asr.l   #7,d1                   ; d1 = ti

        ; Load u
        move.w  (a0),d2                 ; ur
        ext.l   d2
        move.w  2(a0),d3                ; ui
        ext.l   d3

        ; ur + tr, ur - tr, >> 1
        move.l  d2,d4
        add.l   d0,d2
        sub.l   d0,d4
        asr.l   #1,d2                   ; u'_r
        asr.l   #1,d4                   ; v'_r

        ; ui + ti, ui - ti, >> 1
        move.l  d3,d0
        add.l   d1,d3
        sub.l   d1,d0
        asr.l   #1,d3                   ; u'_i
        asr.l   #1,d0                   ; v'_i

        ; Store outputs
        move.w  d2,(a3)
        move.w  d3,2(a3)
        move.w  d4,(a4)
        move.w  d0,2(a4)
