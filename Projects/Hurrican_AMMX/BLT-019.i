; seq/BLT-019.i - Premultiplied Alpha Blending 2x Unrolled with Early Store Overlap (8 pixels)
; Uses: e0, e1, e2, e3, e4, e5, a0, a1, a2
; MUST NOT touch: d5, d6, d7, a5, a6, a7
; Blends 8 pixels across 2 quadwords: dst = (src * alpha) + dst
        load    (a1)+,e1
        load    (a1)+,e4
        load    (a0)+,e0
        load    (a0)+,e3
        pmula   e0,e1,e2
        pmula   e3,e4,e5
        paddw   e0,e2,e2
        store   e2,(a2)+
        paddw   e3,e5,e5
        store   e5,(a2)+
