; ============================================================================
; vbl_probe.s - SAGA GFXMODE & VBL Frequency Diagnostic for Vampire V4
; Tests Amiga INTB_VERTB (VBL interrupt) and WaitTOF() across all SAGA GFXMODEs
;
; Build: vasmm68k_mot -Fhunkexe -m68080 -o bin-v6/vbl_probe vbl_probe.s
; ============================================================================

BPLHPT          equ     $DFF1EC         ; SAGA Video Framebuffer Pointer (32-bit, WRITE-ONLY)
GFXMODE         equ     $DFF1F4         ; SAGA Video Mode Register (16-bit, WRITE-ONLY)
VPOSR           equ     $DFF004         ; Vertical beam position (upper)
VHPOSR          equ     $DFF006         ; Beam position (lower)

; Exec LVOs
_LVOAllocMem        equ -198
_LVOFreeMem         equ -210
_LVOAddIntServer    equ -168
_LVORemIntServer    equ -174
_LVOOpenLibrary     equ -552
_LVOCloseLibrary    equ -414
_LVOOpenDevice      equ -444
_LVOCloseDevice     equ -450
_LVOSetSignal       equ -306
_LVOCreateIORequest equ -654
_LVODeleteIORequest equ -660
_LVOCreateMsgPort   equ -666
_LVODeleteMsgPort   equ -672

; DOS LVOs
_LVOOutput      equ     -60
_LVOWrite       equ     -48
_LVOOpen        equ     -30
_LVOClose       equ     -36

; Gfx LVOs
_LVOWaitTOF     equ     -270

; Timer LVOs
_LVOReadEClock  equ     -60

MEMF_PUBLIC     equ     $00000001
MEMF_CLEAR      equ     $00010000
MODE_NEWFILE    equ     1006

INTB_VERTB      equ     5

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

        ; Get stdout handle
        move.l  d0,a6
        jsr     _LVOOutput(a6)
        move.l  d0,StdOut
        beq     .err_out

        ; Open graphics.library
        move.l  SysBase(pc),a6
        lea     GfxName(pc),a1
        moveq   #36,d0
        jsr     _LVOOpenLibrary(a6)
        move.l  d0,GfxBase
        beq     .err_gfx

        ; Create MsgPort for timer.device
        move.l  SysBase(pc),a6
        jsr     _LVOCreateMsgPort(a6)
        move.l  d0,TimerPort
        beq     .err_timer

        ; Create IORequest for timer.device
        move.l  d0,a0
        moveq   #40,d0                  ; sizeof(struct timerequest)
        jsr     _LVOCreateIORequest(a6)
        move.l  d0,TimerReq
        beq     .err_timer

        ; Open timer.device (UNIT_MICROHZ = 0)
        lea     TimerName(pc),a0
        moveq   #0,d0
        move.l  TimerReq(pc),a1
        moveq   #0,d1
        jsr     _LVOOpenDevice(a6)
        tst.l   d0
        bne     .err_timer
        move.l  TimerReq(pc),a1
        move.l  20(a1),TimerBase        ; io_Device pointer

        ; Read initial EClock frequency
        move.l  TimerBase(pc),a6
        lea     ECVal(pc),a0
        jsr     _LVOReadEClock(a6)
        move.l  d0,EFreq                ; e.g. 709379 Hz

        ; Allocate dummy 1MB chunky buffer for SAGA scanout
        move.l  SysBase(pc),a6
        move.l  #1048576,d0             ; 1 MB
        move.l  #MEMF_PUBLIC!MEMF_CLEAR,d1
        jsr     _LVOAllocMem(a6)
        move.l  d0,ChunkyBuf
        bne.s   .buf_ok
        ; Fallback: 512 KB
        move.l  #524288,d0
        move.l  #MEMF_PUBLIC!MEMF_CLEAR,d1
        jsr     _LVOAllocMem(a6)
        move.l  d0,ChunkyBuf
        beq     .err_mem
        move.l  #524288,ChunkySize
        bra.s   .buf_set
.buf_ok:
        move.l  #1048576,ChunkySize
