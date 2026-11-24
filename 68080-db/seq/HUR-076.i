; seq/HUR-076.i - Hurrican _TileCache Phase 2 .re run setup (optimized: 0 mulu.l, shift-add)
; Uses: d0..d4, a0..a3. MUST NOT touch: d5, d6, d7, a5, a6, a7
        ifnd    HUR076_MAC
HUR076_MAC equ 1
HUR076_BODY macro
        moveq   #5,d0                   ; a3 = sx0 = 5
        move.l  d0,a3
        moveq   #3,d0                   ; a2 = c0 = 3
        move.l  d0,a2
        moveq   #9,d2                   ; d2 = c = 9

        ; Optimized Phase 2 .re setup (0 mulu.l, Dual-Issue superscalar shift-adds)
        move.l  a3,d0                   ; sx0
        move.l  d0,d1
        lsl.l   #2,d0                   ; 4 * sx0
        add.l   d1,d0                   ; 5 * sx0
        lsl.l   #3,d0                   ; 40 * sx0
        move.l  a4,a0
        add.l   d0,a0                   ; a0 = TcBufRow + sx0*40

        move.l  a2,d0                   ; c0
        move.l  d0,d1
        lsl.l   #2,d0                   ; 4 * c0
        add.l   d1,d0                   ; 5 * c0
        lsl.l   #2,d0                   ; 20 * c0
        sub.l   #12,d0                  ; x = c0*20 - TcOX
        move.l  #100,d1                 ; y = TcRowY

        sub.l   a2,d2                   ; d2 = c - c0
        move.l  d2,d3
        lsl.l   #2,d3                   ; 4 * w
        add.l   d2,d3                   ; 5 * w
        lsl.l   #2,d3                   ; 20 * w
        move.l  d3,d2                   ; width = 20 * (c - c0)

        moveq   #20,d3                  ; TILE_H
        move.l  #1360,d4                ; TC_PITCH
        endm
        endif
        HUR076_BODY
