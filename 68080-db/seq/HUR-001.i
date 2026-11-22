; seq/HUR-001.i - Hurrican _DrawTextTint original per-pixel tinting (3x divu.w #255)
; Uses: d0, d1, d2, d3, d4, a0, a1
; MUST NOT touch: d5, d6, d7, a5, a6, a7
        move.w  (a0)+,d2
        moveq   #0,d3
        move.w  d2,d3
        lsr.w   #8,d3
        lsr.w   #3,d3
        mulu.w  #200,d3
        divu.w  #255,d3
        moveq   #0,d4
        move.w  d2,d4
        lsr.w   #5,d4
        and.w   #63,d4
        mulu.w  #150,d4
        divu.w  #255,d4
        moveq   #0,d0
        move.w  d2,d0
        and.w   #31,d0
        mulu.w  #100,d0
        divu.w  #255,d0
        lsl.w   #8,d3
        lsl.w   #3,d3
        lsl.w   #5,d4
        or.w    d4,d3
        or.w    d0,d3
        move.w  d3,(a1)+
