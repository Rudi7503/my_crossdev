; seq/BLT-026.i - Hurrican BlitKeyTint (4 RGB565 pixels, colorkey + constant tint add)
; Source: Hurrican/src/blit.s _BlitKeyTint (.quad)
; Uses: e0(key), e9(tint), e2, e3, e6, e7, e8, a0, a1
; MUST NOT touch: d5, d6, d7, a5, a6, a7
        load    (a0)+,e2
        pcmpeqw e0,e2,e8
        load    (a1),e3
        unpack1632 e3,e6:e7
        paddusb e9,e6,e6
        paddusb e9,e7,e7
        pack3216 e6,e7,e3
        storeilm e3,e8,(a1)+
