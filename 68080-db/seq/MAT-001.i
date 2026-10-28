; seq/MAT-001.i - 2x2 single-precision float matrix multiplication (C = A * B)
; Inputs:  a0 = matrix A (2x2, 16 bytes), a1 = matrix B (2x2, 16 bytes)
; Output:  a2 = matrix C (2x2, 16 bytes)
; Uses:    fp0, fp1, fp2, fp3, a0, a1, a2
; MUST NOT touch: d5, d6, d7, a5, a6, a7
        fmove.s (a0),fp2        ; a00
        fmove.s 4(a0),fp3       ; a01

        ; c00 = a00*b00 + a01*b10
        fmove.s (a1),fp0        ; b00
        fmul    fp2,fp0         ; a00 * b00
        fmove.s 8(a1),fp1       ; b10
        fmul    fp3,fp1         ; a01 * b10
        fadd    fp1,fp0
        fmove.s fp0,(a2)

        ; c01 = a00*b01 + a01*b11
        fmove.s 4(a1),fp0       ; b01
        fmul    fp2,fp0         ; a00 * b01
        fmove.s 12(a1),fp1      ; b11
        fmul    fp3,fp1         ; a01 * b11
        fadd    fp1,fp0
        fmove.s fp0,4(a2)

        fmove.s 8(a0),fp2       ; a10
        fmove.s 12(a0),fp3      ; a11

        ; c10 = a10*b00 + a11*b10
        fmove.s (a1),fp0        ; b00
        fmul    fp2,fp0         ; a10 * b00
        fmove.s 8(a1),fp1       ; b10
        fmul    fp3,fp1         ; a11 * b10
        fadd    fp1,fp0
        fmove.s fp0,8(a2)

        ; c11 = a10*b01 + a11*b11
        fmove.s 4(a1),fp0       ; b01
        fmul    fp2,fp0         ; a10 * b01
        fmove.s 12(a1),fp1      ; b11
        fmul    fp3,fp1         ; a11 * b11
        fadd    fp1,fp0
        fmove.s fp0,12(a2)
