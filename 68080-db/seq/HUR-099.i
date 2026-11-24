; seq/HUR-099.i - tilemap.s: Optimized ParBCov with ParBCovRow (8 blocks: row-hoisted pointer, no per-block mulu/clipping)
; Uses: d0-d4, a0-a3. MUST NOT touch: d5, d6, d7, a5, a6, a7
        ifnd    HUR099_MAC
HUR099_MAC equ 1
HUR099_BODY macro
        ; Setup mock ParBMap at a4 (20x30 bytes)
        clr.l   (a4)
        clr.l   4(a4)
        clr.l   20(a4)
        clr.l   24(a4)
        move.b  #1,20+2(a4)     ; bx=2 opaque
        move.b  #1,20+3(a4)     ; bx=3 opaque

        ; --- Hoisted ParBCovRow setup (once per row: y=48, parY=32) ---
        ; by0 = (48 - 32) >> 4 = 1. by0 * 20 = 20.
        lea     20(a4),a3       ; a3 = ParBPtr (Zeilenanfang in ParBMap)
        moveq   #0,d1           ; ParBRows = by1 - by0 = 0 (1 Zeile)

        moveq   #0,d3           ; d3 = accumulator of visible blocks
        moveq   #0,d4           ; d4 = block x (0, 32, 64, ...)
        moveq   #7,d2           ; 8 blocks loop counter

.blk_loop_99\@:
        ; Inlined / fast ParBCov:
        move.l  d4,d0           ; x0
        bpl.s   .x0ok_99\@
        moveq   #0,d0
.x0ok_99\@:
        move.l  d4,d1
        add.l   #31,d1          ; x1 = min(x+31, 639)
        cmp.l   #639,d1
        ble.s   .x1ok_99\@
        move.l  #639,d1
.x1ok_99\@:
        cmp.l   d0,d1
        blt.s   .nxt_99\@       ; x1 < x0 -> skip (not visible)
        ; add ParBS (45)
        add.l   #45,d0          ; u0 = (x0 + parS) % 640
        cmp.l   #640,d0
        blt.s   .u0ok_99\@
        sub.l   #640,d0
.u0ok_99\@:
        lsr.l   #5,d0           ; bx0 (0..19)
        add.l   #45,d1          ; u1 = (x1 + parS) % 640
        cmp.l   #640,d1
        blt.s   .u1ok_99\@
        sub.l   #640,d1
.u1ok_99\@:
        lsr.l   #5,d1           ; bx1 (0..19)

        ; Test coverage in ParBMap:
        cmp.b   #1,(a3,d0.l)
        bne.s   .vis_99\@
        cmp.b   #1,(a3,d1.l)
        bne.s   .vis_99\@
        ; Both edges covered -> completely covered block!
        bra.s   .nxt_99\@
.vis_99\@:
        addq.l  #1,d3           ; visible block!
.nxt_99\@:
        add.l   #32,d4
        dbra.l  d2,.blk_loop_99\@

        move.l  d3,d0           ; output count of visible blocks
        endm
        endif
        HUR099_BODY
