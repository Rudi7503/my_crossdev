; seq/HUR-002.i - Hurrican _DrawTextTint reciprocal /255 via (n*257+257)>>16 (bit-exact)
; Uses: d0, d2, d3, d4, a0, a1
; MUST NOT touch: d5, d6, d7, a5, a6, a7
        move.w  (a0)+,d2
        moveq   #0,d3
        move.w  d2,d3
        lsr.w   #8,d3
        lsr.w   #3,d3
        mulu.w  #200,d3
        mulu.w  #257,d3
        add.l   #257,d3
        swap    d3
        moveq   #0,d4
        move.w  d2,d4
        lsr.w   #5,d4
        and.w   #63,d4
        mulu.w  #150,d4
        mulu.w  #257,d4
        add.l   #257,d4
        swap    d4
        moveq   #0,d0
        move.w  d2,d0
        and.w   #31,d0
        mulu.w  #100,d0
        mulu.w  #257,d0
        add.l   #257,d0
        swap    d0
        lsl.w   #8,d3
        lsl.w   #3,d3
        lsl.w   #5,d4
        or.w    d4,d3
        or.w    d0,d3
        move.w  d3,(a1)+
