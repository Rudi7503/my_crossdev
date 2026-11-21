; seq/BLT-036.i - 32-Bit ARGB8888 Color-Key + Constant Tint Add (2 pixels)
; Uses: e0(key), e9(tint), e2, e3, e8, a0, a1
; MUST NOT touch: d5, d6, d7, a5, a6, a7
        load    (a0)+,e2
        pcmpeqw e0,e2,e8
        load    (a1),e3
        paddusb e9,e3,e3
        storeilm e3,e8,(a1)+
