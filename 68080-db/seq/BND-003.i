; seq/BND-003.i - two independent ALU instructions (superscalar)
; Uses: d0, d1, d2, d3
; MUST NOT touch: d5, d6, d7, a5, a6, a7
        add.l   d0,d1
        add.l   d2,d3
