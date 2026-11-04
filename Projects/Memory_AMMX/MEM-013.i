; seq/MEM-013.i - AMMX MEMMOVE Streaming Block (Forward/Backward Safe Step)
; Uses: e0, e1, a0, a1, d0, d1
; MUST NOT touch: d5, d6, d7, a5, a6, a7
; Interleaved dual 64-bit transfer
        load    (a0)+,e0
        load    (a0)+,e1
        store   e0,(a1)+
        store   e1,(a1)+
