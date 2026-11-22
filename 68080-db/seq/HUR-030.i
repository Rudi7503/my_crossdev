; seq/HUR-030.i - Hurrican .f6p original 8-pixel scalar 3/4*dst + src (blit.s L2434-2461)
; Uses: d0..d4, a0, a1, a2, a3
; MUST NOT touch: d5, d6, d7, a5, a6, a7
        move.l  (a1),d0
        move.l  4(a1),d2
        move.l  d0,d1
        move.l  d2,d3
        lsr.l   #2,d1
        lsr.l   #2,d3
        and.l   #$39E739E7,d1
        and.l   #$39E739E7,d3
        sub.l   d1,d0
        sub.l   d3,d2
        add.l   (a0)+,d0
        add.l   (a0)+,d2
        move.l  d0,(a1)+
        move.l  d2,(a1)+
        move.l  (a1),d0
        move.l  4(a1),d2
        move.l  d0,d1
        move.l  d2,d3
        lsr.l   #2,d1
        lsr.l   #2,d3
        and.l   #$39E739E7,d1
        and.l   #$39E739E7,d3
        sub.l   d1,d0
        sub.l   d3,d2
        add.l   (a0)+,d0
        add.l   (a0)+,d2
        move.l  d0,(a1)+
        move.l  d2,(a1)+
