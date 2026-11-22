; seq/HUR-009.i - Hurrican _BlitKeyRed Mode 0 original 1-pixel Skeletor flash
; Uses: d0, d1, d2, d3, d4, a0, a1
; MUST NOT touch: d5, d6, d7, a5, a6, a7
        moveq   #0,d0
        move.w  (a0)+,d0
        cmp.w   #$F81F,d0
        beq.s   *+60
        move.l  d0,d1
        and.l   #31,d1
        mulu.l  d3,d1
        lsr.l   #8,d1
        cmp.l   #31,d1
        ble.s   *+4
        moveq   #31,d1
        move.l  d1,d4
        move.l  d0,d1
        lsr.l   #5,d1
        and.l   #63,d1
        mulu.l  d3,d1
        lsr.l   #8,d1
        cmp.l   #63,d1
        ble.s   *+4
        moveq   #63,d1
        lsl.l   #5,d1
        or.l    d1,d4
        lsr.l   #8,d0
        lsr.l   #3,d0
        mulu.l  #3,d0
        cmp.l   #31,d0
        ble.s   *+4
        moveq   #31,d0
        lsl.l   #8,d0
        lsl.l   #3,d0
        or.l    d4,d0
        move.w  d0,(a1)
        addq.l  #2,a1
