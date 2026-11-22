; seq/HUR-074.i - Hurrican DkABF 4-quad (32-byte) burst + dbra (64 px = 4 x 4-quad, hoisted e0=0)
; Uses: d0..d2, a1, a4, e0, e6. MUST NOT touch: d5, d6, d7, a5, a6, a7
        ifnd    HUR074_MAC
HUR074_MAC equ 1
HUR074_BODY macro
        moveq   #64,d2
        move.l  d2,d0
        lsr.l   #4,d0                   ; 16-px (32-byte) blocks
        subq.w  #1,d0
        bmi.s   .q1\@
.b4\@:  store   e0,(a1)+
        store   e0,(a1)+
        store   e0,(a1)+
        store   e0,(a1)+
        dbra    d0,.b4\@
.q1\@:  moveq   #12,d0
        and.l   d2,d0
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
        HUR074_BODY
