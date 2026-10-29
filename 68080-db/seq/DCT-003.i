; seq/DCT-003.i - 1D Fixed AMMX SIMD 2-Point DCT Kernel (2-way parallel reduction, 32-bit store)
; Inputs:  a0 = src (8 words = 16 bytes), a1 = 2 matrix rows (16 words = 32 bytes)
; Output:  a2 = dst (2 words = 4 bytes)
; Uses:    d1, e0, e1, e2, e3, e4, e5, e6, e7, e8, e9, e10, a0, a1, a2
; MUST NOT touch: d5, d6, d7, a5, a6, a7

        load    (a0),e0                 ; [x0, x1, x2, x3]
        load    8(a0),e1                ; [x4, x5, x6, x7]

        ; Point k:
        load    (a1),e2                 ; C_k[0..3]
        load    8(a1),e3                ; C_k[4..7]
        pmulh   e0,e2,e4
        pmulh   e1,e3,e5
        paddw   e5,e4,e4                ; e4 = [p0..p3] for k

        ; Point k+1:
        load    16(a1),e2               ; C_k+1[0..3]
        load    24(a1),e3               ; C_k+1[4..7]
        pmulh   e0,e2,e6
        pmulh   e1,e3,e7
        paddw   e7,e6,e6                ; e6 = [p0..p3] for k+1

        ; Combined horizontal reduction of e4 (k) and e6 (k+1):
        vperm   #$018945cd,e4,e6,e8     ; [a0, b0, a2, b2]
        vperm   #$23ab67ef,e4,e6,e9     ; [a1, b1, a3, b3]
        paddw   e9,e8,e8                ; [a0+a1, b0+b1, a2+a3, b2+b3]
        vperm   #$45670123,e8,e8,e10    ; [a2+a3, b2+b3, a0+a1, b0+b1]
        paddw   e10,e8,e8               ; [sum_a, sum_b, sum_a, sum_b]

        paddw   e8,e8,e8                ; * 2 for Q1.15

        move.l  e8,d1                   ; d1 = [sum_k (high 16), sum_k+1 (low 16)]
        move.l  d1,(a2)                 ; write both output points in one 32-bit store!
