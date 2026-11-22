; seq/HUR-053.i - Hurrican _TileCache Pass 1 (.pc) optimized: incremental sx/slot wrap + key += $10000 (0 div, 0 mul)
; Uses: d0..d4, a0..a4. MUST NOT touch: d5, d6, d7, a5, a6, a7
; a0 = SlKey, a1 = SlAnim, d0 = key, d1 = slot, d2 = sx
        ifnd    HUR053_MAC
HUR053_MAC equ 1
HUR053_BODY macro
        cmp.l   (a0,d1.l*4),d0
        tst.b   (a1,d1.l)
        add.l   #$00010000,d0
        addq.l  #1,d1
        addq.l  #1,d2
        cmp.l   #34,d2
        blt.s   .nw\@
        moveq   #0,d2
        sub.l   #34,d1
.nw\@:
        endm
        endif
        HUR053_BODY
