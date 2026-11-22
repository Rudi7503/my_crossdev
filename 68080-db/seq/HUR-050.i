; seq/HUR-050.i - Hurrican KeyMulA original (4 rows x 20 px, d6=5, d7=0, per-row MCONST + TailMask + old MSCALE)
; Uses: d0..d4, a0..a4 (a4 = const table). MUST NOT touch: d5, d6, d7, a5, a6, a7
        ifnd    HUR050_MAC
HUR050_MAC equ 1
HUR050_BODY macro
        moveq   #5,d2                   ; d6 = 5 quads (20 px)
        moveq   #0,d3                   ; d7 = 0 tail
        moveq   #4,d0                   ; 4 rows
.row\@:
        load    (a4),e10
        load    8(a4),e11
        load    16(a4),e12
        load    24(a4),e13
        load    32(a4),e14
        load    40(a4),e8
        load    48(a4),e9
        load    56(a4),e0
        load    64(a4,d3.l*8),e1
        move.l  a0,a2
        move.l  a1,a3
        move.l  d2,d4
        beq.s   .tail\@
.q\@:   load    (a2)+,e2
        pcmpeqw e0,e2,e6
        pand    e10,e2,e3
        pand    e13,e2,e4
        pand    e14,e2,e5
        pmul88  e11,e3,e3
        pand    e12,e3,e3
        pmul88  e9,e3,e3
        pmul88  e9,e4,e4
        pmul88  e9,e5,e5
        pmull   e8,e3,e3
        pand    e10,e3,e3
        pand    e13,e4,e4
        pand    e14,e5,e5
        por     e4,e3,e3
        por     e5,e3,e2
        storeilm e2,e6,(a3)+
        subq.l  #1,d4
        bne.s   .q\@
.tail\@:
        tst.l   d3
        beq.s   .next\@
        load    (a2),e2
        pcmpeqw e0,e2,e6
        por     e1,e6,e6
        storeilm e2,e6,(a3)
.next\@:
        lea     40(a0),a0
        lea     1280(a1),a1
        subq.l  #1,d0
        bne.s   .row\@
        endm
        endif
        HUR050_BODY
