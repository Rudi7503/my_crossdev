; seq/STR-003.i - AMMX SIMD Base64 24-bit to 32-bit (4x 6-bit unpack step)
; Uses: e0, e1, e2, e3, e4, d0, d1, a0, a1
; MUST NOT touch: d5, d6, d7, a5, a6, a7
; e3 = mask $3F3F3F3F, d0 = 2, d1 = 4
        load    (a0)+,e0
        vperm   #$01122330,e0,e0,e1
        lsrq    d0,e1,e2
        pand    e3,e2,e2
        store   e2,(a1)+
