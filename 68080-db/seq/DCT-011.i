; seq/DCT-011.i - Fused MPEG Video Motion Compensation (IDCT Transpose + Residual Add + PACKUSWB Store)
; Transposes 8x8 IDCT column results, restores 1.0x scale, adds 8-bit reference prediction pixels,
; saturates to unsigned 8-bit [0..255] via PACKUSWB, and stores 64 bytes directly into framebuffer.
; Inputs:  a0 = input 8x8 signed 16-bit IDCT column buffer (128 bytes, 8-byte aligned)
;          a1 = destination 64-byte unsigned 8-bit reconstructed buffer (64 bytes, 8-byte aligned)
;          a2 = reference 64-byte unsigned 8-bit prediction buffer (64 bytes, 8-byte aligned)
;          a3 = scratch buffer for zero vector (8 bytes, 8-byte aligned)
; Uses:    e0-e15, e23, d0, a0, a1, a2, a3
; MUST NOT touch: d5, d6, d7, a5, a6, a7

        ; Setup zero vector in e23
        moveq   #0,d0
        move.l  d0,(a3)
        move.l  d0,4(a3)
        load    (a3),e23

        ; --- Transpose Top Half (Rows 0..3) ---
        load    (a0),e0
        load    16(a0),e1
        load    32(a0),e2
        load    48(a0),e3
        vperm   #$018923ab,e0,e1,e4
        vperm   #$45cd67ef,e0,e1,e5
        vperm   #$018923ab,e2,e3,e6
        vperm   #$45cd67ef,e2,e3,e7
        vperm   #$012389ab,e4,e6,e8     ; Row 0 [0..3]
        vperm   #$4567cdef,e4,e6,e9     ; Row 1 [0..3]
        vperm   #$012389ab,e5,e7,e10    ; Row 2 [0..3]
        vperm   #$4567cdef,e5,e7,e11    ; Row 3 [0..3]

        load    64(a0),e0
        load    80(a0),e1
        load    96(a0),e2
        load    112(a0),e3
        vperm   #$018923ab,e0,e1,e4
        vperm   #$45cd67ef,e0,e1,e5
        vperm   #$018923ab,e2,e3,e6
        vperm   #$45cd67ef,e2,e3,e7
        vperm   #$012389ab,e4,e6,e12    ; Row 0 [4..7]
        vperm   #$4567cdef,e4,e6,e13    ; Row 1 [4..7]
        vperm   #$012389ab,e5,e7,e14    ; Row 2 [4..7]
        vperm   #$4567cdef,e5,e7,e15    ; Row 3 [4..7]

        ; Double to restore 1.0x scale + Unpack Ref Prediction + Add Residual + PACKUSWB Store
        ; Row 0
        paddw   e8,e8,e8
        paddw   e12,e12,e12
        load    (a2),e0
        vperm   #$08090a0b,e23,e0,e1
        vperm   #$0c0d0e0f,e23,e0,e2
        paddw   e1,e8,e8
        paddw   e2,e12,e12
        packuswb e8,e12,e0
        store   e0,(a1)

        ; Row 1
        paddw   e9,e9,e9
        paddw   e13,e13,e13
        load    8(a2),e0
        vperm   #$08090a0b,e23,e0,e1
        vperm   #$0c0d0e0f,e23,e0,e2
        paddw   e1,e9,e9
        paddw   e2,e13,e13
        packuswb e9,e13,e1
        store   e1,8(a1)

        ; Row 2
        paddw   e10,e10,e10
        paddw   e14,e14,e14
        load    16(a2),e0
        vperm   #$08090a0b,e23,e0,e1
        vperm   #$0c0d0e0f,e23,e0,e2
        paddw   e1,e10,e10
        paddw   e2,e14,e14
        packuswb e10,e14,e2
        store   e2,16(a1)

        ; Row 3
        paddw   e11,e11,e11
        paddw   e15,e15,e15
        load    24(a2),e0
        vperm   #$08090a0b,e23,e0,e1
        vperm   #$0c0d0e0f,e23,e0,e2
        paddw   e1,e11,e11
        paddw   e2,e15,e15
        packuswb e11,e15,e3
        store   e3,24(a1)

        ; --- Transpose Bottom Half (Rows 4..7) ---
        load    8(a0),e0
        load    24(a0),e1
        load    40(a0),e2
        load    56(a0),e3
        vperm   #$018923ab,e0,e1,e4
        vperm   #$45cd67ef,e0,e1,e5
        vperm   #$018923ab,e2,e3,e6
        vperm   #$45cd67ef,e2,e3,e7
        vperm   #$012389ab,e4,e6,e8     ; Row 4 [0..3]
        vperm   #$4567cdef,e4,e6,e9     ; Row 5 [0..3]
        vperm   #$012389ab,e5,e7,e10    ; Row 6 [0..3]
        vperm   #$4567cdef,e5,e7,e11    ; Row 7 [0..3]

        load    72(a0),e0
        load    88(a0),e1
        load    104(a0),e2
        load    120(a0),e3
        vperm   #$018923ab,e0,e1,e4
        vperm   #$45cd67ef,e0,e1,e5
        vperm   #$018923ab,e2,e3,e6
        vperm   #$45cd67ef,e2,e3,e7
        vperm   #$012389ab,e4,e6,e12    ; Row 4 [4..7]
        vperm   #$4567cdef,e4,e6,e13    ; Row 5 [4..7]
        vperm   #$012389ab,e5,e7,e14    ; Row 6 [4..7]
        vperm   #$4567cdef,e5,e7,e15    ; Row 7 [4..7]

        ; Row 4
        paddw   e8,e8,e8
        paddw   e12,e12,e12
        load    32(a2),e0
        vperm   #$08090a0b,e23,e0,e1
        vperm   #$0c0d0e0f,e23,e0,e2
        paddw   e1,e8,e8
        paddw   e2,e12,e12
        packuswb e8,e12,e0
        store   e0,32(a1)

        ; Row 5
        paddw   e9,e9,e9
        paddw   e13,e13,e13
        load    40(a2),e0
        vperm   #$08090a0b,e23,e0,e1
        vperm   #$0c0d0e0f,e23,e0,e2
        paddw   e1,e9,e9
        paddw   e2,e13,e13
        packuswb e9,e13,e1
        store   e1,40(a1)

        ; Row 6
        paddw   e10,e10,e10
        paddw   e14,e14,e14
        load    48(a2),e0
        vperm   #$08090a0b,e23,e0,e1
        vperm   #$0c0d0e0f,e23,e0,e2
        paddw   e1,e10,e10
        paddw   e2,e14,e14
        packuswb e10,e14,e2
        store   e2,48(a1)

        ; Row 7
        paddw   e11,e11,e11
        paddw   e15,e15,e15
        load    56(a2),e0
        vperm   #$08090a0b,e23,e0,e1
        vperm   #$0c0d0e0f,e23,e0,e2
        paddw   e1,e11,e11
        paddw   e2,e15,e15
        packuswb e11,e15,e3
        store   e3,56(a1)
