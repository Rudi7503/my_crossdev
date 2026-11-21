; seq/BLT-022.i - Hurrican BlitAdd 1x (4 RGB565 pixels, true per-channel saturating add)
; Source: Hurrican/src/blit.s _BlitAdd (.a1)
; Uses: e2, e3, e4, e5, e6, e7, a0, a1
; MUST NOT touch: d5, d6, d7, a5, a6, a7
        load    (a0)+,e2
        load    (a1),e3
        unpack1632 e2,e4:e5
        unpack1632 e3,e6:e7
        paddusb e4,e6,e6
        paddusb e5,e7,e7
        pack3216 e6,e7,e3
        store   e3,(a1)+
