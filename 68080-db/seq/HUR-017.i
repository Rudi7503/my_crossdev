; seq/HUR-017.i - Hurrican _DrawLevelLayer original stack-RMW + mulu.l #800
; Uses: d0, d1, d2, a0, a1, a2, a3
; MUST NOT touch: d5, d6, d7, a5, a6, a7
        moveq   #0,d0
        move.w  (a0)+,d0
        mulu.l  #800,d0
        lea     (a1,d0.l),a2
        addq.l  #1,4(a3)
        addq.l  #2,8(a3)
        add.l   #20,12(a3)
        move.l  a2,(a4)+
