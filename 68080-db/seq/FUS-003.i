; seq/FUS-003.i - MOVEQ + OR.L (fused immediate/register op)
; Uses: d0, d1
; MUST NOT touch: d5, d6, d7, a5, a6, a7
        moveq   #$12,d0
        or.l    d1,d0
