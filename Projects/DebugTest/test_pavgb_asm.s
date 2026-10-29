        section .text,code
        xdef    _test_pavgb_asm

_test_pavgb_asm:
        move.l  4(sp),a0                ; src1
        move.l  8(sp),a1                ; src2
        move.l  12(sp),a2               ; dst

        load    (a0),e0
        load    (a1),e1
        pavgb   e1,e0,e2
        store   e2,(a2)
        rts
