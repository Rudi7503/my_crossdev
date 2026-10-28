; seq/FIX-002.i - 3x3 16-bit fixed-point matrix multiplication (C = A * B)
; Fully unrolled 64-bit AMMX SIMD execution
; Inputs:  a0 = matrix A (3x3 words = 18 bytes), a1 = matrix B (3x3 words = 18 bytes)
; Output:  a2 = matrix C (3x3 words = 18 bytes)
; Uses:    e0, e1, e2, e4, e5, e6, e8, e9, e10, e12, e13, e14, a0, a1, a2
; MUST NOT touch: d5, d6, d7, a5, a6, a7

        ; Load Matrix A (3 rows of 3 words, each row in 6 bytes / 64-bit load)
        load    (a0),e0         ; Row 0: [a00, a01, a02, x]
        load    6(a0),e1        ; Row 1: [a10, a11, a12, x]
        load    12(a0),e2       ; Row 2: [a20, a21, a22, x]

        ; Load Matrix B (3 rows of 3 words)
        load    (a1),e4         ; Row 0: [b00, b01, b02, x]
        load    6(a1),e5        ; Row 1: [b10, b11, b12, x]
        load    12(a1),e6       ; Row 2: [b20, b21, b22, x]

        ; ==================== ROW 0 ====================
        vperm   #$01010101,e0,e0,e8   ; a00
        vperm   #$23232323,e0,e0,e9   ; a01
        vperm   #$45454545,e0,e0,e10  ; a02

        pmulh   e8,e4,e12       ; a00 * Row 0(B)
        pmulh   e9,e5,e13       ; a01 * Row 1(B)
        pmulh   e10,e6,e14      ; a02 * Row 2(B)

        paddw   e12,e13,e12
        paddw   e12,e14,e12     ; Row 0 complete!
        store   e12,(a2)

        ; ==================== ROW 1 ====================
        vperm   #$01010101,e1,e1,e8   ; a10
        vperm   #$23232323,e1,e1,e9   ; a11
        vperm   #$45454545,e1,e1,e10  ; a12

        pmulh   e8,e4,e12       ; a10 * Row 0(B)
        pmulh   e9,e5,e13       ; a11 * Row 1(B)
        pmulh   e10,e6,e14      ; a12 * Row 2(B)

        paddw   e12,e13,e12
        paddw   e12,e14,e12     ; Row 1 complete!
        store   e12,6(a2)

        ; ==================== ROW 2 ====================
        vperm   #$01010101,e2,e2,e8   ; a20
        vperm   #$23232323,e2,e2,e9   ; a21
        vperm   #$45454545,e2,e2,e10  ; a22

        pmulh   e8,e4,e12       ; a20 * Row 0(B)
        pmulh   e9,e5,e13       ; a21 * Row 1(B)
        pmulh   e10,e6,e14      ; a22 * Row 2(B)

        paddw   e12,e13,e12
        paddw   e12,e14,e12     ; Row 2 complete!
        store   e12,12(a2)
