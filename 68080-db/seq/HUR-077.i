; seq/HUR-077.i - Hurrican _EnemiesDraw dispatch: Original linear comparison chain
; Benchmarks dispatch cost for 2 enemies: ID 5 (Walker), ID 48 (Pflanze)
; Uses: d0-d4, a0-a3. MUST NOT touch: d5, d6, d7, a5, a6, a7
        ifnd    HUR077_MAC
HUR077_MAC equ 1
HUR077_BODY macro
        ; Test ID 5 (Walker)
        moveq   #5,d4
        cmp.l   #98,d4          ; ID_SBOL
        beq.s   .h1_sbol\@
        cmp.l   #156,d4         ; ID_GLUB
        beq.s   .h1_glub\@
        cmp.l   #157,d4         ; ID_GLUB2
        beq.s   .h1_glub2\@
        cmp.l   #2,d4           ; ID_DIAMANT
        beq.s   .h1_dia\@
        cmp.l   #5,d4           ; ID_WALKER
        beq.s   .h1_w\@
        bra.s   .h1_ex\@
.h1_sbol\@: moveq #1,d0; bra.s .e1\@
.h1_glub\@: moveq #2,d0; bra.s .e1\@
.h1_glub2\@: moveq #3,d0; bra.s .e1\@
.h1_dia\@:  moveq #4,d0; bra.s .e1\@
.h1_w\@:    moveq #5,d0; bra.s .e1\@
.h1_ex\@:   moveq #0,d0
.e1\@:

        ; Test ID 48 (Pflanze - deep in chain, 10 comparisons)
        moveq   #48,d4
        cmp.l   #98,d4
        beq.s   .h2_sbol\@
        cmp.l   #156,d4
        beq.s   .h2_sbol\@
        cmp.l   #5,d4
        beq.s   .h2_sbol\@
        cmp.l   #9,d4           ; Muecke
        beq.s   .h2_sbol\@
        cmp.l   #27,d4          ; Faust
        beq.s   .h2_sbol\@
        cmp.l   #35,d4          ; FBoss
        beq.s   .h2_sbol\@
        cmp.l   #40,d4          ; Wall
        beq.s   .h2_sbol\@
        cmp.l   #41,d4          ; Skel
        beq.s   .h2_sbol\@
        cmp.l   #42,d4          ; Metal
        beq.s   .h2_sbol\@
        cmp.l   #48,d4          ; Pflanze
        beq.s   .h2_pf\@
        bra.s   .h2_ex\@
.h2_sbol\@: moveq #1,d1; bra.s .e2\@
.h2_pf\@:   moveq #48,d1; bra.s .e2\@
.h2_ex\@:   moveq #0,d1
.e2\@:
        add.l   d1,d0
        endm
        endif
        HUR077_BODY
