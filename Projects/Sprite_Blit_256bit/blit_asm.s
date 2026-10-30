; ======================================================================
; Apollo 68080 2D Sprite Blitting & Rectangle Copy Suite
; 256-bit Aligned Block Transfer Benchmark
; Routines:
;   1. blit_rect_move16: Standard move16 (2x move16 per 256-bit / 32 bytes)
;   2. blit_rect_m68k:   Classical 68080 ASM (8x move.l per 32 bytes)
;   3. blit_rect_ammx:   AMMX SIMD Quad-Burst (load/store e0-e3, 32 bytes)
; Hardware: Apollo 68080 (Vampire V4)
; ======================================================================

        machine 68080

        xdef    _get_ccc
        xdef    _read_iep1
        xdef    _read_iep2
        xdef    _read_iep3
        xdef    _read_dch
        xdef    _read_dcm
        xdef    _read_bpc
        xdef    _read_bpw
        xdef    _blit_rect_move16
        xdef    _blit_rect_m68k
        xdef    _blit_rect_ammx

        section .text,code

; ======================================================================
; uint32_t get_ccc(void)
; ======================================================================
_get_ccc:
        movec.l CCC,d0
        rts

_read_iep1:
        movec.l IEP1,d0
        rts

_read_iep2:
        movec.l IEP2,d0
        rts

_read_iep3:
        dc.w    $4e7a,$000c
        rts

_read_dch:
        movec.l DCH,d0
        rts

_read_dcm:
        movec.l DCM,d0
        rts

_read_bpc:
        movec.l BPC,d0
        rts

_read_bpw:
        movec.l BPW,d0
        rts

; ======================================================================
; void blit_rect_move16(const uint8_t *src, int src_stride,
;                       uint8_t *dst, int dst_stride,
;                       int width_bytes, int height_lines)
; Standard move16 Blitter: 2x move16 per 256-bit cacheline
; ======================================================================
_blit_rect_move16:
        movem.l d2-d7/a2-a4,-(sp)       ; 9 regs = 36 bytes + 4 = 40
        move.l  40(sp),a2               ; a2 = src
        move.l  44(sp),d4               ; d4 = src_stride
        move.l  48(sp),a3               ; a3 = dst
        move.l  52(sp),d5               ; d5 = dst_stride
        move.l  56(sp),d6               ; d6 = width_bytes
        move.l  60(sp),d7               ; d7 = height_lines

        ; Calculate 32-byte chunks and remainder
        move.l  d6,d2
        lsr.l   #5,d2                   ; d2 = num_32b (width >> 5)
        move.l  d6,d3
        andi.l  #31,d3                  ; d3 = rem (width & 31)

.m16_line_loop:
        movea.l a2,a0                   ; a0 = src row
        movea.l a3,a1                   ; a1 = dst row

        ; 256-bit main burst loop
        tst.l   d2
        beq.s   .m16_rem
        move.l  d2,d0
.m16_loop32:
        move16  (a0)+,(a1)+             ; 16 bytes
        move16  (a0)+,(a1)+             ; 16 bytes -> 32 bytes (256-bit)
        subq.l  #1,d0
        bne.s   .m16_loop32

.m16_rem:
        ; Handle remainder < 32 bytes
        btst    #4,d3                   ; 16 bytes?
        beq.s   .m16_chk8
        move16  (a0)+,(a1)+

.m16_chk8:
        btst    #3,d3                   ; 8 bytes?
        beq.s   .m16_chk4
        move.l  (a0)+,(a1)+
        move.l  (a0)+,(a1)+

.m16_chk4:
        btst    #2,d3                   ; 4 bytes?
        beq.s   .m16_chk2
        move.l  (a0)+,(a1)+

.m16_chk2:
        btst    #1,d3                   ; 2 bytes?
        beq.s   .m16_chk1
        move.w  (a0)+,(a1)+

.m16_chk1:
        btst    #0,d3                   ; 1 byte?
        beq.s   .m16_next_line
        move.b  (a0)+,(a1)+

.m16_next_line:
        adda.l  d4,a2                   ; advance src to next line
        adda.l  d5,a3                   ; advance dst to next line
        subq.l  #1,d7
        bne.s   .m16_line_loop

        movem.l (sp)+,d2-d7/a2-a4
        rts

