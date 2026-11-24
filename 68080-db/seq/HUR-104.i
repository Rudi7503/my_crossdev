; seq/HUR-104.i - player.s: Original BlockAt (8 tile lookups: redundant tests & 2x lea Div20)
; Uses: d0-d4, a0-a3. MUST NOT touch: d5, d6, d7, a5, a6, a7
; a4 = scratch buffer
        ifnd    HUR104_MAC
HUR104_MAC equ 1
HUR104_BODY macro
        ; Setup mock Div20 and RowTab in scratch (a4)
        ; Let Div20 be at a4, RowTab at a4 + 2048, PBlock at a4 + 4096
        ; Setup Div20 table for 0..1000 pixels:
        lea     (a4),a0                 ; Div20 table
        moveq   #0,d0
        moveq   #0,d1                   ; x/20
        moveq   #0,d2                   ; x % 20
.init_div\@:
        move.w  d1,(a0)+
        addq.l  #1,d2
        cmp.l   #20,d2
        blt.s   .d1\@
        moveq   #0,d2
        addq.l  #1,d1
.d1\@:  addq.l  #1,d0
        cmp.l   #1000,d0
        blt.s   .init_div\@

        ; Setup RowTab (rows 0..30): each row is 40 words (80 bytes)
        lea     2048(a4),a0             ; RowTab
        lea     4096(a4),a1             ; PBlock
        moveq   #30,d0
.init_row\@:
        move.l  a1,(a0)+
        lea     80(a1),a1
        dbra    d0,.init_row\@

        ; Write some test words in PBlock:
        move.w  #$0001,4096+10*80+5*2(a4)       ; (100, 200) -> col 5, row 10
        move.w  #$0002,4096+10*80+7*2(a4)       ; (150, 200) -> col 7, row 10
        move.w  #$0004,4096+11*80+11*2(a4)      ; (220, 220) -> col 11, row 11
        move.w  #$0008,4096+9*80+17*2(a4)       ; (340, 180) -> col 17, row 9

        moveq   #0,d3                   ; accumulator for bit-exact check
        moveq   #7,d2                   ; 8 iterations

.query_loop_104\@:
        ; Original BlockAt logic:
        ; Coordinates for iteration d2:
        move.l  d2,d0
        mulu.w  #30,d0
        add.l   #100,d0                 ; x in 100..310
        move.l  d2,d1
        mulu.w  #10,d1
        add.l   #180,d1                 ; y in 180..250

        ; --- Original BlockAt body ---
        tst.l   d1
        bmi.s   .air_104\@
        tst.l   d0
        bmi.s   .wall_104\@
        cmp.l   #16383,d0
        bhi.s   .sx_104\@
        lea     (a4),a0                 ; lea Div20, a0
        move.w  (a0,d0.l*2),d0
.cx_104\@:
        cmp.l   #40,d0                  ; PMapW = 40
        bge.s   .wall_104\@
        cmp.l   #16383,d1
        bhi.s   .sy_104\@
        lea     (a4),a0                 ; 2nd lea Div20, a0 (REDUNDANT!)
        move.w  (a0,d1.l*2),d1
.cy_104\@:
        cmp.l   #30,d1                  ; PMapH = 30
        bge.s   .air_104\@
        lea     2048(a4),a0             ; lea RowTab, a0
        move.l  (a0,d1.l*4),a0
        move.w  (a0,d0.l*2),d0
        bra.s   .accum_104\@
.sx_104\@:
        bra.s   .wall_104\@
.sy_104\@:
        bra.s   .air_104\@
.wall_104\@:
        moveq   #1,d0
        bra.s   .accum_104\@
.air_104\@:
        moveq   #0,d0
.accum_104\@:
        add.l   d0,d3
        dbra.l  d2,.query_loop_104\@

        move.l  d3,d0                   ; return sum
        endm
        endif
        HUR104_BODY
