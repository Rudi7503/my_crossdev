; seq/HUR-022.i - Hurrican DrawWater .row original per-row CamX/20 + PBlock*PMapW*2 mulu
; Uses: d0, d1, d2, d3, d4, a0, a1, a2, a3
; MUST NOT touch: d5, d6, d7, a5, a6, a7
        move.l  (a0),d2
        divu.l  #20,d2
        move.l  d2,d3
        add.l   #33,d3
        cmp.l   d4,d3
        ble.s   *+4
        move.l  d4,d3
        move.l  a1,a2
        move.l  d1,d0
        mulu.l  d4,d0
        add.l   d0,d0
        add.l   d0,a2
        addq.l  #1,d1
        move.l  a2,(a3)+
