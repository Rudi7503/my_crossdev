; seq/HUR-059.i - Hurrican _BlitKey 8-px (.k2) original (2x load + 2x pcmpeqw + 2x storeilm, no skip)
; Uses: e0..e5, a0, a1. MUST NOT touch: d5, d6, d7, a5, a6, a7
        load    (a0)+,e2
        load    (a0)+,e4
        pcmpeqw e0,e2,e3
        pcmpeqw e0,e4,e5
        storeilm e2,e3,(a1)+
        storeilm e4,e5,(a1)+
