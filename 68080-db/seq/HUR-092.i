; seq/HUR-092.i - hurrican.s DrawWater: Original CovCell (bsr CovCell with mulu.l #34 per cell)
; Tests 4 cells in the same row (d6=10, d2=5, 6, 7, 8)
; Uses: d0-d4, a0-a3. MUST NOT touch: d5, d6, d7, a5, a6, a7
        ifnd    HUR092_MAC
HUR092_MAC equ 1
HUR092_BODY macro
        ; Cell 1 (x=5, y=10)
        moveq   #5,d0
        moveq   #10,d1
        bsr.s   .cov_92\@
        move.b  d0,d4

        ; Cell 2 (x=6, y=10)
        moveq   #6,d0
        moveq   #10,d1
        bsr.s   .cov_92\@
        add.b   d0,d4

        ; Cell 3 (x=7, y=10)
        moveq   #7,d0
        moveq   #10,d1
        bsr.s   .cov_92\@
        add.b   d0,d4

        ; Cell 4 (x=8, y=10)
        moveq   #8,d0
        moveq   #10,d1
        bsr.s   .cov_92\@
        add.b   d0,d4
        bra.s   .end_92\@

.cov_92\@:
        move.l  d2,-(sp)
        sub.l   #2,d0           ; CovTX = 2 (mocked)
        bmi.s   .no_92\@
        cmp.l   #34,d0
        bge.s   .no_92\@
        sub.l   #4,d1           ; CovTY = 4 (mocked)
        bmi.s   .no_92\@
        cmp.l   #26,d1
        bge.s   .no_92\@
        mulu.l  #34,d1
        add.l   d0,d1
        move.l  a0,-(sp)
        lea     (a4),a0         ; mock CovF in scratch
        move.b  (a0,d1.l),d0
        move.l  (sp)+,a0
        move.l  (sp)+,d2
        tst.b   d0
        rts
.no_92\@:
        move.l  (sp)+,d2
        moveq   #0,d0
        rts

.end_92\@:
        move.b  d4,d0
        endm
        endif
        HUR092_BODY
