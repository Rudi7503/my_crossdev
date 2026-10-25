; seq/REG-001.i - E register as 64-bit storage, integer path stays in D
; Uses: d0, d2, e0
; MUST NOT touch: d5, d6, d7, a5, a6, a7
        move.l  d0,e0
        move.l  e0,d2
