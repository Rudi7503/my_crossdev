; seq/BLT-003.i - premultiplied-alpha sprite over background with PMULA
; e0 = premultiplied sprite (alpha, R, G, B | alpha, R, G, B)
; e1 = background RGB
; Uses: e0-e2, a0, a1, a2
; MUST NOT touch: d5, d6, d7, a5, a6, a7
        load    (a0)+,e0
        load    (a2)+,e1
        pmula   e0,e1,e2
        store   e2,(a1)+
