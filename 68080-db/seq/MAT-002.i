; seq/MAT-002.i - 3x3 single-precision float matrix multiplication (C = A * B)
; Fully pipelined 68080 FPU scheduling (3 concurrent column accumulators)
; Inputs:  a0 = matrix A (3x3, 36 bytes), a1 = matrix B (3x3, 36 bytes)
; Output:  a2 = matrix C (3x3, 36 bytes)
; Uses:    fp0, fp1, fp2, fp3, fp4, fp5, fp6, fp7, a0, a1, a2
; MUST NOT touch: d5, d6, d7, a5, a6, a7
        ; ==================== ROW 0 ====================
        fmove.s (a0),fp2        ; a00
        fmove.s 4(a0),fp3       ; a01
        fmove.s 8(a0),fp4       ; a02

        ; Term 0: a00 * b0x
        fmove.s (a1),fp0        ; b00
        fmul    fp2,fp0         ; fp0 = a00 * b00
        fmove.s 4(a1),fp1       ; b01
        fmul    fp2,fp1         ; fp1 = a00 * b01
        fmove.s 8(a1),fp5       ; b02
        fmul    fp2,fp5         ; fp5 = a00 * b02

        ; Term 1: a01 * b1x
        fmove.s 12(a1),fp6      ; b10
        fmul    fp3,fp6         ; fp6 = a01 * b10
        fmove.s 16(a1),fp7      ; b11
        fmul    fp3,fp7         ; fp7 = a01 * b11
        fadd    fp6,fp0         ; fp0 += a01 * b10
        fmove.s 20(a1),fp6      ; b12
        fmul    fp3,fp6         ; fp6 = a01 * b12
        fadd    fp7,fp1         ; fp1 += a01 * b11

        ; Term 2: a02 * b2x
        fmove.s 24(a1),fp7      ; b20
        fmul    fp4,fp7         ; fp7 = a02 * b20
        fadd    fp6,fp5         ; fp5 += a01 * b12
        fmove.s 28(a1),fp6      ; b21
        fmul    fp4,fp6         ; fp6 = a02 * b21
        fadd    fp7,fp0         ; fp0 += a02 * b20 -> c00
        fmove.s 32(a1),fp7      ; b22
        fmul    fp4,fp7         ; fp7 = a02 * b22
        fadd    fp6,fp1         ; fp1 += a02 * b21 -> c01
        fadd    fp7,fp5         ; fp5 += a02 * b22 -> c02

        ; Store Row 0
        fmove.s fp0,(a2)
        fmove.s fp1,4(a2)
        fmove.s fp5,8(a2)

        ; ==================== ROW 1 ====================
        fmove.s 12(a0),fp2      ; a10
        fmove.s 16(a0),fp3      ; a11
        fmove.s 20(a0),fp4      ; a12

        ; Term 0: a10 * b0x
        fmove.s (a1),fp0        ; b00
        fmul    fp2,fp0         ; fp0 = a10 * b00
        fmove.s 4(a1),fp1       ; b01
        fmul    fp2,fp1         ; fp1 = a10 * b01
        fmove.s 8(a1),fp5       ; b02
        fmul    fp2,fp5         ; fp5 = a10 * b02

        ; Term 1: a11 * b1x
        fmove.s 12(a1),fp6      ; b10
        fmul    fp3,fp6         ; fp6 = a11 * b10
        fmove.s 16(a1),fp7      ; b11
        fmul    fp3,fp7         ; fp7 = a11 * b11
        fadd    fp6,fp0         ; fp0 += a11 * b10
        fmove.s 20(a1),fp6      ; b12
        fmul    fp3,fp6         ; fp6 = a11 * b12
        fadd    fp7,fp1         ; fp1 += a11 * b11

        ; Term 2: a12 * b2x
        fmove.s 24(a1),fp7      ; b20
        fmul    fp4,fp7         ; fp7 = a12 * b20
        fadd    fp6,fp5         ; fp5 += a11 * b12
        fmove.s 28(a1),fp6      ; b21
        fmul    fp4,fp6         ; fp6 = a12 * b21
        fadd    fp7,fp0         ; fp0 += a12 * b20 -> c10
        fmove.s 32(a1),fp7      ; b22
        fmul    fp4,fp7         ; fp7 = a12 * b22
        fadd    fp6,fp1         ; fp1 += a12 * b21 -> c11
        fadd    fp7,fp5         ; fp5 += a12 * b22 -> c12

        ; Store Row 1
        fmove.s fp0,12(a2)
        fmove.s fp1,16(a2)
        fmove.s fp5,20(a2)

        ; ==================== ROW 2 ====================
        fmove.s 24(a0),fp2      ; a20
        fmove.s 28(a0),fp3      ; a21
        fmove.s 32(a0),fp4      ; a22

        ; Term 0: a20 * b0x
        fmove.s (a1),fp0        ; b00
        fmul    fp2,fp0         ; fp0 = a20 * b00
        fmove.s 4(a1),fp1       ; b01
        fmul    fp2,fp1         ; fp1 = a20 * b01
        fmove.s 8(a1),fp5       ; b02
        fmul    fp2,fp5         ; fp5 = a20 * b02

        ; Term 1: a21 * b1x
        fmove.s 12(a1),fp6      ; b10
        fmul    fp3,fp6         ; fp6 = a21 * b10
        fmove.s 16(a1),fp7      ; b11
        fmul    fp3,fp7         ; fp7 = a21 * b11
        fadd    fp6,fp0         ; fp0 += a21 * b10
        fmove.s 20(a1),fp6      ; b12
        fmul    fp3,fp6         ; fp6 = a21 * b12
        fadd    fp7,fp1         ; fp1 += a21 * b11

        ; Term 2: a22 * b2x
        fmove.s 24(a1),fp7      ; b20
        fmul    fp4,fp7         ; fp7 = a22 * b20
        fadd    fp6,fp5         ; fp5 += a21 * b12
        fmove.s 28(a1),fp6      ; b21
        fmul    fp4,fp6         ; fp6 = a22 * b21
        fadd    fp7,fp0         ; fp0 += a22 * b20 -> c20
        fmove.s 32(a1),fp7      ; b22
        fmul    fp4,fp7         ; fp7 = a22 * b22
        fadd    fp6,fp1         ; fp1 += a22 * b21 -> c21
        fadd    fp7,fp5         ; fp5 += a22 * b22 -> c22

        ; Store Row 2
        fmove.s fp0,24(a2)
        fmove.s fp1,28(a2)
        fmove.s fp5,32(a2)
