; seq/DCT-109.i - MP3 18-Sample Overlap-Add with Interleaved Memory-VEA
; Uses: e1, e2, e3, e4, d0, a0, a1, a2
; MUST NOT touch: d5, d6, d7, a5, a6, a7
        load    (a1),e1
        load    8(a1),e3
        paddw   (a0),e1,e2
        paddw   8(a0),e3,e4
        store   e2,(a2)
        store   e4,8(a2)
        load    16(a1),e1
        load    24(a1),e3
        paddw   16(a0),e1,e2
        paddw   24(a0),e3,e4
        store   e2,16(a2)
        store   e4,24(a2)
        move.l  32(a0),d0
        add.w   32(a1),d0
        swap    d0
        add.w   34(a1),d0
        swap    d0
        move.l  d0,32(a2)
