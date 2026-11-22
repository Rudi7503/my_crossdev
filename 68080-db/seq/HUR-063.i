; seq/HUR-063.i - Hurrican DrawParts original signed divs.l #$168000 + and.l #15 (enemies.s L16664)
; Uses: d0, d1, a0. MUST NOT touch: d5, d6, d7, a5, a6, a7
        move.l  (a0)+,d0
        divs.l  #$168000,d0
        and.l   #15,d0
        move.l  d0,(a1)+
