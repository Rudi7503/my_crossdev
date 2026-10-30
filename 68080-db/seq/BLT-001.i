; seq/BLT-001.i - 256-bit aligned rectangle copy with two move16
; Uses: a0, a1
; MUST NOT touch: d5, d6, d7, a5, a6, a7
        move16  (a0)+,(a1)+
        move16  (a0)+,(a1)+
