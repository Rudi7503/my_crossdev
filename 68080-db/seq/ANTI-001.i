; seq/ANTI-001.i - ANTI-PATTERN: NOP between MOVE and dependent ADD
; Uses: d2, d3, d4
; MUST NOT touch: d5, d6, d7, a5, a6, a7
; NOTE: this sequence measures the anti-pattern on purpose.
        move.l  d2,d3
        nop
        add.l   d4,d3
