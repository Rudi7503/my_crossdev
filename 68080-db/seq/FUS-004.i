; seq/FUS-004.i - two MOVE.L (An)+,(Am)+ -> one 64-bit move
; Uses: a0, a1
; MUST NOT touch: d5, d6, d7, a5, a6, a7
        move.l  (a0)+,(a1)+
        move.l  (a0)+,(a1)+
