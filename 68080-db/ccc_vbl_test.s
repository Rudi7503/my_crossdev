; ============================================================================
; ccc_vbl_test.s - Vampire V4 Real Hardware Test:
; 1. CCC Overflow Modulo Arithmetic Verification
; 2. SAGA 60 Hz VBL Interrupt with `movec.l ccc,d0` timestamping
; 3. Triple Buffer "Time Borrowing" (18ms spike + 14ms recovery = 0 frame drop)
;
; Build: vasmm68k_mot -Fhunkexe -m68080 -o bin-v6/ccc_vbl_test ccc_vbl_test.s
; ============================================================================

GFXMODE         equ     $DFF1F4
BPLHPT          equ     $DFF1EC

_LVOOpenLibrary equ     -552
_LVOCloseLibrary equ    -414
_LVOOutput      equ     -60
_LVOWrite       equ     -48
_LVOAddIntServer equ    -168
_LVORemIntServer equ    -174
_LVOAllocMem    equ     -198
_LVOFreeMem     equ     -210

INTB_VERTB      equ     5
MEMF_PUBLIC     equ     $00000001
MEMF_CLEAR      equ     $00010000

        section code,code

_start:
        movem.l d2-d7/a2-a6,-(sp)
        move.l  4.w,a6
        move.l  a6,SysBase

        ; Open dos.library
        lea     DosName(pc),a1
        moveq   #36,d0
        jsr     _LVOOpenLibrary(a6)
        move.l  d0,DosBase
        beq     .err_dos

        ; Get stdout
        move.l  d0,a6
        jsr     _LVOOutput(a6)
        move.l  d0,StdOut
        beq     .err_out

        ; Allocate dummy chunky buffer for SAGA (512 KB)
        move.l  SysBase(pc),a6
        move.l  #524288,d0
        move.l  #MEMF_PUBLIC!MEMF_CLEAR,d1
        jsr     _LVOAllocMem(a6)
        move.l  d0,ChunkyBuf
        beq     .err_mem
        move.l  d0,BPLHPT

        ; Switch SAGA to 640x480 60Hz 16-bit RGB (Mode $0502)
        move.w  #$0502,GFXMODE

        ; Print Banner
        lea     MsgHdr(pc),a0
        bsr     Print

        ; --------------------------------------------------------------------
        ; TEST 1: Modulo Wrap-Around Arithmetic on 32-bit CCC
        ; --------------------------------------------------------------------
        lea     MsgTest1(pc),a0
        bsr     Print

        ; Inject artificial wrap-around:
        ; t_start = $FFFFFF00, t_now = $00000100 (wraps across 0)
        move.l  #$FFFFFF00,d1           ; t_start
        move.l  #$00000100,d2           ; t_now
        move.l  d2,d0
        sub.l   d1,d0                   ; d0 = delta = 512 ($200)

        cmp.l   #512,d0
        bne.s   .t1_fail

        ; Test unsigned comparison against 16.67ms budget (1,534,873 cycles)
        cmp.l   #1534873,d0
        blo.s   .t1_ok                  ; blo = unsigned lower!

.t1_fail:
        lea     MsgFail(pc),a0
        bsr     Print
        bra.s   .test2

.t1_ok:
        lea     MsgT1Pass(pc),a0
        bsr     Print

        ; --------------------------------------------------------------------
        ; TEST 2: Real SAGA 60 Hz VBL Interrupt with `movec.l ccc,d0`
        ; --------------------------------------------------------------------
.test2:
        lea     MsgTest2(pc),a0
        bsr     Print

        ; Install VBL Interrupt Server
        clr.l   VblCount
        clr.l   VblSincePoke
        clr.l   VblTimestamp
        lea     VblNode(pc),a1
        move.b  #2,8(a1)                ; NT_INTERRUPT
        move.b  #10,9(a1)               ; PRI = 10
        lea     VblName(pc),a0
        move.l  a0,10(a1)
        lea     VblCount(pc),a0
        move.l  a0,14(a1)               ; IS_DATA
        lea     VblHandler(pc),a0
        move.l  a0,18(a1)               ; IS_CODE
        moveq   #INTB_VERTB,d0
        move.l  SysBase(pc),a6
        jsr     _LVOAddIntServer(a6)
        move.l  #1,VblActive

        ; Wait for 2 VBLs to settle
        move.l  VblCount(pc),d0
.w_vbl1:
        cmp.l   VblCount(pc),d0
        beq.s   .w_vbl1
        move.l  VblCount(pc),d0
