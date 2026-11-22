; seq/HUR-004.i - Hurrican CompFall original per-pixel waterfall blend (1 pixel)
; Uses: d0, d1, d2, d3, d4, a0, a1, a2, a3, a4
; MUST NOT touch: d5, d6, d7, a5, a6, a7
        moveq   #0,d0
        move.w  (a0)+,d0
        move.l  d0,d1
        swap    d1
        or.l    d1,d0
        and.l   #$07E0F81F,d0
        moveq   #0,d1
        move.w  (a2)+,d1
        move.l  d1,d2
        swap    d2
        or.l    d2,d1
        and.l   #$07E0F81F,d1
        move.l  a4,d2
        mulu.l  d2,d0
        move.l  a1,d2
        mulu.l  d2,d1
        add.l   d1,d0
        lsr.l   #5,d0
        and.l   #$07E0F81F,d0
        mulu.l  d3,d0
        lsr.l   #3,d0
        and.l   #$07E0F81F,d0
        move.l  d0,d1
        swap    d1
        or.w    d1,d0
        move.w  d0,(a3)+
