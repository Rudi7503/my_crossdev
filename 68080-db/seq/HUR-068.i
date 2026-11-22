; seq/HUR-068.i - Hurrican DkABF optimized (64 px = 2 x 8-quad 64-byte DDR3 burst, hoisted e0=0, dbra)
; Uses: d0..d2, a1, a4, e0. MUST NOT touch: d5, d6, d7, a5, a6, a7
        ifnd    HUR068_MAC
HUR068_MAC equ 1
HUR068_BODY macro
        moveq   #64,d2
        move.l  d2,d0
        lsr.l   #5,d0                   ; 32-px (64-byte) blocks
        subq.w  #1,d0
        bmi.s   .q1\@
.b8\@:  store   e0,(a1)+
        store   e0,(a1)+
        store   e0,(a1)+
        store   e0,(a1)+
        store   e0,(a1)+
        store   e0,(a1)+
        store   e0,(a1)+
        store   e0,(a1)+
        dbra    d0,.b8\@
.q1\@:  move.l  d2,d0
        and.l   #28,d0
        lsr.l   #2,d0
        subq.w  #1,d0
        bmi.s   .t\@
.l1\@:  store   e0,(a1)+
        dbra    d0,.l1\@
.t\@:   moveq   #3,d1
        and.l   d2,d1
        beq.s   .x\@
        load    (a4,d1.l*8),e6
        storeilm e0,e6,(a1)
.x\@:   lea     -128(a1),a1
        endm
        endif
        HUR068_BODY