.buf_set:

        ; Open log file in RAM:
        move.l  DosBase(pc),a6
        lea     LogName(pc),a0
        move.l  a0,d1
        move.l  #MODE_NEWFILE,d2
        jsr     _LVOOpen(a6)
        move.l  d0,LogFh

        ; Set Chunky buffer pointer for SAGA (write-only register)
        move.l  ChunkyBuf(pc),BPLHPT

        ; Install VblServer interrupt handler
        lea     VblNode(pc),a1
        move.b  #2,8(a1)                ; NT_INTERRUPT
        move.b  #10,9(a1)               ; PRI = 10
        lea     VblName(pc),a0
        move.l  a0,10(a1)
        lea     VblCounter(pc),a0
        move.l  a0,14(a1)               ; IS_DATA
        lea     VblHandler(pc),a0
        move.l  a0,18(a1)               ; IS_CODE
        moveq   #INTB_VERTB,d0
        move.l  SysBase(pc),a6
        jsr     _LVOAddIntServer(a6)
        move.l  #1,VblActive

        ; Print Banner
        lea     MsgHeader(pc),a0
        bsr     PrintBoth

        ; Print EClock Frequency
        lea     MsgEFreq(pc),a0
        bsr     PrintBoth
        move.l  EFreq(pc),d0
        bsr     PrintDecBoth
        lea     MsgHz(pc),a0
        bsr     PrintBoth

        lea     MsgTableHdr(pc),a0
        bsr     PrintBoth

        ; --------------------------------------------------------------------
        ; Loop through all modes ($00 .. $15)
        ; --------------------------------------------------------------------
        lea     ModeTable(pc),a4
        moveq   #21,d5                  ; 22 modes total (0..21)

.mode_loop:
        ; Check for Ctrl-C abort
        move.l  SysBase(pc),a6
        moveq   #0,d0
        moveq   #0,d1
        jsr     _LVOSetSignal(a6)
        btst    #12,d0                  ; SIGBREAKB_CTRL_C
        bne     .abort_requested

        move.w  (a4)+,d6                ; Mode ID ($00..$15)
        move.w  (a4)+,d7                ; Nominal Hz (50, 60, 24)
        move.l  (a4)+,a3                ; Mode description string pointer

        ; Switch SAGA GFXMODE:
        ; Mode 0 = 0 (native)
        ; Mode > 0 = (Mode << 8) | $02 (16-bit RGB 565)
        tst.w   d6
        beq.s   .set_native
        move.w  d6,d0
        lsl.w   #8,d0
        or.w    #$02,d0                 ; 16-bit chunky
        move.w  d0,GFXMODE
        bra.s   .mode_set
.set_native:
        clr.w   GFXMODE

.mode_set:
        ; Settling delay: wait ~200ms by E-Clock
        bsr     Delay200ms

        ; Reset measurements
        clr.l   VblCounter
        clr.w   MaxLineSeen

        ; Measure 2.0 seconds by EClock:
        ; Target ticks = EFreq * 2
        move.l  EFreq(pc),d3
        add.l   d3,d3                   ; 2.0 seconds of ticks

        ; Read Start E-Clock
        move.l  TimerBase(pc),a6
        lea     ECVal(pc),a0
        jsr     _LVOReadEClock(a6)
        move.l  ECVal+4(pc),d4          ; Start E-Clock tick
        clr.l   VblCounter              ; Reset VblCounter right at start

.measure_loop:
        ; Sample vertical beam line (VPOSR / VHPOSR)
        move.l  VPOSR,d0
        lsr.l   #8,d0
        and.w   #$01FF,d0               ; 9-bit vertical line (0..511)
        cmp.w   MaxLineSeen(pc),d0
        ble.s   .no_new_max
        move.w  d0,MaxLineSeen
.no_new_max:

        ; Check elapsed E-Clock ticks
        move.l  TimerBase(pc),a6
        lea     ECVal(pc),a0
        jsr     _LVOReadEClock(a6)
        move.l  ECVal+4(pc),d0
        sub.l   d4,d0                   ; elapsed ticks
        cmp.l   d3,d0
        blo.s   .measure_loop           ; Unsigned comparison

        ; End of 2.0s measurement window
        move.l  d0,ElapsedTicks         ; actual elapsed ticks (~2*EFreq)
        move.l  VblCounter(pc),MeasuredVbls ; total VBLs counted by interrupt

        ; Now measure 20x WaitTOF() to see if WaitTOF matches VblServer
        move.l  TimerBase(pc),a6
        lea     ECVal(pc),a0
        jsr     _LVOReadEClock(a6)
        move.l  ECVal+4(pc),d4          ; Start WaitTOF ticks

        move.l  GfxBase(pc),a6
        moveq   #19,d2                  ; 20 frames
