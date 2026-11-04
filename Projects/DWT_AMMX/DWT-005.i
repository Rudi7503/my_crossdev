; seq/DWT-005.i - 2D Haar Wavelet Decomposition (4x4 Matrix Block -> LL, LH, HL, HH)
; Uses: e0, e1, e2, e3, e4, e5, e6, e7, e8, e9, d0, a0, a1, a2, a3, a4
; MUST NOT touch: d5, d6, d7, a5, a6, a7
; d0=1 (shift by 1)
; Horizontal 1D Haar on 2 rows, followed by Vertical 1D Haar -> 4 Subbands
        load    (a0)+,e0
        load    (a0)+,e1
        vperm   #$01450145,e0,e0,e2
        vperm   #$23672367,e0,e0,e3
        paddw   e2,e3,e4
        psubw   e3,e2,e5
        lsrq    d0,e4,e4
        lsrq    d0,e5,e5
        vperm   #$01450145,e1,e1,e6
        vperm   #$23672367,e1,e1,e7
        paddw   e6,e7,e8
        psubw   e7,e6,e9
        lsrq    d0,e8,e8
        lsrq    d0,e9,e9
        paddw   e4,e8,e2
        psubw   e8,e4,e3
        lsrq    d0,e2,e2
        lsrq    d0,e3,e3
        store   e2,(a1)+
        store   e3,(a2)+
        paddw   e5,e9,e6
        psubw   e9,e5,e7
        lsrq    d0,e6,e6
        lsrq    d0,e7,e7
        store   e6,(a3)+
        store   e7,(a4)+
