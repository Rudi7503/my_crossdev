; seq/HUR-110.i - hud.s: Original Number routine (4 numbers: Lives, Score, TimeLeft, Diamonds with divul.l #10, d1:d0)
; Uses: d0-d4, a0-a3. MUST NOT touch: d5, d6, d7, a5, a6, a7
; a4 = scratch buffer
        ifnd    HUR110_MAC
HUR110_MAC equ 1
HUR110_BODY macro
        movem.l d5-d7/a5-a6,-(sp)

        ; Test values: Lives=3, Score=123450, TimeLeft=540, Diamonds=42
        lea     (a4),a5                 ; a5 = output digit buffer
        moveq   #0,d5                   ; accumulator for bit-exact check

        ; 1. Lives (3)
        move.l  #3,d0
        lea     32(a5),a2
        moveq   #0,d7
.dv1_110\@:
        moveq   #0,d1
        divul.l #10,d1:d0
        move.b  d1,-(a2)
        add.b   d1,d5
        addq.l  #1,d7
        tst.l   d0
        bne.s   .dv1_110\@

        ; 2. Score (123450)
        move.l  #123450,d0
        lea     64(a5),a2
        moveq   #0,d7
.dv2_110\@:
        moveq   #0,d1
        divul.l #10,d1:d0
        move.b  d1,-(a2)
        add.b   d1,d5
        addq.l  #1,d7
        tst.l   d0
        bne.s   .dv2_110\@

        ; 3. TimeLeft (540)
        move.l  #540,d0
        lea     96(a5),a2
        moveq   #0,d7
.dv3_110\@:
        moveq   #0,d1
        divul.l #10,d1:d0
        move.b  d1,-(a2)
        add.b   d1,d5
        addq.l  #1,d7
        tst.l   d0
        bne.s   .dv3_110\@

        ; 4. Diamonds (42)
        move.l  #42,d0
        lea     128(a5),a2
        moveq   #0,d7
.dv4_110\@:
        moveq   #0,d1
        divul.l #10,d1:d0
        move.b  d1,-(a2)
        add.b   d1,d5
        addq.l  #1,d7
        tst.l   d0
        bne.s   .dv4_110\@

        move.l  d5,d0                   ; return sum of all digits
        movem.l (sp)+,d5-d7/a5-a6
        endm
        endif
        HUR110_BODY