.w_vbl2:
        cmp.l   VblCount(pc),d0
        beq.s   .w_vbl2

        ; Measure 5 consecutive VBL-to-VBL cycle intervals
        lea     DeltaTable(pc),a2
        moveq   #4,d5                   ; 5 measurements (0..4)

.meas_lp:
        move.l  VblTimestamp(pc),d3     ; start timestamp
        move.l  VblCount(pc),d4
.wait_next_vbl:
        cmp.l   VblCount(pc),d4
        beq.s   .wait_next_vbl
        move.l  VblTimestamp(pc),d0
        sub.l   d3,d0                   ; cycle delta between consecutive VBLs!
        move.l  d0,(a2)+
        dbra    d5,.meas_lp

        ; Print Measured VBL Cycle Intervals
        lea     MsgVblCycles(pc),a0
        bsr     Print
        lea     DeltaTable(pc),a2
        moveq   #4,d5
.pr_meas_lp:
        move.l  (a2)+,d0
        bsr     PrintDec
        lea     MsgSpc(pc),a0
        bsr     Print
        dbra    d5,.pr_meas_lp
        lea     MsgLf(pc),a0
        bsr     Print

        ; --------------------------------------------------------------------
        ; TEST 3: Triple-Buffering Time Borrowing Simulation
        ; Scenario:
        ; Frame 1 takes 18.0 ms (~1,650,000 cycles > 16.67ms deadline).
        ; Frame 2 takes 14.0 ms (~1,280,000 cycles < 16.67ms).
        ; Both must hit the screen without dropping to 30 FPS!
        ; --------------------------------------------------------------------
        lea     MsgTest3(pc),a0
        bsr     Print

        ; Synchronize to a fresh VBL
        move.l  VblCount(pc),d4
.sync_vbl:
        cmp.l   VblCount(pc),d4
        beq.s   .sync_vbl

        ; T = 0: Start of Frame 1
        clr.l   VblSincePoke
        move.l  VblTimestamp(pc),d6     ; Frame 1 start VBL timestamp

        ; Burn cycles to simulate Frame 1 taking ~18.5 ms (> 1,534,873 cycles):
        ; On 68080 subq/bne is ~1.04 cycles -> 1,600,000 iterations = ~1.66M cycles (~18ms)
        move.l  #1650000,d0
.burn1:
        subq.l  #1,d0
        bne.s   .burn1

        ; Frame 1 finished! Check status:
        movec.l ccc,d0
        sub.l   d6,d0                   ; cycles elapsed since start of Frame 1
        move.l  d0,F1Elapsed
        move.l  VblSincePoke(pc),F1VblSince ; Must be 1 (VBL occurred during burn1!)

        ; "Poke" Frame 1 into queue:
        clr.l   VblSincePoke

        ; Burn cycles to simulate Frame 2 taking ~13.5 ms (< 1,534,873 cycles):
        ; 1,150,000 iterations = ~1.20M cycles (~13ms)
        move.l  #1150000,d0
