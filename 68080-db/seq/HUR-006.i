; seq/HUR-006.i - Hurrican AudHandler .sm original 1-byte signed mix + branch clamp (-128..127)
; Uses: d1, d2, a0, a1
; MUST NOT touch: d5, d6, d7, a5, a6, a7
        move.b  (a0),d1
        ext.w   d1
        move.b  (a1)+,d2
        ext.w   d2
        add.w   d2,d1
        cmp.w   #127,d1
        ble.s   *+4
        moveq   #127,d1
        cmp.w   #-128,d1
        bge.s   *+4
        moveq   #-128,d1
        move.b  d1,(a0)+
