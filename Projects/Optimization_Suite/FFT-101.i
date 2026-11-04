; seq/FFT-101.i - 32-bit Float FFT Butterfly (Radix-2) with Parallel Load Scheduling
; Uses: fp0, fp1, fp2, fp3, fp4, fp5, fp6, fp7, a0, a1, a2, a3, a4
; MUST NOT touch: d5, d6, d7, a5, a6, a7
        fmove.s (a2),fp0
        fmove.s 4(a2),fp1
        fmove.s (a0),fp2
        fmove.s 4(a0),fp3
        fmove.s (a1),fp4
        fmove.s 4(a1),fp5
        fmul    fp0,fp4,fp6
        fmul    fp1,fp5,fp7
        fmul    fp0,fp5,fp5
        fmul    fp1,fp4,fp4
        fsub    fp7,fp6,fp6
        fadd    fp4,fp5,fp5
        fadd    fp6,fp2,fp4
        fsub    fp6,fp2,fp2
        fmove.s fp4,(a3)
        fmove.s fp2,(a4)
        fadd    fp5,fp3,fp4
        fsub    fp5,fp3,fp3
        fmove.s fp4,4(a3)
        fmove.s fp3,4(a4)
