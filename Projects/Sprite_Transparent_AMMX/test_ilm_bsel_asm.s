        machine 68080
        xdef    _test_storeilm_vec
        xdef    _test_bsel_vec
        section code,code

; void test_storeilm_vec(uint16_t *dst, const uint16_t *src, const uint16_t *key_vec)
_test_storeilm_vec:
        move.l  4(sp),a0        ; dst
        move.l  8(sp),a1        ; src
        move.l  12(sp),a2       ; key_vec
        load    (a1),e0         ; src
        load    (a2),e1         ; key
        pcmpeqw e1,e0,e2        ; mask: FFFF for match, 0000 for non-match
        storeilm e0,e2,(a0)
        rts

; void test_bsel_vec(uint16_t *dst, const uint16_t *src, const uint16_t *key_vec)
_test_bsel_vec:
        move.l  4(sp),a0        ; dst
        move.l  8(sp),a1        ; src
        move.l  12(sp),a2       ; key_vec
        load    (a0),e1         ; dst
        load    (a1),e0         ; src
        load    (a2),e3         ; key
        pcmpeqw e3,e0,e2        ; mask: FFFF where src==key (transparent), 0000 where opaque
        ; bsel VEA, mask, d: d = (d & ~mask) | (VEA & mask)
        ; If VEA = dst (e1), mask = e2, d = src (e0):
        ; e0 = (src & ~e2) | (dst & e2) -> where mask==FFFF: dst, where mask==0000: src
        bsel    e1,e2,e0
        store   e0,(a0)
        rts
