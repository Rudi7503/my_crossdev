; seq/HUR-095.i - hurrican.s WaterSurf: Optimized Hoisted Wave Indexing (8 tiles: shift-add, extb.l, +3 modulo 64)
; Uses: d0-d4, a0-a3. MUST NOT touch: d5, d6, d7, a5, a6, a7
        ifnd    HUR095_MAC
HUR095_MAC equ 1
HUR095_BODY macro
        bra.s   .after_tab\@
.wavetab\@:
        dc.b    0,0,0,1,1,1,1,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,1,1,1,1,0,0,0,0,0,-1,-1,-1,-1,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-1,-1,-1,-1,0,0
        even
.after_tab\@:
        ; Initial values:
        ; Mock WSinPos = $00150000 (sin pos 21), WRunY = 12, WRunX = 10 (8 tiles: 10..17)
        moveq   #10,d4          ; d4 = current tile x
        moveq   #0,d2           ; d2 = accumulator for bit-exact check

        ; Hoisted setup once before loop:
        move.l  #$00150000,d0   ; move.l WSinPos,d0
        swap    d0
        ext.l   d0
        move.l  #12,d1          ; move.l WRunY,d1
        lsl.l   #4,d1
        add.l   d1,d0
        move.l  d4,d1
        add.l   d1,d1           ; d1 = d4 * 2
        add.l   d4,d1           ; d1 = d4 * 3
        add.l   d1,d0
        and.l   #63,d0          ; d0 = initial index (0..63)

        lea     .wavetab\@(pc),a0
        moveq   #7,d3           ; 8 tiles loop counter (0..7)

.tile_loop\@:
        ; Optimized WaterSurf inner step:
        move.b  (a0,d0.l),d1
        extb.l  d1
        add.l   d1,d2           ; accumulate offset
        addq.l  #3,d0
        and.l   #63,d0          ; advance wave index by 3
        addq.l  #1,d4
        dbra.l  d3,.tile_loop\@

        move.l  d2,d0           ; output accumulated sum
        endm
        endif
        HUR095_BODY
