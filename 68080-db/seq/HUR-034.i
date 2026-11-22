; seq/HUR-034.i - Hurrican Boxes original segment step (2x FIXINT + movem spill + 2x lsl.l #5)
; Uses: d0, d1, d2, d3, d4, a0, a1
; MUST NOT touch: d5, d6, d7, a5, a6, a7
        move.l  d4,d0
        swap    d0
        ext.l   d0
        add.l   #16,d0
        move.l  a0,d1
        swap    d1
        ext.l   d1
        add.l   #16,d1
        movem.l d2-d3,-(sp)
        movem.l (sp)+,d2-d3
        move.l  d4,d0
        swap    d0
        ext.l   d0
        move.l  d0,(a1)+
        move.l  a0,d0
        swap    d0
        ext.l   d0
        move.l  d0,(a1)+
        move.l  d2,d0
        lsl.l   #5,d0
        add.l   d0,d4
        move.l  d3,d0
        lsl.l   #5,d0
        add.l   d0,a0
