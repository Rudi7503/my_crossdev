; seq/AMX-010.i - PACKSSWB emulation (signed saturated pack 8 words to 8 bytes)
; Uses: e0, e1, e2, e3, a0, a1
; MUST NOT touch: d5, d6, d7, a5, a6, a7
; Pre-condition: e2 = $0080008000800080, e3 = $8080808080808080
        paddw   (a0)+,e2,e0
        paddw   (a0)+,e2,e1
        packuswb e0,e1,e0
        psubb   e3,e0,e0
        store   e0,(a1)+
