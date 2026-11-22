; seq/HUR-012.i - Hurrican _BlitAlpha32 AMMX 4-pixel SIMD blend (bit-exact, branchless colorkey)
; Uses: d0, d1, a0, a1, e0..e13
; MUST NOT touch: d5, d6, d7, a5, a6, a7
        load    (a0)+,e0
        load    (a1),e1
        pcmpeqw e7,e0,e8
        lsrq    d0,e0,e2
        pand    e9,e2,e2
        lsrq    d1,e0,e3
        pand    e10,e3,e3
        pand    e9,e0,e4
        lsrq    d0,e1,e11
        pand    e9,e11,e11
        lsrq    d1,e1,e12
        pand    e10,e12,e12
        pand    e9,e1,e13
        pmull   e14,e2,e2
        pmull   e15,e11,e11
        paddw   e11,e2,e2
        lsrq    d1,e2,e2
        pand    e9,e2,e2
        pmull   e14,e3,e3
        pmull   e15,e12,e12
        paddw   e12,e3,e3
        lsrq    d1,e3,e3
        pand    e10,e3,e3
        pmull   e14,e4,e4
        pmull   e15,e13,e13
        paddw   e13,e4,e4
        lsrq    d1,e4,e4
        pand    e9,e4,e4
        lslq    d0,e2,e2
        lslq    d1,e3,e3
        por     e3,e2,e2
        por     e4,e2,e2
        storeilm e2,e8,(a1)+
