; seq/FFT-001.i - 32-bit Single-Precision Float FFT Butterfly (Radix-2)
; Inputs:  a0 = u (complex_float = 8 bytes), a1 = v, a2 = twiddle w
; Outputs: a3 = u', a4 = v'
; Uses:    fp0, fp1, fp2, fp3, fp4, fp5, fp6, fp7, a0, a1, a2, a3, a4
; MUST NOT touch: d5, d6, d7, a5, a6, a7

        ; Load twiddle factor w
        fmove.s (a2),fp0                ; wr
        fmove.s 4(a2),fp1               ; wi

        ; Load v
        fmove.s (a1),fp2                ; vr
        fmove.s 4(a1),fp3               ; vi

        ; Complex rotation: tr = wr*vr - wi*vi, ti = wr*vi + wi*vr
        fmul    fp0,fp2,fp4             ; wr * vr
        fmul    fp1,fp3,fp5             ; wi * vi
        fmul    fp0,fp3,fp6             ; wr * vi
        fmul    fp1,fp2,fp7             ; wi * vr

        fsub    fp5,fp4,fp4             ; tr = wr*vr - wi*vi
        fadd    fp7,fp6,fp6             ; ti = wr*vi + wi*vr

        ; Load u
        fmove.s (a0),fp2                ; ur
        fmove.s 4(a0),fp3               ; ui

        ; Butterfly additions and subtractions
        fadd    fp4,fp2,fp5             ; u'_r = ur + tr
        fsub    fp4,fp2,fp4             ; v'_r = ur - tr
        fadd    fp6,fp3,fp7             ; u'_i = ui + ti
        fsub    fp6,fp3,fp6             ; v'_i = ui - ti

        ; Store outputs
        fmove.s fp5,(a3)
        fmove.s fp7,4(a3)
        fmove.s fp4,(a4)
        fmove.s fp6,4(a4)
