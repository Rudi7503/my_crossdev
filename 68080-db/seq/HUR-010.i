; seq/HUR-010.i - Hurrican _BlitKeyRed Mode 0 AMMX 4-pixel SIMD (bit-exact, branchless)
; Uses: d0, d1, d2, a0, a1, e0..e13
; MUST NOT touch: d5, d6, d7, a5, a6, a7
        load    (a0)+,e0
        pcmpeqw e7,e0,e6
        lsrq    d0,e0,e2
        pand    e9,e2,e2
        lsrq    d1,e0,e3
        pand    e10,e3,e3
        pand    e9,e0,e4
        pmull   e11,e2,e2
        pminuw  e9,e2,e2
        pmull   e12,e3,e3
        lsrq    d2,e3,e3
        pand    e13,e3,e3
        pminuw  e10,e3,e3
        pmull   e12,e4,e4
        lsrq    d2,e4,e4
        pand    e13,e4,e4
        pminuw  e9,e4,e4
        lslq    d0,e2,e2
        lslq    d1,e3,e3
        por     e3,e2,e2
        por     e4,e2,e2
        storeilm e2,e6,(a1)+
