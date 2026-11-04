; seq/MEM-021.i - AMMX 64-Byte Full Cacheline MEMCPY (8x 64-bit load/store burst)
; Uses: e0, e1, e2, e3, e4, e5, e6, e7, a0, a1
; MUST NOT touch: d5, d6, d7, a5, a6, a7
; Interleaved burst copying 64 bytes (full cacheline) per iteration
        load    (a0)+,e0
        load    (a0)+,e1
        store   e0,(a1)+
        load    (a0)+,e2
        store   e1,(a1)+
        load    (a0)+,e3
        store   e2,(a1)+
        load    (a0)+,e4
        store   e3,(a1)+
        load    (a0)+,e5
        store   e4,(a1)+
        load    (a0)+,e6
        store   e5,(a1)+
        load    (a0)+,e7
        store   e6,(a1)+
        store   e7,(a1)+
