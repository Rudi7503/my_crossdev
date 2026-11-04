; seq/MEM-020.i - AMMX 64-Byte Full Cacheline MEMSET (8x 64-bit store burst)
; Uses: e0, a1
; MUST NOT touch: d5, d6, d7, a5, a6, a7
; e0 contains pre-broadcasted 64-bit fill pattern
        store   e0,(a1)+
        store   e0,(a1)+
        store   e0,(a1)+
        store   e0,(a1)+
        store   e0,(a1)+
        store   e0,(a1)+
        store   e0,(a1)+
        store   e0,(a1)+
