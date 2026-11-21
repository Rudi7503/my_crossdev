; seq/BLT-050.i - 16-Bit RGB565 Additive Blit 1x with unpack1632 Memory-VEA test (4 pixels)
; Tests whether unpack1632 supports (a0)+ VEA directly
; Uses: e3, e4, e5, e6, e7, a0, a1
; MUST NOT touch: d5, d6, d7, a5, a6, a7
        unpack1632 (a0)+,e4:e5
        unpack1632 (a1),e6:e7
        paddusb e4,e6,e6
        paddusb e5,e7,e7
        pack3216 e6,e7,e3
        store   e3,(a1)+
