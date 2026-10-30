; seq/BLT-005.i - 4x unrolled PMULA with memory-VEA (one load saved)
; Uses: e1,e2,e4,e5,e7,e8,e10,e11, a0, a1, a2
; MUST NOT touch: d5, d6, d7, a5, a6, a7
        load    (a1)+,e1
        load    (a1)+,e4
        load    (a1)+,e7
        load    (a1)+,e10
        pmula   (a0)+,e1,e2
        pmula   (a0)+,e4,e5
        pmula   (a0)+,e7,e8
        pmula   (a0)+,e10,e11
        store   e2,(a2)+
        store   e5,(a2)+
        store   e8,(a2)+
        store   e11,(a2)+
