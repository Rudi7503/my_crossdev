; seq/FFT-005.i - Table-Accelerated Fixed-Point Bit-Reversal Swap Pair (1 Swap = 4 bytes)
; Inputs:  a0 = data array, a1 = swap_pair array pointer
; Uses:    d0, d1, d2, d3, a0, a1
; MUST NOT touch: d5, d6, d7, a5, a6, a7

        move.w  (a1)+,d0                ; offset index i
        move.w  (a1)+,d1                ; offset index j
        move.l  (a0,d0.w*4),d2          ; load x[i]
        move.l  (a0,d1.w*4),d3          ; load x[j]
        move.l  d3,(a0,d0.w*4)          ; store x[j] -> x[i]
        move.l  d2,(a0,d1.w*4)          ; store x[i] -> x[j]
