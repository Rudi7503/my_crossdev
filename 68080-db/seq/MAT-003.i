; seq/MAT-003.i - 4x4 single-precision float matrix multiplication (C = A * B)
; Fully unrolled 32-register 3-operand FPU execution - Interleaved 16-mult pipeline
; Inputs:  a0 = matrix A (4x4, 64 bytes), a1 = matrix B (4x4, 64 bytes)
; Output:  a2 = matrix C (4x4, 64 bytes)
; Uses:    e0-e23, fp0-fp7, a0, a1, a2
; MUST NOT touch: d5, d6, d7, a5, a6, a7

        ; 1. Preload entire Matrix B into e0..e15 (16 loads ONCE for all 4 rows!)
        fmove.s (a1),e0         ; b00
        fmove.s 4(a1),e1        ; b01
        fmove.s 8(a1),e2        ; b02
        fmove.s 12(a1),e3       ; b03
        fmove.s 16(a1),e4       ; b10
        fmove.s 20(a1),e5       ; b11
        fmove.s 24(a1),e6       ; b12
        fmove.s 28(a1),e7       ; b13
        fmove.s 32(a1),e8       ; b20
        fmove.s 36(a1),e9       ; b21
        fmove.s 40(a1),e10      ; b22
        fmove.s 44(a1),e11      ; b23
        fmove.s 48(a1),e12      ; b30
        fmove.s 52(a1),e13      ; b31
        fmove.s 56(a1),e14      ; b32
        fmove.s 60(a1),e15      ; b33

        ; ==================== ROW 0 ====================
        fmove.s (a0),e16        ; a00
        fmove.s 4(a0),e17       ; a01
        fmove.s 8(a0),e18       ; a02
        fmove.s 12(a0),e19      ; a03

        ; Term 0: a00 * b0x -> fp0..fp3 (issued T0..T3, ready T6..T9)
        fmul    e16,e0,fp0
        fmul    e16,e1,fp1
        fmul    e16,e2,fp2
        fmul    e16,e3,fp3

        ; Term 1: a01 * b1x -> e20..e23 (issued T4..T7, ready T10..T13)
        fmul    e17,e4,e20
        fmul    e17,e5,e21
        fmul    e17,e6,e22
        fmul    e17,e7,e23

        ; Term 2: a02 * b2x -> fp4..fp7 (issued T8..T11, ready T14..T17)
        ; (Fills latency of Term 1, so e20..e23 are completely ready!)
        fmul    e18,e8,fp4
        fmul    e18,e9,fp5
        fmul    e18,e10,fp6
        fmul    e18,e11,fp7

        ; Add Term 0 + Term 1 (issued T12..T15, ready T18..T21)
        ; Zero stalls: fp0..fp3 ready at T6..T9, e20..e23 ready at T10..T13!
        fadd    fp0,e20,fp0
        fadd    fp1,e21,fp1
        fadd    fp2,e22,fp2
        fadd    fp3,e23,fp3

        ; Term 3: a03 * b3x -> e20..e23 (issued T16..T19, ready T22..T25)
        ; (Fills latency of Add 1 and Term 2!)
        fmul    e19,e12,e20
        fmul    e19,e13,e21
        fmul    e19,e14,e22
        fmul    e19,e15,e23

        ; Add + Term 2 (issued T20..T23, ready T26..T29)
        ; Zero stalls: fp0..fp3 ready at T18..T21, fp4..fp7 ready at T14..T17!
        fadd    fp0,fp4,fp0
        fadd    fp1,fp5,fp1
        fadd    fp2,fp6,fp2
        fadd    fp3,fp7,fp3

        ; ==================== ROW 1 PRELOAD ====================
        ; (Overlaps latency of Add 2 and Term 3!)
        fmove.s 16(a0),e16      ; a10
        fmove.s 20(a0),e17      ; a11
        fmove.s 24(a0),e18      ; a12
        fmove.s 28(a0),e19      ; a13

        ; Add + Term 3 (Row 0 complete! ready T32..T35)
        ; Zero stalls: fp0..fp3 ready at T26..T29, e20..e23 ready at T22..T25!
        fadd    fp0,e20,fp0     ; c00
        fadd    fp1,e21,fp1     ; c01
        fadd    fp2,e22,fp2     ; c02
        fadd    fp3,e23,fp3     ; c03

        ; ==================== ROW 1 MATH ====================
        ; Term 0: a10 * b0x -> fp4..fp7 (issued while Row 0 add completes!)
        fmul    e16,e0,fp4
        fmul    e16,e1,fp5
        fmul    e16,e2,fp6
        fmul    e16,e3,fp7

        ; Store Row 0 results (now completely ready!)
        fmove.s fp0,(a2)
        fmove.s fp1,4(a2)
        fmove.s fp2,8(a2)
        fmove.s fp3,12(a2)

        ; Term 1: a11 * b1x -> e20..e23
        fmul    e17,e4,e20
        fmul    e17,e5,e21
        fmul    e17,e6,e22
        fmul    e17,e7,e23

        ; Term 2: a12 * b2x -> fp0..fp3
        fmul    e18,e8,fp0
        fmul    e18,e9,fp1
        fmul    e18,e10,fp2
        fmul    e18,e11,fp3

        ; Add Row 1 Term 0 + Term 1
        fadd    fp4,e20,fp4
        fadd    fp5,e21,fp5
        fadd    fp6,e22,fp6
        fadd    fp7,e23,fp7

        ; Term 3: a13 * b3x -> e20..e23
        fmul    e19,e12,e20
        fmul    e19,e13,e21
        fmul    e19,e14,e22
        fmul    e19,e15,e23

        ; Add Row 1 + Term 2
        fadd    fp4,fp0,fp4
        fadd    fp5,fp1,fp5
        fadd    fp6,fp2,fp6
        fadd    fp7,fp3,fp7

        ; ==================== ROW 2 PRELOAD ====================
        fmove.s 32(a0),e16      ; a20
        fmove.s 36(a0),e17      ; a21
        fmove.s 40(a0),e18      ; a22
        fmove.s 44(a0),e19      ; a23

        ; Add Row 1 + Term 3 (Row 1 complete!)
        fadd    fp4,e20,fp4     ; c10
        fadd    fp5,e21,fp5     ; c11
        fadd    fp6,e22,fp6     ; c12
        fadd    fp7,e23,fp7     ; c13

        ; ==================== ROW 2 MATH ====================
        ; Term 0: a20 * b0x -> fp0..fp3
        fmul    e16,e0,fp0
        fmul    e16,e1,fp1
        fmul    e16,e2,fp2
        fmul    e16,e3,fp3

        ; Store Row 1 results
        fmove.s fp4,16(a2)
        fmove.s fp5,20(a2)
        fmove.s fp6,24(a2)
        fmove.s fp7,28(a2)

        ; Term 1: a21 * b1x -> e20..e23
        fmul    e17,e4,e20
        fmul    e17,e5,e21
        fmul    e17,e6,e22
        fmul    e17,e7,e23

        ; Term 2: a22 * b2x -> fp4..fp7
        fmul    e18,e8,fp4
        fmul    e18,e9,fp5
        fmul    e18,e10,fp6
        fmul    e18,e11,fp7

        ; Add Row 2 Term 0 + Term 1
        fadd    fp0,e20,fp0
        fadd    fp1,e21,fp1
        fadd    fp2,e22,fp2
        fadd    fp3,e23,fp3

        ; Term 3: a23 * b3x -> e20..e23
        fmul    e19,e12,e20
        fmul    e19,e13,e21
        fmul    e19,e14,e22
        fmul    e19,e15,e23

        ; Add Row 2 + Term 2
        fadd    fp0,fp4,fp0
        fadd    fp1,fp5,fp1
        fadd    fp2,fp6,fp2
        fadd    fp3,fp7,fp3

        ; ==================== ROW 3 PRELOAD ====================
        fmove.s 48(a0),e16      ; a30
        fmove.s 52(a0),e17      ; a31
        fmove.s 56(a0),e18      ; a32
        fmove.s 60(a0),e19      ; a33

        ; Add Row 2 + Term 3 (Row 2 complete!)
        fadd    fp0,e20,fp0     ; c20
        fadd    fp1,e21,fp1     ; c21
        fadd    fp2,e22,fp2     ; c22
        fadd    fp3,e23,fp3     ; c23

        ; ==================== ROW 3 MATH ====================
        ; Term 0: a30 * b0x -> fp4..fp7
        fmul    e16,e0,fp4
        fmul    e16,e1,fp5
        fmul    e16,e2,fp6
        fmul    e16,e3,fp7

        ; Store Row 2 results
        fmove.s fp0,32(a2)
        fmove.s fp1,36(a2)
        fmove.s fp2,40(a2)
        fmove.s fp3,44(a2)

        ; Term 1: a31 * b1x -> e20..e23
        fmul    e17,e4,e20
        fmul    e17,e5,e21
        fmul    e17,e6,e22
        fmul    e17,e7,e23

        ; Term 2: a32 * b2x -> fp0..fp3
        fmul    e18,e8,fp0
        fmul    e18,e9,fp1
        fmul    e18,e10,fp2
        fmul    e18,e11,fp3

        ; Add Row 3 Term 0 + Term 1
        fadd    fp4,e20,fp4
        fadd    fp5,e21,fp5
        fadd    fp6,e22,fp6
        fadd    fp7,e23,fp7

        ; Term 3: a33 * b3x -> e20..e23
        fmul    e19,e12,e20
        fmul    e19,e13,e21
        fmul    e19,e14,e22
        fmul    e19,e15,e23

        ; Add Row 3 + Term 2
        fadd    fp4,fp0,fp4
        fadd    fp5,fp1,fp5
        fadd    fp6,fp2,fp6
        fadd    fp7,fp3,fp7

        ; Add Row 3 + Term 3 (Row 3 complete!)
        fadd    fp4,e20,fp4     ; c30
        fadd    fp5,e21,fp5     ; c31
        fadd    fp6,e22,fp6     ; c32
        fadd    fp7,e23,fp7     ; c33

        ; Store Row 3 results
        fmove.s fp4,48(a2)
        fmove.s fp5,52(a2)
        fmove.s fp6,56(a2)
        fmove.s fp7,60(a2)
