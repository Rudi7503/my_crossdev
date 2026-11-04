; seq/BST-016.i - 10 Consecutive Eytzinger Steps (Clean 10x Tree Traversal)
; Uses: d0, d1, d2, a0
; MUST NOT touch: d5, d6, d7, a5, a6, a7
; a0 = base array pointer, d1 = search key, d2 = index k
        move.l  (a0,d2.l*4),d0
        sub.l   d1,d0
        addx.l  d2,d2

        move.l  (a0,d2.l*4),d0
        sub.l   d1,d0
        addx.l  d2,d2

        move.l  (a0,d2.l*4),d0
        sub.l   d1,d0
        addx.l  d2,d2

        move.l  (a0,d2.l*4),d0
        sub.l   d1,d0
        addx.l  d2,d2

        move.l  (a0,d2.l*4),d0
        sub.l   d1,d0
        addx.l  d2,d2

        move.l  (a0,d2.l*4),d0
        sub.l   d1,d0
        addx.l  d2,d2

        move.l  (a0,d2.l*4),d0
        sub.l   d1,d0
        addx.l  d2,d2

        move.l  (a0,d2.l*4),d0
        sub.l   d1,d0
        addx.l  d2,d2

        move.l  (a0,d2.l*4),d0
        sub.l   d1,d0
        addx.l  d2,d2

        move.l  (a0,d2.l*4),d0
        sub.l   d1,d0
        addx.l  d2,d2
