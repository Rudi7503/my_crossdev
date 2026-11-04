; seq/STR-004.i - AMMX SIMD JSON Structural Token Scanner (Dual Matcher)
; Uses: e0, e1, e2, e3, e4, e5, a0, a1
; MUST NOT touch: d5, d6, d7, a5, a6, a7
; e1 = replicated '"', e2 = replicated ':'
        load    (a0)+,e0
        pcmpeqb e1,e0,e3
        pcmpeqb e2,e0,e4
        por     e3,e4,e5
        store   e5,(a1)+
