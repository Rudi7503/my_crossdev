; seq/MEM-011.i - AMMX Fast MEMCPY (32-byte block copy via dual-stream 64-bit load/store)
; Uses: e0, e1, e2, e3, a0, a1
; MUST NOT touch: d5, d6, d7, a5, a6, a7
; 4x 64-bit loads and stores interleaved for dual-pipe overlap
        load    (a0)+,e0
        load    (a0)+,e1
        store   e0,(a1)+
        load    (a0)+,e2
        store   e1,(a1)+
        load    (a0)+,e3
        store   e2,(a1)+
        store   e3,(a1)+
