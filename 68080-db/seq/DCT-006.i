; seq/DCT-006.i - 1D Fast Fixed AMMX SIMD 8-Point DCT Kernel (Loeffler Algorithm)
; Inputs:  a0 = src (8 words = 16 bytes), a1 = dst (8 words = 16 bytes)
; Constants: a2 = constants pointer (c4, c2_s2, s2_mc2, odd1..7)
; Uses:    e0, e1, e2, e3, e4, e5, e6, e7, e8, e9, e10, e11, e12, e13, e14, e15, e16, e17, e18, e19, e20, e21, a0, a1, a2
; MUST NOT touch: d5, d6, d7, a5, a6, a7

        load    (a0),e0                 ; [x0, x1, x2, x3]
        load    8(a0),e1                ; [x4, x5, x6, x7]

        ; --- Stage 1: Mirror & Butterflies ---
        vperm   #$67452301,e1,e1,e2     ; e2 = [x7, x6, x5, x4]
        paddw   e2,e0,e3                ; e3 = [s0, s1, s2, s3] (Even inputs)
        psubw   e2,e0,e4                ; e4 = [d0, d1, d2, d3] (Odd inputs)

        ; --- Stage 2 Even ---
        vperm   #$67452301,e3,e3,e5     ; e5 = [s3, s2, s1, s0]
        paddw   e5,e3,e6                ; e6 = [t0, t1, t1, t0]
        psubw   e5,e3,e7                ; e7 = [t3, t2, -t2, -t3]

        ; X0 = (t0 + t1)*c4, X4 = (t0 - t1)*c4 (product first to prevent overflow)
        load    (a2),e12                ; _fast_c4
        pmulh   e6,e12,e8               ; e8 = [t0*c4, t1*c4, t1*c4, t0*c4]
        vperm   #$23016745,e8,e8,e9     ; e9 = [t1*c4, t0*c4, t0*c4, t1*c4]
        paddw   e9,e8,e10               ; e10[0] = (t0+t1)*c4 = X0/2
        psubw   e9,e8,e11               ; e11[0] = (t0-t1)*c4 = X4/2
        vperm   #$01890189,e10,e11,e13  ; e13 = [X0/2, X4/2, X0/2, X4/2]
        paddw   e13,e13,e13             ; [X0, X4, X0, X4]

        ; X2 = t3*c2 + t2*s2, X6 = t3*s2 - t2*c2
        vperm   #$01010101,e7,e7,e14    ; [t3, t3, t3, t3]
        vperm   #$23232323,e7,e7,e15    ; [t2, t2, t2, t2]
        load    8(a2),e16               ; _fast_c2_s2
        load    16(a2),e17              ; _fast_s2_mc2
        pmulh   e14,e16,e18             ; [t3*c2, t3*s2, ...]
        pmulh   e15,e17,e19             ; [t2*s2, -t2*c2, ...]
        paddw   e19,e18,e18             ; [X2/2, X6/2, ...]
        paddw   e18,e18,e18             ; [X2, X6, X2, X6]

        ; Combine Even outputs: [X0, X2, X4, X6]
        vperm   #$018923ab,e13,e18,e20  ; e20 = [X0, X2, X4, X6]

        ; --- Stage 2 Odd ---
        load    24(a2),e12              ; _fast_odd1
        load    32(a2),e13              ; _fast_odd3
        pmulh   e4,e12,e8               ; p1
        pmulh   e4,e13,e9               ; p3

        load    40(a2),e12              ; _fast_odd5
        load    48(a2),e13              ; _fast_odd7
        pmulh   e4,e12,e10              ; p5
        pmulh   e4,e13,e11              ; p7

        ; 2-way horizontal reduction on (p1, p3):
        vperm   #$018945cd,e8,e9,e16
        vperm   #$23ab67ef,e8,e9,e17
        paddw   e17,e16,e16
        vperm   #$45670123,e16,e16,e18
        paddw   e18,e16,e16
        paddw   e16,e16,e16             ; [X1, X3, X1, X3]

        ; 2-way horizontal reduction on (p5, p7):
        vperm   #$018945cd,e10,e11,e17
        vperm   #$23ab67ef,e10,e11,e18
        paddw   e18,e17,e17
        vperm   #$45670123,e17,e17,e19
        paddw   e19,e17,e17
        paddw   e17,e17,e17             ; [X5, X7, X5, X7]

        ; Combine Odd outputs: [X1, X3, X5, X7]
        vperm   #$012389ab,e16,e17,e21  ; e21 = [X1, X3, X5, X7]

        ; Interleave Even & Odd into [X0, X1, X2, X3] and [X4, X5, X6, X7]
        vperm   #$018923ab,e20,e21,e0
        vperm   #$45cd67ef,e20,e21,e1

        store   e0,(a1)
        store   e1,8(a1)
