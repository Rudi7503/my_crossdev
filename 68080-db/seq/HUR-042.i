; seq/HUR-042.i - Hurrican _BlitCopy original (4 rows x 20 px, d6=5, d7=0, blit.s L188-220)
; Uses: d0..d4, a0..a4 (a4 = TailMask base). MUST NOT touch: d5, d6, d7, a5, a6, a7
        ifnd    HUR042_MAC
HUR042_MAC equ 1
HUR042_BODY macro
        moveq   #5,d2                   ; d6 = 5 quads (20 px)
        moveq   #0,d3                   ; d7 = 0 tail px
        moveq   #4,d0                   ; 4 rows
.row\@:
        move.l  a4,a2                   ; lea TailMask(pc),a2
        load    (a2,d3.l*8),e1          ; reload TailMask every row (even d7=0)
        move.l  a0,a2
        move.l  a1,a3
        move.l  d2,d1
        lsr.l   #2,d1
        beq.s   .r1\@
.q4\@:  load    (a2)+,e2
        load    (a2)+,e3
        load    (a2)+,e4
        load    (a2)+,e5
        store   e2,(a3)+
        store   e3,(a3)+
        store   e4,(a3)+
        store   e5,(a3)+
        subq.l  #1,d1
        bne.s   .q4\@
.r1\@:  move.l  d2,d4
        and.l   #3,d4
        beq.s   .tail\@
.qd\@:  load    (a2)+,e2
        store   e2,(a3)+
        subq.l  #1,d4
        bne.s   .qd\@
.tail\@:
        tst.l   d3
        beq.s   .next\@
        load    (a2),e2
        storeilm e2,e1,(a3)
.next\@:
        lea     40(a0),a0
        lea     1280(a1),a1
        subq.l  #1,d0
        bne.s   .row\@
        endm
        endif
        HUR042_BODY
