; seq/HUR-021.i - Hurrican MhNeck .p bit-exact signed /24 via (n*43691)>>20 + sign
; Uses: d0, d1, d2, a0, a1
; MUST NOT touch: d5, d6, d7, a5, a6, a7
        move.l  (a0)+,d0
        move.l  (a0)+,d1
        muls.l  d1,d0
        muls.l  #43691,d0
        move.l  d0,d1
        moveq   #20,d2
        asr.l   d2,d0
        add.l   d1,d1
        moveq   #0,d1
        addx.l  d1,d0
        move.l  d0,(a1)+
