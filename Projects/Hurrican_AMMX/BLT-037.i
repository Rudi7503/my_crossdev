; seq/BLT-037.i - 24-Bit RGB888 Copy (24 Bytes = 8 pixels, 3x load + 3x store)
; Uses: e2, e3, e4, a0, a1
; MUST NOT touch: d5, d6, d7, a5, a6, a7
        load    (a0)+,e2
        load    (a0)+,e3
        load    (a0)+,e4
        store   e2,(a1)+
        store   e3,(a1)+
        store   e4,(a1)+
