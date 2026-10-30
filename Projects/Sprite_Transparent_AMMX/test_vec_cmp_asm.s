        machine 68080
        xdef    _do_pcmpeqw_vec
        section code,code

; void do_pcmpeqw_vec(uint16_t *mask, const uint16_t *src, const uint16_t *key_vec)
_do_pcmpeqw_vec:
        move.l  4(sp),a0        ; mask
        move.l  8(sp),a1        ; src
        move.l  12(sp),a2       ; key_vec
        load    (a1),e0
        load    (a2),e1
        pcmpeqw e1,e0,e2
        store   e2,(a0)
        rts
