; seq/BLT-023.i - Hurrican BlitAdd 2x Unrolled & Interleaved (8 RGB565 pixels)
; Source: Hurrican/src/blit.s _BlitAdd (.a2)
; Uses: e2, e3, e4, e5, e6, e7, e10, e11, e12, e13, e14, e15, a0, a1
; MUST NOT touch: d5, d6, d7, a5, a6, a7
        load    (a0)+,e2
        load    (a0)+,e10
        load    (a1),e3
        load    8(a1),e11
        unpack1632 e2,e4:e5
        unpack1632 e10,e12:e13
        unpack1632 e3,e6:e7
        unpack1632 e11,e14:e15
        paddusb e4,e6,e6
        paddusb e12,e14,e14
        paddusb e5,e7,e7
        paddusb e13,e15,e15
        pack3216 e6,e7,e3
        pack3216 e14,e15,e11
        store   e3,(a1)+
        store   e11,(a1)+
