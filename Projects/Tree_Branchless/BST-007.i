; seq/BST-007.i - 2-Level Unrolled Pipeline-Interleaved BST Traversal
; Uses: d0, d1, d2, d3, a0, a1
; MUST NOT touch: d5, d6, d7, a5, a6, a7
; a0 = node pointer, d1 = search key
; Level 1:
        move.l  (a0),d0
        sub.l   d1,d0
        subx.w  d2,d2
        move.l  8(a0,d2.w*4),a1
; Level 2:
        move.l  (a1),d3
        sub.l   d1,d3
        subx.w  d2,d2
        move.l  8(a1,d2.w*4),a0
