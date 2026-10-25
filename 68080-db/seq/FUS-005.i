; seq/FUS-005.i - two CLR.L (An)+ -> one quad clear
; Uses: a0
; MUST NOT touch: d5, d6, d7, a5, a6, a7
        clr.l   (a0)+
        clr.l   (a0)+
