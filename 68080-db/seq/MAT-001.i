; seq/MAT-001.i - 2x2 single-precision float matrix multiplication (C = A * B)
; Fully unrolled 32-register 3-operand FPU execution - Zero Pipeline Stalls
; Inputs:  a0 = matrix A (2x2, 16 bytes), a1 = matrix B (2x2, 16 bytes)
; Output:  a2 = matrix C (2x2, 16 bytes)
; Uses:    e0, e1, e2, e3, e4, e5, e6, e7, e8, e9, e10, e11, e12, e13, e14, e15, a0, a1, a2
; MUST NOT touch: d5, d6, d7, a5, a6, a7

        ; 1. Load Matrix A (4 floats into e0..e3)
        fmove.s (a0),e0         ; a00
        fmove.s 4(a0),e1        ; a01
        fmove.s 8(a0),e2        ; a10
        fmove.s 12(a0),e3       ; a11

        ; 2. Load Matrix B (4 floats into e4..e7)
        fmove.s (a1),e4         ; b00
        fmove.s 4(a1),e5        ; b01
        fmove.s 8(a1),e6        ; b10
        fmove.s 12(a1),e7       ; b11

        ; 3. Issue all 8 multiplications in consecutive clock cycles (Zero Stalls!)
        fmul    e0,e4,e8        ; T0: p00 = a00 * b00 (e8 ready at T6)
        fmul    e1,e6,e9        ; T1: p01 = a01 * b10 (e9 ready at T7)
        fmul    e0,e5,e10       ; T2: p02 = a00 * b01 (e10 ready at T8)
        fmul    e1,e7,e11       ; T3: p03 = a01 * b11 (e11 ready at T9)
        fmul    e2,e4,e12       ; T4: p10 = a10 * b00 (e12 ready at T10)
        fmul    e3,e6,e13       ; T5: p11 = a11 * b10 (e13 ready at T11)
        fmul    e2,e5,e14       ; T6: p12 = a10 * b01 (e14 ready at T12)
        fmul    e3,e7,e15       ; T7: p13 = a11 * b11 (e15 ready at T13)

        ; 4. Issue all 4 additions (Zero Stalls! e8 ready at T6, e9 ready at T7, etc.)
        fadd    e8,e9,e8        ; T8:  c00 = p00 + p01 (e8 ready at T14)
        fadd    e10,e11,e10     ; T9:  c01 = p02 + p03 (e10 ready at T15)
        fadd    e12,e13,e12     ; T10: c10 = p10 + p11 (e12 ready at T16)
        fadd    e14,e15,e14     ; T11: c11 = p12 + p13 (e14 ready at T17)

        ; 5. Stores
        fmove.s e8,(a2)         ; store c00
        fmove.s e10,4(a2)       ; store c01
        fmove.s e12,8(a2)       ; store c10
        fmove.s e14,12(a2)      ; store c11
