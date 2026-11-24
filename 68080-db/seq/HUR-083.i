; seq/HUR-083.i - Hurrican BlkCov: Original logic (7-reg movem, 2x mulu.w #3277)
; Measures 1 call to BlkCov for a 32x16 block
; Uses: d0-d4, a0-a3. MUST NOT touch: d5, d6, d7, a5, a6, a7
        ifnd    HUR083_MAC
HUR083_MAC equ 1
HUR083_BODY macro
        ; Setup mock registers: d0 = x (160)
        move.w  #160,d0
        ; Original BlkCov simulation:
        ; 1. movem.l d0-d2/d4/a0-a1,-(sp) (simulating 7 reg save without forbidden regs)
        movem.l d1-d4/a0-a1,-(sp)
        move.l  d0,d2           ; x0
        bpl.s   .a\@
        moveq   #0,d2
.a\@:   add.l   #31,d0          ; x1
        cmp.l   #639,d0
        ble.s   .b\@
        move.l  #639,d0
.b\@:   cmp.l   d2,d0
        blt.s   .no\@
        move.l  (a4),d4         ; CovOX from memory
        add.l   d4,d2
        add.l   d4,d0
        mulu.w  #3277,d2        ; /20
        clr.w   d2
        swap    d2
        mulu.w  #3277,d0        ; /20
        clr.w   d0
        swap    d0
        sub.l   d2,d0           ; Spalten - 1
        move.l  4(a4),d1        ; BCovRows (0)
        lea     (a2,d2.l),a0    ; CovA + d2
.r\@:   move.l  d0,d4
.cl\@:  tst.b   (a0)+
        beq.s   .no\@
        dbra    d4,.cl\@
        moveq   #-1,d0
        movem.l (sp)+,d1-d4/a0-a1
        bra.s   .done\@
.no\@:  moveq   #0,d0
        movem.l (sp)+,d1-d4/a0-a1
.done\@:
        endm
        endif
        HUR083_BODY
