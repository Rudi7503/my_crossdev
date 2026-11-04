; seq/DWT-012.i - 1D CDF 5/3 Integer Lifting (2x Unrolled Interleaved, 16 samples)
; Uses: e0, e1, e2, e3, e4, e5, e6, e7, e8, e9, e10, e11, e12, e14, e15, e16, e17, e18, e19, d0, d1, a0, a1, a2
; MUST NOT touch: d5, d6, d7, a5, a6, a7
; d0=1, d1=2, e10=$7FFF7FFF7FFF7FFF, e11=$0002000200020002, e12=$3FFF3FFF3FFF3FFF
        load    (a0)+,e0
        load    (a0)+,e1
        load    (a0)+,e4
        load    (a0)+,e5
        vperm   #$014589cd,e0,e1,e2
        vperm   #$014589cd,e4,e5,e14
        vperm   #$2367abef,e0,e1,e3
        vperm   #$2367abef,e4,e5,e15
        vperm   #$23456701,e2,e2,e6
        vperm   #$23456701,e14,e14,e16
        paddw   e2,e6,e7
        paddw   e14,e16,e17
        lsrq    d0,e7,e7
        lsrq    d0,e17,e17
        pand    e10,e7,e7
        pand    e10,e17,e17
        psubw   e7,e3,e3
        psubw   e17,e15,e15
        vperm   #$67012345,e3,e3,e8
        vperm   #$67012345,e15,e15,e18
        paddw   e8,e3,e9
        paddw   e18,e15,e19
        paddw   e11,e9,e9
        paddw   e11,e19,e19
        lsrq    d1,e9,e9
        lsrq    d1,e19,e19
        pand    e12,e9,e9
        pand    e12,e19,e19
        paddw   e9,e2,e2
        paddw   e19,e14,e14
        store   e2,(a1)+
        store   e14,(a1)+
        store   e3,(a2)+
        store   e15,(a2)+
