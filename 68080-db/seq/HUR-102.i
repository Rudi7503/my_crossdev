; seq/HUR-102.i - beam.s: Frame Phase calculation with divu.w #10 (36 angles: divu.w #10, and.l #$FFFF, conditional sub)
; Uses: d0-d4, a0-a3. MUST NOT touch: d5, d6, d7, a5, a6, a7
        ifnd    HUR102_MAC
HUR102_MAC equ 1
HUR102_BODY macro
        moveq   #0,d3           ; accumulator for bit-exact check
        moveq   #0,d4           ; angle loop counter (0..35 -> 0, 10, 20 .. 350 deg)
        moveq   #35,d2          ; 36 iterations

.loop_102\@:
        ; Simulate Frame angle calculation for angle d4:
        move.l  d4,d0
        sub.l   #270,d0
        bpl.s   .a_102\@
        add.l   #360,d0
.a_102\@:
        add.l   #180,d0         ; rechts: 180 + W (d0 in 0..539)

        ; Division via divu.w:
        divu.w  #10,d0
        and.l   #$FFFF,d0
        cmp.l   #36,d0
        blt.s   .ok_102\@
        sub.l   #36,d0
.ok_102\@:
        add.l   d0,d3           ; accumulate phase
        add.l   #10,d4          ; next angle (+10 deg)
        dbra.l  d2,.loop_102\@

        move.l  d3,d0           ; output accumulated sum
        endm
        endif
        HUR102_BODY
