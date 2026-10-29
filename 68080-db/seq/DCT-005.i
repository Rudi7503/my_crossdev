; seq/DCT-005.i - 1D Fast Float FPU 8-Point DCT Kernel (Loeffler Algorithm)
; Inputs:  a0 = src (8 floats = 32 bytes), a1 = dst (8 floats = 32 bytes)
; Constants: c4, c2, s2, c1, c3, c5, c7
; Uses:    fp0, fp1, fp2, fp3, fp4, fp5, fp6, fp7, a0, a1
; MUST NOT touch: d5, d6, d7, a5, a6, a7

        ; Stage 1: Butterflies
        fmove.s (a0),fp0                ; x0
        fmove.s 28(a0),fp1              ; x7
        fmove.x fp0,fp2
        fadd.x  fp1,fp2                 ; s0 = x0 + x7
        fsub.x  fp1,fp0                 ; d0 = x0 - x7

        fmove.s 4(a0),fp1               ; x1
        fmove.s 24(a0),fp3              ; x6
        fmove.x fp1,fp4
        fadd.x  fp3,fp4                 ; s1 = x1 + x6
        fsub.x  fp3,fp1                 ; d1 = x1 - x6

        fmove.s 8(a0),fp3               ; x2
        fmove.s 20(a0),fp5              ; x5
        fmove.x fp3,fp6
        fadd.x  fp5,fp6                 ; s2 = x2 + x5
        fsub.x  fp5,fp3                 ; d2 = x2 - x5

        fmove.s 12(a0),fp5              ; x3
        fmove.s 16(a0),fp7              ; x4
        fadd.x  fp7,fp5                 ; s3 = x3 + x4
        fsub.x  fp7,fp7                 ; d3

        ; Even Stage 2:
        ; t0 = s0 + s3, t1 = s1 + s2, t2 = s1 - s2, t3 = s0 - s3
        fmove.x fp2,fp7
        fadd.x  fp5,fp7                 ; t0 = s0 + s3
        fsub.x  fp5,fp2                 ; t3 = s0 - s3

        fmove.x fp4,fp5
        fadd.x  fp6,fp5                 ; t1 = s1 + s2
        fsub.x  fp6,fp4                 ; t2 = s1 - s2

        ; X0 = (t0 + t1) * c4, X4 = (t0 - t1) * c4
        fmove.x fp7,fp6
        fadd.x  fp5,fp6
        fmul.s  #0.35355339,fp6
        fmove.s fp6,(a1)                ; dst[0]

        fsub.x  fp5,fp7
        fmul.s  #0.35355339,fp7
        fmove.s fp7,16(a1)              ; dst[4]

        ; X2 = c2*t3 + s2*t2, X6 = s2*t3 - c2*t2
        fmove.x fp2,fp6
        fmul.s  #0.46193977,fp6         ; c2*t3
        fmove.x fp4,fp7
        fmul.s  #0.19134172,fp7         ; s2*t2
        fadd.x  fp7,fp6
        fmove.s fp6,8(a1)               ; dst[2]

        fmove.x fp2,fp6
        fmul.s  #0.19134172,fp6         ; s2*t3
        fmove.x fp4,fp7
        fmul.s  #0.46193977,fp7         ; c2*t2
        fsub.x  fp7,fp6
        fmove.s fp6,24(a1)              ; dst[6]
