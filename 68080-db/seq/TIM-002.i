; seq/TIM-002.i - produce an address with LEA, not ALU result -> EA
; Uses: a0, a1
; MUST NOT touch: d5, d6, d7, a5, a6, a7
        lea     8(a0),a1
