; seq/FUS-002.i - MOVE.L + NEG.L (fused read-modify-write)
; Uses: d2, d3
; MUST NOT touch: d5, d6, d7, a5, a6, a7
        move.l  d2,d3
        neg.l   d3
