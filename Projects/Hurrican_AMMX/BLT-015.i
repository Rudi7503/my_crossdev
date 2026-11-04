; seq/BLT-015.i - Transparent Sprite Color-Key 2x Unrolled (PCMPEQW + STOREILM)
; Uses: e0, e1, e4, e5, e7, a0, a1
; MUST NOT touch: d5, d6, d7, a5, a6, a7
; Evaluates 8 RGB565 pixels across 2 quadwords using preloaded key in e7
        load    (a0)+,e0
        load    (a0)+,e1
        pcmpeqw e7,e0,e4
        pcmpeqw e7,e1,e5
        storeilm e0,e4,(a1)+
        storeilm e1,e5,(a1)+
