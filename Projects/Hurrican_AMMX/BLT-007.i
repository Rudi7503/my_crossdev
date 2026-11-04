; seq/BLT-007.i - Hurrican Flashlight Fast Darkening 3/8 with LSRQ + PAND + PADDW
; Uses: e2, e3, e4, e5, d0, d1, a0, a1
; MUST NOT touch: d5, d6, d7, a5, a6, a7
; d0=2, d1=3, e2=4x$39E7, e3=4x$18E3, evaluates (x>>2 & $39E7) + (x>>3 & $18E3)
        load    (a0)+,e4
        lsrq    d0,e4,e5
        pand    e2,e5,e5
        lsrq    d1,e4,e4
        pand    e3,e4,e4
        paddw   e5,e4,e4
        store   e4,(a1)+
