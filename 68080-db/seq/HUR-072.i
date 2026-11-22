; seq/HUR-072.i - Hurrican DkA5F optimized (32 px = 4 x 2-quad interleaved, hoisted e1/e3/d3/d4, dbra)
; Uses: d0..d4, a1, a4, e1, e3..e9. MUST NOT touch: d5, d6, d7, a5, a6, a7
        ifnd    HUR072_MAC
HUR072_MAC equ 1
HUR072_BODY macro
        moveq   #32,d2
        move.l  d2,d0
        lsr.l   #3,d0                   ; 8-px (2-quad) blocks
        subq.w  #1,d0
        bmi.s   .q1\@
.l2\@:  load    (a1),e4
        load    8(a1),e7
        lsrq    d3,e4,e5
        lsrq    d3,e7,e8
        pand    e1,e5,e5
        pand    e1,e8,e8
        lsrq    d4,e4,e4
        lsrq    d4,e7,e7
        pand    e3,e4,e4
        pand    e3,e7,e7
        paddw   e5,e4,e4
        paddw   e8,e7,e7
        store   e4,(a1)+
        store   e7,(a1)+
        dbra    d0,.l2\@
.q1\@:  btst    #2,d2
        beq.s   .t\@
        load    (a1),e4
        lsrq    d3,e4,e5
        pand    e1,e5,e5
        lsrq    d4,e4,e4
        pand    e3,e4,e4
        paddw   e5,e4,e4
        store   e4,(a1)+
.t\@:   moveq   #3,d1
        and.l   d2,d1
        beq.s   .x\@
        load    (a1),e4
        lsrq    d3,e4,e5
        pand    e1,e5,e5
        lsrq    d4,e4,e4
        pand    e3,e4,e4
        paddw   e5,e4,e4
        load    (a4,d1.l*8),e6
        storeilm e4,e6,(a1)
.x\@:   lea     -64(a1),a1
        endm
        endif
        HUR072_BODY
