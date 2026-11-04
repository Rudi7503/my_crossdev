; seq/FIX-102.i - 3x3 16-bit fixed-point matrix multiplication with Interleaved Pipes & Early Store
; Uses: e0, e1, e2, e4, e5, e6, e8, e9, e10, e12, e13, e14, e15, e16, e17, a0, a1, a2
; MUST NOT touch: d5, d6, d7, a5, a6, a7
        load    (a0),e0
        load    6(a0),e1
        load    12(a0),e2
        load    (a1),e4
        load    6(a1),e5
        load    12(a1),e6
        vperm   #$01010101,e0,e0,e8
        vperm   #$01010101,e1,e1,e15
        vperm   #$23232323,e0,e0,e9
        vperm   #$23232323,e1,e1,e16
        vperm   #$45454545,e0,e0,e10
        vperm   #$45454545,e1,e1,e17
        pmulh   e8,e4,e12
        pmulh   e15,e4,e3
        pmulh   e9,e5,e13
        pmulh   e16,e5,e7
        pmulh   e10,e6,e14
        pmulh   e17,e6,e11
        paddw   e12,e13,e12
        paddw   e3,e7,e3
        paddw   e12,e14,e12
        paddw   e3,e11,e3
        store   e12,(a2)
        store   e3,6(a2)
        vperm   #$01010101,e2,e2,e8
        vperm   #$23232323,e2,e2,e9
        vperm   #$45454545,e2,e2,e10
        pmulh   e8,e4,e12
        pmulh   e9,e5,e13
        pmulh   e10,e6,e14
        paddw   e12,e13,e12
        paddw   e12,e14,e12
        store   e12,12(a2)
