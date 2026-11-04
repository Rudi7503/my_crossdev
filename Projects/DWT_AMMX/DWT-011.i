; seq/DWT-011.i - 1D 16-Bit Haar Wavelet (2x Unrolled & Early Store Overlap, 16 samples)
; Uses: e0, e1, e2, e3, e4, e5, e6, e7, e8, e9, e10, e14, e15, d0, a0, a1, a2
; MUST NOT touch: d5, d6, d7, a5, a6, a7
; d0=1, e10=$7FFF7FFF7FFF7FFF
; Evaluates 16 samples (8 word pairs) with dual-stream interleaving
        load    (a0)+,e0
        load    (a0)+,e1
        load    (a0)+,e6
        load    (a0)+,e7
        vperm   #$014589cd,e0,e1,e2
        vperm   #$014589cd,e6,e7,e8
        vperm   #$2367abef,e0,e1,e3
        vperm   #$2367abef,e6,e7,e9
        paddw   e2,e3,e4
        paddw   e8,e9,e14
        lsrq    d0,e4,e4
        lsrq    d0,e14,e14
        pand    e10,e4,e4
        pand    e10,e14,e14
        store   e4,(a1)+
        store   e14,(a1)+
        psubw   e3,e2,e5
        psubw   e9,e8,e15
        lsrq    d0,e5,e5
        lsrq    d0,e15,e15
        pand    e10,e5,e5
        pand    e10,e15,e15
        store   e5,(a2)+
        store   e15,(a2)+
