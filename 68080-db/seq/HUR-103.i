; seq/HUR-103.i - beam.s: Frame Phase calculation with mulu.w #6554 (36 angles: reciprocal /10, clr.w, swap)
; Uses: d0-d4, a0-a3. MUST NOT touch: d5, d6, d7, a5, a6, a7
        ifnd    HUR103_MAC
HUR103_MAC equ 1
HUR103_BODY macro
        moveq   #0,d3           ; accumulator for bit-exact check
        moveq   #0,d4           ; angle loop counter (0..35 -> 0, 10, 20 .. 350 deg)
        moveq   #35,d2          ; 36 iterations

.loop_103\@:
        ; Simulate Frame angle calculation for angle d4:
        move.l  d4,d0
        sub.l   #270,d0
        bpl.s   .a_103\@
        add.l   #360,d0
.a_103\@:
        add.l   #180,d0         ; rechts: 180 + W (d0 in 0..539)

        ; Division via Reziprok-Multiplikation mit 1/10:
        mulu.w  #6554,d0        ; d0 * 6554
        clr.w   d0              ; untere 16 Bit loeschen
        swap    d0              ; obere 16 Bit nach unten schieben (Ergebnis /10)
        cmp.l   #36,d0
        blt.s   .ok_103\@
        sub.l   #36,d0
.ok_103\@:
        add.l   d0,d3           ; accumulate phase
        add.l   #10,d4          ; next angle (+10 deg)
        dbra.l  d2,.loop_103\@

        move.l  d3,d0           ; output accumulated sum
        endm
        endif
        HUR103_BODY