.tof_lp:
        jsr     _LVOWaitTOF(a6)
        dbra    d2,.tof_lp

        move.l  TimerBase(pc),a6
        lea     ECVal(pc),a0
        jsr     _LVOReadEClock(a6)
        move.l  ECVal+4(pc),d0
        sub.l   d4,d0                   ; Ticks for 20 WaitTOF
        move.l  d0,WaitTofTicks

        ; Format and print row for this mode:
        ; 1. Mode Hex
        lea     BufOut(pc),a0
        move.b  #'$',(a0)+
        move.w  d6,d0
        bsr     HexByte
        move.b  #' ',(a0)+
        move.b  #'|',(a0)+
        move.b  #' ',(a0)+

        ; 2. Name & Nominal Hz
        move.l  a3,a1
.cp_name:
        move.b  (a1)+,(a0)+
        bne.s   .cp_name
        subq.l  #1,a0                   ; strip null
        move.b  #' ',(a0)+
        move.b  #'|',(a0)+
        move.b  #' ',(a0)+
        clr.b   (a0)
        lea     BufOut(pc),a0
        bsr     PrintBoth

        ; 3. VBLs in 2.0s
        move.l  MeasuredVbls(pc),d0
        bsr     PrintDecBoth
        lea     MsgSep(pc),a0
        bsr     PrintBoth

        ; 4. Measured VBL FPS: (MeasuredVbls * 100 * EFreq) / ElapsedTicks -> Integer.2dec
        move.l  MeasuredVbls(pc),d0
        mulu.l  #100,d0                 ; e.g. 100 * 100 = 10000
        mulu.l  EFreq(pc),d1:d0         ; 64-bit product in d1:d0
        divu.l  ElapsedTicks(pc),d1:d0  ; d0 = FPS * 100 (e.g. 5000 for 50.00 Hz)
        bsr     PrintFixed2Both
        lea     MsgSepHz(pc),a0
        bsr     PrintBoth

        ; 5. Average Frame Time (ms): (ElapsedTicks * 100000) / (MeasuredVbls * EFreq)
        tst.l   MeasuredVbls
        beq.s   .zero_vbl
        move.l  ElapsedTicks(pc),d0
        moveq   #0,d1
        mulu.l  #100000,d1:d0           ; ElapsedTicks * 100000 in d1:d0
        move.l  MeasuredVbls(pc),d2
        mulu.l  EFreq(pc),d2            ; d2 = MeasuredVbls * EFreq
        divu.l  d2,d1:d0                ; d0 = ms * 100 (e.g. 2000 for 20.00 ms)
        bsr     PrintFixed2Both
        bra.s   .pr_ms
.zero_vbl:
        lea     MsgZero(pc),a0
        bsr     PrintBoth
.pr_ms:
        lea     MsgSepMs(pc),a0
        bsr     PrintBoth

        ; 6. Max Line Seen
        moveq   #0,d0
        move.w  MaxLineSeen(pc),d0
        bsr     PrintDecBoth
        lea     MsgSep(pc),a0
        bsr     PrintBoth

        ; 7. WaitTOF FPS: (20 * 100 * EFreq) / WaitTofTicks
        tst.l   WaitTofTicks
        beq.s   .zero_tof
        move.l  #2000,d0
        moveq   #0,d1
        mulu.l  EFreq(pc),d1:d0
        divu.l  WaitTofTicks(pc),d1:d0  ; d0 = WaitTOF FPS * 100
        bsr     PrintFixed2Both
        bra.s   .pr_tof
.zero_tof:
        lea     MsgZero(pc),a0
        bsr     PrintBoth
