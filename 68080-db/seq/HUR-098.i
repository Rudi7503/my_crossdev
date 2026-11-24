; seq/HUR-098.i - tilemap.s: Original ParBCov + ParRectCov (8 blocks: per-block y-clipping, asr.l #4, mulu.w #20, stack save)
; Uses: d0-d4, a0-a3. MUST NOT touch: d5, d6, d7, a5, a6, a7
        ifnd    HUR098_MAC
HUR098_MAC equ 1
HUR098_BODY macro
        ; Setup mock ParBMap at a4 (20x30 bytes)
        clr.l   (a4)
        clr.l   4(a4)
        clr.l   20(a4)
        clr.l   24(a4)
        move.b  #1,20+2(a4)     ; bx=2 opaque
        move.b  #1,20+3(a4)     ; bx=3 opaque

        moveq   #0,d3           ; d3 = accumulator of visible blocks
        moveq   #0,d4           ; d4 = block x (0, 32, 64, ...)
        moveq   #7,d2           ; 8 blocks loop counter

.blk_loop_98\@:
        ; Original ParBCov call:
        move.l  d4,d0           ; x
        moveq   #48,d1          ; y = 48
        bsr.s   .parbcov_98\@
        bne.s   .nxt_98\@       ; Z=0 -> covered, skip
        addq.l  #1,d3           ; Z=1 -> visible!
.nxt_98\@:
        add.l   #32,d4
        dbra.l  d2,.blk_loop_98\@
        bra.s   .end_98\@

.parbcov_98\@:
        move.l  d2,-(sp)
        move.l  d3,-(sp)
        move.l  d4,-(sp)
        move.l  d0,d2           ; x0 = max(x, 0)
        bpl.s   .x0ok_98\@
        moveq   #0,d2
.x0ok_98\@:
        add.l   #31,d0          ; x1 = min(x+31, 639)
        cmp.l   #639,d0
        ble.s   .x1ok_98\@
        move.l  #639,d0
.x1ok_98\@:
        cmp.l   d2,d0
        blt.s   .cov_98\@
        move.l  d1,d3           ; y0 = max(y, 0)
        bpl.s   .y0ok_98\@
        moveq   #0,d3
.y0ok_98\@:
        add.l   #15,d1          ; y1 = min(y+15, 479)
        cmp.l   #479,d1
        ble.s   .y1ok_98\@
        move.l  #479,d1
.y1ok_98\@:
        cmp.l   d3,d1
        blt.s   .cov_98\@
        ; ParRectCov:
        sub.l   #32,d3          ; ParBY = 32
        asr.l   #4,d3           ; by0 = (y0 - parY) >> 4
        bmi.s   .vis_ret_98\@
        sub.l   #32,d1          ; ParBY = 32
        asr.l   #4,d1           ; by1 = (y1 - parY) >> 4
        cmp.l   #30,d1
        bge.s   .vis_ret_98\@
        add.l   #45,d2          ; ParBS = 45 -> u0 = (x0 + parS) % 640
        cmp.l   #640,d2
        blt.s   .u0ok_98\@
        sub.l   #640,d2
.u0ok_98\@:
        lsr.l   #5,d2           ; bx0
        add.l   #45,d0          ; u1 = (x1 + parS) % 640
        cmp.l   #640,d0
        blt.s   .u1ok_98\@
        sub.l   #640,d0
.u1ok_98\@:
        lsr.l   #5,d0           ; bx1
        sub.l   d3,d1           ; by1 - by0
        mulu.w  #20,d3
        lea     (a4),a0         ; mock ParBMap
        add.l   d3,a0
.rl_98\@:
        cmp.b   #1,(a0,d2.l)
        bne.s   .vis_ret_98\@
        cmp.b   #1,(a0,d0.l)
        bne.s   .vis_ret_98\@
        lea     20(a0),a0
        dbra    d1,.rl_98\@
.cov_98\@:
        move.l  (sp)+,d4
        move.l  (sp)+,d3
        move.l  (sp)+,d2
        moveq   #-1,d0          ; Z = 0 -> covered
        rts
.vis_ret_98\@:
        move.l  (sp)+,d4
        move.l  (sp)+,d3
        move.l  (sp)+,d2
        moveq   #0,d0           ; Z = 1 -> visible
        rts

.end_98\@:
        move.l  d3,d0           ; output count of visible blocks
        endm
        endif
        HUR098_BODY
