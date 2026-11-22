; seq/HUR-046.i - Hurrican _BlitAdd original (4 rows x 20 px, d6=5, d7=0, blit.s L405-456)
; Uses: d0..d4, a0..a4 (a4 = TailMask base). MUST NOT touch: d5, d6, d7, a5, a6, a7
        ifnd    HUR046_MAC
HUR046_MAC equ 1
HUR046_BODY macro
        moveq   #5,d2                   ; d6 = 5 quads (20 px)
        moveq   #0,d3                   ; d7 = 0 tail px
        moveq   #4,d0                   ; 4 rows
.row\@:
        move.l  a4,a2                   ; lea TailMask(pc),a2
        load    (a2,d3.l*8),e1          ; reload TailMask every row
        move.l  a0,a2
        move.l  a1,a3
        move.l  d2,d4
        beq.s   .tail\@
        move.l  d4,d1
        lsr.l   #1,d1
        beq.s   .a1\@
.a2\@:  load    (a2)+,e2
        load    (a2)+,e10
        load    (a3),e3
        load    8(a3),e11
        unpack1632 e2,e4:e5
        unpack1632 e10,e12:e13
        unpack1632 e3,e6:e7
        unpack1632 e11,e14:e15
        paddusb e4,e6,e6
        paddusb e12,e14,e14
        paddusb e5,e7,e7
        paddusb e13,e15,e15
        pack3216 e6,e7,e3
        pack3216 e14,e15,e11
        store   e3,(a3)+
        store   e11,(a3)+
        subq.l  #1,d1
        bne.s   .a2\@
.a1\@:  btst    #0,d4
        beq.s   .tail\@
        load    (a2)+,e2
        load    (a3),e3
        unpack1632 e2,e4:e5
        unpack1632 e3,e6:e7
        paddusb e4,e6,e6
        paddusb e5,e7,e7
        pack3216 e6,e7,e3
        store   e3,(a3)+
.tail\@:
        tst.l   d3
        beq.s   .next\@
        load    (a2),e2
        load    (a3),e3
        unpack1632 e2,e4:e5
        unpack1632 e3,e6:e7
        paddusb e4,e6,e6
        paddusb e5,e7,e7
        pack3216 e6,e7,e3
        storeilm e3,e1,(a3)
.next\@:
        lea     40(a0),a0
        lea     1280(a1),a1
        subq.l  #1,d0
        bne.s   .row\@
        endm
        endif
        HUR046_BODY
