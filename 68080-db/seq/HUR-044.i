; seq/HUR-044.i - Hurrican _BlitKey original (4 rows x 20 px, d6=5, d7=0, blit.s L361-393)
; Uses: d0..d4, a0..a4 (a4 = KeyQ/TailMask base). MUST NOT touch: d5, d6, d7, a5, a6, a7
        ifnd    HUR044_MAC
HUR044_MAC equ 1
HUR044_BODY macro
        moveq   #5,d2                   ; d6 = 5 quads (20 px)
        moveq   #0,d3                   ; d7 = 0 tail px
        moveq   #4,d0                   ; 4 rows
.row\@:
        load    (a4),e0                 ; load KeyQ,e0
        move.l  a4,a2                   ; lea TailMask(pc),a2
        load    8(a2,d3.l*8),e1         ; reload TailMask every row
        move.l  a0,a2
        move.l  a1,a3
        move.l  d2,d4
        beq.s   .tail\@
        move.l  d4,d1
        lsr.l   #1,d1
        beq.s   .k1\@
.k2\@:  load    (a2)+,e2
        load    (a2)+,e4
        pcmpeqw e0,e2,e3
        pcmpeqw e0,e4,e5
        storeilm e2,e3,(a3)+
        storeilm e4,e5,(a3)+
        subq.l  #1,d1
        bne.s   .k2\@
.k1\@:  btst    #0,d4
        beq.s   .tail\@
        load    (a2)+,e2
        pcmpeqw e0,e2,e3
        storeilm e2,e3,(a3)+
.tail\@:
        tst.l   d3
        beq.s   .next\@
        load    (a2),e2
        pcmpeqw e0,e2,e3
        por     e1,e3,e3
        storeilm e2,e3,(a3)
.next\@:
        lea     40(a0),a0
        lea     1280(a1),a1
        subq.l  #1,d0
        bne.s   .row\@
        endm
        endif
        HUR044_BODY
