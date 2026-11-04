; seq/SRT-003.i - AMMX 64-Bit Parallel POPCNT (Hamming Weight in 64-bit Vector)
; Uses: e0, e1, e2, e3, e4, d0, d1, a0, a1
; MUST NOT touch: d5, d6, d7, a5, a6, a7
; d0=1, d1=2, e1=$5555555555555555, e2=$3333333333333333
        load    (a0)+,e0
        lsrq    d0,e0,e4
        pand    e1,e0,e0
        pand    e1,e4,e4
        paddb   e4,e0,e0
        lsrq    d1,e0,e4
        pand    e2,e0,e0
        pand    e2,e4,e4
        paddb   e4,e0,e0
        store   e0,(a1)+
