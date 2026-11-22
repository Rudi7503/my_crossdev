; seq/HUR-020.i - Hurrican MhNeck .p original muls.l + divs.l #24 (enemies.s L28777-28779)
; Uses: d0, d1, a0, a1
; MUST NOT touch: d5, d6, d7, a5, a6, a7
        move.l  (a0)+,d0
        move.l  (a0)+,d1
        muls.l  d1,d0
        divs.l  #24,d0
        move.l  d0,(a1)+
