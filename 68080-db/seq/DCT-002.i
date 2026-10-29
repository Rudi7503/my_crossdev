; seq/DCT-002.i - 1D 8-Point Fixed Scalar DCT Point Kernel (8 16-bit MACs, Q1.15)
; Inputs:  a0 = src (8 words = 16 bytes), a1 = matrix row (8 words = 16 bytes)
; Output:  a2 = dst (word = 2 bytes)
; Uses:    d0, d1, d2, a0, a1, a2
; MUST NOT touch: d5, d6, d7, a5, a6, a7

        move.l  #16384,d0

        move.w  (a0),d1
        move.w  (a1),d2
        muls.w  d1,d2
        add.l   d2,d0

        move.w  2(a0),d1
        move.w  2(a1),d2
        muls.w  d1,d2
        add.l   d2,d0

        move.w  4(a0),d1
        move.w  4(a1),d2
        muls.w  d1,d2
        add.l   d2,d0

        move.w  6(a0),d1
        move.w  6(a1),d2
        muls.w  d1,d2
        add.l   d2,d0

        move.w  8(a0),d1
        move.w  8(a1),d2
        muls.w  d1,d2
        add.l   d2,d0

        move.w  10(a0),d1
        move.w  10(a1),d2
        muls.w  d1,d2
        add.l   d2,d0

        move.w  12(a0),d1
        move.w  12(a1),d2
        muls.w  d1,d2
        add.l   d2,d0

        move.w  14(a0),d1
        move.w  14(a1),d2
        muls.w  d1,d2
        add.l   d2,d0

        asr.l   #8,d0
        asr.l   #7,d0
        move.w  d0,(a2)
