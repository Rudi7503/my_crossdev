; seq/BLT-049.i - 16-Bit RGB565 Alpha-Blending (PMULA + Memory-VEA) + Parallel ALU/EA Ops (4 pixels)
; Demonstrates zero-cost ALU/EA execution alongside AMMX pipeline
; Uses: e2, e4, e5, d0..d3, a0, a1, a2
; MUST NOT touch: d5, d6, d7, a5, a6, a7
        load    (a1),e2
        add.l   d0,d1
        unpack1632 e2,e4:e5
        eor.l   d2,d3
        pmula   (a0)+,e4,e4
        addq.l  #8,a2
        pmula   (a0)+,e5,e5
        subq.l  #1,d4
        pack3216 e4,e5,e2
        store   e2,(a1)+
