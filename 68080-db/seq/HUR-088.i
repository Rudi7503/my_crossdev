; seq/HUR-088.i - enemies.s Particle Setup: Original (mulu.l #64 + multi-insn PtTopMode check)
; Tests 4 particles, computes pointer offset + flags sum in d0
; Uses: d0-d4, a0-a3. MUST NOT touch: d5, d6, d7, a5, a6, a7
        ifnd    HUR088_MAC
HUR088_MAC equ 1
HUR088_BODY macro
        ; Test particle 1: Type 5 (d3=5), PtTopMode=0
        moveq   #5,d3
        subq.l  #1,d3
        lea     (a4),a0         ; mock PtTab in scratch
        mulu.l  #64,d3          ; PT_TS = 64
        add.l   d3,a0
        move.l  16(a0),d2       ; flags (mocked)
        moveq   #0,d3
        btst    #19,d2
        beq.s   .tq1\@
        moveq   #1,d3
.tq1\@: cmp.l   #0,d3           ; PtTopMode = 0
        bne.s   .sk1\@
        move.l  a0,d0           ; accumulate pointer
        bra.s   .p2\@
.sk1\@: moveq   #0,d0
.p2\@:

        ; Test particle 2: Type 12
        moveq   #12,d3
        subq.l  #1,d3
        lea     (a4),a0
        mulu.l  #64,d3
        add.l   d3,a0
        move.l  16(a0),d2
        moveq   #0,d3
        btst    #19,d2
        beq.s   .tq2\@
        moveq   #1,d3
.tq2\@: cmp.l   #0,d3
        bne.s   .sk2\@
        add.l   a0,d0
.sk2\@:
.p3\@:

        ; Test particle 3: Type 25
        moveq   #25,d3
        subq.l  #1,d3
        lea     (a4),a0
        mulu.l  #64,d3
        add.l   d3,a0
        move.l  16(a0),d2
        moveq   #0,d3
        btst    #19,d2
        beq.s   .tq3\@
        moveq   #1,d3
.tq3\@: cmp.l   #0,d3
        bne.s   .sk3\@
        add.l   a0,d0
.sk3\@:
.p4\@:

        ; Test particle 4: Type 40
        moveq   #40,d3
        subq.l  #1,d3
        lea     (a4),a0
        mulu.l  #64,d3
        add.l   d3,a0
        move.l  16(a0),d2
        moveq   #0,d3
        btst    #19,d2
        beq.s   .tq4\@
        moveq   #1,d3
.tq4\@: cmp.l   #0,d3
        bne.s   .sk4\@
        add.l   a0,d0
.sk4\@:
        endm
        endif
        HUR088_BODY
