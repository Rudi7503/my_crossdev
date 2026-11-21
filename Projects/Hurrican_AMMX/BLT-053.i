; seq/BLT-053.i - Hurrican BlitKeyTint Fully Optimized (Memory-VEA on src + dst, 4px)
; Saves both load (a0)+ and load (a1) via pcmpeqw (a0)+ and unpack1632 (a1)
; Uses: e0(key), e9(tint), e3, e6, e7, e8, a0, a1
; MUST NOT touch: d5, d6, d7, a5, a6, a7
        pcmpeqw (a0)+,e0,e8
        unpack1632 (a1),e6:e7
        paddusb e9,e6,e6
        paddusb e9,e7,e7
        pack3216 e6,e7,e3
        storeilm e3,e8,(a1)+
