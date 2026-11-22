; seq/HUR-015.i - Hurrican SlopeCheck original div20 (divul.l #20 + 2x divu.l #20)
; Uses: d0, d1, d2, d3, d4, a0
; MUST NOT touch: d5, d6, d7, a5, a6, a7
        move.l  (a0)+,d1
        divul.l #20,d0:d1
        move.l  (a0)+,d2
        divu.l  #20,d2
        move.l  (a0)+,d3
        divu.l  #20,d3
        add.l   d1,d0
        add.l   d2,d0
        add.l   d3,d0
        move.l  d0,(a1)+
