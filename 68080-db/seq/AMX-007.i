; seq/AMX-007.i - four 16-bit lane additions with PADDW
; Uses: e0, e1, e2, a0, a1, a2
; MUST NOT touch: d5, d6, d7, a5, a6, a7
        load    (a0)+,e0
        load    (a1)+,e1
        paddw   e0,e1,e2
        store   e2,(a2)+
