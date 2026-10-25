; seq/MEM-004.i - fill a 32-byte burst window with eight long loads
; Uses: d0, d1, d2, d3, a0
; MUST NOT touch: d5, d6, d7, a5, a6, a7
        move.l  (a0)+,d0
        move.l  (a0)+,d1
        move.l  (a0)+,d2
        move.l  (a0)+,d3
        move.l  (a0)+,d0
        move.l  (a0)+,d1
        move.l  (a0)+,d2
        move.l  (a0)+,d3
