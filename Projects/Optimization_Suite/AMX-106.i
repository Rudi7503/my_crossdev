; seq/AMX-106.i - 2x unrolled PACK3216 with Early Store Overlap (8 pixels)
; Uses: e0, e1, e2, e3, e4, e5, a0, a1
; MUST NOT touch: d5, d6, d7, a5, a6, a7
        load    (a0)+,e0
        load    (a0)+,e1
        load    (a0)+,e3
        load    (a0)+,e4
        pack3216 e0,e1,e2
        store   e2,(a1)+
        pack3216 e3,e4,e5
        store   e5,(a1)+
