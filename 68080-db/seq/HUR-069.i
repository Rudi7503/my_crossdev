; seq/HUR-069.i - Hurrican DkA4 original (32 px = 8 quads: DkAPre 3-load + 1-quad subq/bne)
; Uses: d0..d4, a0, a1, a4, e1..e6. MUST NOT touch: d5, d6, d7, a5, a6, a7
        ifnd    HUR069_MAC
HUR069_MAC equ 1
HUR069_BODY macro
        moveq   #32,d2
        tst.l   d2
        ble.s   .x\@
        ; inline DkAPre cost (without bsr/rts!)
        load    (a4),e1
        load    8(a4),e2
        load    16(a4),e3
        moveq   #1,d3
        moveq   #2,d4
        move.l  d2,d0
        lsr.l   #2,d0
        moveq   #3,d1
        and.l   d2,d1
        lea     (a4),a0
        tst.l   d0
        beq.s   .t\@
.l\@:   load    (a1),e4
        lsrq    d3,e4,e4
        pand    e1,e4,e4
        store   e4,(a1)+
        subq.l  #1,d0
        bne.s   .l\@
.t\@:   tst.l   d1
        beq.s   .x\@
        load    (a1),e4
        lsrq    d3,e4,e4
        pand    e1,e4,e4
        load    (a0,d1.l*8),e6
        storeilm e4,e6,(a1)
.x\@:   lea     -64(a1),a1
        endm
        endif
        HUR069_BODY
