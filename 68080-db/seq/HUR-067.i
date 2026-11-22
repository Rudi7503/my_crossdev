; seq/HUR-067.i - Hurrican DkAB original (64 px = 16 quads black fill: load zero + 1-quad subq/bne)
; Uses: d0..d2, a1, a4, e4. MUST NOT touch: d5, d6, d7, a5, a6, a7
        ifnd    HUR067_MAC
HUR067_MAC equ 1
HUR067_BODY macro
        moveq   #64,d2
        tst.l   d2
        ble.s   .x\@
        load    24(a4),e4
        move.l  d2,d0
        lsr.l   #2,d0
        moveq   #3,d1
        and.l   d2,d1
        tst.l   d0
        beq.s   .t\@
.l\@:   store   e4,(a1)+
        subq.l  #1,d0
        bne.s   .l\@
.t\@:   tst.l   d1
        beq.s   .x\@
        load    (a4,d1.l*8),e6
        storeilm e4,e6,(a1)
.x\@:   lea     -128(a1),a1
        endm
        endif
        HUR067_BODY
