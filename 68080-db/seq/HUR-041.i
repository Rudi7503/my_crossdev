; seq/HUR-041.i - Hurrican WheelUpdate branchless signed /3 (muls.l #$55555556,d1:d0 + addx)
; Uses: d0, d1, a0, a1
; MUST NOT touch: d5, d6, d7, a5, a6, a7
        move.l  (a0)+,d0
        muls.l  #$55555556,d1:d0
        move.l  d1,d0
        add.l   d0,d0
        moveq   #0,d0
        addx.l  d0,d1
        move.l  d1,(a1)+
