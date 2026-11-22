; seq/HUR-054.i - Hurrican _TileCache Pass 2 (.re) original: divul.l #34 on TcRowS + 4x mulu.l (tilemap.s L558-573)
; Uses: d0..d4, a0..a4. MUST NOT touch: d5, d6, d7, a5, a6, a7
        move.l  d2,d0                   ; TcRowS (sy * 34)
        divul.l #34,d1:d0               ; d0 = sy (divides back by 34!)
        mulu.l  #27200,d0               ; TILE_H * TC_PITCH (20 * 1360)
        lea     (a0,d0.l),a2
        move.l  d3,d0                   ; sx0
        mulu.l  #40,d0                  ; TILE_W * 2
        add.l   d0,a2
        move.l  d3,d0                   ; c0
        mulu.l  #20,d0                  ; TILE_W
        move.l  d4,d1                   ; r
        mulu.l  #20,d1                  ; TILE_H
        move.l  a2,(a1)+
