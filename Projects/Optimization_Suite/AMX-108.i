; seq/AMX-108.i - 2x unrolled PCMPGTW + BSEL with Early Store Overlap (8 words)
; Uses: e0, e1, e2, e3, e4, e5, e6, e7, a0, a1, a2
; MUST NOT touch: d5, d6, d7, a5, a6, a7
        load    (a0)+,e0
        load    (a1)+,e1
        load    (a0)+,e4
        load    (a1)+,e5
        pand    e0,e0,e3
        pand    e4,e4,e7
        pcmpgtw e0,e1,e2
        pcmpgtw e4,e5,e6
        bsel    e1,e2,e3
        store   e3,(a2)+
        bsel    e5,e6,e7
        store   e7,(a2)+
