; seq/HUR-090.i - weapons.s Bullet Sprite Info: Original (bsr SprInfo with stack push/pop)
; Tests 4 bullets (2 FX, 2 parts.spr), accumulates sprite pointer sum in d0
; Uses: d0-d4, a0-a3. MUST NOT touch: d5, d6, d7, a5, a6, a7
        ifnd    HUR090_MAC
HUR090_MAC equ 1
HUR090_BODY macro
        ; Bullet 1: ID 15 (FX)
        moveq   #15,d1
        bsr.s   .sprinfo_90\@
        move.l  a1,d0

        ; Bullet 2: ID 40 (FX)
        moveq   #40,d1
        bsr.s   .sprinfo_90\@
        add.l   a1,d0

        ; Bullet 3: ID $10000 + 306 (PartBank)
        move.l  #$10000+306,d1
        bsr.s   .sprinfo_90\@
        add.l   a1,d0

        ; Bullet 4: ID $10000 + 312 (PartBank)
        move.l  #$10000+312,d1
        bsr.s   .sprinfo_90\@
        add.l   a1,d0
        bra.s   .end_90\@

.sprinfo_90\@:
        move.l  d1,-(sp)
        lea     (a4),a1         ; mock FxBank in scratch
        cmp.l   #$10000,d1      ; NEWREL
        blt.s   .fx_90\@
        lea     2048(a4),a1     ; mock PartBank in scratch
        and.l   #$FFFF,d1
.fx_90\@:
        lsl.l   #4,d1
        lea     8(a1,d1.l),a1
        move.l  (sp)+,d1
        rts

.end_90\@:
        endm
        endif
        HUR090_BODY
