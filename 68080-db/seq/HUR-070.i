; seq/HUR-070.i - Hurrican DkA4F optimized (32 px = 4 x 2-quad interleaved, hoisted e1/d3, dbra)
; Uses: d0..d3, a1, a4, e1, e4..e6. MUST NOT touch: d5, d6, d7, a5, a6, a7
        ifnd    HUR070_MAC
HUR070_MAC equ 1
HUR070_BODY macro
        moveq   #32,d2
        move.l  d2,d0
        lsr.l   #3,d0                   ; 8-px (2-quad) blocks
        subq.w  #1,d0
        bmi.s   .q1\@
.l2\@:  load    (a1),e4
        load    8(a1),e5
        lsrq    d3,e4,e4
        lsrq    d3,e5,e5
        pand    e1,e4,e4
        pand    e1,e5,e5
        store   e4,(a1)+
        store   e5,(a1)+
        dbra    d0,.l2\@
.q1\@:  btst    #2,d2
        beq.s   .t\@
        load    (a1),e4
        lsrq    d3,e4,e4
        pand    e1,e4,e4
        store   e4,(a1)+
.t\@:   moveq   #3,d1
        and.l   d2,d1
        beq.s   .x\@
        load    (a1),e4
        lsrq    d3,e4,e4
        pand    e1,e4,e4
        load    (a4,d1.l*8),e6
        storeilm e4,e6,(a1)
.x\@:   lea     -64(a1),a1
        endm
        endif
        HUR070_BODY
