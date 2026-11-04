; seq/BLT-012.i - Hurrican Additive Shading / Water Blit (Explicit Loads)
; Uses: e0, e1, e2, a0, a1, a2
; MUST NOT touch: d5, d6, d7, a5, a6, a7
; Evaluates 4 RGB565 pixels (1 quadword) with saturation clamp
        load    (a0)+,e0
        load    (a1)+,e1
        paddusw e0,e1,e2
        store   e2,(a2)+
