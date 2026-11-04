; seq/DWT-003.i - 1D CDF 9/7 Lossy Fixed-Point Wavelet Lifting (JPEG 2000)
; Uses: e0, e1, e2, e3, e4, e5, e6, e7, e12, e13, e14, e15, a0, a1, a2
; MUST NOT touch: d5, d6, d7, a5, a6, a7
; Evaluates 4 lifting steps: Predict1, Update1, Predict2, Update2
        load    (a0)+,e0
        load    (a0)+,e1
        vperm   #$014589cd,e0,e1,e2
        vperm   #$2367abef,e0,e1,e3
        vperm   #$23456701,e2,e2,e4
        paddw   e2,e4,e5
        pmulh   e12,e5,e5
        paddw   e5,e3,e3
        vperm   #$67012345,e3,e3,e6
        paddw   e6,e3,e7
        pmulh   e13,e7,e7
        paddw   e7,e2,e2
        vperm   #$23456701,e2,e2,e4
        paddw   e2,e4,e5
        pmulh   e14,e5,e5
        paddw   e5,e3,e3
        vperm   #$67012345,e3,e3,e6
        paddw   e6,e3,e7
        pmulh   e15,e7,e7
        paddw   e7,e2,e2
        store   e2,(a1)+
        store   e3,(a2)+
