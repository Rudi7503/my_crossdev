; seq/HUR-075.i - Hurrican _TileCache Phase 2 .re run setup (original: 3x mulu.l)
; Sets up a0 (src), d0 (dst_x), d1 (dst_y), d2 (width_px), d3 (height=20), d4 (stride=1360)
; Inputs for benchmark: a3=sx0(5), a2=c0(3), d2=c(9) -> width=6 tiles
; Uses: d0..d4, a0..a3. MUST NOT touch: d5, d6, d7, a5, a6, a7
        ifnd    HUR075_MAC
HUR075_MAC equ 1
HUR075_BODY macro
        moveq   #5,d0                   ; a3 = sx0 = 5
        move.l  d0,a3
        moveq   #3,d0                   ; a2 = c0 = 3
        move.l  d0,a2
        moveq   #9,d2                   ; d2 = c = 9

        ; Original Phase 2 .re setup (tilemap.s lines 742-753)
        move.l  a4,a0                   ; a0 = TcBufRow
        move.l  a3,d0
        mulu.l  #40,d0                  ; TILE_W*2
        add.l   d0,a0
        move.l  a2,d0                   ; x = c0*20 - ox
        mulu.l  #20,d0                  ; TILE_W
        sub.l   #12,d0                  ; - TcOX
        move.l  #100,d1                 ; TcRowY
        sub.l   a2,d2                   ; Breite = (c - c0) * 20
        mulu.l  #20,d2
        moveq   #20,d3                  ; TILE_H
        move.l  #1360,d4                ; TC_PITCH
        endm
        endif
        HUR075_BODY
