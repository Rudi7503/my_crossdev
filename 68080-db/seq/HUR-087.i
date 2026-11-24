; seq/HUR-087.i - Buffer zeroing: 64B via compact loop with AMMX store e0,(a2)+ and dbra.l (8 iters)
; Tests compact 8-byte AMMX store loop with Pipe 2 branch folding
; Uses: d0, a2, e0. MUST NOT touch: d5, d6, d7, a5, a6, a7
        ifnd    HUR087_MAC
HUR087_MAC equ 1
HUR087_BODY macro
        moveq   #8-1,d0
.l\@:   store   e0,(a2)+
        dbra.l  d0,.l\@
        endm
        endif
        HUR087_BODY
