; seq/STR-002.i - AMMX SIMD MEMCHR (8 chars/loop broadcast match with PCMPEQB)
; Uses: e0, e1, e2, d0, a0
; MUST NOT touch: d5, d6, d7, a5, a6, a7
; e1 is preloaded with replicated target character
        load    (a0)+,e0
        pcmpeqb e1,e0,e2
        move.l  e2,d0
