; seq/BLT-014.i - Hurrican Additive Shading / Water Blit (2x Unrolled Memory-VEA)
; Uses: e1, e2, e3, e4, a0, a1, a2
; MUST NOT touch: d5, d6, d7, a5, a6, a7
; Evaluates 8 RGB565 pixels across 2 quadwords using direct memory VEA
        load    (a1)+,e1
        load    (a1)+,e3
        paddusw (a0)+,e1,e2
        paddusw (a0)+,e3,e4
        store   e2,(a2)+
        store   e4,(a2)+
