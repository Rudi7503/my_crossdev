; seq/PAT-004.i - branchless byte blend with AMMX compare/select
; Uses: e0-e3, a0, a1, a2
; MUST NOT touch: d5, d6, d7, a5, a6, a7
        load    (a0)+,e0
        load    (a1)+,e1
        pand    e0,e0,e3
        pcmpgtb e0,e1,e2
        bsel    e1,e2,e3
        store   e3,(a2)+
