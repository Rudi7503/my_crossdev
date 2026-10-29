; seq/DCT-012.i - Ultra-Fast 4x4 2D Hadamard Transform Kernel in AMMX SIMD
; Fully in-register 2D 4x4 Hadamard Transform (H4 x Matrix x H4^T)
; Used in H.264 / AVC for Intra-16x16 DC and Chroma DC transform.
; Inputs:  a0 = input 4x4 signed 16-bit matrix (32 bytes, 8-byte aligned)
;          a1 = destination 4x4 signed 16-bit matrix (32 bytes, 8-byte aligned)
; Uses:    e0-e7, a0, a1
; MUST NOT touch: d5, d6, d7, a5, a6, a7

        ; Load 4 rows into e0..e3
        load    (a0),e0
        load    8(a0),e1
        load    16(a0),e2
        load    24(a0),e3

        ; --- Pass 1: 1D Hadamard on Columns (Dual-Issue Superscalar) ---
        paddw   e0,e3,e4                ; e4 = x0 + x3
        psubw   e3,e0,e7                ; e7 = x0 - x3
        paddw   e1,e2,e5                ; e5 = x1 + x2
        psubw   e2,e1,e6                ; e6 = x1 - x2
        paddw   e4,e5,e0                ; row 0 = (x0+x3) + (x1+x2)
        psubw   e5,e4,e2                ; row 2 = (x0+x3) - (x1+x2)
        paddw   e7,e6,e1                ; row 1 = (x0-x3) + (x1-x2)
        psubw   e6,e7,e3                ; row 3 = (x0-x3) - (x1-x2)

        ; --- In-Register 4x4 Matrix Transposition via AMMX vperm ---
        vperm   #$018923ab,e0,e1,e4     ; T0 = [r00, r10, r01, r11]
        vperm   #$45cd67ef,e0,e1,e5     ; T1 = [r02, r12, r03, r13]
        vperm   #$018923ab,e2,e3,e6     ; T2 = [r20, r30, r21, r31]
        vperm   #$45cd67ef,e2,e3,e7     ; T3 = [r22, r32, r23, r33]
        vperm   #$012389ab,e4,e6,e0     ; Col 0: [r00, r10, r20, r30]
        vperm   #$4567cdef,e4,e6,e1     ; Col 1: [r01, r11, r21, r31]
        vperm   #$012389ab,e5,e7,e2     ; Col 2: [r02, r12, r22, r32]
        vperm   #$4567cdef,e5,e7,e3     ; Col 3: [r03, r13, r23, r33]

        ; --- Pass 2: 1D Hadamard on Rows ---
        paddw   e0,e3,e4
        psubw   e3,e0,e7
        paddw   e1,e2,e5
        psubw   e2,e1,e6
        paddw   e4,e5,e0                ; Final row 0
        psubw   e5,e4,e2                ; Final row 2
        paddw   e7,e6,e1                ; Final row 1
        psubw   e6,e7,e3                ; Final row 3

        ; Store 4 transformed rows directly
        store   e0,(a1)
        store   e1,8(a1)
        store   e2,16(a1)
        store   e3,24(a1)
