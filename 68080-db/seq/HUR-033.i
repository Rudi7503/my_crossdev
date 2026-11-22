; seq/HUR-033.i - Hurrican BlockAt unsigned cmp #81919 + mulu.l #52429 + lsr.l #20 (no LUT)
; Uses: d0, d1, d2, d3, d4, a0, a1
; MUST NOT touch: d5, d6, d7, a5, a6, a7
        move.l  (a0)+,d0
        move.l  (a0)+,d1
        cmp.l   #81919,d1
        bhi.s   *+28
        cmp.l   #81919,d0
        bhi.s   *+22
        moveq   #20,d4
        mulu.l  #52429,d0
        lsr.l   d4,d0
        cmp.l   d2,d0
        bge.s   *+8
        mulu.l  #52429,d1
        lsr.l   d4,d1
        add.l   d1,d0
        move.l  d0,(a1)+
