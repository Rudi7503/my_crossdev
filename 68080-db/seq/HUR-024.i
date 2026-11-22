; seq/HUR-024.i - Hurrican UpdateShots original 2-pass scan (pass 1 update check + pass 2 ActList build)
; Uses: d0, d1, d2, a0, a1, a2
; MUST NOT touch: d5, d6, d7, a5, a6, a7
        move.l  (a0),d0
        beq.s   *+4
        addq.l  #1,d1
        lea     64(a0),a0
        tst.l   (a2)
        beq.s   *+6
        move.l  a2,(a1)+
        addq.l  #1,d2
        lea     64(a2),a2
