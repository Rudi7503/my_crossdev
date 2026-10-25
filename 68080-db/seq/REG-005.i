; seq/REG-005.i - do not write 3-operand source syntax; let the core fuse
; Uses: d2, d3, d4
; MUST NOT touch: d5, d6, d7, a5, a6, a7
        move.l  d2,d3
        add.l   d4,d3
