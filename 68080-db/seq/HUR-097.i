; seq/HUR-097.i - hurrican.s DrawFalls: Optimized Inlined Coverage (8 tiles: hoisted CovF row pointer, no bsr, no mulu)
; Uses: d0-d4, a0-a3. MUST NOT touch: d5, d6, d7, a5, a6, a7
; a4 = scratch buffer
        ifnd    HUR097_MAC
HUR097_MAC equ 1
HUR097_BODY macro
        ; Setup mock PBlock tile data in scratch (a4)
        ; 8 tiles at x=10..17:
        lea     (a4),a2                 ; a2 = PBlock row
        move.w  #$8000,20(a2)           ; x=10 (*2 = 20)
        move.w  #$8001,22(a2)           ; x=11
        move.w  #$8040,24(a2)           ; x=12
        move.w  #$0000,26(a2)           ; x=13
        move.w  #$8000,28(a2)           ; x=14
        move.w  #$8041,30(a2)           ; x=15
        move.w  #$8000,32(a2)           ; x=16
        move.w  #$0001,34(a2)           ; x=17

        ; Mock CovF buffer at a4 + 256
        clr.b   256+204+9(a4)
        move.b  #1,256+204+10(a4)       ; x=12 is covered!
        clr.b   256+204+13(a4)

        ; Hoisted row setup once per row:
        ; Let row Y = 10, CovTY = 4 -> dy = 6. dy * 34 = 204.
        moveq   #10-4,d0
        mulu.l  #34,d0
        lea     256(a4,d0.l),a3         ; a3 = Zeilenanfang in CovF!

        moveq   #10,d4                  ; d4 = current x
        moveq   #0,d3                   ; d3 = accumulator of visible waterfall tiles
        moveq   #7,d2                   ; 8 tiles loop counter

.scan_97\@:
        move.w  (a2,d4.l*2),d0
        bpl.s   .cn_97\@                ; kein Wasserfall
        and.w   #65,d0                  ; WAND | VERDECKEN?
        beq.s   .vis_97\@               ; weder WAND noch VERDECKEN -> sofort sichtbar!
        ; Inlined cell check with hoisted row pointer a3:
        move.l  d4,d0
        subq.l  #2,d0                   ; x - CovTX (2)
        bmi.s   .vis_97\@
        cmp.l   #34,d0
        bge.s   .vis_97\@
        tst.b   (a3,d0.l)               ; verdeckt?
        bne.s   .cn_97\@                ; ja -> ueberspringen
.vis_97\@:
        addq.l  #1,d3                   ; sichtbar!
.cn_97\@:
        addq.l  #1,d4
        dbra.l  d2,.scan_97\@

        move.l  d3,d0
        endm
        endif
        HUR097_BODY
