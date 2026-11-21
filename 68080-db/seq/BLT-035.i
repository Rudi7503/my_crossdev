; seq/BLT-035.i - 32-Bit ARGB8888 Color-Key + Additive Blit 2x (4 pixels)
; Uses: e0(key), e2, e3, e8, e9, e10, e11, a0, a1
; MUST NOT touch: d5, d6, d7, a5, a6, a7
        load    (a0)+,e2
        load    (a0)+,e10
        pcmpeqw e0,e2,e8
        pcmpeqw e0,e10,e9
        load    (a1),e3
        load    8(a1),e11
        paddusb e2,e3,e3
        paddusb e10,e11,e11
        storeilm e3,e8,(a1)+
        storeilm e11,e9,(a1)+
