; seq/ANTI-004.i - ANTI-PATTERN: two multicycle instructions on pipe 1
; Uses: d0-d3, a0
; MUST NOT touch: d5, d6, d7, a5, a6, a7
; NOTE: this sequence measures the anti-pattern on purpose.
        movem.l d0-d3,(a0)
        mulu.w  d0,d1
