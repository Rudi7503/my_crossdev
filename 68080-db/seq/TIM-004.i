; seq/TIM-004.i - save five registers with MOVEM, not five MOVE.L
; Uses: d0-d4, a0
; MUST NOT touch: d5, d6, d7, a5, a6, a7
        movem.l d0-d4,(a0)
