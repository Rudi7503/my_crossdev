; seq/HUR-003.i - Hurrican _DrawTextTint precomputed channel LUTs (a2=R_lut, a3=G_lut, a4=B_lut, bit-exact)
; Uses: d0, d2, d3, d4, a0, a1, a2, a3, a4
; MUST NOT touch: d5, d6, d7, a5, a6, a7
        move.w  (a0)+,d2
        moveq   #0,d3
        move.w  d2,d3
        lsr.w   #8,d3
        lsr.w   #3,d3
        move.w  (a2,d3.w*2),d3
        moveq   #0,d4
        move.w  d2,d4
        lsr.w   #5,d4
        and.w   #63,d4
        or.w    (a3,d4.w*2),d3
        and.w   #31,d2
        or.w    (a4,d2.w*2),d3
        move.w  d3,(a1)+
