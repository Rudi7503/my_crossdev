; verify_blt_parity.asm - Bit-exact parity test on physical Vampire V4
; Compares optimized / Memory-VEA / VPERM kernels against reference implementations.

        incdir  "/home/papa/Vampire_V4/ApolloCrossDev/68080-db"

_LVOOpenLibrary   equ -552
_LVOCloseLibrary  equ -414
_LVOOutput        equ -60
_LVOWrite         equ -48

        section code,code
start:
        movem.l d2-d7/a2-a6,-(sp)
        move.l  4.w,a6
        lea     dosname(pc),a1
        moveq   #0,d0
        jsr     _LVOOpenLibrary(a6)
        move.l  d0,dosbase
        beq     .exit
        move.l  d0,a6
        jsr     _LVOOutput(a6)
        move.l  d0,stdout

        bsr     test_blt045_copy
        bsr     test_blt047_add32_vea
        bsr     test_blt050_add16_vea
        bsr     test_blt048_pmula16_vea
        bsr     test_blt049_alu_ammx
        bsr     test_blt032_key32_exact
        bsr     test_blt043_add24_vperm
        bsr     test_blt051_keyadd_vea
        bsr     test_blt053_keytint_vea

        lea     msg_done(pc),a0
        bsr     print_str

        move.l  4.w,a6
        move.l  dosbase(pc),a1
        jsr     _LVOCloseLibrary(a6)
.exit:
        movem.l (sp)+,d2-d7/a2-a6
        moveq   #0,d0
        rts

; ---------------------------------------------------------------------------
; 1. BLT-045 (2x Copy) vs BLT-029 (4x Copy) - 64 bytes
; ---------------------------------------------------------------------------
test_blt045_copy:
        bsr     init_bufs
        lea     src_buf(pc),a0
        lea     ref_buf(pc),a1
        moveq   #1,d7
.r1:    load    (a0)+,e2
        load    (a0)+,e3
        load    (a0)+,e4
        load    (a0)+,e5
        store   e2,(a1)+
        store   e3,(a1)+
        store   e4,(a1)+
        store   e5,(a1)+
        dbra    d7,.r1

        lea     src_buf(pc),a0
        lea     opt_buf(pc),a1
        moveq   #3,d7
.o1:    load    (a0)+,e2
        load    (a0)+,e3
        store   e2,(a1)+
        store   e3,(a1)+
        dbra    d7,.o1

        moveq   #64,d0
        lea     name_045(pc),a0
        bra     compare_64

; ---------------------------------------------------------------------------
; 2. BLT-047 (32-bit Add VEA) vs BLT-034 (explicit load) - 64 bytes
; ---------------------------------------------------------------------------
test_blt047_add32_vea:
        bsr     init_bufs
        lea     src_buf(pc),a0
        lea     ref_buf(pc),a1
        moveq   #3,d7
.r2:    load    (a0)+,e2
        load    (a0)+,e10
        load    (a1),e3
        load    8(a1),e11
        paddusb e2,e3,e3
        paddusb e10,e11,e11
        store   e3,(a1)+
        store   e11,(a1)+
        dbra    d7,.r2

        lea     src_buf(pc),a0
        lea     opt_buf(pc),a1
        moveq   #3,d7
.o2:    load    (a1),e3
        load    8(a1),e11
        paddusb (a0)+,e3,e3
        paddusb (a0)+,e11,e11
        store   e3,(a1)+
        store   e11,(a1)+
        dbra    d7,.o2

        moveq   #64,d0
        lea     name_047(pc),a0
        bra     compare_64

; ---------------------------------------------------------------------------
; 3. BLT-050 (16-bit Add VEA) vs BLT-022 (blit.s explicit load) - 64 bytes
; ---------------------------------------------------------------------------
test_blt050_add16_vea:
        bsr     init_bufs
        lea     src_buf(pc),a0
        lea     ref_buf(pc),a1
        moveq   #7,d7
.r3:    load    (a0)+,e2
        load    (a1),e3
        unpack1632 e2,e4:e5
        unpack1632 e3,e6:e7
        paddusb e4,e6,e6
        paddusb e5,e7,e7
        pack3216 e6,e7,e3
        store   e3,(a1)+
        dbra    d7,.r3

        lea     src_buf(pc),a0
        lea     opt_buf(pc),a1
        moveq   #7,d7
