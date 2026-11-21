; seq/BLT-031.i - 32-Bit ARGB8888 Color-Key 2x Unrolled (4 pixels, pcmpeqw + storeilm)
; Key = 0x00000000 (or symmetric 16-bit halfwords) preloaded in e0
; Uses: e0(key), e2, e3, e4, e5, a0, a1
; MUST NOT touch: d5, d6, d7, a5, a6, a7
        load    (a0)+,e2
        load    (a0)+,e4
        pcmpeqw e0,e2,e3
        pcmpeqw e0,e4,e5
        storeilm e2,e3,(a1)+
        storeilm e4,e5,(a1)+
