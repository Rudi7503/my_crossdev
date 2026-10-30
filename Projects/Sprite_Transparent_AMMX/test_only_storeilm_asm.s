        machine 68080
        xdef    _test_only_storeilm
        section code,code

; void test_only_storeilm(uint16_t *dst, const uint16_t *src, const uint16_t *key_vec)
_test_only_storeilm:
        move.l  4(sp),a0        ; dst
        move.l  8(sp),a1        ; src
        move.l  12(sp),a2       ; key_vec
        load    (a1),e0         ; src
        load    (a2),e1         ; key
        pcmpeqw e1,e0,e2        ; mask: FFFF for match, 0000 for non-match
        storeilm e0,e2,(a0)
        rts
