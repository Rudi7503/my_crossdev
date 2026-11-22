; seq/HUR-043.i - Hurrican _BlitCopy optimized (4 rows x 20 px, d6=5, d7=0: hoisted counts, direct a0/a1 stride, dbra, TailMask only on d7!=0)
; Uses: d0..d4, a0..a4 (a4 = TailMask base). MUST NOT touch: d5, d6, d7, a5, a6, a7
        ifnd    HUR043_MAC
HUR043_MAC equ 1
HUR043_BODY macro
        moveq   #5,d2                   ; d6 = 5 quads (20 px)
        moveq   #0,d3                   ; d7 = 0 tail px
        moveq   #4-1,d0                 ; 4 rows (for dbra)
        move.l  d2,d1
        lsl.l   #3,d1                   ; d6*8 = 40
        moveq   #40,d4
        sub.l   d1,d4                   ; srcStride = srcPitch - d6*8
        move.l  d4,a2                   ; a2 = srcStride
        move.l  #1280,d4
        sub.l   d1,d4                   ; dstStride = dstPitch - d6*8
        move.l  d4,a3                   ; a3 = dstStride
        move.l  d2,d4
        and.l   #3,d4
        subq.w  #1,d4                   ; d4 = (d6&3)-1 for dbra
        lsr.l   #2,d2
        subq.w  #1,d2                   ; d2 = (d6>>2)-1 for dbra
.row\@:
        move.w  d2,d1
        bmi.s   .r1\@
.q4\@:  load    (a0)+,e2
        load    (a0)+,e3
        load    (a0)+,e4
        load    (a0)+,e5
        store   e2,(a1)+
        store   e3,(a1)+
        store   e4,(a1)+
        store   e5,(a1)+
        dbra    d1,.q4\@
.r1\@:  move.w  d4,d1
        bmi.s   .tail\@
.qd\@:  load    (a0)+,e2
        store   e2,(a1)+
        dbra    d1,.qd\@
.tail\@:
        tst.l   d3
        beq.s   .next\@
        load    (a4,d3.l*8),e1
        load    (a0),e2
        storeilm e2,e1,(a1)
.next\@:
        add.l   a2,a0
        add.l   a3,a1
        dbra    d0,.row\@
        endm
        endif
        HUR043_BODY
