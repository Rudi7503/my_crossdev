; seq/FFT-004.i - Stage 1 Vectorized AMMX SIMD Butterfly (2 Butterflies, zero twiddles)
; Inputs:  a0 = buffer (16 bytes = 4 complex numbers: u0, v0, u1, v1)
; Uses:    e0, e1, e2, e3, e4, e5, e10, e11, e12, e13, a0
; MUST NOT touch: d5, d6, d7, a5, a6, a7

        load.w  #$8000,e10              ; e10 = $8000 for >> 1 scaling
        peor    e11,e11,e11             ; e11 = 0

        load    (a0),e0                 ; [u0, v0]
        load    8(a0),e1                ; [u1, v1]

        vperm   #$012389ab,e0,e1,e2     ; e2 = [u0, u1]
        vperm   #$4567cdef,e0,e1,e3     ; e3 = [v0, v1]

        pmulh   e10,e2,e4
        psubw   e4,e11,e2               ; e2 = u_half

        pmulh   e10,e3,e5
        psubw   e5,e11,e3               ; e3 = v_half

        bflyw   e3,e2,e12:e13           ; e12 = u', e13 = v'

        vperm   #$012389ab,e12,e13,e0   ; e0 = [u0', v0']
        vperm   #$4567cdef,e12,e13,e1   ; e1 = [u1', v1']

        store   e0,(a0)
        store   e1,8(a0)
