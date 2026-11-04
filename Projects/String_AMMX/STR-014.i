; seq/STR-014.i - AMMX SIMD JSON 4-Token Parallel Scanner
; Uses: e0, e1, e2, e3, e4, e5, e6, e7, a0, a1
; MUST NOT touch: d5, d6, d7, a5, a6, a7
; e1='"', e2=':', e3='{', e4='}'
        load    (a0)+,e0
        pcmpeqb e1,e0,e5
        pcmpeqb e2,e0,e6
        pcmpeqb e3,e0,e7
        por     e5,e6,e5
        pcmpeqb e4,e0,e6
        por     e7,e6,e6
        por     e5,e6,e5
        store   e5,(a1)+
