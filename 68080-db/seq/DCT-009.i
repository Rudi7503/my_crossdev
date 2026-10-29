; seq/DCT-009.i - MP3 18-Sample 50% Overlap-Add Kernel via AMMX SIMD (PADDW)
; Adds current 18-sample subband to previous 18-sample overlap buffer
; Inputs:  a0 = current subband window (18 int16 words, 36 bytes)
;          a1 = previous subband tail (18 int16 words, 36 bytes)
; Output:  a2 = output PCM buffer (18 int16 words, 36 bytes)
; Uses:    e0, e1, e2, d0, a0, a1, a2
; MUST NOT touch: d5, d6, d7, a5, a6, a7

        ; Samples 0..3 (Quadword 0)
        load    (a0),e0
        load    (a1),e1
        paddw   e1,e0,e2
        store   e2,(a2)

        ; Samples 4..7 (Quadword 1)
        load    8(a0),e0
        load    8(a1),e1
        paddw   e1,e0,e2
        store   e2,8(a2)

        ; Samples 8..11 (Quadword 2)
        load    16(a0),e0
        load    16(a1),e1
        paddw   e1,e0,e2
        store   e2,16(a2)

        ; Samples 12..15 (Quadword 3)
        load    24(a0),e0
        load    24(a1),e1
        paddw   e1,e0,e2
        store   e2,24(a2)

        ; Samples 16..17 (Scalar Words)
        move.w  32(a0),d0
        add.w   32(a1),d0
        move.w  d0,32(a2)

        move.w  34(a0),d0
        add.w   34(a1),d0
        move.w  d0,34(a2)
