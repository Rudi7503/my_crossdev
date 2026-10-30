        machine 68080
        xdef    _do_pcmpeqw_imm
        xdef    _do_pcmpeqw_vec
        xdef    _do_pcmpeqw_reg
        section code,code

; void do_pcmpeqw_imm(uint16_t *mask, const uint16_t *src)
_do_pcmpeqw_imm:
        move.l  4(sp),a0        ; mask
        move.l  8(sp),a1        ; src
        load    (a1),e0
        pcmpeqw #$f81f,e0,e2
        store   e2,(a0)
        rts

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

; void do_pcmpeqw_reg(uint16_t *mask, const uint16_t *src, uint32_t key_packed)
_do_pcmpeqw_reg:
        move.l  4(sp),a0        ; mask
        move.l  8(sp),a1        ; src
        move.l  12(sp),d0       ; key_packed
        load    (a1),e0
        pcmpeqw d0,e0,e2
        store   e2,(a0)
        rts
