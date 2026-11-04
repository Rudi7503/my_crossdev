; seq/STR-001.i - AMMX SIMD STRLEN (8 chars/loop with PCMPEQB)
; Uses: e0, e1, e2, d0, a0
; MUST NOT touch: d5, d6, d7, a5, a6, a7
; e1 is preloaded with 0 (null-terminator match)
        load    (a0)+,e0
        pcmpeqb e1,e0,e2
        move.l  e2,d0
