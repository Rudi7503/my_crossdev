; seq/BLT-033.i - 32-Bit ARGB8888 Additive Blit 1x (2 pixels, native paddusb)
; Uses: e2, e3, a0, a1
; MUST NOT touch: d5, d6, d7, a5, a6, a7
        load    (a0)+,e2
        load    (a1),e3
        paddusb e2,e3,e3
        store   e3,(a1)+
