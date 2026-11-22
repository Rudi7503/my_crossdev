; seq/HUR-029.i - Hurrican _BlitAlpha AMMX 4-pixel safe avg-chain (no pavgb/unpack1632, bit-exact)
; Uses: d0, a0, a1, e0..e9
; MUST NOT touch: d5, d6, d7, a5, a6, a7
        load    (a0)+,e0
        load    (a1),e1
        pcmpeqw e8,e0,e9
        peor    e0,e1,e2
        pand    e7,e2,e2
        lsrq    d0,e2,e2
        pand    e0,e1,e3
        paddw   e3,e2,e2
        peor    e2,e1,e3
        pand    e7,e3,e3
        lsrq    d0,e3,e3
        pand    e1,e2,e2
        paddw   e3,e2,e2
        peor    e2,e1,e3
        pand    e7,e3,e3
        lsrq    d0,e3,e3
        pand    e1,e2,e2
        paddw   e3,e2,e2
        storeilm e2,e9,(a1)+
