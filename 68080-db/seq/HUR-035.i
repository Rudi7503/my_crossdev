; seq/HUR-035.i - Hurrican Boxes optimized segment step (hoisted 32*dir, single FIXINT, no spill)
; Uses: d0, d1, d2, d3, d4, a0, a1
; MUST NOT touch: d5, d6, d7, a5, a6, a7
        move.l  d4,d0
        swap    d0
        ext.l   d0
        move.l  a0,d1
        swap    d1
        ext.l   d1
        move.l  d0,(a1)+
        move.l  d1,(a1)+
        add.l   #16,d0
        add.l   #16,d1
        add.l   d2,d4
        add.l   d3,a0
