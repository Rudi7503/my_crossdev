; seq/HUR-016.i - Hurrican SlopeCheck reciprocal /20 via (x*52429)>>20 + rem (bit-exact <81920)
; Uses: d0, d1, d2, d3, d4, a0, a1
; MUST NOT touch: d5, d6, d7, a5, a6, a7
        move.l  (a0)+,d4
        move.l  d4,d1
        mulu.l  #52429,d1
        moveq   #20,d0
        lsr.l   d0,d1
        move.l  d1,d2
        mulu.l  #20,d2
        sub.l   d2,d4
        move.l  (a0)+,d2
        mulu.l  #52429,d2
        lsr.l   d0,d2
        move.l  (a0)+,d3
        mulu.l  #52429,d3
        lsr.l   d0,d3
        add.l   d1,d4
        add.l   d2,d4
        add.l   d3,d4
        move.l  d4,(a1)+
