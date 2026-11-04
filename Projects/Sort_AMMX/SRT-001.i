; seq/SRT-001.i - AMMX SIMD 8-Element Sorting Network (Branchless Min/Max Shuffle)
; Uses: e0, e1, e2, e3, a0, a1
; MUST NOT touch: d5, d6, d7, a5, a6, a7
        load    (a0)+,e0
        vperm   #$10325476,e0,e0,e1
        pminub  e0,e1,e2
        pmaxub  e0,e1,e3
        vperm   #$092b4d6f,e2,e3,e0
        store   e0,(a1)+
