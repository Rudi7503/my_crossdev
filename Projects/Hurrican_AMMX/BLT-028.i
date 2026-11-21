; seq/BLT-028.i - Hurrican KeyMulA (4 RGB565 pixels, colorkey + MSCALE + storeilm)
; Source: Hurrican/src/blit.s KeyMulA (.q)
; Uses: e0(key), e2, e3, e4, e5, e6, e8..e14(MSCALE), a0, a1
; MUST NOT touch: d5, d6, d7, a5, a6, a7
        load    (a0)+,e2
        pcmpeqw e0,e2,e6
        pand    e10,e2,e3
        pand    e13,e2,e4
        pand    e14,e2,e5
        pmul88  e11,e3,e3
        pand    e12,e3,e3
        pmul88  e9,e3,e3
        pmul88  e9,e4,e4
        pmul88  e9,e5,e5
        pmull   e8,e3,e3
        pand    e10,e3,e3
        pand    e13,e4,e4
        pand    e14,e5,e5
        por     e4,e3,e3
        por     e5,e3,e2
        storeilm e2,e6,(a1)+
