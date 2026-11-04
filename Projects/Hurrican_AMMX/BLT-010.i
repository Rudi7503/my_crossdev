; seq/BLT-010.i - Hurrican Flashlight Fast Darkening 3/8 (4x Unrolled Quad-Stream)
; Uses: e2, e3, e4, e5, e6, e7, e8, e9, e10, e11, d0, d1, a0, a1
; MUST NOT touch: d5, d6, d7, a5, a6, a7
; d0=2, d1=3, e2=4x$39E7, e3=4x$18E3
; Evaluates 16 pixels (4 quadwords): (x>>2 & $39E7) + (x>>3 & $18E3)
        load    (a0)+,e4
        load    (a0)+,e6
        load    (a0)+,e8
        load    (a0)+,e10
        lsrq    d0,e4,e5
        lsrq    d0,e6,e7
        lsrq    d0,e8,e9
        lsrq    d0,e10,e11
        pand    e2,e5,e5
        pand    e2,e7,e7
        pand    e2,e9,e9
        pand    e2,e11,e11
        lsrq    d1,e4,e4
        lsrq    d1,e6,e6
        lsrq    d1,e8,e8
        lsrq    d1,e10,e10
        pand    e3,e4,e4
        pand    e3,e6,e6
        pand    e3,e8,e8
        pand    e3,e10,e10
        paddw   e5,e4,e4
        paddw   e7,e6,e6
        paddw   e9,e8,e8
        paddw   e11,e10,e10
        store   e4,(a1)+
        store   e6,(a1)+
        store   e8,(a1)+
        store   e10,(a1)+
