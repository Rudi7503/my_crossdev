; seq/ANTI-005.i - ANTI-PATTERN: unaligned long writes
; Uses: d2, d3, a0
; MUST NOT touch: d5, d6, d7, a5, a6, a7
; NOTE: this sequence measures the anti-pattern on purpose.
        move.l  d2,1(a0)
        move.l  d3,5(a0)
