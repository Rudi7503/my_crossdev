; seq/HUR-079.i - Hurrican _BuildCover cell inner loop: Original with bsr.s .op
; Simulates 2 cell evaluations (4 layers) using subroutine calls and memory-read CovPh
; Uses: d0-d4, a0-a4. MUST NOT touch: d5, d6, d7, a5, a6, a7
        ifnd    HUR079_MAC
HUR079_MAC equ 1
HUR079_BODY macro
        ; Scratch pointers: a0 = back map, a1 = front map, a2 = OpqTab, a3 = CovB out, a4 = CovPh in memory
        ; Cell 1
        moveq   #0,d2
        move.w  (a0)+,d2
        bsr.s   .op1_79\@
        move.b  d3,(a3)
        move.l  d3,d1
        moveq   #0,d2
        move.w  (a1)+,d2
        bsr.s   .op1_79\@
        move.b  d3,40(a3)
        or.b    d1,d3
        move.b  d3,80(a3)
        addq.l  #1,a3

        ; Cell 2
        moveq   #0,d2
        move.w  (a0)+,d2
        bsr.s   .op1_79\@
        move.b  d3,(a3)
        move.l  d3,d1
        moveq   #0,d2
        move.w  (a1)+,d2
        bsr.s   .op1_79\@
        move.b  d3,40(a3)
        or.b    d1,d3
        move.b  d3,80(a3)
        addq.l  #1,a3
        bra.s   .done_79\@

.op1_79\@:
        moveq   #0,d3
        tst.l   d2
        beq.s   .o0_79\@
        bclr    #15,d2
        beq.s   .o1_79\@
        add.l   (a4),d2         ; CovPh read from memory
.o1_79\@:
        move.b  (a2,d2.l),d3
.o0_79\@:
        rts

.done_79\@:
        move.b  (a3),d0
        endm
        endif
        HUR079_BODY
