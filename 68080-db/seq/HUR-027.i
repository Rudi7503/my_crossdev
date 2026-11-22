; seq/HUR-027.i - Hurrican _BlitKeyRed Mode 1 AMMX 4-pixel SIMD (bit-exact, branchless)
; Uses: d0, d1, a0, a1, e0..e12
; MUST NOT touch: d5, d6, d7, a5, a6, a7
        load    (a0)+,e0
        load    (a1),e1
        pcmpeqw e7,e0,e4
        lsrq    d0,e0,e2
        pand    e8,e2,e2
        pmull   e9,e2,e2
        lsrq    d1,e2,e2
        pand    e10,e2,e2
        lsrq    d0,e1,e3
        pand    e8,e3,e3
        paddw   e2,e3,e3
        pminuw  e8,e3,e3
        lslq    d0,e3,e3
        pand    e11,e1,e1
        por     e3,e1,e1
        storeilm e1,e4,(a1)+
