; seq/ANTI-002.i - ANTI-PATTERN: 68060-style independent instruction breaks fusion
; Uses: d0, d1, d2, d3, d4
; MUST NOT touch: d5, d6, d7, a5, a6, a7
; NOTE: this sequence measures the anti-pattern on purpose.
        move.l  d2,d3
        move.l  d0,d1
        add.l   d4,d3
