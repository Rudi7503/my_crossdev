; seq/BST-002.i - Optimized Scalar BST Step with Base Displacement (No ADDI)
; Uses: d0, d1, d2, a0
; MUST NOT touch: d5, d6, d7, a5, a6, a7
; a0 = node pointer, d1 = search key
        move.l  (a0),d0         ; Lade Schlüssel
        cmp.l   d1,d0           ; Vergleiche
        slt     d2              ; d2.b = $FF oder $00
        andi.w  #4,d2           ; d2.w = 4 oder 0
        move.l  4(a0,d2.w),a0   ; 4(a0, 0)=links (Offset 4) oder 4(a0, 4)=rechts (Offset 8)
