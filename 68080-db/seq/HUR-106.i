; seq/HUR-106.i - enemies.s: Original ShotHits projectile-test loop (10 shots: movem.l d2-d5,-(sp), jsr SprInfo, movem.l (sp)+,d2-d5)
; Uses: d0-d4, a0-a3. MUST NOT touch: d5, d6, d7, a5, a6, a7
; a4 = scratch buffer
        ifnd    HUR106_MAC
HUR106_MAC equ 1
HUR106_BODY macro
        ; Setup mock SprInfo table and ActList in scratch (a4)
        ; a4 = FxBank (sprite 0..15: 16 bytes each: width, height, pitch, ptr)
        ; Let width = 16, height = 16 for all sprites
        lea     (a4),a0
        moveq   #15,d0
.init_spr\@:
        move.l  #16,(a0)+               ; width
        move.l  #16,(a0)+               ; height
        move.l  #32,(a0)+               ; pitch
        move.l  #0,(a0)+                ; ptr
        dbra    d0,.init_spr\@

        ; Mock 10 projectiles at a4+1024 (each P_SIZE = 48 bytes)
        ; ActList pointers at a4+2048
        lea     1024(a4),a0             ; shots
        lea     2048(a4),a1             ; ActList
        moveq   #9,d0
.init_shots\@:
        move.l  a0,(a1)+
        move.l  #1,(a0)                 ; P_TYPE = 1 (Streuschuss)
        ; Distribute shots around x=50..150, y=180..260
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

        ; Enemy bounding box:
        ; d2 = left (100), d4 = right (140)
        ; d3 = top (200), d5 = bottom (240)
        ; Wait! HUR body cannot touch d5, d6, d7 across the macro!
        ; So we map enemy bounding box to:
        ; a1 = left (100), a2 = right (140)
        ; a3 = top (200), a5 = bottom (240) -> wait, a5 must not be touched!
        ; Let's keep enemy box in a1 (left), a2 (right), a3 (top) and stack or memory!
        ; Better: in the macro, save and restore d5, d6, d7, a5 so we don't violate Harness rules!
        movem.l d5-d7/a5-a6,-(sp)

        move.l  #100,d2                 ; enemy left
        move.l  #200,d3                 ; enemy top
        move.l  #140,d4                 ; enemy right
        move.l  #240,d5                 ; enemy bottom

        lea     2048(a4),a6             ; a6 = ActList
        moveq   #9,d7                   ; 10 shots
        moveq   #0,d6                   ; hit accumulator

.lp_106\@:
        move.l  (a6)+,a3
        tst.l   (a3)                    ; P_TYPE
        beq.s   .next_106\@

        ; --- Original ShotHits logic ---
        movem.l d2-d5,-(sp)
        move.l  20(a3),d1               ; P_SPR
        ; jsr SprInfo inline (mock with FxBank = a4)
        lsl.l   #4,d1
        lea     (a4,d1.l),a1            ; a1 = sprite info
        move.l  4(a3),d0                ; P_X
        asr.l   #8,d0
        asr.l   #8,d0                   ; FIXINT d0
        move.l  (a1),d2                 ; width
        lsr.l   #1,d2
        add.l   d2,d0                   ; center x
        move.l  8(a3),d1                ; P_Y
        asr.l   #8,d1
        asr.l   #8,d1                   ; FIXINT d1
        move.l  4(a1),d2                ; height
        lsr.l   #1,d2
        add.l   d2,d1                   ; center y
        movem.l (sp)+,d2-d5             ; restore enemy rect

        cmp.l   d2,d0
        blt.s   .next_106\@
        cmp.l   d4,d0
        bge.s   .next_106\@
        cmp.l   d3,d1
        blt.s   .next_106\@
        cmp.l   d5,d1
        bge.s   .next_106\@

        ; Hit!
        addq.l  #1,d6

.next_106\@:
        dbra.l  d7,.lp_106\@

        move.l  d6,d0                   ; return hit count
        movem.l (sp)+,d5-d7/a5-a6
        endm
        endif
        HUR106_BODY
