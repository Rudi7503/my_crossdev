; seq/HUR-107.i - enemies.s: Optimized ShotHits projectile-test loop (10 shots: zero stack traffic, asr.l #16, early X-reject)
; Uses: d0-d4, a0-a3. MUST NOT touch: d5, d6, d7, a5, a6, a7
; a4 = scratch buffer
        ifnd    HUR107_MAC
HUR107_MAC equ 1
HUR107_BODY macro
        ; Setup mock SprInfo table and ActList in scratch (a4)
        lea     (a4),a0
        moveq   #15,d0
.init_spr\@:
        move.l  #16,(a0)+               ; width
        move.l  #16,(a0)+               ; height
        move.l  #32,(a0)+               ; pitch
        move.l  #0,(a0)+                ; ptr
        dbra    d0,.init_spr\@

        ; Mock 10 projectiles at a4+1024
        lea     1024(a4),a0             ; shots
        lea     2048(a4),a1             ; ActList
        moveq   #9,d0
.init_shots\@:
        move.l  a0,(a1)+
        move.l  #1,(a0)                 ; P_TYPE = 1 (Streuschuss)
        move.l  d0,d1
        mulu.w  #12,d1
        add.l   #60,d1
        swap    d1
        clr.w   d1                      ; 16.16
        move.l  d1,4(a0)                ; P_X
        move.l  d0,d1
        mulu.w  #8,d1
        add.l   #190,d1
        swap    d1
        clr.w   d1
        move.l  d1,8(a0)                ; P_Y
        move.l  #2,20(a0)               ; P_SPR = 2
        lea     48(a0),a0
        dbra    d0,.init_shots\@

        movem.l d5-d7/a5-a6,-(sp)

        move.l  #100,d2                 ; enemy left
        move.l  #200,d3                 ; enemy top
        move.l  #140,d4                 ; enemy right
        move.l  #240,d5                 ; enemy bottom

        lea     2048(a4),a6             ; a6 = ActList
        moveq   #9,d7                   ; 10 shots
        moveq   #0,d6                   ; hit accumulator

.lp_107\@:
        move.l  (a6)+,a3
        tst.l   (a3)                    ; P_TYPE
        beq.s   .next_107\@

        ; --- Optimized ShotHits: ZERO STACK, inlined sprite lookup, early X-reject ---
        move.l  20(a3),d1               ; P_SPR
        lsl.l   #4,d1
        lea     (a4,d1.l),a1            ; a1 = sprite info
        move.l  4(a3),d0                ; P_X
        swap    d0
        ext.l   d0                      ; FIXINT via swap/ext.l (2 cyc vs 4 cyc)
        move.l  (a1),d1                 ; width
        lsr.l   #1,d1
        add.l   d1,d0                   ; center x

        ; Early-Reject auf X-Koordinate (kein Y-Aufwand wenn X verfehlt!)
        cmp.l   d2,d0
        blt.s   .next_107\@
        cmp.l   d4,d0
        bge.s   .next_107\@

        ; X passt: Jetzt Y pruefen
        move.l  8(a3),d1                ; P_Y
        swap    d1
        ext.l   d1                      ; FIXINT via swap/ext.l
        move.l  4(a1),d0                ; height
        lsr.l   #1,d0
        add.l   d0,d1                   ; center y

        cmp.l   d3,d1
        blt.s   .next_107\@
        cmp.l   d5,d1
        bge.s   .next_107\@

        ; Hit!
        addq.l  #1,d6

.next_107\@:
        dbra.l  d7,.lp_107\@

        move.l  d6,d0                   ; return hit count
        movem.l (sp)+,d5-d7/a5-a6
        endm
        endif
        HUR107_BODY
