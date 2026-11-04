; seq/BST-001.i - Branchless BST Step with SLT + ANDI + ADDI (User Proposal)
; Uses: d0, d1, d2, a0
; MUST NOT touch: d5, d6, d7, a5, a6, a7
; a0 = node pointer, d1 = search key
        move.l  (a0),d0         ; Lade Knoten-Schlüssel
        cmp.l   d1,d0           ; Vergleiche Ziel mit Schlüssel
        slt     d2              ; d2 = $FF wenn d1 < d0, sonst $00
        extb.l  d2              ; Byte auf 32-Bit erweitern ($FFFFFFFF bzw. 0)
        andi.l  #4,d2           ; d2 = 4 (links) oder 0 (rechts)
        addi.l  #4,d2           ; d2 = 8 (rechts) oder 4 (links)
        move.l  0(a0,d2.l),a0   ; Lade nächsten Kindknoten über berechneten Offset
