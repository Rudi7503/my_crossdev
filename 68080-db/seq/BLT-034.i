; seq/BLT-034.i - 32-Bit ARGB8888 Additive Blit 2x Unrolled & Interleaved (4 pixels)
; Uses: e2, e3, e10, e11, a0, a1
; MUST NOT touch: d5, d6, d7, a5, a6, a7
        load    (a0)+,e2
        load    (a0)+,e10
        load    (a1),e3
        load    8(a1),e11
        paddusb e2,e3,e3
        paddusb e10,e11,e11
        store   e3,(a1)+
        store   e11,(a1)+
