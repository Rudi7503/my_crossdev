; seq/HUR-040.i - Hurrican WheelUpdate original divs.l #3 (player.s L355, L363)
; Uses: d0, a0, a1
; MUST NOT touch: d5, d6, d7, a5, a6, a7
        move.l  (a0)+,d0
        divs.l  #3,d0
        move.l  d0,(a1)+
