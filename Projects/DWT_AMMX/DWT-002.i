; seq/DWT-002.i - 1D CDF 5/3 Reversible Integer Wavelet Lifting (Lossless JPEG 2000)
; Uses: e0, e1, e2, e3, e6, e7, e8, e9, e10, e11, e12, d0, d1, a0, a1, a2
; MUST NOT touch: d5, d6, d7, a5, a6, a7
; d0=1, d1=2, e10=$7FFF7FFF7FFF7FFF, e11=$0002000200020002, e12=$3FFF3FFF3FFF3FFF
; Evaluates Predict: d = odd - floor((even[n]+even[n+1])/2)
; Evaluates Update:  s = even + floor((d[n-1]+d[n]+2)/4)
        load    (a0)+,e0
        load    (a0)+,e1
        vperm   #$014589cd,e0,e1,e2
        vperm   #$2367abef,e0,e1,e3
        vperm   #$23456701,e2,e2,e6
        paddw   e2,e6,e7
        lsrq    d0,e7,e7
        pand    e10,e7,e7
        psubw   e7,e3,e3
        vperm   #$67012345,e3,e3,e8
        paddw   e8,e3,e9
        paddw   e11,e9,e9
        lsrq    d1,e9,e9
        pand    e12,e9,e9
        paddw   e9,e2,e2
        store   e2,(a1)+
        store   e3,(a2)+
