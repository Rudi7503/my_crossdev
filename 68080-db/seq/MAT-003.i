; seq/MAT-003.i - 4x4 single-precision float matrix multiplication (C = A * B)
; Fully pipelined 68080 FPU scheduling (4 concurrent column accumulators)
; Inputs:  a0 = matrix A (4x4, 64 bytes), a1 = matrix B (4x4, 64 bytes)
; Output:  a2 = matrix C (4x4, 64 bytes)
; Uses:    fp0, fp1, fp2, fp3, fp4, fp5, fp6, a0, a1, a2
; MUST NOT touch: d5, d6, d7, a5, a6, a7

        ; ==================== ROW 0 ====================
        ; Term 0: a00 * b0x
        fmove.s (a0),fp4        ; a00
        fmove.s (a1),fp0        ; b00
        fmul    fp4,fp0         ; fp0 = a00 * b00 (Col 0)
        fmove.s 4(a1),fp1       ; b01
        fmul    fp4,fp1         ; fp1 = a00 * b01 (Col 1)
        fmove.s 8(a1),fp2       ; b02
        fmul    fp4,fp2         ; fp2 = a00 * b02 (Col 2)
        fmove.s 12(a1),fp3      ; b03
        fmul    fp4,fp3         ; fp3 = a00 * b03 (Col 3)

        ; Term 1: a01 * b1x
        fmove.s 4(a0),fp4       ; a01
        fmove.s 16(a1),fp5      ; b10
        fmul    fp4,fp5
        fmove.s 20(a1),fp6      ; b11
        fmul    fp4,fp6
        fadd    fp5,fp0         ; fp0 += a01 * b10
        fmove.s 24(a1),fp5      ; b12
        fmul    fp4,fp5
        fadd    fp6,fp1         ; fp1 += a01 * b11
        fmove.s 28(a1),fp6      ; b13
        fmul    fp4,fp6
        fadd    fp5,fp2         ; fp2 += a01 * b12
        fadd    fp6,fp3         ; fp3 += a01 * b13

        ; Term 2: a02 * b2x
        fmove.s 8(a0),fp4       ; a02
        fmove.s 32(a1),fp5      ; b20
        fmul    fp4,fp5
        fmove.s 36(a1),fp6      ; b21
        fmul    fp4,fp6
        fadd    fp5,fp0         ; fp0 += a02 * b20
        fmove.s 40(a1),fp5      ; b22
        fmul    fp4,fp5
        fadd    fp6,fp1         ; fp1 += a02 * b21
        fmove.s 44(a1),fp6      ; b23
        fmul    fp4,fp6
        fadd    fp5,fp2         ; fp2 += a02 * b22
        fadd    fp6,fp3         ; fp3 += a02 * b23

        ; Term 3: a03 * b3x
        fmove.s 12(a0),fp4      ; a03
        fmove.s 48(a1),fp5      ; b30
        fmul    fp4,fp5
        fmove.s 52(a1),fp6      ; b31
        fmul    fp4,fp6
        fadd    fp5,fp0         ; fp0 += a03 * b30 -> c00
        fmove.s 56(a1),fp5      ; b32
        fmul    fp4,fp5
        fadd    fp6,fp1         ; fp1 += a03 * b31 -> c01
        fmove.s 60(a1),fp6      ; b33
        fmul    fp4,fp6
        fadd    fp5,fp2         ; fp2 += a03 * b32 -> c02
        fadd    fp6,fp3         ; fp3 += a03 * b33 -> c03

        ; Store Row 0
        fmove.s fp0,(a2)
        fmove.s fp1,4(a2)
        fmove.s fp2,8(a2)
        fmove.s fp3,12(a2)

        ; ==================== ROW 1 ====================
        ; Term 0: a10 * b0x
        fmove.s 16(a0),fp4      ; a10
        fmove.s (a1),fp0        ; b00
        fmul    fp4,fp0         ; fp0 = a10 * b00 (Col 0)
        fmove.s 4(a1),fp1       ; b01
        fmul    fp4,fp1         ; fp1 = a10 * b01 (Col 1)
        fmove.s 8(a1),fp2       ; b02
        fmul    fp4,fp2         ; fp2 = a10 * b02 (Col 2)
        fmove.s 12(a1),fp3      ; b03
        fmul    fp4,fp3         ; fp3 = a10 * b03 (Col 3)

        ; Term 1: a11 * b1x
        fmove.s 20(a0),fp4      ; a11
        fmove.s 16(a1),fp5      ; b10
        fmul    fp4,fp5
        fmove.s 20(a1),fp6      ; b11
        fmul    fp4,fp6
        fadd    fp5,fp0         ; fp0 += a11 * b10
        fmove.s 24(a1),fp5      ; b12
        fmul    fp4,fp5
        fadd    fp6,fp1         ; fp1 += a11 * b11
        fmove.s 28(a1),fp6      ; b13
        fmul    fp4,fp6
        fadd    fp5,fp2         ; fp2 += a11 * b12
        fadd    fp6,fp3         ; fp3 += a11 * b13

        ; Term 2: a12 * b2x
        fmove.s 24(a0),fp4      ; a12
        fmove.s 32(a1),fp5      ; b20
        fmul    fp4,fp5
        fmove.s 36(a1),fp6      ; b21
        fmul    fp4,fp6
        fadd    fp5,fp0         ; fp0 += a12 * b20
        fmove.s 40(a1),fp5      ; b22
        fmul    fp4,fp5
        fadd    fp6,fp1         ; fp1 += a12 * b21
        fmove.s 44(a1),fp6      ; b23
        fmul    fp4,fp6
        fadd    fp5,fp2         ; fp2 += a12 * b22
        fadd    fp6,fp3         ; fp3 += a12 * b23

        ; Term 3: a13 * b3x
        fmove.s 28(a0),fp4      ; a13
        fmove.s 48(a1),fp5      ; b30
        fmul    fp4,fp5
        fmove.s 52(a1),fp6      ; b31
        fmul    fp4,fp6
        fadd    fp5,fp0         ; fp0 += a13 * b30 -> c10
        fmove.s 56(a1),fp5      ; b32
        fmul    fp4,fp5
        fadd    fp6,fp1         ; fp1 += a13 * b31 -> c11
        fmove.s 60(a1),fp6      ; b33
        fmul    fp4,fp6
        fadd    fp5,fp2         ; fp2 += a13 * b32 -> c12
        fadd    fp6,fp3         ; fp3 += a13 * b33 -> c13

        ; Store Row 1
        fmove.s fp0,16(a2)
        fmove.s fp1,20(a2)
        fmove.s fp2,24(a2)
        fmove.s fp3,28(a2)

        ; ==================== ROW 2 ====================
        ; Term 0: a20 * b0x
        fmove.s 32(a0),fp4      ; a20
        fmove.s (a1),fp0        ; b00
        fmul    fp4,fp0         ; fp0 = a20 * b00 (Col 0)
        fmove.s 4(a1),fp1       ; b01
        fmul    fp4,fp1         ; fp1 = a20 * b01 (Col 1)
        fmove.s 8(a1),fp2       ; b02
        fmul    fp4,fp2         ; fp2 = a20 * b02 (Col 2)
        fmove.s 12(a1),fp3      ; b03
        fmul    fp4,fp3         ; fp3 = a20 * b03 (Col 3)

        ; Term 1: a21 * b1x
        fmove.s 36(a0),fp4      ; a21
        fmove.s 16(a1),fp5      ; b10
        fmul    fp4,fp5
        fmove.s 20(a1),fp6      ; b11
        fmul    fp4,fp6
        fadd    fp5,fp0         ; fp0 += a21 * b10
        fmove.s 24(a1),fp5      ; b12
        fmul    fp4,fp5
        fadd    fp6,fp1         ; fp1 += a21 * b11
        fmove.s 28(a1),fp6      ; b13
        fmul    fp4,fp6
        fadd    fp5,fp2         ; fp2 += a21 * b12
        fadd    fp6,fp3         ; fp3 += a21 * b13

        ; Term 2: a22 * b2x
        fmove.s 40(a0),fp4      ; a22
        fmove.s 32(a1),fp5      ; b20
        fmul    fp4,fp5
        fmove.s 36(a1),fp6      ; b21
        fmul    fp4,fp6
        fadd    fp5,fp0         ; fp0 += a22 * b20
        fmove.s 40(a1),fp5      ; b22
        fmul    fp4,fp5
        fadd    fp6,fp1         ; fp1 += a22 * b21
        fmove.s 44(a1),fp6      ; b23
        fmul    fp4,fp6
        fadd    fp5,fp2         ; fp2 += a22 * b22
        fadd    fp6,fp3         ; fp3 += a22 * b23

        ; Term 3: a23 * b3x
        fmove.s 44(a0),fp4      ; a23
        fmove.s 48(a1),fp5      ; b30
        fmul    fp4,fp5
        fmove.s 52(a1),fp6      ; b31
        fmul    fp4,fp6
        fadd    fp5,fp0         ; fp0 += a23 * b30 -> c20
        fmove.s 56(a1),fp5      ; b32
        fmul    fp4,fp5
        fadd    fp6,fp1         ; fp1 += a23 * b31 -> c21
        fmove.s 60(a1),fp6      ; b33
        fmul    fp4,fp6
        fadd    fp5,fp2         ; fp2 += a23 * b32 -> c22
        fadd    fp6,fp3         ; fp3 += a23 * b33 -> c23

        ; Store Row 2
        fmove.s fp0,32(a2)
        fmove.s fp1,36(a2)
        fmove.s fp2,40(a2)
        fmove.s fp3,44(a2)

        ; ==================== ROW 3 ====================
        ; Term 0: a30 * b0x
        fmove.s 48(a0),fp4      ; a30
        fmove.s (a1),fp0        ; b00
        fmul    fp4,fp0         ; fp0 = a30 * b00 (Col 0)
        fmove.s 4(a1),fp1       ; b01
        fmul    fp4,fp1         ; fp1 = a30 * b01 (Col 1)
        fmove.s 8(a1),fp2       ; b02
        fmul    fp4,fp2         ; fp2 = a30 * b02 (Col 2)
        fmove.s 12(a1),fp3      ; b03
        fmul    fp4,fp3         ; fp3 = a30 * b03 (Col 3)

        ; Term 1: a31 * b1x
        fmove.s 52(a0),fp4      ; a31
        fmove.s 16(a1),fp5      ; b10
        fmul    fp4,fp5
        fmove.s 20(a1),fp6      ; b11
        fmul    fp4,fp6
        fadd    fp5,fp0         ; fp0 += a31 * b10
        fmove.s 24(a1),fp5      ; b12
        fmul    fp4,fp5
        fadd    fp6,fp1         ; fp1 += a31 * b11
        fmove.s 28(a1),fp6      ; b13
        fmul    fp4,fp6
        fadd    fp5,fp2         ; fp2 += a31 * b12
        fadd    fp6,fp3         ; fp3 += a31 * b13

        ; Term 2: a32 * b2x
        fmove.s 56(a0),fp4      ; a32
        fmove.s 32(a1),fp5      ; b20
        fmul    fp4,fp5
        fmove.s 36(a1),fp6      ; b21
        fmul    fp4,fp6
        fadd    fp5,fp0         ; fp0 += a32 * b20
        fmove.s 40(a1),fp5      ; b22
        fmul    fp4,fp5
        fadd    fp6,fp1         ; fp1 += a32 * b21
        fmove.s 44(a1),fp6      ; b23
        fmul    fp4,fp6
        fadd    fp5,fp2         ; fp2 += a32 * b22
        fadd    fp6,fp3         ; fp3 += a32 * b23

        ; Term 3: a33 * b3x
        fmove.s 60(a0),fp4      ; a33
        fmove.s 48(a1),fp5      ; b30
        fmul    fp4,fp5
        fmove.s 52(a1),fp6      ; b31
        fmul    fp4,fp6
        fadd    fp5,fp0         ; fp0 += a33 * b30 -> c30
        fmove.s 56(a1),fp5      ; b32
        fmul    fp4,fp5
        fadd    fp6,fp1         ; fp1 += a33 * b31 -> c31
        fmove.s 60(a1),fp6      ; b33
        fmul    fp4,fp6
        fadd    fp5,fp2         ; fp2 += a33 * b32 -> c32
        fadd    fp6,fp3         ; fp3 += a33 * b33 -> c33

        ; Store Row 3
        fmove.s fp0,48(a2)
        fmove.s fp1,52(a2)
        fmove.s fp2,56(a2)
        fmove.s fp3,60(a2)
