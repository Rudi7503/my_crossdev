; seq/BST-006.i - Eytzinger Implicit Array-Tree Step (Zero Pointers, ADDX Step)
; Uses: d0, d1, d2, a0
; MUST NOT touch: d5, d6, d7, a5, a6, a7
; a0 = base array pointer, d1 = search key, d2 = current index k
; move.l loads key at A[k]
; sub.l sets X=1 if d0 < d1 (right child) or X=0 (left child)
; addx.l updates d2 = 2*k + X in 1 single instruction!
        move.l  (a0,d2.l*4),d0  ; Lade Schlüssel A[k] aus flachem Array
        sub.l   d1,d0           ; Vergleiche Ziel d1 mit Schlüssel d0
        addx.l  d2,d2           ; d2 = 2*k + X (1 Takt für komplette Verzweigung!)
