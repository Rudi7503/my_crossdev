; seq/BST-003.i - Dual-EA Parallel Load + Bitwise Selection (Zero ALU-to-EA Stall)
; Uses: d0, d1, d2, d3, d4, a0
; MUST NOT touch: d5, d6, d7, a5, a6, a7
        move.l  (a0),d0         ; Lade Schlüssel
        move.l  4(a0),d2        ; Lade linken Zeiger parallel (statischer Offset)
        move.l  8(a0),d3        ; Lade rechten Zeiger parallel (statischer Offset)
        cmp.l   d1,d0           ; Vergleiche
        slt     d4              ; Maske
        extb.l  d4              ; $FFFFFFFF oder $00000000
        and.l   d4,d2           ; d2 = left & mask
        not.l   d4
        and.l   d4,d3           ; d3 = right & ~mask
        or.l    d2,d3           ; d3 = gewählter Zeiger
        move.l  d3,a0
