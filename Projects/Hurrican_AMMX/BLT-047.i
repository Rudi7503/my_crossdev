; seq/BLT-047.i - 32-Bit ARGB8888 Additive Blit 2x with Memory-VEA (4 pixels)
; Optimizes BLT-034 by eliminating both source loads via (a0)+ VEA
; Uses: e3, e11, a0, a1
; MUST NOT touch: d5, d6, d7, a5, a6, a7
        load    (a1),e3
        load    8(a1),e11
        paddusb (a0)+,e3,e3
        paddusb (a0)+,e11,e11
        store   e3,(a1)+
        store   e11,(a1)+
