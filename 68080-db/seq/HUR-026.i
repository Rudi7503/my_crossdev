; seq/HUR-026.i - Hurrican _BlitKeyRed Mode 1 original 1-pixel additive red (blit.s L4055-4073)
; Uses: d0, d1, d2, d3, a0, a1
; MUST NOT touch: d5, d6, d7, a5, a6, a7
        moveq   #0,d0
        move.w  (a0)+,d0
        cmp.w   #$F81F,d0
        beq.s   *+38
        lsr.l   #8,d0
        lsr.l   #3,d0
        mulu.l  d3,d0
        lsr.l   #8,d0
        moveq   #0,d1
        move.w  (a1),d1
        move.l  d1,d2
        lsr.l   #8,d2
        lsr.l   #3,d2
        add.l   d0,d2
        cmp.l   #31,d2
        ble.s   *+4
        moveq   #31,d2
        lsl.l   #8,d2
        lsl.l   #3,d2
        and.l   #$07FF,d1
        or.l    d2,d1
        move.w  d1,(a1)
        addq.l  #2,a1
