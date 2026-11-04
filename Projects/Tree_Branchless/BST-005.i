; seq/BST-005.i - Ultra-Compact 4-Instruction BST Step with SUBX and EA Scaling
; Uses: d0, d1, d2, a0
; MUST NOT touch: d5, d6, d7, a5, a6, a7
; a0 = node pointer, d1 = search key
; sub.l sets X=1 if d0 < d1 (right) or X=0 if d0 >= d1 (left)
; subx.w produces d2.w = -X (-1 or 0) regardless of prior d2 content
; 8(a0, d2.w*4) computes 8 + (-1)*4 = 4 (left) or 8 + 0*4 = 8 (right)
        move.l  (a0),d0         ; Lade Schlüssel
        sub.l   d1,d0           ; Vergleiche Ziel d1 mit Schlüssel d0
        subx.w  d2,d2           ; d2.w = -X (-1 oder 0) in 1 Takt!
        move.l  8(a0,d2.w*4),a0 ; Hardware EA-Scaler: 8+(-4)=4 oder 8+0=8
