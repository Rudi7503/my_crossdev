; seq/HUR-045.i - Hurrican _BlitKey optimized (4 rows x 20 px, d6=5, d7=0: hoisted counts, direct a0/a1 stride, dbra, TailMask only on d7!=0)
; Uses: d0..d4, a0..a4 (a4 = KeyQ/TailMask base). MUST NOT touch: d5, d6, d7, a5, a6, a7
        ifnd    HUR045_MAC
HUR045_MAC equ 1
HUR045_BODY macro
        moveq   #5,d2                   ; d6 = 5 quads (20 px)
        moveq   #0,d3                   ; d7 = 0 tail px
        moveq   #4-1,d0                 ; 4 rows (for dbra)
        move.l  d2,d1
        lsl.l   #3,d1                   ; d6*8 = 40
        moveq   #40,d4
        sub.l   d1,d4
        move.l  d4,a2                   ; a2 = srcStride
        move.l  #1280,d4
        sub.l   d1,d4
        move.l  d4,a3                   ; a3 = dstStride
        move.l  d2,d4                   ; d4 = d6 (for btst #0)
        lsr.l   #1,d2
        subq.w  #1,d2                   ; d2 = (d6>>1)-1 for dbra
.row\@:
        load    (a4),e0                 ; load KeyQ,e0 (task-switch safe)
        move.w  d2,d1
        bmi.s   .k1\@
.k2\@:  load    (a0)+,e2
        load    (a0)+,e4
        pcmpeqw e0,e2,e3
        pcmpeqw e0,e4,e5
        storeilm e2,e3,(a1)+
        storeilm e4,e5,(a1)+
        dbra    d1,.k2\@
.k1\@:  btst    #0,d4
        beq.s   .tail\@
        load    (a0)+,e2
        pcmpeqw e0,e2,e3
        storeilm e2,e3,(a1)+
.tail\@:
        tst.l   d3
        beq.s   .next\@
        load    8(a4,d3.l*8),e1
        load    (a0),e2
        pcmpeqw e0,e2,e3
        por     e1,e3,e3
        storeilm e2,e3,(a1)
.next\@:
        add.l   a2,a0
        add.l   a3,a1
        dbra    d0,.row\@
        endm
        endif
        HUR045_BODY
