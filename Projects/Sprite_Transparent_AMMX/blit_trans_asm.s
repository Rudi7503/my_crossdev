        machine 68080
        xdef    _get_ccc
        xdef    _read_iep1
        xdef    _read_iep2
        xdef    _read_iep3
        xdef    _read_dch
        xdef    _read_dcm
        xdef    _read_bpc
        xdef    _read_bpw

        xdef    _blit_trans_8_storeilm
        xdef    _blit_trans_8_bsel
        xdef    _blit_trans_16_storeilm
        xdef    _blit_trans_16_bsel
        xdef    _blit_trans_32_storeilm
        xdef    _blit_trans_32_bsel

        section code,code

; --- Hardware Performance Counters ---
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
        dc.w    $4e7a,$000c     ; movec.l IEP3,d0
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

; ============================================================================
; 16-BIT RGB565 TRANSPARENT BLITTER (STOREILM)
; void blit_trans_16_storeilm(src, src_stride, dst, dst_stride, width, height, key)
; 4(sp)=src, 8(sp)=src_stride, 12(sp)=dst, 16(sp)=dst_stride, 20(sp)=width, 24(sp)=height, 28(sp)=key
; ============================================================================
_blit_trans_16_storeilm:
        movem.l d2-d7/a2-a6,-(sp)
        move.l  48(sp),a0       ; src
        move.l  52(sp),d2       ; src_stride
        move.l  56(sp),a1       ; dst
        move.l  60(sp),d3       ; dst_stride
        move.l  64(sp),d4       ; width (pixels)
        move.l  68(sp),d5       ; height (lines)
        move.l  72(sp),d6       ; key (16-bit in low word)

        ; Setup key vector in e7 (4 identical words)
        andi.l  #$FFFF,d6
        move.l  d6,d7
        swap    d6
        or.l    d7,d6           ; d6 = key | (key<<16)
        move.l  d6,-(sp)
        move.l  d6,-(sp)
        load    (sp)+,e7        ; e7 = 4x key word

        ; Byte width = width * 2
        lsl.l   #1,d4
        sub.l   d4,d2           ; d2 = src_step to next line
        sub.l   d4,d3           ; d3 = dst_step to next line

.line_loop_16_ilm:
        move.l  d4,d0           ; bytes remaining in line

        ; Fast path: 32 bytes (16 pixels) per chunk (256-bit burst)
        lsr.l   #5,d0
        beq.b   .tail_8_ilm

.chunk32_16_ilm:
        load    (a0)+,e0
        load    (a0)+,e1
        load    (a0)+,e2
        load    (a0)+,e3
        pcmpeqw e7,e0,e4
        pcmpeqw e7,e1,e5
        pcmpeqw e7,e2,e6
        pcmpeqw e7,e3,e8
        storeilm e0,e4,(a1)+
        storeilm e1,e5,(a1)+
        storeilm e2,e6,(a1)+
        storeilm e3,e8,(a1)+
        subq.l  #1,d0
        bne.b   .chunk32_16_ilm

.tail_8_ilm:
        move.l  d4,d0
        andi.l  #31,d0          ; remaining bytes
        lsr.l   #3,d0           ; remaining 8-byte quads
        beq.b   .next_line_16_ilm

.quad_16_ilm:
        load    (a0)+,e0
        pcmpeqw e7,e0,e4
        storeilm e0,e4,(a1)+
        subq.l  #1,d0
        bne.b   .quad_16_ilm

.next_line_16_ilm:
        add.l   d2,a0
        add.l   d3,a1
        subq.l  #1,d5
        bne.b   .line_loop_16_ilm

        movem.l (sp)+,d2-d7/a2-a6
        rts

; ============================================================================
; 16-BIT RGB565 TRANSPARENT BLITTER (BSEL + STORE)
; ============================================================================
_blit_trans_16_bsel:
        movem.l d2-d7/a2-a6,-(sp)
        move.l  48(sp),a0       ; src
        move.l  52(sp),d2       ; src_stride
        move.l  56(sp),a1       ; dst
        move.l  60(sp),d3       ; dst_stride
        move.l  64(sp),d4       ; width (pixels)
        move.l  68(sp),d5       ; height (lines)
        move.l  72(sp),d6       ; key (16-bit)

        andi.l  #$FFFF,d6
        move.l  d6,d7
        swap    d6
        or.l    d7,d6
        move.l  d6,-(sp)
        move.l  d6,-(sp)
        load    (sp)+,e7        ; e7 = 4x key word

        lsl.l   #1,d4
        sub.l   d4,d2
        sub.l   d4,d3

.line_loop_16_bsel:
        move.l  d4,d0
        lsr.l   #5,d0
        beq.b   .tail_8_bsel

