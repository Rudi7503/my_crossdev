; seq/BND-005.i - three independent ALU instructions (3-pipe cores)
; Uses: d0, d1, d2, d3, d4
; MUST NOT touch: d5, d6, d7, a5, a6, a7
        add.l   d0,d1
        add.l   d2,d3
        eor.l   d4,d4
