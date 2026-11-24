; seq/HUR-096.i - hurrican.s DrawFalls: Original FallCov + CovCell (8 tiles: bsr FallCov, per-tile mulu.l #34, stack push/pop)
; Uses: d0-d4, a0-a3. MUST NOT touch: d5, d6, d7, a5, a6, a7
; a4 = scratch buffer
        ifnd    HUR096_MAC
HUR096_MAC equ 1
HUR096_BODY macro
        ; Setup mock PBlock tile data in scratch (a4)
        ; 8 tiles at x=10..17:
        ; Tile 0 (x=10): $8000 (Waterfall, not covered)
        ; Tile 1 (x=11): $8001 (Waterfall + WAND, checked in CovF)
        ; Tile 2 (x=12): $8040 (Waterfall + VERDECKEN)
        ; Tile 3 (x=13): $0000 (Empty)
        ; Tile 4 (x=14): $8000 (Waterfall)
        ; Tile 5 (x=15): $8041 (Waterfall + WAND + VERDECKEN)
        ; Tile 6 (x=16): $8000 (Waterfall)
        ; Tile 7 (x=17): $0001 (Wall)
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
        ; Let row Y = 10, CovTY = 4 -> row_idx = 6 * 34 = 204
        ; Let CovTX = 2.
        ; Set byte for x=12 (col = 10 -> offset 204 + 10 = 214) = 1 (covered)
        clr.b   256+204+9(a4)
        move.b  #1,256+204+10(a4)       ; x=12 is covered!
        clr.b   256+204+13(a4)

        moveq   #10,d4                  ; d4 = current x
        moveq   #0,d3                   ; d3 = accumulator of visible waterfall tiles
        moveq   #7,d2                   ; 8 tiles loop counter

.scan_96\@:
        move.w  (a2,d4.l*2),d0
        bpl.s   .cn_96\@
        bsr.s   .fallcov_96\@
        bne.s   .cn_96\@                ; bne = covered (Z=0)
        addq.l  #1,d3                   ; visible waterfall tile!
.cn_96\@:
        addq.l  #1,d4
        dbra.l  d2,.scan_96\@
        bra.s   .end_96\@

.fallcov_96\@:
        move.w  (a2,d4.l*2),d0
        and.w   #65,d0                  ; WAND | VERDECKEN
        beq.s   .no_cov_96\@
        move.l  d4,d0
        moveq   #10,d1                  ; mock Y = 10
        ; CovCell:
        sub.l   #2,d0                   ; CovTX = 2
        bmi.s   .no_cov_96\@
        cmp.l   #34,d0
        bge.s   .no_cov_96\@
        sub.l   #4,d1                   ; CovTY = 4
        bmi.s   .no_cov_96\@
        cmp.l   #26,d1
        bge.s   .no_cov_96\@
        mulu.l  #34,d1
        add.l   d0,d1
        lea     256(a4),a0              ; CovF mock
        move.b  (a0,d1.l),d0
        tst.b   d0
        rts
.no_cov_96\@:
        moveq   #0,d0                   ; Z=1 (not covered)
        rts

.end_96\@:
        move.l  d3,d0
        endm
        endif
        HUR096_BODY
