; seq/SRT-002.i - AMMX Branchless Binary Search Step (PCMPGTW + BSEL)
; Uses: e0, e1, e2, e3, e4, a0, a1
; MUST NOT touch: d5, d6, d7, a5, a6, a7
; e0 = target, e1 = midpoint value, e2 = left bound, e3 = right bound
        load    (a0)+,e1
        pcmpgtw e0,e1,e4
        bsel    e2,e4,e3
        store   e3,(a1)+