.chunk32_16_bsel:
        load    (a0)+,e0
        load    (a0)+,e1
        load    (a0)+,e2
        load    (a0)+,e3
        pcmpeqw e7,e0,e4
        pcmpeqw e7,e1,e5
        pcmpeqw e7,e2,e6
        pcmpeqw e7,e3,e8
        bsel    (a1),e4,e0
        bsel    8(a1),e5,e1
        bsel    16(a1),e6,e2
        bsel    24(a1),e8,e3
        store   e0,(a1)+
        store   e1,(a1)+
        store   e2,(a1)+
        store   e3,(a1)+
        subq.l  #1,d0
        bne.b   .chunk32_16_bsel

.tail_8_bsel:
        move.l  d4,d0
        andi.l  #31,d0
        lsr.l   #3,d0
        beq.b   .next_line_16_bsel

.quad_16_bsel:
        load    (a0)+,e0
        pcmpeqw e7,e0,e4
        bsel    (a1),e4,e0
        store   e0,(a1)+
        subq.l  #1,d0
        bne.b   .quad_16_bsel

.next_line_16_bsel:
        add.l   d2,a0
        add.l   d3,a1
        subq.l  #1,d5
        bne.b   .line_loop_16_bsel

        movem.l (sp)+,d2-d7/a2-a6
        rts

; ============================================================================
; 8-BIT TRANSPARENT BLITTER (STOREILM)
; ============================================================================
_blit_trans_8_storeilm:
        movem.l d2-d7/a2-a6,-(sp)
        move.l  48(sp),a0       ; src
        move.l  52(sp),d2       ; src_stride
        move.l  56(sp),a1       ; dst
        move.l  60(sp),d3       ; dst_stride
        move.l  64(sp),d4       ; width (pixels/bytes)
        move.l  68(sp),d5       ; height (lines)
        move.l  72(sp),d6       ; key (8-bit)

        andi.l  #$FF,d6
        move.l  d6,d7
        lsl.l   #8,d7
        or.l    d7,d6           ; d6 = 2x key
        move.l  d6,d7
        swap    d6
        or.l    d7,d6           ; d6 = 4x key
        move.l  d6,-(sp)
        move.l  d6,-(sp)
        load    (sp)+,e7        ; e7 = 8x key byte

        sub.l   d4,d2
        sub.l   d4,d3

.line_loop_8_ilm:
        move.l  d4,d0
        lsr.l   #5,d0
        beq.b   .tail_8_ilm8

.chunk32_8_ilm:
        load    (a0)+,e0
        load    (a0)+,e1
        load    (a0)+,e2
        load    (a0)+,e3
        pcmpeqb e7,e0,e4
        pcmpeqb e7,e1,e5
        pcmpeqb e7,e2,e6
        pcmpeqb e7,e3,e8
        storeilm e0,e4,(a1)+
        storeilm e1,e5,(a1)+
        storeilm e2,e6,(a1)+
        storeilm e3,e8,(a1)+
        subq.l  #1,d0
        bne.b   .chunk32_8_ilm

.tail_8_ilm8:
        move.l  d4,d0
        andi.l  #31,d0
        lsr.l   #3,d0
        beq.b   .next_line_8_ilm

.quad_8_ilm8:
        load    (a0)+,e0
        pcmpeqb e7,e0,e4
        storeilm e0,e4,(a1)+
        subq.l  #1,d0
        bne.b   .quad_8_ilm8

.next_line_8_ilm:
        add.l   d2,a0
        add.l   d3,a1
        subq.l  #1,d5
        bne.b   .line_loop_8_ilm

        movem.l (sp)+,d2-d7/a2-a6
        rts

; ============================================================================
; 8-BIT TRANSPARENT BLITTER (BSEL + STORE)
; ============================================================================
_blit_trans_8_bsel:
        movem.l d2-d7/a2-a6,-(sp)
        move.l  48(sp),a0       ; src
        move.l  52(sp),d2       ; src_stride
        move.l  56(sp),a1       ; dst
        move.l  60(sp),d3       ; dst_stride
        move.l  64(sp),d4       ; width
        move.l  68(sp),d5       ; height
        move.l  72(sp),d6       ; key

        andi.l  #$FF,d6
        move.l  d6,d7
        lsl.l   #8,d7
        or.l    d7,d6
        move.l  d6,d7
        swap    d6
        or.l    d7,d6
        move.l  d6,-(sp)
        move.l  d6,-(sp)
        load    (sp)+,e7

        sub.l   d4,d2
        sub.l   d4,d3

.line_loop_8_bsel:
        move.l  d4,d0
        lsr.l   #5,d0
        beq.b   .tail_8_bsel8

.chunk32_8_bsel:
        load    (a0)+,e0
        load    (a0)+,e1
        load    (a0)+,e2
        load    (a0)+,e3
        pcmpeqb e7,e0,e4
        pcmpeqb e7,e1,e5
        pcmpeqb e7,e2,e6
        pcmpeqb e7,e3,e8
        bsel    (a1),e4,e0
        bsel    8(a1),e5,e1
        bsel    16(a1),e6,e2
        bsel    24(a1),e8,e3
        store   e0,(a1)+
        store   e1,(a1)+
        store   e2,(a1)+
        store   e3,(a1)+
        subq.l  #1,d0
        bne.b   .chunk32_8_bsel

