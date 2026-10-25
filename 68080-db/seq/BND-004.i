; seq/BND-004.i - MOVEM stays on pipe 1, independent ALU can use pipe 2
; Uses: d0-d3, d4, a0
; MUST NOT touch: d5, d6, d7, a5, a6, a7
        movem.l d0-d3,(a0)
        addq.l  #1,d4
