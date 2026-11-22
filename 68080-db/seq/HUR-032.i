; seq/HUR-032.i - Hurrican BlockAt original bounds + Div20 LUT (player.s L1099-1113)
; Uses: d0, d1, d2, d3, a0, a1, a2
; MUST NOT touch: d5, d6, d7, a5, a6, a7
        move.l  (a0)+,d0
        move.l  (a0)+,d1
        tst.l   d1
        bmi.s   *+36
        tst.l   d0
        bmi.s   *+32
        cmp.l   #16383,d0
        bhi.s   *+26
        move.w  (a2,d0.l*2),d0
        cmp.l   d2,d0
        bge.s   *+18
        cmp.l   #16383,d1
        bhi.s   *+12
        move.w  (a2,d1.l*2),d1
        cmp.l   d3,d1
        bge.s   *+4
        add.l   d1,d0
        move.l  d0,(a1)+
