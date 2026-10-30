; PMULA golden-test helpers (Apollo 68080)
        section code,code
        xdef _pmula8
        xdef _pmula_blit
        xdef _pmula_blit4
        xdef _pmula_blit4_mem
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

; void pmula_blit4(const uint8_t *a, const uint8_t *b, uint8_t *out, int pixels)
; 4 independent PMULA chains, 8 pixels / 32 bytes per iteration.
_pmula_blit4:
        movem.l d2/a2,-(sp)
        move.l  12(sp),a0
        move.l  16(sp),a1
        move.l  20(sp),a2
        move.l  24(sp),d2
        lsr.l   #3,d2
        beq.s   .done4
.loop4:
        load    (a0)+,e0
        load    (a1)+,e1
        load    (a0)+,e3
        load    (a1)+,e4
        load    (a0)+,e6
        load    (a1)+,e7
        load    (a0)+,e9
        load    (a1)+,e10
        pmula   e0,e1,e2
        pmula   e3,e4,e5
        pmula   e6,e7,e8
        pmula   e9,e10,e11
        store   e2,(a2)+
        store   e5,(a2)+
        store   e8,(a2)+
        store   e11,(a2)+
        subq.l  #1,d2
        bne.s   .loop4
.done4:
        movem.l (sp)+,d2/a2
        rts

; void pmula_blit4_mem(const uint8_t *a, const uint8_t *b, uint8_t *out, int pixels)
; Memory-VEA PMULA: one explicit LOAD (b) per pixel pair instead of two.
_pmula_blit4_mem:
        movem.l d2/a2,-(sp)
        move.l  12(sp),a0
        move.l  16(sp),a1
        move.l  20(sp),a2
        move.l  24(sp),d2
        lsr.l   #3,d2
        beq.s   .done4m
.loop4m:
        load    (a1)+,e1
        load    (a1)+,e4
        load    (a1)+,e7
        load    (a1)+,e10
        pmula   (a0)+,e1,e2
        pmula   (a0)+,e4,e5
        pmula   (a0)+,e7,e8
        pmula   (a0)+,e10,e11
        store   e2,(a2)+
        store   e5,(a2)+
        store   e8,(a2)+
        store   e11,(a2)+
        subq.l  #1,d2
        bne.s   .loop4m
.done4m:
        movem.l (sp)+,d2/a2
        rts
        end
