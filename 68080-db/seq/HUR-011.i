; seq/HUR-011.i - Hurrican _BlitAlpha32 original 1-pixel alpha blend (a in 1..31, colorkey $F81F)
; Uses: d0, d1, d2, d3, d4, a0, a1
; MUST NOT touch: d5, d6, d7, a5, a6, a7
        move.w  (a0)+,d0
        cmp.w   #$F81F,d0
        beq.s   *+40
        move.w  d0,d2
        swap    d2
        move.w  d0,d2
        and.l   #$07E0F81F,d2
        mulu.l  d3,d2
        move.w  (a1),d1
        move.w  d1,d0
        swap    d0
        move.w  d1,d0
        and.l   #$07E0F81F,d0
        mulu.l  d4,d0
        add.l   d2,d0
        lsr.l   #5,d0
        and.l   #$07E0F81F,d0
        move.l  d0,d1
        swap    d1
        or.w    d1,d0
        move.w  d0,(a1)
        addq.l  #2,a1
