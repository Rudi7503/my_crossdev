; seq/AMX-005.i - chunky-to-planar bit transpose with C2P
; Uses: e0, e1, a0, a1
; MUST NOT touch: d5, d6, d7, a5, a6, a7
        load    (a0)+,e0
        c2p     e0,e1
        store   e1,(a1)+
