; seq/BLT-048.i - 16-Bit RGB565 Alpha-Blending (PMULA) with Memory-VEA (4 pixels)
; Optimizes BLT-039 by eliminating both sprite loads via pmula (a0)+,e4,e4
; Uses: e2, e4, e5, a0, a1
; MUST NOT touch: d5, d6, d7, a5, a6, a7
        load    (a1),e2
        unpack1632 e2,e4:e5
        pmula   (a0)+,e4,e4
        pmula   (a0)+,e5,e5
        pack3216 e4,e5,e2
        store   e2,(a1)+
