; seq/DCT-004.i - 4x4 16-bit Matrix Transpose Kernel via AMMX vperm (8 vperms, 4 loads, 4 stores)
; Inputs:  a0 = matrix (4 quadwords = 32 bytes)
; Output:  a1 = transposed matrix (4 quadwords = 32 bytes)
; Uses:    e0, e1, e2, e3, e4, e5, e6, e7, e8, e9, e10, e11, a0, a1
; MUST NOT touch: d5, d6, d7, a5, a6, a7

        load    (a0),e0                 ; row 0: [a0, a1, a2, a3]
        load    8(a0),e1                ; row 1: [b0, b1, b2, b3]
        load    16(a0),e2               ; row 2: [c0, c1, c2, c3]
        load    24(a0),e3               ; row 3: [d0, d1, d2, d3]

        vperm   #$018923ab,e0,e1,e4     ; T0 = [a0, b0, a1, b1]
        vperm   #$45cd67ef,e0,e1,e5     ; T1 = [a2, b2, a3, b3]
        vperm   #$018923ab,e2,e3,e6     ; T2 = [c0, d0, c1, d1]
        vperm   #$45cd67ef,e2,e3,e7     ; T3 = [c2, d2, c3, d3]

        vperm   #$012389ab,e4,e6,e8     ; col 0 = [a0, b0, c0, d0]
        vperm   #$4567cdef,e4,e6,e9     ; col 1 = [a1, b1, c1, d1]
        vperm   #$012389ab,e5,e7,e10    ; col 2 = [a2, b2, c2, d2]
        vperm   #$4567cdef,e5,e7,e11    ; col 3 = [a3, b3, c3, d3]

        store   e8,(a1)
        store   e9,8(a1)
        store   e10,16(a1)
        store   e11,24(a1)