.pr_tof:
        lea     MsgHzEnd(pc),a0
        bsr     PrintBoth

        dbra    d5,.mode_loop

        ; --------------------------------------------------------------------
        ; Detailed 10-Second Tests for Key Modes (Native vs 640x480)
        ; --------------------------------------------------------------------
        lea     Msg10SecHdr(pc),a0
        bsr     PrintBoth

        ; --- 10s Test for Mode $00 (Native PAL) ---
        clr.w   GFXMODE
        bsr     Delay200ms
        lea     Msg10SecM00(pc),a0
        bsr     PrintBoth
        moveq   #0,d6                   ; Mode 0
        bsr     Run10SecTest

        ; --- 10s Test for Mode $05 (640x480 60Hz) ---
        move.w  #$0502,GFXMODE
        bsr     Delay200ms
        lea     Msg10SecM05(pc),a0
        bsr     PrintBoth
        move.w  #$05,d6                 ; Mode $05
        bsr     Run10SecTest

        ; --- 10s Test for Mode $06 (640x512 50Hz) ---
        move.w  #$0602,GFXMODE
        bsr     Delay200ms
        lea     Msg10SecM06(pc),a0
        bsr     PrintBoth
        move.w  #$06,d6                 ; Mode $06
        bsr     Run10SecTest

.abort_requested:
        ; Restore native mode before exit
        clr.w   GFXMODE

        ; Print Summary footer
        lea     MsgDone(pc),a0
        bsr     PrintBoth

        ; Cleanup
.clean_exit:
        ; Restore GFXMODE to 0 (native Amiga)
        clr.w   GFXMODE

        ; Remove VblServer
        tst.l   VblActive
        beq.s   .no_vbl_rem
        lea     VblNode(pc),a1
        moveq   #INTB_VERTB,d0
        move.l  SysBase(pc),a6
        jsr     _LVORemIntServer(a6)
        clr.l   VblActive
.no_vbl_rem:

        ; Close Log file
        tst.l   LogFh
        beq.s   .no_log
        move.l  DosBase(pc),a6
        move.l  LogFh(pc),d1
        jsr     _LVOClose(a6)
        clr.l   LogFh
.no_log:

        ; Free Chunky buffer
        tst.l   ChunkyBuf
        beq.s   .no_buf
        move.l  SysBase(pc),a6
        move.l  ChunkyBuf(pc),a1
        move.l  ChunkySize(pc),d0
        jsr     _LVOFreeMem(a6)
        clr.l   ChunkyBuf
.no_buf:

        ; Close timer.device
        tst.l   TimerBase
        beq.s   .no_timer
        move.l  SysBase(pc),a6
        move.l  TimerReq(pc),a1
        jsr     _LVOCloseDevice(a6)
        clr.l   TimerBase
.no_timer:

        ; Delete IORequest
        tst.l   TimerReq
        beq.s   .no_treq
        move.l  SysBase(pc),a6
        move.l  TimerReq(pc),a0
        jsr     _LVODeleteIORequest(a6)
        clr.l   TimerReq
.no_treq:

        ; Delete MsgPort
        tst.l   TimerPort
        beq.s   .no_tport
        move.l  SysBase(pc),a6
        move.l  TimerPort(pc),a0
        jsr     _LVODeleteMsgPort(a6)
        clr.l   TimerPort
.no_tport:

        ; Close graphics.library
        tst.l   GfxBase
        beq.s   .no_gfx
        move.l  SysBase(pc),a6
        move.l  GfxBase(pc),a1
        jsr     _LVOCloseLibrary(a6)
        clr.l   GfxBase
.no_gfx:

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
.err_gfx:
.err_timer:
.err_mem:
        bra     .clean_exit

; ----------------------------------------------------------------------------
; Interrupt Server: VblHandler
; ----------------------------------------------------------------------------
VblHandler:
        addq.l  #1,(a1)                 ; VblCounter++
        moveq   #0,d0                   ; continue interrupt chain
        rts

; ----------------------------------------------------------------------------
; Delay ~200ms using EClock
; ----------------------------------------------------------------------------
Delay200ms:
        movem.l d0-d2/a0/a6,-(sp)
        move.l  EFreq(pc),d2
        moveq   #0,d0
        divu.l  #5,d0:d2                ; EFreq / 5 = 200 ms
        move.l  TimerBase(pc),a6
        lea     ECVal(pc),a0
        jsr     _LVOReadEClock(a6)
        move.l  ECVal+4(pc),d1          ; start tick
