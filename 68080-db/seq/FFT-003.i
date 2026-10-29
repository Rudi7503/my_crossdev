; seq/FFT-003.i - 16-bit Q1.15 Fixed-Point AMMX SIMD Butterfly (2 Butterflies parallel)
; Inputs:  a0 = u (2 complex numbers = 8 bytes), a1 = v (8 bytes), a2 = twiddle table (16 bytes)
; Outputs: a3 = u', a4 = v'
; Uses:    e0, e1, e2, e3, e4, e5, e6, e7, e8, e9, e10, e11, e12, e13, a0, a1, a2, a3, a4
; MUST NOT touch: d5, d6, d7, a5, a6, a7

        ; Load twiddle vectors
        load    (a2),e8                 ; e8 = E_W1 = [wr0, wi0, wr1, wi1]
        load    8(a2),e9                ; e9 = E_W2 = [-wi0, wr0, -wi1, wr1]

        load.w  #$8000,e10              ; e10 = $8000
        peor    e11,e11,e11             ; e11 = 0

        ; Load V
        load    (a1),e0

        ; Permute V into real/imag duplicated lanes
        vperm   #$01014545,e0,e0,e1     ; e1 = [v0.r, v0.r, v1.r, v1.r]
        vperm   #$23236767,e0,e0,e2     ; e2 = [v0.i, v0.i, v1.i, v1.i]

        ; Complex rotation (already >> 16, i.e., t/2)
        pmulh   e8,e1,e3                ; W1 * V1
        pmulh   e9,e2,e4                ; W2 * V2
        paddw   e4,e3,e5                ; e5 = t_half

        ; Load U and scale down by >> 1
        load    (a0),e6
        pmulh   e10,e6,e7               ; -(u >> 1)
        psubw   e7,e11,e6               ; +(u >> 1) = u_half

        ; Single-cycle Butterfly: Addition and Subtraction
        bflyw   e5,e6,e12:e13           ; e12 = u', e13 = v'

        ; Store outputs
        store   e12,(a3)
        store   e13,(a4)
