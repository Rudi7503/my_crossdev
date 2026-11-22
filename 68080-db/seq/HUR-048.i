; seq/HUR-048.i - Hurrican MSCALE original (4 px RGB565 * f/32, 14 ops / 5 multiplies, blit.s L4512-4527)
; Uses: e2..e14 (a0/a1 = src/dst). MUST NOT touch: d5, d6, d7, a5, a6, a7
        load    (a0)+,e2
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
        store   e2,(a1)+
