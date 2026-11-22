; seq/HUR-036.i - Hurrican _WeaponsDraw original FIXINT (move.l+swap+ext.l) + WAnim per shot
; Uses: d0, d1, d2, a0, a1, a2
; MUST NOT touch: d5, d6, d7, a5, a6, a7
        move.l  (a2),d0
        swap    d0
        ext.l   d0
        move.l  (a0)+,d1
        swap    d1
        ext.l   d1
        sub.l   d2,d1
        move.l  (a0)+,d3
        swap    d3
        ext.l   d3
        sub.l   d4,d3
        add.l   d0,d1
        add.l   d3,d1
        move.l  d1,(a1)+
