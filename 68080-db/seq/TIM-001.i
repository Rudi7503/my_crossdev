; seq/TIM-001.i - DIV by power of two versus logical shift
; Uses: d0
; MUST NOT touch: d5, d6, d7, a5, a6, a7
        moveq   #8,d0
        lsr.l   #3,d0
