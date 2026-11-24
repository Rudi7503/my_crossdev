; seq/HUR-084.i - Hurrican BlkCov: Optimized (minimal reg save, fast div lookup)
; Measures 1 call to optimized BlkCov for a 32x16 block
; Uses: d0-d4, a0-a3. MUST NOT touch: d5, d6, d7, a5, a6, a7
        ifnd    HUR084_MAC
HUR084_MAC equ 1
HUR084_BODY macro
        ; Setup mock registers: d0 = x (160)
        move.w  #160,d0
        ; Optimized BlkCov:
        ; Save only caller-critical registers: d1-d2/a0
        movem.l d1-d2/a0,-(sp)
        move.l  d0,d2           ; x0
        bpl.s   .a\@
        moveq   #0,d2
.a\@:   add.l   #31,d0          ; x1
        cmp.l   #639,d0
        ble.s   .b\@
        move.l  #639,d0
.b\@:   cmp.l   d2,d0
        blt.s   .no\@
        add.l   (a4),d2         ; + CovOX
        add.l   (a4),d0         ; + CovOX
        ; Fast table lookup from Div20Tab in a3:
        move.b  (a3,d2.w),d2    ; col0 = x0 / 20
        move.b  (a3,d0.w),d0    ; col1 = x1 / 20
        sub.b   d2,d0           ; Spalten - 1 (in d0.b)
        and.l   #$FF,d2
        lea     (a2,d2.l),a0    ; CovA + col0
        tst.b   (a0)            ; Early out: check first byte immediately!
        beq.s   .no\@
        ; If first byte non-zero, check remaining 1..2 bytes
        tst.b   1(a0)
        beq.s   .no\@
        subq.b  #1,d0
        beq.s   .cov\@
        tst.b   2(a0)
        beq.s   .no\@
.cov\@: moveq   #-1,d0
        movem.l (sp)+,d1-d2/a0
        bra.s   .done\@
.no\@:  moveq   #0,d0
        movem.l (sp)+,d1-d2/a0
.done\@:
        endm
        endif
        HUR084_BODY
