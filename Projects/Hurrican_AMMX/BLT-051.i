; seq/BLT-051.i - Hurrican BlitKeyAdd Optimized with Memory-VEA on dst (4px)
; Saves load (a1),e3 via unpack1632 (a1),e6:e7
; Uses: e0(key), e2, e3, e4, e5, e6, e7, e8, a0, a1
; MUST NOT touch: d5, d6, d7, a5, a6, a7
        load    (a0)+,e2
        pcmpeqw e0,e2,e8
        unpack1632 e2,e4:e5
        unpack1632 (a1),e6:e7
        paddusb e4,e6,e6
        paddusb e5,e7,e7
        pack3216 e6,e7,e3
        storeilm e3,e8,(a1)+
