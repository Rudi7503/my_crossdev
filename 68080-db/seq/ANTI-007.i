; seq/ANTI-007.i - ANTI-PATTERN: blind loop unrolling / flattened code
; Uses: d0
; MUST NOT touch: d5, d6, d7, a5, a6, a7
; NOTE: this sequence measures the anti-pattern on purpose. 16 unrolled
; independent increments represent a flattened, non-looping instruction stream.
        addq.l  #1,d0
        addq.l  #1,d0
        addq.l  #1,d0
        addq.l  #1,d0
        addq.l  #1,d0
        addq.l  #1,d0
        addq.l  #1,d0
        addq.l  #1,d0
        addq.l  #1,d0
        addq.l  #1,d0
        addq.l  #1,d0
        addq.l  #1,d0
        addq.l  #1,d0
        addq.l  #1,d0
        addq.l  #1,d0
        addq.l  #1,d0
