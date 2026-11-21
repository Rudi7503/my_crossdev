; seq/BLT-032.i - 32-Bit ARGB8888 Exact 32-Bit Color-Key (4 pixels, any 32-bit key)
; Combines upper+lower 16-bit word equality via vperm #$23016745 + pand
; Uses: e0(key), e2..e7, a0, a1
; MUST NOT touch: d5, d6, d7, a5, a6, a7
        load    (a0)+,e2
        load    (a0)+,e4
        pcmpeqw e0,e2,e3
        pcmpeqw e0,e4,e5
        vperm   #$23016745,e3,e3,e6
        vperm   #$23016745,e5,e5,e7
        pand    e6,e3,e3
        pand    e7,e5,e5
        storeilm e2,e3,(a1)+
        storeilm e4,e5,(a1)+
