; seq/BLT-039.i - 16-Bit RGB565 Alpha-Blending via unpack1632 + PMULA + pack3216 (4 pixels)
; Premultiplied ARGB8888 sprite (a0) over RGB565 framebuffer (a1)
; Uses: e0, e1, e2, e4, e5, a0, a1
; MUST NOT touch: d5, d6, d7, a5, a6, a7
        load    (a0)+,e0
        load    (a0)+,e1
        load    (a1),e2
        unpack1632 e2,e4:e5
        pmula   e0,e4,e4
        pmula   e1,e5,e5
        pack3216 e4,e5,e2
        store   e2,(a1)+
