        machine 68080
        xdef    _test_storem3_b
        xdef    _test_storem3_w
        xdef    _test_storem3_l
        xdef    _test_storeilm_b
        xdef    _test_storeilm_w
        xdef    _test_bsel_w
        xdef    _test_storem_b
        xdef    _test_storec
        section code,code

; void test_storem3_b(uint8_t *dst, const uint8_t *src, int mode)
; 4(sp) = dst, 8(sp) = src, 12(sp) = mode
_test_storem3_b:
        move.l  4(sp),a0
        move.l  8(sp),a1
        move.l  12(sp),d0
        load    (a1),e0
        storem3 e0,d0,(a0)
        rts

; void test_storem3_w(uint16_t *dst, const uint16_t *src, int mode)
; 4(sp) = dst, 8(sp) = src, 12(sp) = mode
_test_storem3_w:
        move.l  4(sp),a0
        move.l  8(sp),a1
        move.l  12(sp),d0
        load    (a1),e0
        storem3 e0,d0,(a0)
        rts

; void test_storem3_l(uint32_t *dst, const uint32_t *src, int mode)
; 4(sp) = dst, 8(sp) = src, 12(sp) = mode
_test_storem3_l:
        move.l  4(sp),a0
        move.l  8(sp),a1
        move.l  12(sp),d0
        load    (a1),e0
        storem3 e0,d0,(a0)
        rts

; void test_storeilm_b(uint8_t *dst, const uint8_t *src, uint32_t key)
; 4(sp) = dst, 8(sp) = src, 12(sp) = key
_test_storeilm_b:
        move.l  4(sp),a0
        move.l  8(sp),a1
        move.l  12(sp),d0
        load    (a1),e0
        pcmpeqb d0,e0,e2
        storeilm e0,e2,(a0)
        rts

; void test_storeilm_w(uint16_t *dst, const uint16_t *src, uint32_t key)
; 4(sp) = dst, 8(sp) = src, 12(sp) = key
_test_storeilm_w:
        move.l  4(sp),a0
        move.l  8(sp),a1
        move.l  12(sp),d0
        load    (a1),e0
        pcmpeqw d0,e0,e2
        storeilm e0,e2,(a0)
        rts

; void test_bsel_w(uint16_t *dst, const uint16_t *src, uint32_t key)
; 4(sp) = dst, 8(sp) = src, 12(sp) = key
_test_bsel_w:
        move.l  4(sp),a0
        move.l  8(sp),a1
        move.l  12(sp),d0
        load    (a0),e1         ; e1 = dst
        load    (a1),e0         ; e0 = src
        pcmpeqw d0,e0,e2        ; e2 = mask: $FFFF where src == key, $0000 where src != key
        ; bsel VEA, mask, d: d = (d & ~mask) | (VEA & mask)
        ; If VEA = dst (e1), d = src (e0), mask = e2:
        ; e0 = (src & ~e2) | (dst & e2) -> if mask==FFFF: dst, if mask==0000: src
        bsel    e1,e2,e0
        store   e0,(a0)
        rts

; void test_storem_b(uint8_t *dst, const uint8_t *src, uint32_t mask)
; 4(sp) = dst, 8(sp) = src, 12(sp) = mask
_test_storem_b:
        move.l  4(sp),a0
        move.l  8(sp),a1
        move.l  12(sp),d0
        load    (a1),e0
        storem  e0,d0,(a0)
        rts

; void test_storec(uint8_t *dst, const uint8_t *src, uint32_t count)
; 4(sp) = dst, 8(sp) = src, 12(sp) = count
_test_storec:
        move.l  4(sp),a0
        move.l  8(sp),a1
        move.l  12(sp),d0
        load    (a1),e0
        storec  e0,d0,(a0)
        rts
