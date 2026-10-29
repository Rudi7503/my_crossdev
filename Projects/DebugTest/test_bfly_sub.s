        section .text,code
        xdef    _test_bflyw

_test_bflyw:
        move.l  4(sp),a0                ; src1
        move.l  8(sp),a1                ; src2
        move.l  12(sp),a2               ; dst_sum
        move.l  16(sp),a3               ; dst_sub

        load    (a0),e0                 ; e0 = s1
        load    (a1),e1                 ; e1 = s2

        ; bflyw src1, src2, dest_pair
        bflyw   e1,e0,e2:e3             ; e2 = e0 + e1, e3 = e0 - e1

        store   e2,(a2)
        store   e3,(a3)
        rts
