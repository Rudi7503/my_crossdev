; seq/HUR-019.i - Hurrican _DrawLevelLayer register-cached + shift-add *800 ((16+8+1)<<5)
; Uses: d0, d1, d2, d3, d4, a0, a1, a2, a4
; MUST NOT touch: d5, d6, d7, a5, a6, a7
        moveq   #0,d0
        move.w  (a0)+,d0
        move.l  d0,d1
        lsl.l   #4,d1
        add.l   d0,d1
        lsl.l   #3,d0
        add.l   d1,d0
        lsl.l   #5,d0
        lea     (a1,d0.l),a2
        addq.l  #1,d2
        addq.l  #2,d3
        add.l   #20,d4
        move.l  a2,(a4)+
