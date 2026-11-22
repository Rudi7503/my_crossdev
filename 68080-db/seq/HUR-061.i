; seq/HUR-061.i - Hurrican _BlitAdd 8-px (.a2) tst.l early-exit Zero-Skip (all-zero source)
; Uses: e2..e15, a0, a1. MUST NOT touch: d5, d6, d7, a5, a6, a7
        ifnd    HUR061_MAC
HUR061_MAC equ 1
HUR061_BODY macro
        tst.l   (a0)
        bne.s   .do\@
        tst.l   4(a0)
        bne.s   .do\@
        tst.l   8(a0)
        bne.s   .do\@
        tst.l   12(a0)
        bne.s   .do\@
        lea     16(a0),a0
        lea     16(a1),a1
        bra.s   .dn\@
.do\@:  load    (a0)+,e2
        load    (a0)+,e10
        load    (a1),e3
        load    8(a1),e11
        unpack1632 e2,e4:e5
        unpack1632 e10,e12:e13
        unpack1632 e3,e6:e7
        unpack1632 e11,e14:e15
        paddusb e4,e6,e6
        paddusb e12,e14,e14
        paddusb e5,e7,e7
        paddusb e13,e15,e15
        pack3216 e6,e7,e3
        pack3216 e14,e15,e11
        store   e3,(a1)+
        store   e11,(a1)+
.dn\@:
        endm
        endif
        HUR061_BODY
