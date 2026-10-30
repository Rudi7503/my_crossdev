        machine 68080
        xdef    _test_only_bsel
        section code,code

; void test_only_bsel(uint16_t *dst, const uint16_t *src, const uint16_t *key_vec)
; dst = 4(sp), src = 8(sp), key_vec = 12(sp)
_test_only_bsel:
        move.l  4(sp),a0        ; dst
        move.l  8(sp),a1        ; src
        move.l  12(sp),a2       ; key_vec
        load    (a1),e0         ; src
        load    (a2),e3         ; key
        pcmpeqw e3,e0,e2        ; mask: FFFF where src==key (transparent), 0000 where opaque
        ; bsel <vea>, mask, d: where mask=1 take <vea>, where mask=0 keep d
        ; If <vea> = (a0) (dst memory), mask = e2, d = e0 (src):
        ; where mask=FFFF (transparent): take (a0) (preserve dst)
        ; where mask=0000 (opaque): keep e0 (src)
        bsel    (a0),e2,e0
        store   e0,(a0)
        rts
