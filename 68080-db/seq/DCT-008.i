; seq/DCT-008.i - 8x8 Block Quantization Kernel via AMMX SIMD (Q1.15 PMULH)
; Fully unrolled 16-quadword vector quantization (64 int16 points)
; Inputs:  a0 = src matrix (8x8 int16, 128 bytes), a1 = dst matrix (8x8 int16, 128 bytes)
; Constants: a2 = recip_q table (8x8 int16, 128 bytes)
; Uses:    e0, e1, e2, e3, e4, e5, e6, e7, e8, e9, e10, e11, a0, a1, a2
; MUST NOT touch: d5, d6, d7, a5, a6, a7

        ; Quadwords 0..3 (Rows 0..1)
        load    (a0),e0
        load    (a2),e1
        pmulh   e0,e1,e2
        paddw   e2,e2,e2
        store   e2,(a1)

        load    8(a0),e3
        load    8(a2),e4
        pmulh   e3,e4,e5
        paddw   e5,e5,e5
        store   e5,8(a1)

        load    16(a0),e6
        load    16(a2),e7
        pmulh   e6,e7,e8
        paddw   e8,e8,e8
        store   e8,16(a1)

        load    24(a0),e9
        load    24(a2),e10
        pmulh   e9,e10,e11
        paddw   e11,e11,e11
        store   e11,24(a1)

        ; Quadwords 4..7 (Rows 2..3)
        load    32(a0),e0
        load    32(a2),e1
        pmulh   e0,e1,e2
        paddw   e2,e2,e2
        store   e2,32(a1)

        load    40(a0),e3
        load    40(a2),e4
        pmulh   e3,e4,e5
        paddw   e5,e5,e5
        store   e5,40(a1)

        load    48(a0),e6
        load    48(a2),e7
        pmulh   e6,e7,e8
        paddw   e8,e8,e8
        store   e8,48(a1)

        load    56(a0),e9
        load    56(a2),e10
        pmulh   e9,e10,e11
        paddw   e11,e11,e11
        store   e11,56(a1)

        ; Quadwords 8..11 (Rows 4..5)
        load    64(a0),e0
        load    64(a2),e1
        pmulh   e0,e1,e2
        paddw   e2,e2,e2
        store   e2,64(a1)

        load    72(a0),e3
        load    72(a2),e4
        pmulh   e3,e4,e5
        paddw   e5,e5,e5
        store   e5,72(a1)

        load    80(a0),e6
        load    80(a2),e7
        pmulh   e6,e7,e8
        paddw   e8,e8,e8
        store   e8,80(a1)

        load    88(a0),e9
        load    88(a2),e10
        pmulh   e9,e10,e11
        paddw   e11,e11,e11
        store   e11,88(a1)

        ; Quadwords 12..15 (Rows 6..7)
        load    96(a0),e0
        load    96(a2),e1
        pmulh   e0,e1,e2
        paddw   e2,e2,e2
        store   e2,96(a1)

        load    104(a0),e3
        load    104(a2),e4
        pmulh   e3,e4,e5
        paddw   e5,e5,e5
        store   e5,104(a1)

        load    112(a0),e6
        load    112(a2),e7
        pmulh   e6,e7,e8
        paddw   e8,e8,e8
        store   e8,112(a1)

        load    120(a0),e9
        load    120(a2),e10
        pmulh   e9,e10,e11
        paddw   e11,e11,e11
        store   e11,120(a1)
