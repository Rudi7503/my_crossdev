; seq/HUR-064.i - Hurrican DrawParts reciprocal / $168000 (muls.l #$5B05B05C + asr #19 + addx, 0 div)
; Uses: d0..d3, a0, a1. MUST NOT touch: d5, d6, d7, a5, a6, a7
        move.l  (a0)+,d0
        move.l  d0,d2
        muls.l  #$5B05B05C,d1:d0
        moveq   #19,d3
        asr.l   d3,d1
        moveq   #0,d3
        add.l   d2,d2
        addx.l  d3,d1
        and.l   #15,d1
        move.l  d1,(a1)+
