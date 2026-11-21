; seq/BLT-041.i - 24-Bit RGB888 Additive Blit via VPERM + PADDUSB + STOREC (2 pixels = 6 bytes)
; Uses: e0..e4, e15(zero), d0(=6), a0, a1
; MUST NOT touch: d5, d6, d7, a5, a6, a7
        load    (a0),e0
        load    (a1),e1
        vperm   #$80128345,e0,e15,e2
        vperm   #$80128345,e1,e15,e3
        paddusb e2,e3,e3
        vperm   #$12356700,e3,e15,e4
        storec  e4,d0,(a1)
        addq.l  #6,a0
        addq.l  #6,a1
