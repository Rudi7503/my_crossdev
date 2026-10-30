; PMULA golden-test helpers (Apollo 68080)
        section code,code
        xdef _pmula8
        xdef _pmula_blit
        xdef _get_ccc

; uint32_t get_ccc(void)
_get_ccc:
        movec.l ccc,d0
        rts

; void pmula8(const uint8_t *a, const uint8_t *b, uint8_t *d)
_pmula8:
        movem.l a2-a3,-(sp)
        move.l  12(sp),a0
        move.l  16(sp),a1
        move.l  20(sp),a2
        load    (a0),e0
        load    (a1),e1
        pmula   e0,e1,e2
        store   e2,(a2)
        movem.l (sp)+,a2-a3
        rts

; void pmula_blit(const uint8_t *a, const uint8_t *b, uint8_t *out, int pixels)
; a = background * (1-alpha) with alpha in byte0 of each 32-bit pixel
; b = source RGB (alpha byte ignored)
_pmula_blit:
        movem.l d2-d3/a2-a4,-(sp)
        move.l  24(sp),a0
        move.l  28(sp),a1
        move.l  32(sp),a2
        move.l  36(sp),d2
        lsr.l   #1,d2
        beq.s   .done
.loop:
        load    (a0)+,e0
        load    (a1)+,e1
        pmula   e0,e1,e2
        store   e2,(a2)+
        subq.l  #1,d2
        bne.s   .loop
.done:
        movem.l (sp)+,d2-d3/a2-a4
        rts
        end
