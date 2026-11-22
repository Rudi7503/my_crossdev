; seq/HUR-055.i - Hurrican _TileCache Pass 2 (.re) optimized: hoisted TcBufRow (a0) & rowY (d4), 0 div, only 2x mulu.l
; Uses: d0..d4, a0..a4. MUST NOT touch: d5, d6, d7, a5, a6, a7
        move.l  d3,d0                   ; sx0
        mulu.l  #40,d0                  ; TILE_W * 2
        lea     (a0,d0.l),a2            ; a0 = precomputed TcBuf + sy*27200
        move.l  d3,d0                   ; c0
        mulu.l  #20,d0                  ; TILE_W
        move.l  d4,d1                   ; precomputed y = r*20 - TcOY
        move.l  a2,(a1)+
