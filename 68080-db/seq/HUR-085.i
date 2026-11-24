; seq/HUR-085.i - Buffer zeroing: 64B via compact loop with paired clr.l (a2)+ and dbra.l on Pipe 2
; Loop body is exactly 8 bytes (1 quadword): fits in a single I-cache fetch slot
; Uses: d0, a2. MUST NOT touch: d5, d6, d7, a5, a6, a7
        ifnd    HUR085_MAC
HUR085_MAC equ 1
HUR085_BODY macro
        moveq   #8-1,d0
.l\@:   clr.l   (a2)+
        clr.l   (a2)+
        dbra.l  d0,.l\@
        endm
        endif
        HUR085_BODY
