; seq/AMX-001.i - 64-bit LOAD/STORE instead of two 32-bit moves
; Uses: e0, a0, a1
; MUST NOT touch: d5, d6, d7, a5, a6, a7
        load    (a0)+,e0
        store   e0,(a1)+
