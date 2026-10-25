; seq/ANTI-006.i - ANTI-PATTERN: TOUCH immediately before the load
; Uses: d0, d1, a0
; MUST NOT touch: d5, d6, d7, a5, a6, a7
; NOTE: this sequence measures the anti-pattern on purpose.
        moveq   #0,d0
        touch   (a0,d0.w)
        move.l  (a0),d1
