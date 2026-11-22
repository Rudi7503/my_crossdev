; seq/HUR-037.i - Hurrican _WeaponsDraw optimized move.w+ext.l + hoisted WAnim
; Uses: d0, d1, d2, d3, d4, a0, a1
; MUST NOT touch: d5, d6, d7, a5, a6, a7
        move.w  (a0),d1
        ext.l   d1
        sub.l   d2,d1
        move.w  4(a0),d3
        ext.l   d3
        sub.l   d4,d3
        addq.l  #8,a0
        add.l   d0,d1
        add.l   d3,d1
        move.l  d1,(a1)+
