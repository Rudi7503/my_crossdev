; seq/DCT-101.i - 1D 8-Point Float FPU DCT with Dual-Accumulator Tree
; Uses: fp0, fp1, fp2, fp3, a0, a1, a2
; MUST NOT touch: d5, d6, d7, a5, a6, a7
        fmove.s (a0),fp0
        fmul.s  (a1),fp0
        fmove.s 4(a0),fp2
        fmul.s  4(a1),fp2
        fmove.s 8(a0),fp1
        fmul.s  8(a1),fp1
        fadd.x  fp1,fp0
        fmove.s 12(a0),fp3
        fmul.s  12(a1),fp3
        fadd.x  fp3,fp2
        fmove.s 16(a0),fp1
        fmul.s  16(a1),fp1
        fadd.x  fp1,fp0
        fmove.s 20(a0),fp3
        fmul.s  20(a1),fp3
        fadd.x  fp3,fp2
        fmove.s 24(a0),fp1
        fmul.s  24(a1),fp1
        fadd.x  fp1,fp0
        fmove.s 28(a0),fp3
        fmul.s  28(a1),fp3
        fadd.x  fp3,fp2
        fadd.x  fp2,fp0
        fmove.s fp0,(a2)
