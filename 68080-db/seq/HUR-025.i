; seq/HUR-025.i - Hurrican UpdateShots single-pass merged (update + ActList in one scan)
; Uses: d0, d1, a0, a1
; MUST NOT touch: d5, d6, d7, a5, a6, a7
        move.l  (a0),d0
        beq.s   *+6
        move.l  a0,(a1)+
        addq.l  #1,d1
        lea     64(a0),a0
