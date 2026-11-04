; seq/BLT-016.i - Hurrican Flashlight Fast Darkening 3/8 (Early Store Overlap)
; Uses: e2, e3, e4, e5, e6, e7, d0, d1, a0, a1
; MUST NOT touch: d5, d6, d7, a5, a6, a7
; d0=2, d1=3, e2=4x$39E7, e3=4x$18E3
; Evaluates 8 pixels (2 quadwords) with early store overlap
        load    (a0)+,e4
        load    (a0)+,e6
        lsrq    d0,e4,e5
        lsrq    d0,e6,e7
        pand    e2,e5,e5
        pand    e2,e7,e7
        lsrq    d1,e4,e4
        lsrq    d1,e6,e6
        pand    e3,e4,e4
        pand    e3,e6,e6
        paddw   e5,e4,e4
        store   e4,(a1)+
        paddw   e7,e6,e6
        store   e6,(a1)+
