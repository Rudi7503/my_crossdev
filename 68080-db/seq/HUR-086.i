; seq/HUR-086.i - Buffer zeroing: 64B via compact loop with single clr.l (a2)+ and dbra.l (16 iters)
; Tests penalty of single-store loop without pairing
; Uses: d0, a2. MUST NOT touch: d5, d6, d7, a5, a6, a7
        ifnd    HUR086_MAC
HUR086_MAC equ 1
HUR086_BODY macro
        moveq   #16-1,d0
.l\@:   clr.l   (a2)+
        dbra.l  d0,.l\@
        endm
        endif
        HUR086_BODY
