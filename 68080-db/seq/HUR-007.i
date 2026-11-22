; seq/HUR-007.i - Hurrican AudHandler AMMX 8-byte signed saturated mix (bit-exact, 8 bytes)
; Uses: a0, a1, e0..e3, e7(8x$80), e8(8x$00)
; MUST NOT touch: d5, d6, d7, a5, a6, a7
        load    (a0),e0
        load    (a1)+,e1
        pmaxsb  e8,e1,e2
        pminsb  e8,e1,e3
        psubb   e3,e8,e3
        peor    e7,e0,e0
        paddusb e2,e0,e0
        psubusb e3,e0,e0
        peor    e7,e0,e0
        store   e0,(a0)+
