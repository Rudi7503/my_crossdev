; ======================================================================
; Apollo 68080 Tiled Sprite Blitting & 256-bit Aligned Copy Routines
; Includes:
;   1. blit_rect_m16: 2D rectangular blit using 256-bit move16 bursts
;   2. copy_line_m16: 1D scanline copy using move16 bursts
;   3. copy_line_peeled_m16: Alignment-Peeled move16 (aligns dst to 256-bit)
; ======================================================================

        machine 68080

        xdef    _get_ccc
        xdef    _blit_rect_m16
        xdef    _copy_line_m16
        xdef    _copy_line_peeled_m16

        section .text,code

_get_ccc:
        movec.l CCC,d0
        rts

; ======================================================================
; void blit_rect_m16(const uint8_t *src, int src_stride,
;                    uint8_t *dst, int dst_stride,
;                    int width_bytes, int height_lines)
; ======================================================================
_blit_rect_m16:
        movem.l d2-d7/a2-a4,-(sp)
        move.l  40(sp),a2               ; src
        move.l  44(sp),d4               ; src_stride
        move.l  48(sp),a3               ; dst
        move.l  52(sp),d5               ; dst_stride
        move.l  56(sp),d6               ; width_bytes
        move.l  60(sp),d7               ; height_lines

        move.l  d6,d2
        lsr.l   #5,d2                   ; num_32b
        move.l  d6,d3
        andi.l  #31,d3                  ; rem

.r_line_loop:
        movea.l a2,a0
        movea.l a3,a1

        tst.l   d2
        beq.s   .r_rem
        move.l  d2,d0
.r_loop32:
        move16  (a0)+,(a1)+
        move16  (a0)+,(a1)+
        subq.l  #1,d0
        bne.s   .r_loop32

.r_rem:
        btst    #4,d3
        beq.s   .r_chk8
        move16  (a0)+,(a1)+

.r_chk8:
        btst    #3,d3
        beq.s   .r_chk4
        move.l  (a0)+,(a1)+
        move.l  (a0)+,(a1)+

.r_chk4:
        btst    #2,d3
        beq.s   .r_chk2
        move.l  (a0)+,(a1)+

.r_chk2:
        btst    #1,d3
        beq.s   .r_chk1
        move.w  (a0)+,(a1)+

.r_chk1:
        btst    #0,d3
        beq.s   .r_next
        move.b  (a0)+,(a1)+

.r_next:
        adda.l  d4,a2
        adda.l  d5,a3
        subq.l  #1,d7
        bne.s   .r_line_loop

        movem.l (sp)+,d2-d7/a2-a4
        rts

; ======================================================================
; void copy_line_m16(const uint8_t *src, uint8_t *dst, int count_bytes)
; 1D Scanline Copy using 256-bit move16 bursts
; ======================================================================
_copy_line_m16:
        movem.l d2-d4,-(sp)
        move.l  16(sp),a0               ; src
        move.l  20(sp),a1               ; dst
        move.l  24(sp),d1               ; count_bytes

        move.l  d1,d2
        lsr.l   #5,d2                   ; num_32b
        move.l  d1,d3
        andi.l  #31,d3                  ; rem

        tst.l   d2
        beq.s   .l_rem
.l_loop32:
        move16  (a0)+,(a1)+
        move16  (a0)+,(a1)+
        subq.l  #1,d2
        bne.s   .l_loop32

.l_rem:
        btst    #4,d3
        beq.s   .l_chk8
        move16  (a0)+,(a1)+

.l_chk8:
        btst    #3,d3
        beq.s   .l_chk4
        move.l  (a0)+,(a1)+
        move.l  (a0)+,(a1)+

.l_chk4:
        btst    #2,d3
        beq.s   .l_chk2
        move.l  (a0)+,(a1)+

.l_chk2:
        btst    #1,d3
        beq.s   .l_chk1
        move.w  (a0)+,(a1)+

.l_chk1:
        btst    #0,d3
        beq.s   .l_done
        move.b  (a0)+,(a1)+

.l_done:
        movem.l (sp)+,d2-d4
        rts

; ======================================================================
; void copy_line_peeled_m16(const uint8_t *src, uint8_t *dst, int count_bytes)
; Alignment-Peeled move16: Aligns dst to 32-byte (256-bit) boundary,
; then executes 100% aligned 256-bit cacheline bursts.
; ======================================================================
_copy_line_peeled_m16:
        movem.l d2-d6,-(sp)
        move.l  24(sp),a0               ; src
        move.l  28(sp),a1               ; dst
        move.l  32(sp),d0               ; count_bytes

        cmp.l   #32,d0
        blt     .p_small                ; for small counts, copy directly

        ; Calculate bytes needed to align dst to 32-byte boundary
        move.l  a1,d1
        neg.l   d1
        andi.l  #31,d1                  ; d1 = head_bytes (0..31)
        beq.s   .p_body                 ; already aligned!

        sub.l   d1,d0                   ; remaining bytes

        ; Peel head bytes: byte, word, long, 16-byte
        btst    #0,d1
        beq.s   .p_h2
        move.b  (a0)+,(a1)+

.p_h2:
        btst    #1,d1
        beq.s   .p_h4
        move.w  (a0)+,(a1)+

.p_h4:
        btst    #2,d1
        beq.s   .p_h8
        move.l  (a0)+,(a1)+

.p_h8:
        btst    #3,d1
        beq.s   .p_h16
        move.l  (a0)+,(a1)+
        move.l  (a0)+,(a1)+

.p_h16:
        btst    #4,d1
        beq.s   .p_body
        ; Here dst is 16-byte aligned, copy 16 bytes via move16 or 4x move.l
        move.l  (a0)+,(a1)+
        move.l  (a0)+,(a1)+
        move.l  (a0)+,(a1)+
        move.l  (a0)+,(a1)+

.p_body:
        ; Now a1 (dst) is strictly 32-byte (256-bit) aligned!
        move.l  d0,d2
        lsr.l   #5,d2                   ; num_32b
        move.l  d0,d3
        andi.l  #31,d3                  ; rem

        tst.l   d2
        beq.s   .p_rem
.p_loop32:
        move16  (a0)+,(a1)+             ; perfectly 256-bit aligned write!
        move16  (a0)+,(a1)+
        subq.l  #1,d2
        bne.s   .p_loop32

.p_rem:
        ; Peel tail
        btst    #4,d3
        beq.s   .p_t8
        move16  (a0)+,(a1)+

.p_t8:
        btst    #3,d3
        beq.s   .p_t4
        move.l  (a0)+,(a1)+
        move.l  (a0)+,(a1)+

.p_t4:
        btst    #2,d3
        beq.s   .p_t2
        move.l  (a0)+,(a1)+

.p_t2:
        btst    #1,d3
        beq.s   .p_t1
        move.w  (a0)+,(a1)+

.p_t1:
        btst    #0,d3
        beq.s   .p_done
        move.b  (a0)+,(a1)+

.p_done:
        movem.l (sp)+,d2-d6
        rts

.p_small:
        ; Fallback for < 32 bytes
        tst.l   d0
        beq.s   .p_done
.p_s_loop:
        move.b  (a0)+,(a1)+
        subq.l  #1,d0
        bne.s   .p_s_loop
        bra.s   .p_done
