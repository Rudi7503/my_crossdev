; seq/BLT-011.i - Hurrican MSCALE RGB565 8-Pixel Scale (2x Unrolled Interleaved Dual Stream)
; Uses: e0, e1, e3, e4, e5, e6, e7, e8, e9, e10, e11, e12, e13, e14, e15, a0, a1
; MUST NOT touch: d5, d6, d7, a5, a6, a7
; Splits 8 pixels into 3 channels across 2 quadwords, scales with pmul88 by e9, recomposes with por
        load    (a0)+,e0
        load    (a0)+,e1
        pand    e10,e0,e3
        pand    e10,e1,e6
        pand    e13,e0,e4
        pand    e13,e1,e7
        pand    e14,e0,e5
        pand    e14,e1,e15
        pmul88  e11,e3,e3
        pmul88  e11,e6,e6
        pand    e12,e3,e3
        pand    e12,e6,e6
        pmul88  e9,e3,e3
        pmul88  e9,e6,e6
        pmul88  e9,e4,e4
        pmul88  e9,e7,e7
        pmul88  e9,e5,e5
        pmul88  e9,e15,e15
        pmull   e8,e3,e3
        pmull   e8,e6,e6
        pand    e10,e3,e3
        pand    e10,e6,e6
        pand    e13,e4,e4
        pand    e13,e7,e7
        pand    e14,e5,e5
        pand    e14,e15,e15
        por     e4,e3,e3
        por     e7,e6,e6
        por     e5,e3,e0
        por     e15,e6,e1
        store   e0,(a1)+
        store   e1,(a1)+