.o3:    unpack1632 (a0)+,e4:e5
        unpack1632 (a1),e6:e7
        paddusb e4,e6,e6
        paddusb e5,e7,e7
        pack3216 e6,e7,e3
        store   e3,(a1)+
        dbra    d7,.o3

        moveq   #64,d0
        lea     name_050(pc),a0
        bra     compare_64

; ---------------------------------------------------------------------------
; 4. BLT-048 (16-bit PMULA VEA) vs BLT-039 (explicit load) - 32 bytes dst
; ---------------------------------------------------------------------------
test_blt048_pmula16_vea:
        bsr     init_bufs
        lea     alpha_src(pc),a0
        lea     ref_buf(pc),a1
        moveq   #3,d7
.r4:    load    (a0)+,e0
        load    (a0)+,e1
        load    (a1),e2
        unpack1632 e2,e4:e5
        pmula   e0,e4,e4
        pmula   e1,e5,e5
        pack3216 e4,e5,e2
        store   e2,(a1)+
        dbra    d7,.r4

        lea     alpha_src(pc),a0
        lea     opt_buf(pc),a1
        moveq   #3,d7
.o4:    load    (a1),e2
        unpack1632 e2,e4:e5
        pmula   (a0)+,e4,e4
        pmula   (a0)+,e5,e5
        pack3216 e4,e5,e2
        store   e2,(a1)+
        dbra    d7,.o4

        moveq   #32,d0
        lea     name_048(pc),a0
        bra     compare_64

; ---------------------------------------------------------------------------
; 5. BLT-049 (16-bit PMULA VEA + parallel ALU/EA) vs BLT-048 - 32 bytes dst
; ---------------------------------------------------------------------------
test_blt049_alu_ammx:
        bsr     init_bufs
        lea     alpha_src(pc),a0
        lea     ref_buf(pc),a1
        moveq   #3,d7
.r5:    load    (a1),e2
        unpack1632 e2,e4:e5
        pmula   (a0)+,e4,e4
        pmula   (a0)+,e5,e5
        pack3216 e4,e5,e2
        store   e2,(a1)+
        dbra    d7,.r5

        lea     alpha_src(pc),a0
        lea     opt_buf(pc),a1
        lea     src_buf(pc),a2
        moveq   #1,d0
        moveq   #2,d1
        moveq   #3,d2
        moveq   #4,d3
        moveq   #10,d4
        moveq   #3,d7
.o5:    load    (a1),e2
        add.l   d0,d1
        unpack1632 e2,e4:e5
        eor.l   d2,d3
        pmula   (a0)+,e4,e4
        addq.l  #8,a2
        pmula   (a0)+,e5,e5
        subq.l  #1,d4
        pack3216 e4,e5,e2
        store   e2,(a1)+
        dbra    d7,.o5

        moveq   #32,d0
        lea     name_049(pc),a0
        bra     compare_64

; ---------------------------------------------------------------------------
; 6. BLT-032 (32-bit exact Color-Key) vs scalar 32-bit loop - 32 bytes
; ---------------------------------------------------------------------------
test_blt032_key32_exact:
        bsr     init_bufs
        ; Scalar reference: copy 8 longs from key32_src to ref_buf if != $00FF00FF
        lea     key32_src(pc),a0
        lea     ref_buf(pc),a1
        move.l  #$00FF00FF,d1
        moveq   #7,d7
.r6:    move.l  (a0)+,d0
        cmp.l   d1,d0
        beq.s   .sk6
        move.l  d0,(a1)
.sk6:   addq.l  #4,a1
        dbra    d7,.r6

        ; AMMX BLT-032
        lea     key32_key(pc),a2
        load    (a2),e0
        lea     key32_src(pc),a0
        lea     opt_buf(pc),a1
        moveq   #1,d7
.o6:    load    (a0)+,e2
        load    (a0)+,e4
        pcmpeqw e0,e2,e3
        pcmpeqw e0,e4,e5
        vperm   #$23016745,e3,e3,e6
        vperm   #$23016745,e5,e5,e7
        pand    e6,e3,e3
        pand    e7,e5,e5
        storeilm e2,e3,(a1)+
        storeilm e4,e5,(a1)+
        dbra    d7,.o6

        moveq   #32,d0
        lea     name_032(pc),a0
        bra     compare_64

