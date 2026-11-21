; seq/BLT-052.i - Hurrican BlitKeyTint Optimized with Memory-VEA on dst (4px)
; Saves load (a1),e3 via unpack1632 (a1),e6:e7
; Uses: e0(key), e9(tint), e2, e3, e6, e7, e8, a0, a1
; MUST NOT touch: d5, d6, d7, a5, a6, a7
        load    (a0)+,e2
        pcmpeqw e0,e2,e8
        unpack1632 (a1),e6:e7
        paddusb e9,e6,e6
        paddusb e9,e7,e7
        pack3216 e6,e7,e3
        storeilm e3,e8,(a1)+
