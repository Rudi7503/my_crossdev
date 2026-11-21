; seq/BLT-040.i - 16-Bit RGB565 Alpha-Blending 2x Unrolled (8 pixels, unpack1632 + PMULA + pack3216)
; Uses: e0..e11, a0, a1
; MUST NOT touch: d5, d6, d7, a5, a6, a7
        load    (a0)+,e0
        load    (a0)+,e1
        load    (a0)+,e6
        load    (a0)+,e7
        load    (a1),e2
        load    8(a1),e3
        unpack1632 e2,e4:e5
        unpack1632 e3,e8:e9
        pmula   e0,e4,e4
        pmula   e1,e5,e5
        pmula   e6,e8,e8
        pmula   e7,e9,e9
        pack3216 e4,e5,e2
        pack3216 e8,e9,e3
        store   e2,(a1)+
        store   e3,(a1)+
