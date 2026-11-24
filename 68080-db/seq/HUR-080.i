; seq/HUR-080.i - Hurrican _BuildCover cell inner loop: Inlined .op + CovPh in register
; Simulates 2 cell evaluations (4 layers) with inlined logic and CovPh in register d0
; Uses: d0-d4, a0-a4. MUST NOT touch: d5, d6, d7, a5, a6, a7
        ifnd    HUR080_MAC
HUR080_MAC equ 1
HUR080_BODY macro
        ; Scratch pointers: a0 = back map, a1 = front map, a2 = OpqTab, a3 = CovB out
        ; Register d0 holds CovPh hoisted outside the loop
        ; Cell 1
        moveq   #0,d3
        move.w  (a0)+,d2
        beq.s   .c1_b0\@
        bclr    #15,d2
        beq.s   .c1_b1\@
        add.l   d0,d2           ; d0 = CovPh in register
.c1_b1\@:
        move.b  (a2,d2.l),d3
.c1_b0\@:
        move.b  d3,(a3)
        move.l  d3,d1

        moveq   #0,d3
        move.w  (a1)+,d2
        beq.s   .c1_f0\@
        bclr    #15,d2
        beq.s   .c1_f1\@
        add.l   d0,d2
.c1_f1\@:
        move.b  (a2,d2.l),d3
.c1_f0\@:
        move.b  d3,40(a3)
        or.b    d1,d3
        move.b  d3,80(a3)
        addq.l  #1,a3

        ; Cell 2
        moveq   #0,d3
        move.w  (a0)+,d2
        beq.s   .c2_b0\@
        bclr    #15,d2
        beq.s   .c2_b1\@
        add.l   d0,d2
.c2_b1\@:
        move.b  (a2,d2.l),d3
.c2_b0\@:
        move.b  d3,(a3)
        move.l  d3,d1

        moveq   #0,d3
        move.w  (a1)+,d2
        beq.s   .c2_f0\@
        bclr    #15,d2
        beq.s   .c2_f1\@
        add.l   d0,d2
.c2_f1\@:
        move.b  (a2,d2.l),d3
.c2_f0\@:
        move.b  d3,40(a3)
        or.b    d1,d3
        move.b  d3,80(a3)
        addq.l  #1,a3

        move.b  (a3),d0
        endm
        endif
        HUR080_BODY
