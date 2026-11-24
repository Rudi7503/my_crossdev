; seq/HUR-091.i - weapons.s Bullet Sprite Info: Optimized (Inlined without bsr/stack push/pop)
; Tests exact same 4 bullets, accumulates sprite pointer sum in d0
; Uses: d0-d4, a0-a3. MUST NOT touch: d5, d6, d7, a5, a6, a7
        ifnd    HUR091_MAC
HUR091_MAC equ 1
HUR091_BODY macro
        ; Bullet 1: ID 15 (FX)
        moveq   #15,d1
        lea     (a4),a1         ; mock FxBank in scratch
        cmp.l   #$10000,d1
        blt.s   .fx1_91\@
        lea     2048(a4),a1     ; mock PartBank in scratch
        and.l   #$FFFF,d1
.fx1_91\@:
        lsl.l   #4,d1
        lea     8(a1,d1.l),a1
        move.l  a1,d0

        ; Bullet 2: ID 40 (FX)
        moveq   #40,d1
        lea     (a4),a1
        cmp.l   #$10000,d1
        blt.s   .fx2_91\@
        lea     2048(a4),a1
        and.l   #$FFFF,d1
.fx2_91\@:
        lsl.l   #4,d1
        lea     8(a1,d1.l),a1
        add.l   a1,d0

        ; Bullet 3: ID $10000 + 306 (PartBank)
        move.l  #$10000+306,d1
        lea     (a4),a1
        cmp.l   #$10000,d1
        blt.s   .fx3_91\@
        lea     2048(a4),a1
        and.l   #$FFFF,d1
.fx3_91\@:
        lsl.l   #4,d1
        lea     8(a1,d1.l),a1
        add.l   a1,d0

        ; Bullet 4: ID $10000 + 312 (PartBank)
        move.l  #$10000+312,d1
        lea     (a4),a1
        cmp.l   #$10000,d1
        blt.s   .fx4_91\@
        lea     2048(a4),a1
        and.l   #$FFFF,d1
.fx4_91\@:
        lsl.l   #4,d1
        lea     8(a1,d1.l),a1
        add.l   a1,d0
        endm
        endif
        HUR091_BODY
