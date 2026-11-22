; seq/HUR-073.i - Hurrican DkABF 1-quad dbra (64 px = 16 quads, hoisted e0=0, no DkMasks load)
; Uses: d0..d2, a1, a4, e0, e6. MUST NOT touch: d5, d6, d7, a5, a6, a7
        ifnd    HUR073_MAC
HUR073_MAC equ 1
HUR073_BODY macro
        moveq   #64,d2
        move.l  d2,d0
        lsr.l   #2,d0
        subq.w  #1,d0
        bmi.s   .t\@
.l\@:   store   e0,(a1)+
        dbra    d0,.l\@
.t\@:   moveq   #3,d1
        and.l   d2,d1
        beq.s   .x\@
        load    (a4,d1.l*8),e6
        storeilm e0,e6,(a1)
.x\@:   lea     -128(a1),a1
        endm
        endif
        HUR073_BODY