.d_lp:
        lea     ECVal(pc),a0
        jsr     _LVOReadEClock(a6)
        move.l  ECVal+4(pc),d0
        sub.l   d1,d0
        cmp.l   d2,d0
        blo.s   .d_lp
        movem.l (sp)+,d0-d2/a0/a6
        rts

; ----------------------------------------------------------------------------
; Run10SecTest - runs a 10.0-second exact measurement by E-Clock
; d6.w = Mode ID
; ----------------------------------------------------------------------------
Run10SecTest:
        movem.l d2-d7/a0/a6,-(sp)
        ; Target ticks = 10 * EFreq
        move.l  EFreq(pc),d3
        mulu.l  #10,d3

        clr.l   VblCounter
        move.l  TimerBase(pc),a6
        lea     ECVal(pc),a0
        jsr     _LVOReadEClock(a6)
        move.l  ECVal+4(pc),d4          ; start tick
        clr.l   VblCounter

.t10_lp:
        lea     ECVal(pc),a0
        jsr     _LVOReadEClock(a6)
        move.l  ECVal+4(pc),d0
        sub.l   d4,d0
        cmp.l   d3,d0
        blo.s   .t10_lp

        ; Done 10s
        move.l  d0,d5                   ; actual elapsed ticks
        move.l  VblCounter(pc),d6       ; counted VBLs

        ; Print: "<d6> VBLs in "
        move.l  d6,d0
        bsr     PrintDecBoth
        lea     Msg10In(pc),a0
        bsr     PrintBoth

        ; Print seconds: (d5 * 100) / EFreq -> Integer.2dec
        move.l  d5,d0
        mulu.l  #100,d0
        moveq   #0,d1
        divu.l  EFreq(pc),d1:d0
        bsr     PrintFixed2Both
        lea     Msg10SecArrow(pc),a0
        bsr     PrintBoth

        ; Print FPS: (d6 * 1000 * EFreq) / d5 -> Integer.3dec
        move.l  d6,d0
        mulu.l  #1000,d0
        mulu.l  EFreq(pc),d1:d0
        divu.l  d5,d1:d0                ; d0 = FPS * 1000
        ; Print fixed with 3 decimals:
        moveq   #0,d1
        divu.l  #1000,d1:d0             ; d0 = integer, d1 = fraction (0..999)
        move.l  d1,-(sp)
        bsr     PrintDecBoth
        lea     DotStr(pc),a0
        bsr     PrintBoth
        move.l  (sp)+,d2                ; 3 digits fraction (0..999)
        ; Print 3 digits:
        move.l  d2,d0
        moveq   #0,d1
        divu.l  #100,d1:d0              ; d0 = hundreds, d1 = tens+ones
        add.b   #'0',d0
        move.b  d0,FracBuf
        move.l  d1,d0
        moveq   #0,d1
        divu.l  #10,d1:d0               ; d0 = tens, d1 = ones
        add.b   #'0',d0
        move.b  d0,FracBuf+1
        add.b   #'0',d1
        move.b  d1,FracBuf+2
        clr.b   FracBuf+3
        lea     FracBuf(pc),a0
        bsr     PrintBoth
        lea     MsgHzLf(pc),a0
        bsr     PrintBoth

        movem.l (sp)+,d2-d7/a0/a6
        rts

; ----------------------------------------------------------------------------
; Helpers: PrintBoth (stdout + Log file)
; a0 = NUL-terminated string
; ----------------------------------------------------------------------------
PrintBoth:
        movem.l d0-d3/a0-a1/a6,-(sp)
        move.l  a0,d2                   ; buffer
        ; calculate string length
        moveq   #0,d3
        move.l  a0,a1
.len:   tst.b   (a1)+
        beq.s   .len_done
        addq.l  #1,d3
        bra.s   .len
.len_done:
        tst.l   d3
        beq.s   .done

        ; Write to stdout
        move.l  DosBase(pc),a6
        move.l  StdOut(pc),d1
        jsr     _LVOWrite(a6)

        ; Write to log file if open
        tst.l   LogFh
        beq.s   .done
        move.l  DosBase(pc),a6
        move.l  LogFh(pc),d1
        move.l  d2,d0
        move.l  d0,d2                   ; buffer
        jsr     _LVOWrite(a6)

