; seq/FIX-003.i - 4x4 16-bit fixed-point matrix multiplication (C = A * B)
; Fully unrolled 64-bit AMMX SIMD execution - dual-issue optimized
; Inputs:  a0 = matrix A (4x4 words = 32 bytes), a1 = matrix B (4x4 words = 32 bytes)
; Output:  a2 = matrix C (4x4 words = 32 bytes)
; Uses:    e0-e15, a0, a1, a2
; MUST NOT touch: d5, d6, d7, a5, a6, a7

        ; 1. Load Matrix A (4 quad-loads = 32 bytes)
        load    (a0),e0         ; Row 0
        load    8(a0),e1        ; Row 1
        load    16(a0),e2       ; Row 2
        load    24(a0),e3       ; Row 3

        ; 2. Load Matrix B (4 quad-loads = 32 bytes)
        load    (a1),e4         ; Row 0
        load    8(a1),e5        ; Row 1
        load    16(a1),e6       ; Row 2
        load    24(a1),e7       ; Row 3

        ; ==================== ROW 0 ====================
        vperm   #$01010101,e0,e0,e8   ; splat a00
        vperm   #$23232323,e0,e0,e9   ; splat a01
        vperm   #$45454545,e0,e0,e10  ; splat a02
        vperm   #$67676767,e0,e0,e11  ; splat a03

        pmulh   e8,e4,e12       ; a00 * Row 0(B)
        pmulh   e9,e5,e13       ; a01 * Row 1(B)
        pmulh   e10,e6,e14      ; a02 * Row 2(B)
        pmulh   e11,e7,e15      ; a03 * Row 3(B)

        paddw   e12,e13,e12
        paddw   e14,e15,e14
        paddw   e12,e14,e12     ; Row 0 of C complete in e12!

        ; ==================== ROW 1 ====================
        vperm   #$01010101,e1,e1,e8   ; splat a10
        vperm   #$23232323,e1,e1,e9   ; splat a11
        vperm   #$45454545,e1,e1,e10  ; splat a12
        vperm   #$67676767,e1,e1,e11  ; splat a13
        store   e12,(a2)              ; Dual-issue store Row 0!

        pmulh   e8,e4,e12       ; a10 * Row 0(B)
        pmulh   e9,e5,e13       ; a11 * Row 1(B)
        pmulh   e10,e6,e14      ; a12 * Row 2(B)
        pmulh   e11,e7,e15      ; a13 * Row 3(B)

        paddw   e12,e13,e12
        paddw   e14,e15,e14
        paddw   e12,e14,e12     ; Row 1 of C complete in e12!

        ; ==================== ROW 2 ====================
        vperm   #$01010101,e2,e2,e8   ; splat a20
        vperm   #$23232323,e2,e2,e9   ; splat a21
        vperm   #$45454545,e2,e2,e10  ; splat a22
        vperm   #$67676767,e2,e2,e11  ; splat a23
        store   e12,8(a2)             ; Dual-issue store Row 1!

        pmulh   e8,e4,e12       ; a20 * Row 0(B)
        pmulh   e9,e5,e13       ; a21 * Row 1(B)
        pmulh   e10,e6,e14      ; a22 * Row 2(B)
        pmulh   e11,e7,e15      ; a23 * Row 3(B)

        paddw   e12,e13,e12
        paddw   e14,e15,e14
        paddw   e12,e14,e12     ; Row 2 of C complete in e12!

        ; ==================== ROW 3 ====================
        vperm   #$01010101,e3,e3,e8   ; splat a30
        vperm   #$23232323,e3,e3,e9   ; splat a31
        vperm   #$45454545,e3,e3,e10  ; splat a32
        vperm   #$67676767,e3,e3,e11  ; splat a33
        store   e12,16(a2)            ; Dual-issue store Row 2!

        pmulh   e8,e4,e12       ; a30 * Row 0(B)
        pmulh   e9,e5,e13       ; a31 * Row 1(B)
        pmulh   e10,e6,e14      ; a32 * Row 2(B)
        pmulh   e11,e7,e15      ; a33 * Row 3(B)

        paddw   e12,e13,e12
        paddw   e14,e15,e14
        paddw   e12,e14,e12     ; Row 3 of C complete in e12!
        store   e12,24(a2)            ; Store Row 3!
