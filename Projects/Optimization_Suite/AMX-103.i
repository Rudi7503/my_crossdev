; seq/AMX-103.i - 2x unrolled PADDUSB with Memory-VEA (16 bytes)
; Uses: e1, e2, e3, e4, a0, a1, a2
; MUST NOT touch: d5, d6, d7, a5, a6, a7
        load    (a1)+,e1
        load    (a1)+,e3
        paddusb (a0)+,e1,e2
        paddusb (a0)+,e3,e4
        store   e2,(a2)+
        store   e4,(a2)+