.done:
        movem.l (sp)+,d0-d3/a0-a1/a6
        rts

; ----------------------------------------------------------------------------
; PrintDecBoth - prints unsigned decimal in d0
; ----------------------------------------------------------------------------
PrintDecBoth:
        movem.l d0-d3/a0-a2,-(sp)
        lea     NumBuf+16(pc),a2
        clr.b   (a2)
.dv:    move.l  d0,d2
        moveq   #0,d1
        divu.l  #10,d1:d2               ; d2 = q, d1 = r
        add.b   #'0',d1
        move.b  d1,-(a2)
        move.l  d2,d0
        bne.s   .dv
        move.l  a2,a0
        bsr     PrintBoth
        movem.l (sp)+,d0-d3/a0-a2
        rts

; ----------------------------------------------------------------------------
; PrintFixed2Both - prints fixed point d0 (val * 100) as integer.frac (e.g. 59.94)
; ----------------------------------------------------------------------------
PrintFixed2Both:
        movem.l d0-d4/a0-a2,-(sp)
        move.l  d0,d2
        moveq   #0,d1
        divu.l  #100,d1:d2              ; d2 = integer, d1 = fraction (0..99)
        move.l  d2,d0
        bsr     PrintDecBoth

        lea     DotStr(pc),a0
        bsr     PrintBoth

        ; Print 2 digits for fraction:
        lea     FracBuf(pc),a0
        move.l  d1,d0
        moveq   #0,d1
        divu.l  #10,d1:d0
        add.b   #'0',d0
        move.b  d0,(a0)
        add.b   #'0',d1
        move.b  d1,1(a0)
        clr.b   2(a0)
        bsr     PrintBoth

        movem.l (sp)+,d0-d4/a0-a2
        rts

; a0 = buffer, d0.w = byte to convert to 2 hex digits
HexByte:
        move.w  d0,d1
        lsr.w   #4,d1
        and.w   #$F,d1
        move.b  HexTab(pc,d1.w),(a0)+
        and.w   #$F,d0
        move.b  HexTab(pc,d0.w),(a0)+
        rts

HexTab: dc.b    "0123456789ABCDEF"
DotStr: dc.b    ".",0
FracBuf: ds.b   8
        even

; ----------------------------------------------------------------------------
; Mode Table: Mode ID (.w), Nominal Hz (.w), Description pointer (.l)
; ----------------------------------------------------------------------------
ModeTable:
        dc.w    $00, 50
        dc.l    .m00
        dc.w    $01, 60
        dc.l    .m01
        dc.w    $02, 60
        dc.l    .m02
        dc.w    $03, 60
        dc.l    .m03
        dc.w    $04, 60
        dc.l    .m04
        dc.w    $05, 60
        dc.l    .m05
        dc.w    $06, 50
        dc.l    .m06
        dc.w    $07, 60
        dc.l    .m07
        dc.w    $08, 60
        dc.l    .m08
        dc.w    $09, 60
        dc.l    .m09
        dc.w    $0A, 50
        dc.l    .m0A
        dc.w    $0B, 50
        dc.l    .m0B
        dc.w    $0C, 60
        dc.l    .m0C
        dc.w    $0D, 60
        dc.l    .m0D
        dc.w    $0E, 50
        dc.l    .m0E
        dc.w    $0F, 60
        dc.l    .m0F
        dc.w    $10, 60
        dc.l    .m10
        dc.w    $11, 24
        dc.l    .m11
        dc.w    $12, 50
        dc.l    .m12
        dc.w    $13, 60
        dc.l    .m13
        dc.w    $14, 50
        dc.l    .m14
        dc.w    $15, 50
        dc.l    .m15

