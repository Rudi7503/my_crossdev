; seq/BND-001.i - dependent MOVEQ + ADD with two destinations (bonding)
; Uses: d1, d2
; MUST NOT touch: d5, d6, d7, a5, a6, a7
        move.l  #$12,d1
        add.l   d1,d2
