; seq/BLT-029.i - 32-Bit ARGB8888 Copy (32 Bytes = 8 pixels, 4x load + 4x store)
; Uses: e2, e3, e4, e5, a0, a1
; MUST NOT touch: d5, d6, d7, a5, a6, a7
        load    (a0)+,e2
        load    (a0)+,e3
        load    (a0)+,e4
        load    (a0)+,e5
        store   e2,(a1)+
        store   e3,(a1)+
        store   e4,(a1)+
        store   e5,(a1)+
