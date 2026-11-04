; seq/DWT-001.i - 1D 16-Bit Haar Wavelet Transform (8 samples, 4 word pairs)
; Uses: e0, e1, e2, e3, e4, e5, e10, d0, a0, a1, a2
; MUST NOT touch: d5, d6, d7, a5, a6, a7
; d0=1 (shift by 1), e10=$7FFF7FFF7FFF7FFF
; Evaluates Low-pass L = (A+B)/2 and High-pass H = (A-B)/2 for 4 pairs in parallel
        load    (a0)+,e0
        load    (a0)+,e1
        vperm   #$014589cd,e0,e1,e2
        vperm   #$2367abef,e0,e1,e3
        paddw   e2,e3,e4
        psubw   e3,e2,e5
        lsrq    d0,e4,e4
        pand    e10,e4,e4
        lsrq    d0,e5,e5
        pand    e10,e5,e5
        store   e4,(a1)+
        store   e5,(a2)+
