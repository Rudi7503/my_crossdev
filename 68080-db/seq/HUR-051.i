; seq/HUR-051.i - Hurrican KeyMulA optimized (4 rows x 20 px, d6=5, d7=0: hoisted 5-load MCONST, new MSCALE, direct stride, dbra, TailMask-skip)
; Uses: d0..d4, a0..a4 (a4 = const table). MUST NOT touch: d5, d6, d7, a5, a6, a7
        ifnd    HUR051_MAC
HUR051_MAC equ 1
HUR051_BODY macro
        moveq   #5,d2                   ; d6 = 5 quads (20 px)
        moveq   #0,d3                   ; d7 = 0 tail
        moveq   #4-1,d0                 ; 4 rows (for dbra)
        load    (a4),e10                ; $F800 (hoisted once!)
        load    24(a4),e13              ; $07E0
        load    32(a4),e14              ; $001F
        load    48(a4),e9               ; MFac
        load    56(a4),e0               ; MKeyQ
        move.l  d2,d1
        lsl.l   #3,d1
        moveq   #40,d4
        sub.l   d1,d4
        move.l  d4,a2                   ; srcStride
        move.l  #1280,d4
        sub.l   d1,d4
        move.l  d4,a3                   ; dstStride
        subq.w  #1,d2                   ; d6-1 for dbra
        moveq   #8,d1                   ; shift count = 8 in register (68080 AMMX lsrq/lslq)
.row\@:
        move.w  d2,d4
        bmi.s   .tail\@
.q\@:   load    (a0)+,e2
        pcmpeqw e0,e2,e6
        pand    e10,e2,e3
        lsrq    d1,e3,e3
        pand    e13,e2,e4
        pand    e14,e2,e5
        pmul88  e9,e3,e3
        pmul88  e9,e4,e4
        pmul88  e9,e5,e5
        lslq    d1,e3,e3
        pand    e10,e3,e3
        pand    e13,e4,e4
        pand    e14,e5,e5
        por     e4,e3,e3
        por     e5,e3,e2
        storeilm e2,e6,(a1)+
        dbra    d4,.q\@
.tail\@:
        tst.l   d3
        beq.s   .next\@
        load    64(a4,d3.l*8),e1
        load    (a0),e2
        pcmpeqw e0,e2,e6
        por     e1,e6,e6
        storeilm e2,e6,(a1)
.next\@:
        add.l   a2,a0
        add.l   a3,a1
        dbra    d0,.row\@
        endm
        endif
        HUR051_BODY
