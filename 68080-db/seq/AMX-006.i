; seq/AMX-006.i - 32-bit ARGB to RGB565 packing with PACK3216
; Uses: e0, e1, e2, a0, a1
; MUST NOT touch: d5, d6, d7, a5, a6, a7
        load    (a0)+,e0
        load    (a0)+,e1
        pack3216 e0,e1,e2
        store   e2,(a1)+