; ---------------------------------------------------------------------------
; 7. BLT-043 (24-bit VPERM 8px Additive) vs scalar byte saturating add - 24B
; ---------------------------------------------------------------------------
test_blt043_add24_vperm:
        bsr     init_bufs
        lea     src_buf(pc),a0
        lea     ref_buf(pc),a1
        moveq   #23,d7
.r7:    moveq   #0,d0
        moveq   #0,d1
        move.b  (a0)+,d0
        move.b  (a1),d1
        add.l   d0,d1
        cmp.l   #255,d1
        ble.s   .ok7
        move.l  #255,d1
.ok7:   move.b  d1,(a1)+
        dbra    d7,.r7

        lea     zero_q(pc),a2
        load    (a2),e15
        lea     src_buf(pc),a0
        lea     opt_buf(pc),a1
        load    (a0)+,e0
        load    (a0)+,e1
        load    (a0)+,e2
        load    (a1),e3
        load    8(a1),e4
        load    16(a1),e5
        vperm   #$80128345,e0,e15,e6
        vperm   #$067809AB,e0,e1,e7
        vperm   #$04560789,e1,e2,e8
        vperm   #$82348567,e2,e15,e9
        vperm   #$80128345,e3,e15,e10
        vperm   #$067809AB,e3,e4,e11
        vperm   #$04560789,e4,e5,e12
        vperm   #$82348567,e5,e15,e13
        paddusb e6,e10,e10
        paddusb e7,e11,e11
        paddusb e8,e12,e12
        paddusb e9,e13,e13
        vperm   #$1235679A,e10,e11,e3
        vperm   #$35679ABD,e11,e12,e4
        vperm   #$679ABDEF,e12,e13,e5
        store   e3,(a1)+
        store   e4,(a1)+
        store   e5,(a1)+

        moveq   #24,d0
        lea     name_043(pc),a0
        bra     compare_64

; ---------------------------------------------------------------------------
; 8. BLT-051 (BlitKeyAdd VEA) vs BLT-024 (blit.s explicit load) - 64 bytes
; ---------------------------------------------------------------------------
test_blt051_keyadd_vea:
        bsr     init_bufs
        lea     key16_key(pc),a2
        load    (a2),e0
        lea     key16_src(pc),a0
        lea     ref_buf(pc),a1
        moveq   #3,d7
.r8:    load    (a0)+,e2
        pcmpeqw e0,e2,e8
        load    (a1),e3
        unpack1632 e2,e4:e5
        unpack1632 e3,e6:e7
        paddusb e4,e6,e6
        paddusb e5,e7,e7
        pack3216 e6,e7,e3
        storeilm e3,e8,(a1)+
        dbra    d7,.r8

        lea     key16_src(pc),a0
        lea     opt_buf(pc),a1
        moveq   #3,d7
.o8:    load    (a0)+,e2
        pcmpeqw e0,e2,e8
        unpack1632 e2,e4:e5
        unpack1632 (a1),e6:e7
        paddusb e4,e6,e6
        paddusb e5,e7,e7
        pack3216 e6,e7,e3
        storeilm e3,e8,(a1)+
        dbra    d7,.o8

        moveq   #32,d0
        lea     name_051(pc),a0
        bra     compare_64

; ---------------------------------------------------------------------------
; 9. BLT-053 (BlitKeyTint Dual-VEA) vs BLT-026 (blit.s explicit load) - 32 bytes
; ---------------------------------------------------------------------------
test_blt053_keytint_vea:
        bsr     init_bufs
        lea     key16_key(pc),a2
        load    (a2),e0
        lea     tint_q(pc),a2
        load    (a2),e9
        lea     key16_src(pc),a0
        lea     ref_buf(pc),a1
        moveq   #3,d7
.r9:    load    (a0)+,e2
        pcmpeqw e0,e2,e8
        load    (a1),e3
        unpack1632 e3,e6:e7
        paddusb e9,e6,e6
        paddusb e9,e7,e7
        pack3216 e6,e7,e3
        storeilm e3,e8,(a1)+
        dbra    d7,.r9

        lea     key16_src(pc),a0
        lea     opt_buf(pc),a1
        moveq   #3,d7
.o9:    pcmpeqw (a0)+,e0,e8
        unpack1632 (a1),e6:e7
        paddusb e9,e6,e6
        paddusb e9,e7,e7
        pack3216 e6,e7,e3
        storeilm e3,e8,(a1)+
        dbra    d7,.o9

        moveq   #32,d0
        lea     name_053(pc),a0
        bra     compare_64