; ======================================================================
; void blit_rect_m68k(const uint8_t *src, int src_stride,
;                     uint8_t *dst, int dst_stride,
;                     int width_bytes, int height_lines)
; Classical 68080 ASM: 8x move.l per 256-bit (32-byte) chunk
; ======================================================================
_blit_rect_m68k:
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

.m68k_line_loop:
        movea.l a2,a0
        movea.l a3,a1

        tst.l   d2
        beq.s   .m68k_rem
        move.l  d2,d0
.m68k_loop32:
        move.l  (a0)+,(a1)+
        move.l  (a0)+,(a1)+
        move.l  (a0)+,(a1)+
        move.l  (a0)+,(a1)+
        move.l  (a0)+,(a1)+
        move.l  (a0)+,(a1)+
        move.l  (a0)+,(a1)+
        move.l  (a0)+,(a1)+
        subq.l  #1,d0
        bne.s   .m68k_loop32

.m68k_rem:
        btst    #4,d3                   ; 16 bytes?
        beq.s   .m68k_chk8
        move.l  (a0)+,(a1)+
        move.l  (a0)+,(a1)+
        move.l  (a0)+,(a1)+
        move.l  (a0)+,(a1)+

.m68k_chk8:
        btst    #3,d3                   ; 8 bytes?
        beq.s   .m68k_chk4
        move.l  (a0)+,(a1)+
        move.l  (a0)+,(a1)+

.m68k_chk4:
        btst    #2,d3                   ; 4 bytes?
        beq.s   .m68k_chk2
        move.l  (a0)+,(a1)+

.m68k_chk2:
        btst    #1,d3                   ; 2 bytes?
        beq.s   .m68k_chk1
        move.w  (a0)+,(a1)+

.m68k_chk1:
        btst    #0,d3                   ; 1 byte?
        beq.s   .m68k_next_line
        move.b  (a0)+,(a1)+

.m68k_next_line:
        adda.l  d4,a2
        adda.l  d5,a3
        subq.l  #1,d7
        bne.s   .m68k_line_loop

        movem.l (sp)+,d2-d7/a2-a4
        rts

; ======================================================================
; void blit_rect_ammx(const uint8_t *src, int src_stride,
;                     uint8_t *dst, int dst_stride,
;                     int width_bytes, int height_lines)
; AMMX SIMD Quad-Burst: load/store e0-e3 (32 bytes = 256 bits)
; ======================================================================
_blit_rect_ammx:
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

.ammx_line_loop:
        movea.l a2,a0
        movea.l a3,a1

        tst.l   d2
        beq.s   .ammx_rem
        move.l  d2,d0
.ammx_loop32:
        load    (a0)+,e0
        load    (a0)+,e1
        load    (a0)+,e2
        load    (a0)+,e3
        store   e0,(a1)+
        store   e1,(a1)+
        store   e2,(a1)+
        store   e3,(a1)+
        subq.l  #1,d0
        bne.s   .ammx_loop32

.ammx_rem:
        btst    #4,d3                   ; 16 bytes?
        beq.s   .ammx_chk8
        load    (a0)+,e0
        load    (a0)+,e1
        store   e0,(a1)+
        store   e1,(a1)+

.ammx_chk8:
        btst    #3,d3                   ; 8 bytes?
        beq.s   .ammx_chk4
        load    (a0)+,e0
        store   e0,(a1)+

.ammx_chk4:
        btst    #2,d3                   ; 4 bytes?
        beq.s   .ammx_chk2
        move.l  (a0)+,(a1)+

.ammx_chk2:
        btst    #1,d3                   ; 2 bytes?
        beq.s   .ammx_chk1
        move.w  (a0)+,(a1)+

.ammx_chk1:
        btst    #0,d3                   ; 1 byte?
        beq.s   .ammx_next_line
        move.b  (a0)+,(a1)+

.ammx_next_line:
        adda.l  d4,a2
        adda.l  d5,a3
        subq.l  #1,d7
        bne.s   .ammx_line_loop

        movem.l (sp)+,d2-d7/a2-a4
        rts
