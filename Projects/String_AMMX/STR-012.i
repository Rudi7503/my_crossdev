; seq/STR-012.i - AMMX 16-Character SIMD MEMCHR (2x Unrolled Dual-Quadword)
; Uses: e0, e1, e2, e3, e4, e5, d0, a0
; MUST NOT touch: d5, d6, d7, a5, a6, a7
; e1 is preloaded with replicated target character
        load    (a0)+,e0
        load    (a0)+,e3
        pcmpeqb e1,e0,e2
        pcmpeqb e1,e3,e4
        por     e2,e4,e5
        move.l  e5,d0
