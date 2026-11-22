; seq/HUR-028.i - Hurrican _BlitAlpha .l1p original 2-pixel scalar avg-chain (blit.s L1009-1040)
; Uses: d0, d1, d2, d3, d4, a0, a1
; MUST NOT touch: d5, d6, d7, a5, a6, a7
        move.l  (a0)+,d0
        move.l  (a1),d1
        cmp.w   d4,d0
        bne.s   *+4
        move.w  d1,d0
        swap    d0
        swap    d1
        cmp.w   d4,d0
        bne.s   *+4
        move.w  d1,d0
        swap    d0
        swap    d1
        move.l  d1,d2
        move.l  d2,d3
        eor.l   d0,d3
        and.l   #$F7DEF7DE,d3
        lsr.l   #1,d3
        and.l   d0,d2
        add.l   d3,d2
        move.l  d2,d3
        eor.l   d1,d3
        and.l   #$F7DEF7DE,d3
        lsr.l   #1,d3
        and.l   d1,d2
        add.l   d3,d2
        move.l  d2,d3
        eor.l   d1,d3
        and.l   #$F7DEF7DE,d3
        lsr.l   #1,d3
        and.l   d1,d2
        add.l   d3,d2
        move.l  d2,(a1)+
