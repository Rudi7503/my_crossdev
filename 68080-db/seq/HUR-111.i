; seq/HUR-111.i - hud.s: Optimized Number routine (4 numbers with reciprocal mulu.l #$CCCCCCCD)
; Uses: d0-d4, a0-a3. MUST NOT touch: d5, d6, d7, a5, a6, a7
; a4 = scratch buffer
        ifnd    HUR111_MAC
HUR111_MAC equ 1
HUR111_BODY macro
        movem.l d5-d7/a5-a6,-(sp)

        lea     (a4),a5
        moveq   #0,d5
        move.l  #$CCCCCCCD,d3           ; Reziprok 1/10 vorbereitet

        ; 1. Lives (3)
        move.l  #3,d0
        lea     32(a5),a2
        moveq   #0,d7
.dv1_111\@:
        move.l  d0,d1                   ; d1 = N
        mulu.l  d3,d2:d0                ; d2 = (N * $CCCCCCCD) >> 32
        lsr.l   #3,d2                   ; d2 = q = N / 10
        move.l  d2,d0
        lsl.l   #2,d0                   ; 4*q
        add.l   d2,d0                   ; 5*q
        add.l   d0,d0                   ; 10*q
        sub.l   d0,d1                   ; d1 = remainder (0..9)
        move.b  d1,-(a2)
        add.b   d1,d5
        addq.l  #1,d7
        move.l  d2,d0                   ; N = q
        bne.s   .dv1_111\@

        ; 2. Score (123450)
        move.l  #123450,d0
        lea     64(a5),a2
        moveq   #0,d7
.dv2_111\@:
        move.l  d0,d1
        mulu.l  d3,d2:d0
        lsr.l   #3,d2
        move.l  d2,d0
        lsl.l   #2,d0
        add.l   d2,d0
        add.l   d0,d0
        sub.l   d0,d1
        move.b  d1,-(a2)
        add.b   d1,d5
        addq.l  #1,d7
        move.l  d2,d0
        bne.s   .dv2_111\@

        ; 3. TimeLeft (540)
        move.l  #540,d0
        lea     96(a5),a2
        moveq   #0,d7
.dv3_111\@:
        move.l  d0,d1
        mulu.l  d3,d2:d0
        lsr.l   #3,d2
        move.l  d2,d0
        lsl.l   #2,d0
        add.l   d2,d0
        add.l   d0,d0
        sub.l   d0,d1
        move.b  d1,-(a2)
        add.b   d1,d5
        addq.l  #1,d7
        move.l  d2,d0
        bne.s   .dv3_111\@

        ; 4. Diamonds (42)
        move.l  #42,d0
        lea     128(a5),a2
        moveq   #0,d7
.dv4_111\@:
        move.l  d0,d1
        mulu.l  d3,d2:d0
        lsr.l   #3,d2
        move.l  d2,d0
        lsl.l   #2,d0
        add.l   d2,d0
        add.l   d0,d0
        sub.l   d0,d1
        move.b  d1,-(a2)
        add.b   d1,d5
        addq.l  #1,d7
        move.l  d2,d0
        bne.s   .dv4_111\@

        move.l  d5,d0                   ; return sum of all digits
        movem.l (sp)+,d5-d7/a5-a6
        endm
        endif
        HUR111_BODY
