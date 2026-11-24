; seq/HUR-100.i - beam.s: Original Boxes Segment Loop (8 segments: 3x stack push, 1x pop, 2x stack read, addq.l #8,sp)
; Uses: d0-d4, a0-a3. MUST NOT touch: d5, d6, d7, a5, a6, a7
; a4 = scratch buffer
        ifnd    HUR100_MAC
HUR100_MAC equ 1
HUR100_BODY macro
        ; Setup mock BeamBoxes in scratch (a4)
        lea     (a4),a2                 ; a2 = BeamBoxes output
        ; Initial coordinates:
        ; Let StartX = 100<<16, StartY = 150<<16
        ; Let dx = 28<<16, dy = 16<<16 (direction vector * 32)
        move.l  #100<<16,a1             ; a1 = current seg_x (16.16)
        move.l  #150<<16,a3             ; a3 = current seg_y (16.16)
        move.l  #28<<16,d2              ; d2 = 32 * dx
        move.l  #16<<16,d3              ; d3 = 32 * dy
        moveq   #7,d4                   ; 8 segments loop counter

.seg_loop_100\@:
        ; Original Boxes code from beam.s:
        move.l  a1,d0                   ; move.l d4,d0
        swap    d0
        ext.l   d0                      ; FIXINT d0
        move.l  d0,-(sp)                ; x_int fuer BeamBoxes merken
        add.l   #16,d0
        move.l  a3,d1                   ; move.l d5,d1
        swap    d1
        ext.l   d1                      ; FIXINT d1
        move.l  d1,-(sp)                ; y_int fuer BeamBoxes merken
        add.l   #16,d1
        move.l  d0,-(sp)

        ; Mock WallHit (preserves d1)
        ; jsr WallHit
        move.l  (sp)+,d0

        ; Mock BlockAt (returns 0 for no wall)
        ; jsr BlockAt
        moveq   #0,d0                   ; no wall hit (pass)
        btst    #0,d0
        beq.s   .ok_100\@
        addq.l  #8,sp
        bra.s   .end_100\@

.ok_100\@:
        move.l  4(sp),(a2)+             ; store x_int
        move.l  (sp),(a2)+              ; store y_int
        addq.l  #8,sp

        add.l   d2,a1                   ; add.l d2,d4
        add.l   d3,a3                   ; add.l d3,d5
        dbra.l  d4,.seg_loop_100\@

.end_100\@:
        move.l  -4(a2),d0               ; return last stored y_int for bit-exact check
        endm
        endif
        HUR100_BODY