; ---------------------------------------------------------------------------
; Helpers
; ---------------------------------------------------------------------------
init_bufs:
        lea     dst_init(pc),a0
        lea     ref_buf(pc),a1
        lea     opt_buf(pc),a2
        moveq   #15,d7
.ib:    move.l  (a0),(a1)+
        move.l  (a0)+,(a2)+
        dbra    d7,.ib
        rts

compare_64:
        ; a0 = name, d0 = byte count
        movem.l d2-d3/a2-a3,-(sp)
        move.l  a0,a3
        lea     ref_buf(pc),a1
        lea     opt_buf(pc),a2
        subq.l  #1,d0
        moveq   #0,d2
.cmp:   cmpm.b  (a1)+,(a2)+
        beq.s   .ok
        addq.l  #1,d2
.ok:    dbra    d0,.cmp
        move.l  a3,a0
        bsr     print_str
        tst.l   d2
        bne.s   .fail
        lea     msg_pass(pc),a0
        bsr     print_str
        bra.s   .ret
.fail:  lea     msg_fail(pc),a0
        bsr     print_str
.ret:   movem.l (sp)+,d2-d3/a2-a3
        rts

print_str:
        movem.l d2-d3/a6,-(sp)
        move.l  a0,d2
        moveq   #0,d3
.len:   tst.b   (a0)+
        beq.s   .wr
        addq.l  #1,d3
        bra.s   .len
.wr:    move.l  dosbase(pc),a6
        move.l  stdout(pc),d1
        jsr     _LVOWrite(a6)
        movem.l (sp)+,d2-d3/a6
        rts

; --- Data (same section for PC-relative access) ---
dosname: dc.b "dos.library",0
msg_pass: dc.b ": PASS (diff=0)",10,0
msg_fail: dc.b ": FAIL!",10,0
msg_done: dc.b "ALL PARITY CHECKS FINISHED.",10,0
name_045: dc.b "BLT-045 (Copy 2x vs 4x)",0
name_047: dc.b "BLT-047 (Add32 VEA vs Load)",0
name_050: dc.b "BLT-050 (Add16 VEA vs blit.s)",0
name_048: dc.b "BLT-048 (PMULA16 VEA vs Load)",0
name_049: dc.b "BLT-049 (ALU+AMMX vs AMMX)",0
name_032: dc.b "BLT-032 (Exact 32b Key vs Scalar)",0
name_043: dc.b "BLT-043 (24b VPERM Add vs Scalar)",0
name_051: dc.b "BLT-051 (KeyAdd VEA vs blit.s)",0
name_053: dc.b "BLT-053 (KeyTint Dual-VEA vs blit.s)",0
        even

        cnop    0,8
zero_q: dc.l 0,0
key16_key: dc.w $F81F,$F81F,$F81F,$F81F
tint_q: dc.l $00402010,$00402010
key16_src:
        dc.w $F81F,$1234,$F81F,$FFFF
        dc.w $0000,$F81F,$07E0,$F81F
        dc.w $8410,$4208,$F81F,$001F
        dc.w $F800,$F81F,$F81F,$ABCD
key32_key: dc.l $00FF00FF,$00FF00FF
key32_src:
        dc.l $00FF00FF,$11223344,$00FF1234,$567800FF
        dc.l $AABBCCDD,$00FF00FF,$FFFF00FF,$00FF0000
src_buf:
        dc.l $FF804020,$10F0A050,$FFE0C080,$01020304
        dc.l $F81F07E0,$001FF800,$FFFF8410,$42082104
        dc.l $12345678,$9ABCDEF0,$80808080,$7F7F7F7F
        dc.l $FF00FF00,$00FF00FF,$CAFEBABE,$DEADBEEF
dst_init:
        dc.l $209050F0,$F02060B0,$103050F0,$FEFCF8F0
        dc.l $07E0F81F,$F800001F,$00007BEF,$39E718E3
        dc.l $11111111,$22222222,$80808080,$81818181
        dc.l $01010101,$02020202,$12345678,$87654321
alpha_src:
        dc.l $00000000,$FF102030,$80402010,$40081018
        dc.l $20100804,$FF808080,$00FFFFFF,$C0603018
        dc.l $1004080C,$55223344,$AA556677,$FF000000
        dc.l $01010101,$7F404040,$80404040,$FE7F7F7F
ref_buf: ds.b 64
opt_buf: ds.b 64
dosbase: dc.l 0
stdout:  dc.l 0