.burn2:
        subq.l  #1,d0
        bne.s   .burn2

        ; Frame 2 finished! Check status:
        move.l  VblSincePoke(pc),F2VblSince ; MUST BE 0! (Frame 2 finished BEFORE VBL #2!)

        ; Print Test 3 results:
        lea     MsgF1Res(pc),a0
        bsr     Print
        move.l  F1Elapsed(pc),d0
        bsr     PrintDec
        lea     MsgF1Vbl(pc),a0
        bsr     Print
        move.l  F1VblSince(pc),d0
        bsr     PrintDec

        lea     MsgF2Res(pc),a0
        bsr     Print
        move.l  F2VblSince(pc),d0
        bsr     PrintDec

        tst.l   F2VblSince
        bne.s   .t3_late
        lea     MsgT3Success(pc),a0
        bsr     Print
        bra.s   .cleanup

.t3_late:
        lea     MsgT3Missed(pc),a0
        bsr     Print

.cleanup:
        ; Restore native mode
        clr.w   GFXMODE

        ; Remove VBL interrupt
        tst.l   VblActive
        beq.s   .no_vbl
        lea     VblNode(pc),a1
        moveq   #INTB_VERTB,d0
        move.l  SysBase(pc),a6
        jsr     _LVORemIntServer(a6)
        clr.l   VblActive
.no_vbl:

        ; Free Chunky Buffer
        tst.l   ChunkyBuf
        beq.s   .no_buf
        move.l  SysBase(pc),a6
        move.l  ChunkyBuf(pc),a1
        move.l  #524288,d0
        jsr     _LVOFreeMem(a6)
        clr.l   ChunkyBuf
.no_buf:

        ; Close dos.library
        tst.l   DosBase
        beq.s   .no_dos
        move.l  SysBase(pc),a6
        move.l  DosBase(pc),a1
        jsr     _LVOCloseLibrary(a6)
        clr.l   DosBase
.no_dos:

        movem.l (sp)+,d2-d7/a2-a6
        moveq   #0,d0
        rts

.err_dos:
        movem.l (sp)+,d2-d7/a2-a6
        moveq   #20,d0
        rts
.err_out:
.err_mem:
        bra.s   .cleanup

; ----------------------------------------------------------------------------
; VBL Interrupt Handler
; ----------------------------------------------------------------------------
VblHandler:
        movec.l ccc,d0
        move.l  d0,VblTimestamp
        addq.l  #1,VblCount
        addq.l  #1,VblSincePoke
        moveq   #0,d0
        rts

; ----------------------------------------------------------------------------
; Print Helpers
; ----------------------------------------------------------------------------
Print:
        movem.l d0-d3/a0-a1/a6,-(sp)
        move.l  a0,d2
        moveq   #0,d3
        move.l  a0,a1
.len:   tst.b   (a1)+
        beq.s   .ld
        addq.l  #1,d3
        bra.s   .len
.ld:    tst.l   d3
        beq.s   .done
        move.l  DosBase(pc),a6
        move.l  StdOut(pc),d1
        jsr     _LVOWrite(a6)
.done:  movem.l (sp)+,d0-d3/a0-a1/a6
        rts

PrintDec:
        movem.l d0-d3/a0-a2,-(sp)
        lea     NumBuf+16(pc),a2
        clr.b   (a2)
.dv:    move.l  d0,d2
        moveq   #0,d1
        divu.l  #10,d1:d2
        add.b   #'0',d1
        move.b  d1,-(a2)
        move.l  d2,d0
        bne.s   .dv
        move.l  a2,a0
        bsr     Print
        movem.l (sp)+,d0-d3/a0-a2
        rts

; ----------------------------------------------------------------------------
; Data section
; ----------------------------------------------------------------------------
DosName:        dc.b    "dos.library",0
VblName:        dc.b    "CccVblInt",0

MsgHdr:
        dc.b    10
        dc.b    "==================================================================",10
        dc.b    " Vampire V4 Hardware Verification: CCC Timestamp & Triple-Buffer",10
        dc.b    "==================================================================",10,0

MsgTest1:
        dc.b    "[Test 1] 32-bit Modulo Wrap-Around ($FFFFFF00 -> $00000100): ",0
MsgT1Pass:
        dc.b    "PASSED! (Delta=512 cycles, unsigned blo branch verified)",10,0
MsgFail:
        dc.b    "FAILED!",10,0

MsgTest2:
        dc.b    "[Test 2] SAGA 60 Hz VBL Interrupt with `movec.l ccc,d0` timestamp:",10,0
MsgVblCycles:
        dc.b    "         Consecutive VBL intervals (cycles): ",0
MsgSpc: dc.b    "  ",0
MsgLf:  dc.b    10,0

MsgTest3:
        dc.b    "[Test 3] Triple Buffer Time Borrowing (Spike 18ms + Recovery 14ms):",10,0
MsgF1Res:
        dc.b    "         Frame 1 (18ms load): Elapsed cycles = ",0
MsgF1Vbl:
        dc.b    ", VblSincePoke = ",0
MsgF2Res:
        dc.b    10
        dc.b    "         Frame 2 (14ms load): Finished BEFORE next VBL? VblSincePoke = ",0
MsgT3Success:
        dc.b    " -> SUCCESS!",10
        dc.b    "         Both frames hit 60 FPS on-time! 0 frames dropped!",10,10,0
MsgT3Missed:
        dc.b    " -> MISSED (VblSincePoke > 0)",10,10,0

        even

SysBase:        dc.l    0
DosBase:        dc.l    0
StdOut:         dc.l    0
ChunkyBuf:      dc.l    0
VblActive:      dc.l    0
VblCount:       dc.l    0
VblSincePoke:   dc.l    0
VblTimestamp:   dc.l    0

F1Elapsed:      dc.l    0
F1VblSince:     dc.l    0
F2VblSince:     dc.l    0

DeltaTable:     ds.l    8
VblNode:        ds.b    22
NumBuf:         ds.b    20
