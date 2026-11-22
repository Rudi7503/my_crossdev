; seq/HUR-066.i - Hurrican _BlitFallF .f5p AMMX 8-px SIMD (5/8*dst + src: e6=$7BEF, e7=$18E3, d0=1, d1=3)
; Uses: d0, d1, e0..e7, a0, a1. MUST NOT touch: d5, d6, d7, a5, a6, a7
        load    (a1),e0
        load    8(a1),e1
        lsrq    d0,e0,e2
        lsrq    d0,e1,e3
        pand    e6,e2,e2
        pand    e6,e3,e3
        lsrq    d1,e0,e4
        lsrq    d1,e1,e5
        pand    e7,e4,e4
        pand    e7,e5,e5
        paddw   e4,e2,e2
        paddw   e5,e3,e3
        paddw   (a0)+,e2,e2
        paddw   (a0)+,e3,e3
        store   e2,(a1)+
        store   e3,(a1)+
