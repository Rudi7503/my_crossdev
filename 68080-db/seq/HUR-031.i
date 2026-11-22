; seq/HUR-031.i - Hurrican .f6p AMMX 8-pixel SIMD 3/4*dst + src (bit-exact)
; Uses: d0, a0, a1, e0..e3, e7(4x$39E7)
; MUST NOT touch: d5, d6, d7, a5, a6, a7
        load    (a1),e0
        load    8(a1),e1
        lsrq    d0,e0,e2
        lsrq    d0,e1,e3
        pand    e7,e2,e2
        pand    e7,e3,e3
        psubw   e2,e0,e0
        psubw   e3,e1,e1
        paddw   (a0)+,e0,e0
        paddw   (a0)+,e1,e1
        store   e0,(a1)+
        store   e1,(a1)+
