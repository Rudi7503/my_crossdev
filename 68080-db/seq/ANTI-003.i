; seq/ANTI-003.i - ANTI-PATTERN: word updates instead of long updates
; Uses: d2, d3, d4
; MUST NOT touch: d5, d6, d7, a5, a6, a7
; NOTE: this sequence measures the anti-pattern on purpose.
        move.w  d2,d3
        add.w   d4,d3
