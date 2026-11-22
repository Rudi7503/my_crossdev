; seq/HUR-052.i - Hurrican _TileCache Pass 1 (.pc) original: divul.l #34 + mulu.l #34 + key build + SlKey/SlAnim check (tilemap.s L486-502)
; Uses: d0..d4, a0..a4. MUST NOT touch: d5, d6, d7, a5, a6, a7
; a0 = SlKey, a1 = SlAnim, d0 = tx, d1 = ty, d2 = TcSY
        move.l  d0,d4
        divul.l #34,d3:d4               ; d3 = sx = tx % 34
        move.l  d2,d4
        mulu.l  #34,d4                  ; TcSY * 34 (redundant per-tile!)
        add.l   d3,d4                   ; slot = sy*34 + sx
        move.l  d0,d3
        swap    d3
        clr.w   d3
        or.l    d1,d3                   ; key = (tx<<16) | ty
        cmp.l   (a0,d4.l*4),d3
        tst.b   (a1,d4.l)
        addq.l  #1,d0
