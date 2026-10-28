; seq/MAT-003.i - 4x4 single-precision float matrix multiplication (C = A * B)
; Inputs:  a0 = matrix A (4x4, 64 bytes), a1 = matrix B (4x4, 64 bytes)
; Output:  a2 = matrix C (4x4, 64 bytes)
; Uses:    fp0, fp1, fp2, fp3, fp4, fp5, a0, a1, a2
; MUST NOT touch: d5, d6, d7, a5, a6, a7
        ; ==================== ROW 0 ====================
        fmove.s (a0),fp2        ; a00
        fmove.s 4(a0),fp3       ; a01
        fmove.s 8(a0),fp4       ; a02
        fmove.s 12(a0),fp5      ; a03

        ; c00 = a00*b00 + a01*b10 + a02*b20 + a03*b30
        fmove.s (a1),fp0        ; b00
        fmul    fp2,fp0
        fmove.s 16(a1),fp1      ; b10
        fmul    fp3,fp1
        fadd    fp1,fp0
        fmove.s 32(a1),fp1      ; b20
        fmul    fp4,fp1
        fadd    fp1,fp0
        fmove.s 48(a1),fp1      ; b30
        fmul    fp5,fp1
        fadd    fp1,fp0
        fmove.s fp0,(a2)

        ; c01 = a00*b01 + a01*b11 + a02*b21 + a03*b31
        fmove.s 4(a1),fp0       ; b01
        fmul    fp2,fp0
        fmove.s 20(a1),fp1      ; b11
        fmul    fp3,fp1
        fadd    fp1,fp0
        fmove.s 36(a1),fp1      ; b21
        fmul    fp4,fp1
        fadd    fp1,fp0
        fmove.s 52(a1),fp1      ; b31
        fmul    fp5,fp1
        fadd    fp1,fp0
        fmove.s fp0,4(a2)

        ; c02 = a00*b02 + a01*b12 + a02*b22 + a03*b32
        fmove.s 8(a1),fp0       ; b02
        fmul    fp2,fp0
        fmove.s 24(a1),fp1      ; b12
        fmul    fp3,fp1
        fadd    fp1,fp0
        fmove.s 40(a1),fp1      ; b22
        fmul    fp4,fp1
        fadd    fp1,fp0
        fmove.s 56(a1),fp1      ; b32
        fmul    fp5,fp1
        fadd    fp1,fp0
        fmove.s fp0,8(a2)

        ; c03 = a00*b03 + a01*b13 + a02*b23 + a03*b33
        fmove.s 12(a1),fp0      ; b03
        fmul    fp2,fp0
        fmove.s 28(a1),fp1      ; b13
        fmul    fp3,fp1
        fadd    fp1,fp0
        fmove.s 44(a1),fp1      ; b23
        fmul    fp4,fp1
        fadd    fp1,fp0
        fmove.s 60(a1),fp1      ; b33
        fmul    fp5,fp1
        fadd    fp1,fp0
        fmove.s fp0,12(a2)

        ; ==================== ROW 1 ====================
        fmove.s 16(a0),fp2      ; a10
        fmove.s 20(a0),fp3      ; a11
        fmove.s 24(a0),fp4      ; a12
        fmove.s 28(a0),fp5      ; a13

        ; c10 = a10*b00 + a11*b10 + a12*b20 + a13*b30
        fmove.s (a1),fp0        ; b00
        fmul    fp2,fp0
        fmove.s 16(a1),fp1      ; b10
        fmul    fp3,fp1
        fadd    fp1,fp0
        fmove.s 32(a1),fp1      ; b20
        fmul    fp4,fp1
        fadd    fp1,fp0
        fmove.s 48(a1),fp1      ; b30
        fmul    fp5,fp1
        fadd    fp1,fp0
        fmove.s fp0,16(a2)

        ; c11 = a10*b01 + a11*b11 + a12*b21 + a13*b31
        fmove.s 4(a1),fp0       ; b01
        fmul    fp2,fp0
        fmove.s 20(a1),fp1      ; b11
        fmul    fp3,fp1
        fadd    fp1,fp0
        fmove.s 36(a1),fp1      ; b21
        fmul    fp4,fp1
        fadd    fp1,fp0
        fmove.s 52(a1),fp1      ; b31
        fmul    fp5,fp1
        fadd    fp1,fp0
        fmove.s fp0,20(a2)

        ; c12 = a10*b02 + a11*b12 + a12*b22 + a13*b32
        fmove.s 8(a1),fp0       ; b02
        fmul    fp2,fp0
        fmove.s 24(a1),fp1      ; b12
        fmul    fp3,fp1
        fadd    fp1,fp0
        fmove.s 40(a1),fp1      ; b22
        fmul    fp4,fp1
        fadd    fp1,fp0
        fmove.s 56(a1),fp1      ; b32
        fmul    fp5,fp1
        fadd    fp1,fp0
        fmove.s fp0,24(a2)

        ; c13 = a10*b03 + a11*b13 + a12*b23 + a13*b33
        fmove.s 12(a1),fp0      ; b03
        fmul    fp2,fp0
        fmove.s 28(a1),fp1      ; b13
        fmul    fp3,fp1
        fadd    fp1,fp0
        fmove.s 44(a1),fp1      ; b23
        fmul    fp4,fp1
        fadd    fp1,fp0
        fmove.s 60(a1),fp1      ; b33
        fmul    fp5,fp1
        fadd    fp1,fp0
        fmove.s fp0,28(a2)

        ; ==================== ROW 2 ====================
        fmove.s 32(a0),fp2      ; a20
        fmove.s 36(a0),fp3      ; a21
        fmove.s 40(a0),fp4      ; a22
        fmove.s 44(a0),fp5      ; a23

        ; c20 = a20*b00 + a21*b10 + a22*b20 + a23*b30
        fmove.s (a1),fp0        ; b00
        fmul    fp2,fp0
        fmove.s 16(a1),fp1      ; b10
        fmul    fp3,fp1
        fadd    fp1,fp0
        fmove.s 32(a1),fp1      ; b20
        fmul    fp4,fp1
        fadd    fp1,fp0
        fmove.s 48(a1),fp1      ; b30
        fmul    fp5,fp1
        fadd    fp1,fp0
        fmove.s fp0,32(a2)

        ; c21 = a20*b01 + a21*b11 + a22*b21 + a23*b31
        fmove.s 4(a1),fp0       ; b01
        fmul    fp2,fp0
        fmove.s 20(a1),fp1      ; b11
        fmul    fp3,fp1
        fadd    fp1,fp0
        fmove.s 36(a1),fp1      ; b21
        fmul    fp4,fp1
        fadd    fp1,fp0
        fmove.s 52(a1),fp1      ; b31
        fmul    fp5,fp1
        fadd    fp1,fp0
        fmove.s fp0,36(a2)

        ; c22 = a20*b02 + a21*b12 + a22*b22 + a23*b32
        fmove.s 8(a1),fp0       ; b02
        fmul    fp2,fp0
        fmove.s 24(a1),fp1      ; b12
        fmul    fp3,fp1
        fadd    fp1,fp0
        fmove.s 40(a1),fp1      ; b22
        fmul    fp4,fp1
        fadd    fp1,fp0
        fmove.s 56(a1),fp1      ; b32
        fmul    fp5,fp1
        fadd    fp1,fp0
        fmove.s fp0,40(a2)

        ; c23 = a20*b03 + a21*b13 + a22*b23 + a23*b33
        fmove.s 12(a1),fp0      ; b03
        fmul    fp2,fp0
        fmove.s 28(a1),fp1      ; b13
        fmul    fp3,fp1
        fadd    fp1,fp0
        fmove.s 44(a1),fp1      ; b23
        fmul    fp4,fp1
        fadd    fp1,fp0
        fmove.s 60(a1),fp1      ; b33
        fmul    fp5,fp1
        fadd    fp1,fp0
        fmove.s fp0,44(a2)

        ; ==================== ROW 3 ====================
        fmove.s 48(a0),fp2      ; a30
        fmove.s 52(a0),fp3      ; a31
        fmove.s 56(a0),fp4      ; a32
        fmove.s 60(a0),fp5      ; a33

        ; c30 = a30*b00 + a31*b10 + a32*b20 + a33*b30
        fmove.s (a1),fp0        ; b00
        fmul    fp2,fp0
        fmove.s 16(a1),fp1      ; b10
        fmul    fp3,fp1
        fadd    fp1,fp0
        fmove.s 32(a1),fp1      ; b20
        fmul    fp4,fp1
        fadd    fp1,fp0
        fmove.s 48(a1),fp1      ; b30
        fmul    fp5,fp1
        fadd    fp1,fp0
        fmove.s fp0,48(a2)

        ; c31 = a30*b01 + a31*b11 + a32*b21 + a33*b31
        fmove.s 4(a1),fp0       ; b01
        fmul    fp2,fp0
        fmove.s 20(a1),fp1      ; b11
        fmul    fp3,fp1
        fadd    fp1,fp0
        fmove.s 36(a1),fp1      ; b21
        fmul    fp4,fp1
        fadd    fp1,fp0
        fmove.s 52(a1),fp1      ; b31
        fmul    fp5,fp1
        fadd    fp1,fp0
        fmove.s fp0,52(a2)

        ; c32 = a30*b02 + a31*b12 + a32*b22 + a33*b32
        fmove.s 8(a1),fp0       ; b02
        fmul    fp2,fp0
        fmove.s 24(a1),fp1      ; b12
        fmul    fp3,fp1
        fadd    fp1,fp0
        fmove.s 40(a1),fp1      ; b22
        fmul    fp4,fp1
        fadd    fp1,fp0
        fmove.s 56(a1),fp1      ; b32
        fmul    fp5,fp1
        fadd    fp1,fp0
        fmove.s fp0,56(a2)

        ; c33 = a30*b03 + a31*b13 + a32*b23 + a33*b33
        fmove.s 12(a1),fp0      ; b03
        fmul    fp2,fp0
        fmove.s 28(a1),fp1      ; b13
        fmul    fp3,fp1
        fadd    fp1,fp0
        fmove.s 44(a1),fp1      ; b23
        fmul    fp4,fp1
        fadd    fp1,fp0
        fmove.s 60(a1),fp1      ; b33
        fmul    fp5,fp1
        fadd    fp1,fp0
        fmove.s fp0,60(a2)
