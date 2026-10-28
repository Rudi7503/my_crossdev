; seq/MAT-002.i - 3x3 single-precision float matrix multiplication (C = A * B)
; Inputs:  a0 = matrix A (3x3, 36 bytes), a1 = matrix B (3x3, 36 bytes)
; Output:  a2 = matrix C (3x3, 36 bytes)
; Uses:    fp0, fp1, fp2, fp3, fp4, a0, a1, a2
; MUST NOT touch: d5, d6, d7, a5, a6, a7
        ; --- Row 0 ---
        fmove.s (a0),fp2        ; a00
        fmove.s 4(a0),fp3       ; a01
        fmove.s 8(a0),fp4       ; a02

        ; c00 = a00*b00 + a01*b10 + a02*b20
        fmove.s (a1),fp0        ; b00
        fmul    fp2,fp0
        fmove.s 12(a1),fp1      ; b10
        fmul    fp3,fp1
        fadd    fp1,fp0
        fmove.s 24(a1),fp1      ; b20
        fmul    fp4,fp1
        fadd    fp1,fp0
        fmove.s fp0,(a2)

        ; c01 = a00*b01 + a01*b11 + a02*b21
        fmove.s 4(a1),fp0       ; b01
        fmul    fp2,fp0
        fmove.s 16(a1),fp1      ; b11
        fmul    fp3,fp1
        fadd    fp1,fp0
        fmove.s 28(a1),fp1      ; b21
        fmul    fp4,fp1
        fadd    fp1,fp0
        fmove.s fp0,4(a2)

        ; c02 = a00*b02 + a01*b12 + a02*b22
        fmove.s 8(a1),fp0       ; b02
        fmul    fp2,fp0
        fmove.s 20(a1),fp1      ; b12
        fmul    fp3,fp1
        fadd    fp1,fp0
        fmove.s 32(a1),fp1      ; b22
        fmul    fp4,fp1
        fadd    fp1,fp0
        fmove.s fp0,8(a2)

        ; --- Row 1 ---
        fmove.s 12(a0),fp2      ; a10
        fmove.s 16(a0),fp3      ; a11
        fmove.s 20(a0),fp4      ; a12

        ; c10 = a10*b00 + a11*b10 + a12*b20
        fmove.s (a1),fp0        ; b00
        fmul    fp2,fp0
        fmove.s 12(a1),fp1      ; b10
        fmul    fp3,fp1
        fadd    fp1,fp0
        fmove.s 24(a1),fp1      ; b20
        fmul    fp4,fp1
        fadd    fp1,fp0
        fmove.s fp0,12(a2)

        ; c11 = a10*b01 + a11*b11 + a12*b21
        fmove.s 4(a1),fp0       ; b01
        fmul    fp2,fp0
        fmove.s 16(a1),fp1      ; b11
        fmul    fp3,fp1
        fadd    fp1,fp0
        fmove.s 28(a1),fp1      ; b21
        fmul    fp4,fp1
        fadd    fp1,fp0
        fmove.s fp0,16(a2)

        ; c12 = a10*b02 + a11*b12 + a12*b22
        fmove.s 8(a1),fp0       ; b02
        fmul    fp2,fp0
        fmove.s 20(a1),fp1      ; b12
        fmul    fp3,fp1
        fadd    fp1,fp0
        fmove.s 32(a1),fp1      ; b22
        fmul    fp4,fp1
        fadd    fp1,fp0
        fmove.s fp0,20(a2)

        ; --- Row 2 ---
        fmove.s 24(a0),fp2      ; a20
        fmove.s 28(a0),fp3      ; a21
        fmove.s 32(a0),fp4      ; a22

        ; c20 = a20*b00 + a21*b10 + a22*b20
        fmove.s (a1),fp0        ; b00
        fmul    fp2,fp0
        fmove.s 12(a1),fp1      ; b10
        fmul    fp3,fp1
        fadd    fp1,fp0
        fmove.s 24(a1),fp1      ; b20
        fmul    fp4,fp1
        fadd    fp1,fp0
        fmove.s fp0,24(a2)

        ; c21 = a20*b01 + a21*b11 + a22*b21
        fmove.s 4(a1),fp0       ; b01
        fmul    fp2,fp0
        fmove.s 16(a1),fp1      ; b11
        fmul    fp3,fp1
        fadd    fp1,fp0
        fmove.s 28(a1),fp1      ; b21
        fmul    fp4,fp1
        fadd    fp1,fp0
        fmove.s fp0,28(a2)

        ; c22 = a20*b02 + a21*b12 + a22*b22
        fmove.s 8(a1),fp0       ; b02
        fmul    fp2,fp0
        fmove.s 20(a1),fp1      ; b12
        fmul    fp3,fp1
        fadd    fp1,fp0
        fmove.s 32(a1),fp1      ; b22
        fmul    fp4,fp1
        fadd    fp1,fp0
        fmove.s fp0,32(a2)
