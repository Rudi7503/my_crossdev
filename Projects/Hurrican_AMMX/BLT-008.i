; seq/BLT-008.i - Hurrican MSCALE RGB565 4-Pixel Scale with PMUL88 + POR
; Uses: e0, e3, e4, e5, e8, e9, e10, e11, e12, e13, e14, a0, a1
; MUST NOT touch: d5, d6, d7, a5, a6, a7
; Splits RGB565 into 3 channels, scales with pmul88 by e9, recomposes with por
        load    (a0)+,e0
        pand    e10,e0,e3
        pand    e13,e0,e4
        pand    e14,e0,e5
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
        por     e5,e3,e0
        store   e0,(a1)+
