; seq/FFT-006.i - Stage 2 Vectorized AMMX SIMD Butterfly (2 Butterflies, W0=1, W1=-i, zero twiddle loads)
; Inputs:  a0 = buffer (16 bytes = 4 complex numbers: u0, u1, v0, v1)
; Uses:    e0, e1, e2, e3, e4, e5, e10, e11, e12, e13, a0
; MUST NOT touch: d5, d6, d7, a5, a6, a7

        load.w  #$8000,e10              ; e10 = $8000 for >> 1 scaling
        peor    e11,e11,e11             ; e11 = 0

        load    (a0),e0                 ; [u0, u1]
        load    8(a0),e1                ; [v0, v1]

        pmulh   e10,e0,e2
        psubw   e2,e11,e0               ; e0 = u_half = [u0, u1]

        pmulh   e10,e1,e3
        psubw   e3,e11,e1               ; e1 = v_half = [v0, v1]

        psubw   e1,e11,e4               ; e4 = -v_half
        vperm   #$012367cd,e1,e4,e5     ; e5 = T = [v0.r, v0.i, v1.i, -v1.r]

        bflyw   e5,e0,e12:e13           ; e12 = u', e13 = v'

        store   e12,(a0)
        store   e13,8(a0)
