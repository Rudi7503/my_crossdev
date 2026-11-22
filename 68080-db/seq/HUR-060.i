; seq/HUR-060.i - Hurrican _BlitKey 8-px (.k2) with All-Key-Skip (all-key source = transparent parallax sky, key=0 in scratch_a)
; Uses: d0, e0..e5, a0, a1. MUST NOT touch: d5, d6, d7, a5, a6, a7
        ifnd    HUR060_MAC
HUR060_MAC equ 1
HUR060_BODY macro
        moveq   #0,d0                   ; key pair (0 matches zeroed scratch_a)
        cmp.l   (a0),d0
        bne.s   .do\@
        cmp.l   4(a0),d0
        bne.s   .do\@
        cmp.l   8(a0),d0
        bne.s   .do\@
        cmp.l   12(a0),d0
        bne.s   .do\@
        lea     16(a0),a0
        lea     16(a1),a1
        bra.s   .dn\@
.do\@:  load    (a0)+,e2
        load    (a0)+,e4
        pcmpeqw e0,e2,e3
        pcmpeqw e0,e4,e5
        storeilm e2,e3,(a1)+
        storeilm e4,e5,(a1)+
.dn\@:
        endm
        endif
        HUR060_BODY
