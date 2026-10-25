; seq/MEM-001.i - align two long writes to one quad boundary
; Uses: d2, d3, a0
; MUST NOT touch: d5, d6, d7, a5, a6, a7
        move.l  d2,(a0)
        move.l  d3,4(a0)
