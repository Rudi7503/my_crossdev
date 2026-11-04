; seq/BLT-013.i - Hurrican Additive Shading / Water Blit (Memory-VEA)
; Uses: e1, e2, a0, a1, a2
; MUST NOT touch: d5, d6, d7, a5, a6, a7
; Evaluates 4 RGB565 pixels with 1 explicit load saved via (a0)+
        load    (a1)+,e1
        paddusw (a0)+,e1,e2
        store   e2,(a2)+
