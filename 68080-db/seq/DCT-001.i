; seq/DCT-001.i - 1D 8-Point Float FPU DCT Point Kernel (8 single-precision MACs)
; Inputs:  a0 = src (8 floats = 32 bytes), a1 = matrix row (8 floats = 32 bytes)
; Output:  a2 = dst (float = 4 bytes)
; Uses:    fp0, fp1, a0, a1, a2
; MUST NOT touch: d5, d6, d7, a5, a6, a7

        fmove.s (a0),fp0
        fmul.s  (a1),fp0

        fmove.s 4(a0),fp1
        fmul.s  4(a1),fp1
        fadd.x  fp1,fp0

        fmove.s 8(a0),fp1
        fmul.s  8(a1),fp1
        fadd.x  fp1,fp0

        fmove.s 12(a0),fp1
        fmul.s  12(a1),fp1
        fadd.x  fp1,fp0

        fmove.s 16(a0),fp1
        fmul.s  16(a1),fp1
        fadd.x  fp1,fp0

        fmove.s 20(a0),fp1
        fmul.s  20(a1),fp1
        fadd.x  fp1,fp0

        fmove.s 24(a0),fp1
        fmul.s  24(a1),fp1
        fadd.x  fp1,fp0

        fmove.s 28(a0),fp1
        fmul.s  28(a1),fp1
        fadd.x  fp1,fp0

        fmove.s fp0,(a2)
