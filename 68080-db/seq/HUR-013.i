; seq/HUR-013.i - Hurrican GetTinted original 1-pixel 3x LUT lookup (tilemap.s L1023-1042)
; Uses: d0, d1, d3, a0, a1, a2
; MUST NOT touch: d5, d6, d7, a5, a6, a7
        move.w  (a0)+,d0
        cmp.w   #$F81F,d0
        beq.s   *+34
        moveq   #0,d1
        move.w  d0,d1
        lsr.w   #8,d1
        lsr.w   #3,d1
        move.w  (a2,d1.l*2),d3
        move.w  d0,d1
        lsr.w   #5,d1
        and.w   #63,d1
        or.w    64(a2,d1.l*2),d3
        and.l   #31,d0
        or.w    192(a2,d0.l*2),d3
        cmp.w   #$F81F,d3
        bne.s   *+4
        subq.w  #1,d3
        move.w  d3,d0
        move.w  d0,(a1)+
