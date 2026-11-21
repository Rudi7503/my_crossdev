; seq/BLT-045.i - 32-Bit ARGB8888 Copy 2x Streaming (16 Bytes = 4 pixels, 2x load + 2x store)
; Optimizes BLT-029 (4x) using MEM-013 2x streaming principle
; Uses: e2, e3, a0, a1
; MUST NOT touch: d5, d6, d7, a5, a6, a7
        load    (a0)+,e2
        load    (a0)+,e3
        store   e2,(a1)+
        store   e3,(a1)+
