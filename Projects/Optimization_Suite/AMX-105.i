; seq/AMX-105.i - 2x unrolled C2P Chunky-to-Planar Transpose (16 bytes)
; Uses: e0, e1, e2, e3, a0, a1
; MUST NOT touch: d5, d6, d7, a5, a6, a7
        load    (a0)+,e0
        load    (a0)+,e2
        c2p     e0,e1
        store   e1,(a1)+
        c2p     e2,e3
        store   e3,(a1)+
