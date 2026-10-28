; seq/MAT-002.i - 3x3 single-precision float matrix multiplication (C = A * B)
; Fully unrolled 32-register 3-operand FPU execution
; Inputs:  a0 = matrix A (3x3, 36 bytes), a1 = matrix B (3x3, 36 bytes)
; Output:  a2 = matrix C (3x3, 36 bytes)
; Uses:    e0-e23, fp0-fp7, a0, a1, a2
; MUST NOT touch: d5, d6, d7, a5, a6, a7

        ; 1. Load Matrix A into e0..e8 (9 loads)
        fmove.s (a0),e0         ; a00
        fmove.s 4(a0),e1        ; a01
        fmove.s 8(a0),e2        ; a02
        fmove.s 12(a0),e3       ; a10
        fmove.s 16(a0),e4       ; a11
        fmove.s 20(a0),e5       ; a12
        fmove.s 24(a0),e6       ; a20
        fmove.s 28(a0),e7       ; a21
        fmove.s 32(a0),e8       ; a22

        ; 2. Load Matrix B into e9..e17 (9 loads, loaded ONCE for all 3 rows!)
        fmove.s (a1),e9         ; b00
        fmove.s 4(a1),e10       ; b01
        fmove.s 8(a1),e11       ; b02
        fmove.s 12(a1),e12      ; b10
        fmove.s 16(a1),e13      ; b11
        fmove.s 20(a1),e14      ; b12
        fmove.s 24(a1),e15      ; b20
        fmove.s 28(a1),e16      ; b21
        fmove.s 32(a1),e17      ; b22

        ; ==================== ROW 0 ====================
        fmul    e0,e9,fp0       ; T0: p00 (ready T6)
        fmul    e0,e10,fp1      ; T1: p01 (ready T7)
        fmul    e0,e11,fp2      ; T2: p02 (ready T8)

        fmul    e1,e12,fp3      ; T3: p10 (ready T9)
        fmul    e1,e13,fp4      ; T4: p11 (ready T10)
        fmul    e1,e14,fp5      ; T5: p12 (ready T11)

        fmul    e2,e15,fp6      ; T6: p20 (ready T12)
        fmul    e2,e16,fp7      ; T7: p21 (ready T13)
        fmul    e2,e17,e18      ; T8: p22 (ready T14)

        ; Adds Term 0 + Term 1
        fadd    fp0,fp3,fp0     ; T9:  fp0 = p00 + p10 (ready T15)
        fadd    fp1,fp4,fp1     ; T10: fp1 = p01 + p11 (ready T16)
        fadd    fp2,fp5,fp2     ; T11: fp2 = p02 + p12 (ready T17)

        ; Adds + Term 2
        ; At T15, fp0 is ready; fp6 was ready at T12
        fadd    fp0,fp6,fp0     ; T15: c00 (ready T21)
        fadd    fp1,fp7,fp1     ; T16: c01 (ready T22)
        fadd    fp2,e18,fp2     ; T17: c02 (ready T23)

        ; ==================== ROW 1 ====================
        fmul    e3,e9,fp3       ; p10_0
        fmul    e3,e10,fp4      ; p10_1
        fmul    e3,e11,fp5      ; p10_2

        fmul    e4,e12,fp6      ; p11_0
        fmul    e4,e13,fp7      ; p11_1
        fmul    e4,e14,e18      ; p11_2

        fmul    e5,e15,e19      ; p12_0
        fmul    e5,e16,e20      ; p12_1
        fmul    e5,e17,e21      ; p12_2

        ; Store Row 0
        fmove.s fp0,(a2)
        fmove.s fp1,4(a2)
        fmove.s fp2,8(a2)

        ; Adds Row 1
        fadd    fp3,fp6,fp3
        fadd    fp4,fp7,fp4
        fadd    fp5,e18,fp5

        fadd    fp3,e19,fp3     ; c10
        fadd    fp4,e20,fp4     ; c11
        fadd    fp5,e21,fp5     ; c12

        ; ==================== ROW 2 ====================
        fmul    e6,e9,fp0
        fmul    e6,e10,fp1
        fmul    e6,e11,fp2

        fmul    e7,e12,fp6
        fmul    e7,e13,fp7
        fmul    e7,e14,e18

        fmul    e8,e15,e19
        fmul    e8,e16,e20
        fmul    e8,e17,e21

        ; Store Row 1
        fmove.s fp3,12(a2)
        fmove.s fp4,16(a2)
        fmove.s fp5,20(a2)

        ; Adds Row 2
        fadd    fp0,fp6,fp0
        fadd    fp1,fp7,fp1
        fadd    fp2,e18,fp2

        fadd    fp0,e19,fp0     ; c20
        fadd    fp1,e20,fp1     ; c21
        fadd    fp2,e21,fp2     ; c22

        ; Store Row 2
        fmove.s fp0,24(a2)
        fmove.s fp1,28(a2)
        fmove.s fp2,32(a2)
