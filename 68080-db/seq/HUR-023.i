; seq/HUR-023.i - Hurrican DrawWater .row hoisted CamX/20 + incremental row pointer add
; Uses: d1, d4, a2, a3
; MUST NOT touch: d5, d6, d7, a5, a6, a7
        add.l   d4,a2
        addq.l  #1,d1
        move.l  a2,(a3)+
