; seq/HUR-108.i - weapons.s: Original WeaponsDraw shot loop (10 shots: 5 pushes, 5 pops, bsr/rts per shot)
; Uses: d0-d4, a0-a3. MUST NOT touch: d5, d6, d7, a5, a6, a7
; a4 = scratch buffer
        ifnd    HUR108_MAC
HUR108_MAC equ 1
HUR108_BODY macro
        ; Setup mock 10 shots in a4 (each P_SIZE = 48 bytes)
        ; a4 = shots, a4+1024 = ActList, a4+2048 = FxBank
        lea     (a4),a0
        lea     1024(a4),a1
        moveq   #9,d0
.init_108\@:
        move.l  a0,(a1)+
        move.l  #2,20(a0)               ; P_SPR = 2
        clr.l   28(a0)                  ; P_ANIM = 0
        move.l  #1,(a0)                 ; P_TYPE = 1 (Streuschuss)
        move.l  #$00640000,4(a0)        ; P_X = 100
        move.l  #$00C80000,8(a0)        ; P_Y = 200
        lea     48(a0),a0
        dbra    d0,.init_108\@

        ; Mock FxBank sprite 2 at a4+2048+32:
        lea     2048(a4),a0
        move.l  #16,32(a0)              ; width
        move.l  #16,36(a0)              ; height
        move.l  #32,40(a0)              ; pitch
        move.l  a4,44(a0)               ; ptr

        movem.l d5-d7/a5-a6,-(sp)

        lea     2048(a4),a6             ; a6 = mock FxBank
        lea     1024(a4),a3             ; a3 = ActList
        moveq   #9,d5                   ; 10 shots
        moveq   #0,d4                   ; WAnim = 0
        move.l  #50,d6                  ; CamX
        move.l  #80,d7                  ; CamY
        lea     4096(a4),a5             ; a5 = mock DstBuf

        moveq   #0,d3                   ; accumulator

.sl_108\@:
        move.l  (a3)+,a2
        move.l  20(a2),d1               ; P_SPR
        tst.l   28(a2)                  ; P_ANIM
        beq.s   .s0_108\@
        add.l   d4,d1
.s0_108\@:
        ; SprInfo inlined
        move.l  a6,a1                   ; FxBank
        lsl.l   #4,d1
        lea     8(a1,d1.l),a1
        move.w  4(a2),d0                ; P_X integer
        ext.l   d0
        sub.l   d6,d0                   ; screen x
        move.w  8(a2),d1                ; P_Y integer
        ext.l   d1
        sub.l   d7,d1                   ; screen y

        ; --- Original DrawKeyed call with stack saves ---
        move.l  a3,-(sp)
        move.l  d4,-(sp)
        ; Mock bsr DrawKeyed:
        movem.l d5-d7,-(sp)
        move.l  (a1),d2
        move.l  4(a1),d3
        move.l  8(a1),d4
        move.l  12(a1),a0
        move.l  a5,a1
        move.l  #$F81F,d5
        ; Mock BlitKey: mock work (accumulate coords)
        add.l   d0,d2
        add.l   d1,d3
        movem.l (sp)+,d5-d7
        ; Return from DrawKeyed:
        move.l  (sp)+,d4
        move.l  (sp)+,a3

        add.l   d0,d3
        dbra.l  d5,.sl_108\@

        move.l  d3,d0                   ; return sum
        movem.l (sp)+,d5-d7/a5-a6
        endm
        endif
        HUR108_BODY
