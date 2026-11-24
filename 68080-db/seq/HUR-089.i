; seq/HUR-089.i - enemies.s Particle Setup: Optimized (lsl.l #6 + direct btst #19 check)
; Tests exact same 4 particles, computes pointer offset + flags sum in d0
; Uses: d0-d4, a0-a3. MUST NOT touch: d5, d6, d7, a5, a6, a7
        ifnd    HUR089_MAC
HUR089_MAC equ 1
HUR089_BODY macro
        ; Test particle 1: Type 5 (d3=5), PtTopMode=0
        moveq   #5,d3
        subq.l  #1,d3
        lsl.l   #6,d3           ; d3 * 64 (PT_TS = 64) via 6-bit shift
        lea     (a4,d3.l),a0    ; scaled base lookup in scratch
        move.l  16(a0),d2       ; flags
        btst    #19,d2          ; For PtTopMode=0: PF_TOP set -> skip!
        bne.s   .sk1\@
        move.l  a0,d0           ; accumulate pointer
        bra.s   .p2\@
.sk1\@: moveq   #0,d0
.p2\@:

        ; Test particle 2: Type 12
        moveq   #12,d3
        subq.l  #1,d3
        lsl.l   #6,d3
        lea     (a4,d3.l),a0
        move.l  16(a0),d2
        btst    #19,d2
        bne.s   .sk2\@
        add.l   a0,d0
.sk2\@:
.p3\@:

        ; Test particle 3: Type 25
        moveq   #25,d3
        subq.l  #1,d3
        lsl.l   #6,d3
        lea     (a4,d3.l),a0
        move.l  16(a0),d2
        btst    #19,d2
        bne.s   .sk3\@
        add.l   a0,d0
.sk3\@:
.p4\@:

        ; Test particle 4: Type 40
        moveq   #40,d3
        subq.l  #1,d3
        lsl.l   #6,d3
        lea     (a4,d3.l),a0
        move.l  16(a0),d2
        btst    #19,d2
        bne.s   .sk4\@
        add.l   a0,d0
.sk4\@:
        endm
        endif
        HUR089_BODY
