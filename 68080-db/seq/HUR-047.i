; seq/HUR-047.i - Hurrican _BlitAdd optimized (4 rows x 20 px, d6=5, d7=0: hoisted counts, direct a0/a1 stride, dbra, TailMask only on d7!=0)
; Uses: d0..d4, a0..a4 (a4 = TailMask base). MUST NOT touch: d5, d6, d7, a5, a6, a7
        ifnd    HUR047_MAC
HUR047_MAC equ 1
HUR047_BODY macro
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
        move.w  d2,d1
        bmi.s   .a1\@
.a2\@:  load    (a0)+,e2
        load    (a0)+,e10
        load    (a1),e3
        load    8(a1),e11
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
        store   e3,(a1)+
        store   e11,(a1)+
        dbra    d1,.a2\@
.a1\@:  btst    #0,d4
        beq.s   .tail\@
        load    (a0)+,e2
        load    (a1),e3
        unpack1632 e2,e4:e5
        unpack1632 e3,e6:e7
        paddusb e4,e6,e6
        paddusb e5,e7,e7
        pack3216 e6,e7,e3
        store   e3,(a1)+
.tail\@:
        tst.l   d3
        beq.s   .next\@
        load    (a4,d3.l*8),e1
        load    (a0),e2
        load    (a1),e3
        unpack1632 e2,e4:e5
        unpack1632 e3,e6:e7
        paddusb e4,e6,e6
        paddusb e5,e7,e7
        pack3216 e6,e7,e3
        storeilm e3,e1,(a1)
.next\@:
        add.l   a2,a0
        add.l   a3,a1
        dbra    d0,.row\@
        endm
        endif
        HUR047_BODY