.tail_8_bsel8:
        move.l  d4,d0
        andi.l  #31,d0
        lsr.l   #3,d0
        beq.b   .next_line_8_bsel

.quad_8_bsel8:
        load    (a0)+,e0
        pcmpeqb e7,e0,e4
        bsel    (a1),e4,e0
        store   e0,(a1)+
        subq.l  #1,d0
        bne.b   .quad_8_bsel8

.next_line_8_bsel:
        add.l   d2,a0
        add.l   d3,a1
        subq.l  #1,d5
        bne.b   .line_loop_8_bsel

        movem.l (sp)+,d2-d7/a2-a6
        rts

; ============================================================================
; 32-BIT ARGB TRANSPARENT BLITTER (STOREILM)
; ============================================================================
_blit_trans_32_storeilm:
        movem.l d2-d7/a2-a6,-(sp)
        move.l  48(sp),a0       ; src
        move.l  52(sp),d2       ; src_stride
        move.l  56(sp),a1       ; dst
        move.l  60(sp),d3       ; dst_stride
        move.l  64(sp),d4       ; width (pixels)
        move.l  68(sp),d5       ; height (lines)
        move.l  72(sp),d6       ; key (32-bit)

        move.l  d6,-(sp)
        move.l  d6,-(sp)
        load    (sp)+,e7        ; e7 = 2x 32-bit key

        lsl.l   #2,d4           ; d4 = width in bytes
        sub.l   d4,d2
        sub.l   d4,d3

.line_loop_32_ilm:
        move.l  d4,d0
        lsr.l   #5,d0
        beq.b   .tail_8_ilm32

.chunk32_32_ilm:
        load    (a0)+,e0
        load    (a0)+,e1
        load    (a0)+,e2
        load    (a0)+,e3
        pcmpeqw e7,e0,e4
        pcmpeqw e7,e1,e5
        pcmpeqw e7,e2,e6
        pcmpeqw e7,e3,e8
        storeilm e0,e4,(a1)+
        storeilm e1,e5,(a1)+
        storeilm e2,e6,(a1)+
        storeilm e3,e8,(a1)+
        subq.l  #1,d0
        bne.b   .chunk32_32_ilm

.tail_8_ilm32:
        move.l  d4,d0
        andi.l  #31,d0
        lsr.l   #3,d0
        beq.b   .next_line_32_ilm

.quad_32_ilm32:
        load    (a0)+,e0
        pcmpeqw e7,e0,e4
        storeilm e0,e4,(a1)+
        subq.l  #1,d0
        bne.b   .quad_32_ilm32

.next_line_32_ilm:
        add.l   d2,a0
        add.l   d3,a1
        subq.l  #1,d5
        bne.b   .line_loop_32_ilm

        movem.l (sp)+,d2-d7/a2-a6
        rts

; ============================================================================
; 32-BIT ARGB TRANSPARENT BLITTER (BSEL + STORE)
; ============================================================================
_blit_trans_32_bsel:
        movem.l d2-d7/a2-a6,-(sp)
        move.l  48(sp),a0       ; src
        move.l  52(sp),d2       ; src_stride
        move.l  56(sp),a1       ; dst
        move.l  60(sp),d3       ; dst_stride
        move.l  64(sp),d4       ; width (pixels)
        move.l  68(sp),d5       ; height (lines)
        move.l  72(sp),d6       ; key (32-bit)

        move.l  d6,-(sp)
        move.l  d6,-(sp)
        load    (sp)+,e7

        lsl.l   #2,d4           ; d4 = width in bytes
        sub.l   d4,d2
        sub.l   d4,d3

.line_loop_32_bsel:
        move.l  d4,d0
        lsr.l   #5,d0
        beq.b   .tail_8_bsel32

.chunk32_32_bsel:
        load    (a0)+,e0
        load    (a0)+,e1
        load    (a0)+,e2
        load    (a0)+,e3
        pcmpeqw e7,e0,e4
        pcmpeqw e7,e1,e5
        pcmpeqw e7,e2,e6
        pcmpeqw e7,e3,e8
        bsel    (a1),e4,e0
        bsel    8(a1),e5,e1
        bsel    16(a1),e6,e2
        bsel    24(a1),e8,e3
        store   e0,(a1)+
        store   e1,(a1)+
        store   e2,(a1)+
        store   e3,(a1)+
        subq.l  #1,d0
        bne.b   .chunk32_32_bsel

.tail_8_bsel32:
        move.l  d4,d0
        andi.l  #31,d0
        lsr.l   #3,d0
        beq.b   .next_line_32_bsel

.quad_32_bsel32:
        load    (a0)+,e0
        pcmpeqw e7,e0,e4
        bsel    (a1),e4,e0
        store   e0,(a1)+
        subq.l  #1,d0
        bne.b   .quad_32_bsel32

.next_line_32_bsel:
        add.l   d2,a0
        add.l   d3,a1
        subq.l  #1,d5
        bne.b   .line_loop_32_bsel

        movem.l (sp)+,d2-d7/a2-a6
        rts
