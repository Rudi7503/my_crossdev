; seq/DWT-014.i - 1D Daubechies-4 (Db4) with Coupled Low/High Multiplications
; Uses: e0, e1, e2, e3, e4, e5, e6, e7, e8, e9, e10, e11, e12, e13, e14, e15, a0, a1, a2
; MUST NOT touch: d5, d6, d7, a5, a6, a7
; Evaluates Low and High pass concurrently, maximizing register reuse of input taps
        load    (a0)+,e0
        load    (a0)+,e1
        vperm   #$23456789,e0,e1,e2
        vperm   #$456789ab,e0,e1,e3
        pmulh   e10,e0,e4
        pmulh   e13,e0,e8
        pmulh   e11,e2,e5
        pmulh   e12,e2,e9
        pmulh   e12,e3,e6
        pmulh   e11,e3,e14
        pmulh   e13,e1,e7
        pmulh   e10,e1,e15
        paddw   e4,e5,e4
        psubw   e9,e8,e8
        paddw   e6,e7,e6
        paddw   e14,e15,e14
        paddw   e4,e6,e4
        store   e4,(a1)+
        paddw   e8,e14,e8
        store   e8,(a2)+
