; seq/BLT-044.i - 24-Bit RGB888 Alpha-Blending 8-Pixel Block (24 bytes, VPERM + PMULA + 3x STORE)
; Premul ARGB8888 sprite (a0) over 24-bit RGB888 framebuffer (a1)
; Uses: e0..e13, e15(zero), a0, a1
; MUST NOT touch: d5, d6, d7, a5, a6, a7
        load    (a0)+,e6
        load    (a0)+,e7
        load    (a0)+,e8
        load    (a0)+,e9
        load    (a1),e3
        load    8(a1),e4
        load    16(a1),e5
        vperm   #$80128345,e3,e15,e10
        vperm   #$067809AB,e3,e4,e11
        vperm   #$04560789,e4,e5,e12
        vperm   #$82348567,e5,e15,e13
        pmula   e6,e10,e10
        pmula   e7,e11,e11
        pmula   e8,e12,e12
        pmula   e9,e13,e13
        vperm   #$1235679A,e10,e11,e3
        vperm   #$35679ABD,e11,e12,e4
        vperm   #$679ABDEF,e12,e13,e5
        store   e3,(a1)+
        store   e4,(a1)+
        store   e5,(a1)+
