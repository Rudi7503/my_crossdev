; seq/BLT-004.i - 4x unrolled PMULA with explicit register loads
; Uses: e0-e11, a0, a1, a2
; MUST NOT touch: d5, d6, d7, a5, a6, a7
        load    (a0)+,e0
        load    (a1)+,e1
        load    (a0)+,e3
        load    (a1)+,e4
        load    (a0)+,e6
        load    (a1)+,e7
        load    (a0)+,e9
        load    (a1)+,e10
        pmula   e0,e1,e2
        pmula   e3,e4,e5
        pmula   e6,e7,e8
        pmula   e9,e10,e11
        store   e2,(a2)+
        store   e5,(a2)+
        store   e8,(a2)+
        store   e11,(a2)+
