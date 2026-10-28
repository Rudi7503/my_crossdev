; seq/AMX-010-opt.i - 2x unrolled PACKSSWB emulation (16 words to 16 bytes)
; Uses: e0, e1, e2, e3, e4, e5, a0, a1
; MUST NOT touch: d5, d6, d7, a5, a6, a7
; Pre-condition: e2 = $0080008000800080, e3 = $8080808080808080
        paddw   (a0)+,e2,e0
        paddw   (a0)+,e2,e1
        paddw   (a0)+,e2,e4
        paddw   (a0)+,e2,e5
        packuswb e0,e1,e0
        packuswb e4,e5,e4
        psubb   e3,e0,e0
        psubb   e3,e4,e4
        store   e0,(a1)+
        store   e4,(a1)+
