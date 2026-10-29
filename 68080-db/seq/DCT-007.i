; seq/DCT-007.i - 8x8 16-Bit Matrix Transpose Kernel via AMMX SIMD (VPERM)
; Transposes 64 int16 words (128 bytes) in-register using two-stage VPERM permutation network
; Inputs:  a0 = src matrix (8x8 int16, 128 bytes), a1 = dst matrix (8x8 int16, 128 bytes)
; Uses:    e0, e1, e2, e3, e4, e5, e6, e7, e8, e9, e10, e11, e12, e13, e14, e15, a0, a1
; MUST NOT touch: d5, d6, d7, a5, a6, a7

        ; Transpose Top-Left 4x4 (rows 0..3, cols 0..3) & Top-Right 4x4 (rows 0..3, cols 4..7)
        load    (a0),e0                 ; row 0, cols 0..3
        load    16(a0),e1               ; row 1, cols 0..3
        load    32(a0),e2               ; row 2, cols 0..3
        load    48(a0),e3               ; row 3, cols 0..3

        ; Stage 1: 2x2 word interleaving
        vperm   #$018923ab,e0,e1,e4
        vperm   #$45cd67ef,e0,e1,e5
        vperm   #$018923ab,e2,e3,e6
        vperm   #$45cd67ef,e2,e3,e7

        ; Stage 2: 4x4 transposition
        vperm   #$012389ab,e4,e6,e8     ; trans row 0 (cols 0..3)
        vperm   #$4567cdef,e4,e6,e9     ; trans row 1 (cols 0..3)
        vperm   #$012389ab,e5,e7,e10    ; trans row 2 (cols 0..3)
        vperm   #$4567cdef,e5,e7,e11    ; trans row 3 (cols 0..3)

        ; Rows 4..7, cols 0..3
        load    64(a0),e0               ; row 4, cols 0..3
        load    80(a0),e1               ; row 5, cols 0..3
        load    96(a0),e2               ; row 6, cols 0..3
        load    112(a0),e3              ; row 7, cols 0..3

        vperm   #$018923ab,e0,e1,e4
        vperm   #$45cd67ef,e0,e1,e5
        vperm   #$018923ab,e2,e3,e6
        vperm   #$45cd67ef,e2,e3,e7

        vperm   #$012389ab,e4,e6,e12    ; trans row 0 (cols 4..7)
        vperm   #$4567cdef,e4,e6,e13    ; trans row 1 (cols 4..7)
        vperm   #$012389ab,e5,e7,e14    ; trans row 2 (cols 4..7)
        vperm   #$4567cdef,e5,e7,e15    ; trans row 3 (cols 4..7)

        ; Store transposed rows 0..3 (8 quadwords)
        store   e8,(a1)                 ; row 0, first half
        store   e12,8(a1)               ; row 0, second half
        store   e9,16(a1)               ; row 1, first half
        store   e13,24(a1)              ; row 1, second half
        store   e10,32(a1)              ; row 2, first half
        store   e14,40(a1)              ; row 2, second half
        store   e11,48(a1)              ; row 3, first half
        store   e15,56(a1)              ; row 3, second half

        ; Rows 0..3, cols 4..7
        load    8(a0),e0
        load    24(a0),e1
        load    40(a0),e2
        load    56(a0),e3

        vperm   #$018923ab,e0,e1,e4
        vperm   #$45cd67ef,e0,e1,e5
        vperm   #$018923ab,e2,e3,e6
        vperm   #$45cd67ef,e2,e3,e7

        vperm   #$012389ab,e4,e6,e8     ; trans row 4 (cols 0..3)
        vperm   #$4567cdef,e4,e6,e9     ; trans row 5 (cols 0..3)
        vperm   #$012389ab,e5,e7,e10    ; trans row 6 (cols 0..3)
        vperm   #$4567cdef,e5,e7,e11    ; trans row 7 (cols 0..3)

        ; Rows 4..7, cols 4..7
        load    72(a0),e0
        load    88(a0),e1
        load    104(a0),e2
        load    120(a0),e3

        vperm   #$018923ab,e0,e1,e4
        vperm   #$45cd67ef,e0,e1,e5
        vperm   #$018923ab,e2,e3,e6
        vperm   #$45cd67ef,e2,e3,e7

        vperm   #$012389ab,e4,e6,e12    ; trans row 4 (cols 4..7)
        vperm   #$4567cdef,e4,e6,e13    ; trans row 5 (cols 4..7)
        vperm   #$012389ab,e5,e7,e14    ; trans row 6 (cols 4..7)
        vperm   #$4567cdef,e5,e7,e15    ; trans row 7 (cols 4..7)

        ; Store transposed rows 4..7 (8 quadwords)
        store   e8,64(a1)
        store   e12,72(a1)
        store   e9,80(a1)
        store   e13,88(a1)
        store   e10,96(a1)
        store   e14,104(a1)
        store   e11,112(a1)
        store   e15,120(a1)
