; seq/AMX-009.i - branchless scalar four 16-bit lane maxima (reference)
; Uses: d1-d3, a0-a2
; MUST NOT touch: d5, d6, d7, a5, a6, a7
        move.w  (a0)+,d1
        move.w  (a1)+,d2
        move.w  d1,d3
        cmp.w   d2,d1
        sge.b   d3
        ext.w   d3
        and.w   d3,d1
        not.w   d3
        and.w   d3,d2
        or.w    d2,d1
        move.w  d1,(a2)+

        move.w  (a0)+,d1
        move.w  (a1)+,d2
        move.w  d1,d3
        cmp.w   d2,d1
        sge.b   d3
        ext.w   d3
        and.w   d3,d1
        not.w   d3
        and.w   d3,d2
        or.w    d2,d1
        move.w  d1,(a2)+

        move.w  (a0)+,d1
        move.w  (a1)+,d2
        move.w  d1,d3
        cmp.w   d2,d1
        sge.b   d3
        ext.w   d3
        and.w   d3,d1
        not.w   d3
        and.w   d3,d2
        or.w    d2,d1
        move.w  d1,(a2)+

        move.w  (a0)+,d1
        move.w  (a1)+,d2
        move.w  d1,d3
        cmp.w   d2,d1
        sge.b   d3
        ext.w   d3
        and.w   d3,d1
        not.w   d3
        and.w   d3,d2
        or.w    d2,d1
        move.w  d1,(a2)+
