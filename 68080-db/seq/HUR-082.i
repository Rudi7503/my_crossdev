; seq/HUR-082.i - Buffer zeroing: 64 bytes via AMMX 64-bit store e0,(a2)+ (8 stores)
; Tests native 64-bit AMMX writes to SDRAM
; Uses: a2, e0. MUST NOT touch: d5, d6, d7, a5, a6, a7
        ifnd    HUR082_MAC
HUR082_MAC equ 1
HUR082_BODY macro
        store   e0,(a2)+
        store   e0,(a2)+
        store   e0,(a2)+
        store   e0,(a2)+
        store   e0,(a2)+
        store   e0,(a2)+
        store   e0,(a2)+
        store   e0,(a2)+
        endm
        endif
        HUR082_BODY
