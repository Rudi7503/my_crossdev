; seq/BST-004.i - AMMX 4-Way SIMD Tree Node (Parallel 4-Key Compare in 1 Cycle)
; Uses: e0, e1, e2, d0, a0
; MUST NOT touch: d5, d6, d7, a5, a6, a7
; e0 = search key replicated, a0 = node with 4 keys
        load    (a0),e1         ; Lade alle 4 Knoten-Schlüssel auf einmal (64 Bit)
        pcmpgtw e0,e1,e2        ; Vergleiche alle 4 Schlüssel simultan in 1 Takt!
        move.l  e2,d0           ; d0 = Treffer-Maske für direkten Kind-Branch
