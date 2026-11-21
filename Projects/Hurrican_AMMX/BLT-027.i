; seq/BLT-027.i - Hurrican BlitCross (4 RGB565 pixels, 256-level crossfade A*(256-t)+B*t)
; Source: Hurrican/src/blit.s _BlitCross (.l)
; Uses: e1..e7, e8..e15(constants/t), a0, a1, a2
; MUST NOT touch: d5, d6, d7, a5, a6, a7
        load    (a0)+,e1
        load    (a1)+,e2
        pand    e10,e1,e3
        pand    e13,e1,e4
        pand    e14,e1,e5
        pmul88  e11,e3,e3
        pand    e12,e3,e3
        pmul88  e9,e3,e3
        pmul88  e9,e4,e4
        pmul88  e9,e5,e5
        pand    e10,e2,e6
        pand    e13,e2,e7
        pand    e14,e2,e1
        pmul88  e11,e6,e6
        pand    e12,e6,e6
        pmul88  e15,e6,e6
        pmul88  e15,e7,e7
        pmul88  e15,e1,e1
        paddw   e6,e3,e3
        paddw   e7,e4,e4
        paddw   e1,e5,e5
        pmull   e8,e3,e3
        pand    e10,e3,e3
        pand    e13,e4,e4
        pand    e14,e5,e5
        por     e4,e3,e3
        por     e5,e3,e3
        store   e3,(a2)+
