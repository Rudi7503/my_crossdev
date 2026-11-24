; seq/HUR-081.i - Buffer zeroing: 64 bytes via paired clr.l (a2)+ (8 pairs = 16 stores)
; Tests instruction fusion / superscalar issue on 68080
; Uses: a2. MUST NOT touch: d5, d6, d7, a5, a6, a7
        ifnd    HUR081_MAC
HUR081_MAC equ 1
HUR081_BODY macro
        clr.l   (a2)+
        clr.l   (a2)+
        clr.l   (a2)+
        clr.l   (a2)+
        clr.l   (a2)+
        clr.l   (a2)+
        clr.l   (a2)+
        clr.l   (a2)+
        clr.l   (a2)+
        clr.l   (a2)+
        clr.l   (a2)+
        clr.l   (a2)+
        clr.l   (a2)+
        clr.l   (a2)+
        clr.l   (a2)+
        clr.l   (a2)+
        endm
        endif
        HUR081_BODY
