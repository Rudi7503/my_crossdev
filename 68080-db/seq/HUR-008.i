; seq/HUR-008.i - Hurrican AudHandler AMMX 2x unrolled 16-byte signed saturated mix
; Uses: a0, a1, e0..e6, e7(8x$80), e8(8x$00)
; MUST NOT touch: d5, d6, d7, a5, a6, a7
        load    (a0),e0
        load    8(a0),e4
        load    (a1)+,e1
        load    (a1)+,e5
        pmaxsb  e8,e1,e2
        pmaxsb  e8,e5,e6
        pminsb  e8,e1,e3
        pminsb  e8,e5,e1
        psubb   e3,e8,e3
        psubb   e1,e8,e1
        peor    e7,e0,e0
        peor    e7,e4,e4
        paddusb e2,e0,e0
        paddusb e6,e4,e4
        psubusb e3,e0,e0
        psubusb e1,e4,e4
        peor    e7,e0,e0
        peor    e7,e4,e4
        store   e0,(a0)+
        store   e4,(a0)+