.m00:   dc.b    "Native (Workbench)     (50Hz)",0
.m01:   dc.b    "320x200 28MHz x2x2     (60Hz)",0
.m02:   dc.b    "320x240 28MHz x2x2     (60Hz)",0
.m03:   dc.b    "320x256 28MHz x2x2     (60Hz)",0
.m04:   dc.b    "640x400 28MHz          (60Hz)",0
.m05:   dc.b    "640x480 28MHz          (60Hz)",0
.m06:   dc.b    "640x512 28MHz          (50Hz)",0
.m07:   dc.b    "960x540 42MHz          (60Hz)",0
.m08:   dc.b    "480x270 42MHz x2x2     (60Hz)",0
.m09:   dc.b    "304x224 28MHz x2x2     (60Hz)",0
.m0A:   dc.b    "1280x720 85MHz         (50Hz)",0
.m0B:   dc.b    "640x360 85MHz x2x2     (50Hz)",0
.m0C:   dc.b    "800x600 42MHz          (60Hz)",0
.m0D:   dc.b    "1024x768 85MHz         (60Hz)",0
.m0E:   dc.b    "720x576 28MHz          (50Hz)",0
.m0F:   dc.b    "848x480 42MHz          (60Hz)",0
.m10:   dc.b    "640x200 28MHz          (60Hz)",0
.m11:   dc.b    "1920x1080 85MHz        (24Hz)",0
.m12:   dc.b    "1280x1024 85MHz        (50Hz)",0
.m13:   dc.b    "1280x800 84MHz         (60Hz)",0
.m14:   dc.b    "1440x900 85MHz         (50Hz)",0
.m15:   dc.b    "720x405 28MHz          (50Hz)",0
        even

; ----------------------------------------------------------------------------
; Data section
; ----------------------------------------------------------------------------
DosName:        dc.b    "dos.library",0
GfxName:        dc.b    "graphics.library",0
TimerName:      dc.b    "timer.device",0
LogName:        dc.b    "RAM:vbl_probe.log",0
VblName:        dc.b    "VblProbeInt",0

MsgHeader:
        dc.b    10
        dc.b    "==================================================================",10
        dc.b    " SAGA GFXMODE & VBL Timing Probe - Vampire V4 (Apollo Core)",10
        dc.b    " Measuring INTB_VERTB (VblServer) and WaitTOF() across SAGA modes",10
        dc.b    "==================================================================",10,0

MsgEFreq:
        dc.b    "Hardware E-Clock Frequency: ",0
MsgHz:
        dc.b    " Hz",10,10,0

MsgTableHdr:
        dc.b    "Mode | Mode Description       (Nom.) | VBLs/2s | VBL FPS  | Frame (ms)| MaxLine | WaitTOF",10
        dc.b    "-----+-------------------------------+---------+----------+-----------+---------+--------",10,0

MsgSep:         dc.b    "     | ",0
MsgSepHz:       dc.b    " Hz | ",0
MsgSepMs:       dc.b    " ms | ",0
MsgHzEnd:       dc.b    " Hz",10,0
MsgZero:        dc.b    "   0.00",0
MsgDone:
        dc.b    "-----+-------------------------------+---------+----------+-----------+---------+--------",10
        dc.b    "Log written to RAM:vbl_probe.log",10
        dc.b    "SAGA GFXMODE restored to initial state.",10,10,0

Msg10SecHdr:
        dc.b    10
        dc.b    "------------------------------------------------------------------",10
        dc.b    " Extended 10-Second VBL Counts (Exact E-Clock Gated)",10
        dc.b    "------------------------------------------------------------------",10,0

Msg10SecM00:    dc.b    "Mode $00 Native PAL (Nom. 50 Hz):   ",0
Msg10SecM05:    dc.b    "Mode $05 640x480    (Nom. 60 Hz):   ",0
Msg10SecM06:    dc.b    "Mode $06 640x512    (Nom. 50 Hz):   ",0
Msg10In:        dc.b    " VBLs in ",0
Msg10SecArrow:  dc.b    " s -> ",0
MsgHzLf:        dc.b    " Hz",10,0

        even

SysBase:        dc.l    0
DosBase:        dc.l    0
GfxBase:        dc.l    0
TimerBase:      dc.l    0
TimerPort:      dc.l    0
TimerReq:       dc.l    0
StdOut:         dc.l    0
LogFh:          dc.l    0
ChunkyBuf:      dc.l    0
ChunkySize:     dc.l    0
EFreq:          dc.l    0
VblActive:      dc.l    0
VblCounter:     dc.l    0
ElapsedTicks:   dc.l    0
MeasuredVbls:   dc.l    0
WaitTofTicks:   dc.l    0
MaxLineSeen:    dc.w    0

ECVal:          dc.l    0,0
VblNode:        ds.b    22
NumBuf:         ds.b    20
BufOut:         ds.b    128
