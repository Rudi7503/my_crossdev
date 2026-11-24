; seq/HUR-101.i - beam.s: Optimized Boxes Segment Loop (8 segments: speculative buffer write, zero stack operations)
; Uses: d0-d4, a0-a3. MUST NOT touch: d5, d6, d7, a5, a6, a7
; a4 = scratch buffer
        ifnd    HUR101_MAC
HUR101_MAC equ 1
HUR101_BODY macro
        ; Setup mock BeamBoxes in scratch (a4)
        lea     (a4),a2                 ; a2 = BeamBoxes output
        ; Initial coordinates:
        ; StartX = 100<<16, StartY = 150<<16
        ; dx = 28<<16, dy = 16<<16 (direction vector * 32)
        move.l  #100<<16,a1             ; a1 = current seg_x (16.16)
        move.l  #150<<16,a3             ; a3 = current seg_y (16.16)
        move.l  #28<<16,d2              ; d2 = 32 * dx
        move.l  #16<<16,d3              ; d3 = 32 * dy
        moveq   #7,d4                   ; 8 segments loop counter

.seg_loop_101\@:
        ; Optimized Boxes: spekulativer Puffer-Store direkt in BeamBoxes (kein Stack!)
        move.l  a1,d0
        swap    d0
        ext.l   d0                      ; x_int
        move.l  d0,(a2)                 ; spekulativ x_int ablegen

        move.l  a3,d1
        swap    d1
        ext.l   d1                      ; y_int
        move.l  d1,4(a2)                ; spekulativ y_int ablegen

        add.l   #16,d0                  ; Segmentmitte x
        add.l   #16,d1                  ; Segmentmitte y

        ; Mock WallHit (preserves d1)
        ; Mock BlockAt (returns 0 for no wall)
        moveq   #0,d0
        btst    #0,d0
        bne.s   .end_101\@

        ; Commit: Segment ist gueltig!
        addq.l  #8,a2                   ; naechste BeamBox
        add.l   d2,a1                   ; seg_x += 32*dx
        add.l   d3,a3                   ; seg_y += 32*dy
        dbra.l  d4,.seg_loop_101\@

.end_101\@:
        move.l  -4(a2),d0               ; return last stored y_int for bit-exact check
        endm
        endif
        HUR101_BODY
