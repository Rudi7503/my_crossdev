; seq/BLT-020.i - Transparent Sprite Color-Key 4x Unrolled (PCMPEQW + STOREILM)
; Uses: e0, e1, e2, e3, e4, e5, e6, e8, e7, a0, a1
; MUST NOT touch: d5, d6, d7, a5, a6, a7
; Evaluates 16 pixels (4 quadwords) with preloaded key in e7
        load    (a0)+,e0
        load    (a0)+,e1
        load    (a0)+,e2
        load    (a0)+,e3
        pcmpeqw e7,e0,e4
        pcmpeqw e7,e1,e5
        pcmpeqw e7,e2,e6
        pcmpeqw e7,e3,e8
        storeilm e0,e4,(a1)+
        storeilm e1,e5,(a1)+
        storeilm e2,e6,(a1)+
        storeilm e3,e8,(a1)+
