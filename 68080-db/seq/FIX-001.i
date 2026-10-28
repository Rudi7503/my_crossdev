; seq/FIX-001.i - 2x2 16-bit fixed-point matrix multiplication (C = A * B)
; Fully unrolled 64-bit AMMX SIMD execution
; Inputs:  a0 = matrix A (4 words = 8 bytes), a1 = matrix B (4 words = 8 bytes)
; Output:  a2 = matrix C (4 words = 8 bytes)
; Uses:    e0, e1, e2, e3, e4, e5, e6, e7, e8, e9, e10, e11, a0, a1, a2
; MUST NOT touch: d5, d6, d7, a5, a6, a7

        ; Load Matrix A (4 words in one 64-bit load!)
        load    (a0),e0         ; [a00, a01, a10, a11]
        ; Load Matrix B (4 words in one 64-bit load!)
        load    (a1),e1         ; [b00, b01, b10, b11]

        ; Broadcast elements of A
        vperm   #$01010101,e0,e0,e2   ; a00
        vperm   #$23232323,e0,e0,e3   ; a01
        vperm   #$45454545,e0,e0,e4   ; a10
        vperm   #$67676767,e0,e0,e5   ; a11

        ; Duplicate rows of B
        vperm   #$01230123,e1,e1,e6   ; [b00, b01, b00, b01]
        vperm   #$45674567,e1,e1,e7   ; [b10, b11, b10, b11]

        ; Row 0: c00, c01
        pmulh   e2,e6,e8        ; a00 * [b00, b01, ...]
        pmulh   e3,e7,e9        ; a01 * [b10, b11, ...]
        paddw   e8,e9,e8        ; [c00, c01, c00, c01]

        ; Row 1: c10, c11
        pmulh   e4,e6,e10       ; a10 * [b00, b01, ...]
        pmulh   e5,e7,e11       ; a11 * [b10, b11, ...]
        paddw   e10,e11,e10     ; [c10, c11, c10, c11]

        ; Pack into single 4-word matrix C: [c00, c01, c10, c11]
        vperm   #$012389ab,e8,e10,e0
        store   e0,(a2)
