; seq/FIX-101.i - 2x2 16-bit fixed-point matrix multiplication with Interleaved Dual Pipes
; Uses: e0-e11, a0, a1, a2
; MUST NOT touch: d5, d6, d7, a5, a6, a7
        load    (a0),e0
        load    (a1),e1
        vperm   #$01010101,e0,e0,e2
        vperm   #$45454545,e0,e0,e4
        vperm   #$23232323,e0,e0,e3
        vperm   #$67676767,e0,e0,e5
        vperm   #$01230123,e1,e1,e6
        vperm   #$45674567,e1,e1,e7
        pmulh   e2,e6,e8
        pmulh   e4,e6,e10
        pmulh   e3,e7,e9
        pmulh   e5,e7,e11
        paddw   e8,e9,e8
        paddw   e10,e11,e10
        vperm   #$012389ab,e8,e10,e0
        store   e0,(a2)
