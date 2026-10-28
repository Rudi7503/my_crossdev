; seq/MAT-001.i - 2x2 single-precision float matrix multiplication (C = A * B)
; Fully pipelined 68080 FPU scheduling
; Inputs:  a0 = matrix A (2x2, 16 bytes), a1 = matrix B (2x2, 16 bytes)
; Output:  a2 = matrix C (2x2, 16 bytes)
; Uses:    d0, d1, fp0, fp1, fp2, fp3, fp4, fp5, fp6, fp7, a0, a1, a2
; MUST NOT touch: d5, d6, d7, a5, a6, a7
        ; 1. Preload Matrix B into fp4..fp7
        fmove.s (a1),fp4        ; b00
        fmove.s 4(a1),fp5       ; b01
        fmove.s 8(a1),fp6       ; b10
        fmove.s 12(a1),fp7      ; b11

        ; 2. Load Row 0 of A into fp0, fp1
        fmove.s (a0),fp0        ; a00
        fmove.s 4(a0),fp1       ; a01

        ; 3. Issue Row 0 multiplications
        fmove   fp0,fp2
        fmul    fp4,fp2         ; p00 = a00 * b00
        fmove   fp1,fp3
        fmul    fp6,fp3         ; p01 = a01 * b10
        fmul    fp5,fp0         ; p02 = a00 * b01
        fmul    fp7,fp1         ; p03 = a01 * b11

        ; 4. Preload Row 1 of A into integer scratch registers d0, d1 (hides FPU latency)
        move.l  8(a0),d0        ; a10
        move.l  12(a0),d1       ; a11

        ; 5. Add Row 0 products
        fadd    fp3,fp2         ; c00 = p00 + p01
        fadd    fp1,fp0         ; c01 = p02 + p03

        ; 6. Load Row 1 into fp1, fp3
        fmove.s d0,fp1          ; a10
        fmove.s d1,fp3          ; a11

        ; Store Row 0 results
        fmove.s fp2,(a2)        ; store c00
        fmove.s fp0,4(a2)       ; store c01

        ; 7. Issue Row 1 multiplications
        fmove   fp1,fp2
        fmul    fp4,fp2         ; p10 = a10 * b00
        fmove   fp3,fp0
        fmul    fp6,fp0         ; p11 = a11 * b10
        fmul    fp5,fp1         ; p12 = a10 * b01
        fmul    fp7,fp3         ; p13 = a11 * b11

        ; 8. Add Row 1 products
        fadd    fp0,fp2         ; c10 = p10 + p11
        fadd    fp3,fp1         ; c11 = p12 + p13

        ; Store Row 1 results
        fmove.s fp2,8(a2)       ; store c10
        fmove.s fp1,12(a2)      ; store c11
