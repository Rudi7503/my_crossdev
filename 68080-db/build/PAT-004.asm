; ---------------------------------------------------------------------------
; harness.asm - cycle measurement for 68080 best-practice entries
;
; Build (example):
;   vasmm68k_mot -Fhunkexe -m68080 -o bin/FUS-001 build/FUS-001.asm
;
; Environment : ApolloOS / AmigaOS 3.x on Vampire V4 (Apollo 68080)
; Output      : stdout (dos.library/Write) + direct UART (SERDAT/SERPER), 115200 8N1
;
; This file is the unchanged template for every entry.  The only generated
; parts are the ID string and the sequence include marked below.
;
; Register contract inside the measurement loop (sequence MUST NOT touch):
;   d5 = MOVEC start, d6 = MOVEC stop/delta, d7 = outer counter
;   a5 = cycle_buffer write pointer, a6 = exec_base, a7 = stack
; Free for the sequence: d0-d4, a0-a4 (and AMMX E registers)
; The Apollo bit (SR bit 11) is set at startup so E registers are task state.
;
; IMPORTANT: vasm wants the symbolic control-register name CCC.  It rejects
; both "movec.l $809,d5" and an EQU alias.  CCC is the Clock Cycle Counter
; ($809 in the Apollo PRM, page 29).
; ---------------------------------------------------------------------------

NUM_RUNS        equ     100

; AmigaOS/exec constants used below (verified against the NDK 3.9/47 headers)
_LVOForbid      equ     -132
_LVOPermit      equ     -138
_LVOCloseLibrary equ    -414
_LVOOpenLibrary equ    -552
_LVOWrite       equ     -48          ; dos.library
_LVOOutput      equ     -60          ; dos.library

; Direct UART registers, same path as ApolloCrossDev_Debug.c.
; This is NOT serial.device: on the tested V4 the USB-serial adapter is on
; the SERDAT/SERPER UART, while serial.device has no usable output.
SERDAT          equ     $dff030
SERDATR         equ     $dff018
SERPER          equ     $dff032
INTREQ          equ     $dff09c
CIAB_PRA        equ     $bfd000
CIAB_DDRA       equ     $bfd200
SERDATR_TBE     equ     13
SERDAT_STP8     equ     $0100
SERPER_115200   equ     30              ; SERPER_BAUD(3546895,115200)

        section code,code
_start:
        movem.l d0-d7/a0-a6,-(sp)
        move.l  4.w,a6
        move.l  a6,exec_base

        ; Apollo bit: make E0-E23 part of this task's saved state.  The 68080
        ; allows this in user mode (PRM p. 8); it is needed for AMMX sequences
        ; that use E registers across a task switch.
        ori     #$800,sr

        ; Deterministic start values.  They are set once, not before every
        ; run: data-dependent sequences must say so in their entry.
        bsr     init_scratch

        ; ---- Warmup: 3 x sequence, not recorded --------------------------
        moveq   #2,d7
.warmup:
        include "seq/PAT-004.i"
        dbra    d7,.warmup

        ; ---- Forbid ------------------------------------------------------
        move.l  exec_base,a6
        jsr     _LVOForbid(a6)

        ; ---- Baseline: MOVEC overhead -> cycle_buffer[0] -----------------
        movec.l CCC,d5
        movec.l CCC,d6
        sub.l   d5,d6
        move.l  d6,cycle_buffer+0

        ; ---- 10 x 10 runs, inner loop unrolled ---------------------------
        lea     cycle_buffer+4,a5
        moveq   #10-1,d7
.outer:
        ; --- run 1 ---
        movec.l CCC,d5
        include "seq/PAT-004.i"
        movec.l CCC,d6
        sub.l   d5,d6
        move.l  d6,(a5)+
        ; --- run 2 ---
        movec.l CCC,d5
        include "seq/PAT-004.i"
        movec.l CCC,d6
        sub.l   d5,d6
        move.l  d6,(a5)+
        ; --- run 3 ---
        movec.l CCC,d5
        include "seq/PAT-004.i"
        movec.l CCC,d6
        sub.l   d5,d6
        move.l  d6,(a5)+
        ; --- run 4 ---
        movec.l CCC,d5
        include "seq/PAT-004.i"
        movec.l CCC,d6
        sub.l   d5,d6
        move.l  d6,(a5)+
        ; --- run 5 ---
        movec.l CCC,d5
        include "seq/PAT-004.i"
        movec.l CCC,d6
        sub.l   d5,d6
        move.l  d6,(a5)+
        ; --- run 6 ---
        movec.l CCC,d5
        include "seq/PAT-004.i"
        movec.l CCC,d6
        sub.l   d5,d6
        move.l  d6,(a5)+
        ; --- run 7 ---
        movec.l CCC,d5
        include "seq/PAT-004.i"
        movec.l CCC,d6
        sub.l   d5,d6
        move.l  d6,(a5)+
        ; --- run 8 ---
        movec.l CCC,d5
        include "seq/PAT-004.i"
        movec.l CCC,d6
        sub.l   d5,d6
        move.l  d6,(a5)+
        ; --- run 9 ---
        movec.l CCC,d5
        include "seq/PAT-004.i"
        movec.l CCC,d6
        sub.l   d5,d6
        move.l  d6,(a5)+
        ; --- run 10 ---
        movec.l CCC,d5
        include "seq/PAT-004.i"
        movec.l CCC,d6
        sub.l   d5,d6
        move.l  d6,(a5)+

        dbra    d7,.outer

        ; ---- Permit ------------------------------------------------------
        move.l  exec_base,a6
        jsr     _LVOPermit(a6)

        ; ---- Statistics and output (all outside Forbid) ------------------
        bsr     calc_stats
        bsr     open_output
        bsr     print_all
        bsr     close_output

        movem.l (sp)+,d0-d7/a0-a6
        moveq   #0,d0
        rts

; ---------------------------------------------------------------------------
; init_scratch: known register/memory state before the warmup.
; ---------------------------------------------------------------------------
init_scratch:
        moveq   #1,d0
        moveq   #2,d1
        moveq   #3,d2
        moveq   #4,d3
        moveq   #5,d4
        lea     scratch_a,a0
        lea     scratch_b,a1
        lea     scratch_c,a2
        lea     scratch_d,a3
        lea     scratch_e,a4
        rts

; ---------------------------------------------------------------------------
; calc_stats: MIN, MAX, SUM, AVG over cycle_buffer[1..100].
; Runs after Permit and may use d0-d4/a0-a4 freely.
; AVG uses DIVU.W #100.  This is safe while AVG < 65536 cycles; the corpus
; sequences are far below that.  The sum is 32 bit.
; ---------------------------------------------------------------------------
calc_stats:
        lea     cycle_buffer+4,a0
        moveq   #NUM_RUNS-1,d3
        move.l  #$7fffffff,stat_min
        moveq   #0,d0                   ; MAX
        moveq   #0,d1                   ; SUM
.loop:
        move.l  (a0)+,d4
        cmp.l   stat_min,d4
        bcc.s   .no_min
        move.l  d4,stat_min
.no_min:
        cmp.l   d0,d4
        bls.s   .no_max
        move.l  d4,d0
.no_max:
        add.l   d4,d1
        dbra    d3,.loop
        move.l  d0,stat_max
        move.l  d1,stat_sum
        move.l  d1,d0
        move.l  #NUM_RUNS,d1
        divu.w  d1,d0                   ; quotient in low word, remainder high
        and.l   #$ffff,d0
        move.l  d0,stat_avg
        rts

; ---------------------------------------------------------------------------
; open_output: dos.library/Output + direct UART.  Called AFTER Permit().
; No serial.device is opened; the tested V4 debug/serial adapter is driven
; through SERDAT/SERPER exactly like ApolloCrossDev_Debug.c.
; ---------------------------------------------------------------------------
open_output:
        clr.l   stdout_fh
        clr.l   serial_ok

        lea     dos_name,a0
        move.l  a0,d0
        moveq   #0,d1
        move.l  exec_base,a6
        jsr     _LVOOpenLibrary(a6)
        move.l  d0,dos_base
        beq.s   .no_dos
        move.l  d0,a6
        jsr     _LVOOutput(a6)
        move.l  d0,stdout_fh
.no_dos:
        bsr     uart_init
        move.l  #1,serial_ok
        rts

; ---------------------------------------------------------------------------
; close_output
; ---------------------------------------------------------------------------
close_output:
        bsr     uart_flush
        move.l  dos_base,a1
        beq.s   .done
        move.l  exec_base,a6
        jsr     _LVOCloseLibrary(a6)
.done:
        rts

; ---------------------------------------------------------------------------
; emit_line: line_buf/line_len -> stdout and serial
; ---------------------------------------------------------------------------
emit_line:
        lea     line_buf,a0
        move.l  line_len,d0
        clr.b   (a0,d0.l)
        move.l  a0,d0
        move.l  line_len,d1
        movem.l d0-d1,-(sp)
        bsr     stdout_write
        movem.l (sp)+,d0-d1
        bsr     serial_write
        rts

; d0 = buffer, d1 = length
stdout_write:
        tst.l   stdout_fh
        beq.s   .done
        movem.l d2-d3,-(sp)             ; keep caller loop counters
        move.l  d0,d2
        move.l  d1,d3
        move.l  stdout_fh,d1
        move.l  dos_base,a6
        jsr     _LVOWrite(a6)
        movem.l (sp)+,d2-d3
.done:
        rts

; d0 = buffer, d1 = length
serial_write:
        tst.l   serial_ok
        beq.s   .done
        movem.l d0-d2/a0,-(sp)
        move.l  d0,a0
        move.l  d1,d2
.loop:
        tst.l   d2
        beq.s   .end
        move.b  (a0)+,d0
        bsr     uart_putc
        subq.l  #1,d2
        bra.s   .loop
.end:
        movem.l (sp)+,d0-d2/a0
.done:
        rts

; ---------------------------------------------------------------------------
; direct UART, 115200 8N1
; ---------------------------------------------------------------------------
uart_init:
        move.b  #$c0,CIAB_DDRA
        clr.b   CIAB_PRA
        move.w  #SERPER_115200,SERPER
        rts

; d0.b = character, LF is expanded to CR/LF
uart_putc:
        cmp.b   #10,d0
        bne.s   uart_putraw
        movem.l d0,-(sp)
        moveq   #13,d0
        bsr     uart_putraw
        movem.l (sp)+,d0

uart_putraw:
        move.w  SERDATR,d1
        btst    #SERDATR_TBE,d1
        beq.s   uart_putraw
        move.w  #1,INTREQ
        and.l   #$ff,d0
        or.l    #SERDAT_STP8,d0
        move.w  d0,SERDAT
        rts

uart_flush:
        move.w  SERDATR,d1
        btst    #12,d1                  ; TSRE
        beq.s   uart_flush
        rts

; ---------------------------------------------------------------------------
; small line-buffer helpers
; ---------------------------------------------------------------------------
buf_reset:
        clr.l   line_len
        rts

; d0.b = character
buf_putc:
        lea     line_buf,a1
        add.l   line_len,a1
        move.b  d0,(a1)
        addq.l  #1,line_len
        rts

; a0 = NUL-terminated string
buf_puts:
        move.b  (a0)+,d0
        beq.s   .done
        bsr     buf_putc
        bra.s   buf_puts
.done:
        rts

buf_crlf:
        moveq   #13,d0
        bsr     buf_putc
        moveq   #10,d0
        bsr     buf_putc
        rts

; d0.l = unsigned value
buf_u32:
        movem.l d0-d5/a2,-(sp)
        tst.l   d0
        bne.s   .nonzero
        moveq   #48,d0
        bsr     buf_putc
        bra.s   .out
.nonzero:
        lea     pow10_table,a2
        moveq   #0,d4                   ; "already emitted a digit"
.next_power:
        move.l  (a2)+,d3
        tst.l   d3
        beq.s   .out
        moveq   #0,d2                   ; digit
.subtract:
        cmp.l   d3,d0
        blo.s   .digit_ready
        sub.l   d3,d0
        addq.l  #1,d2
        bra.s   .subtract
.digit_ready:
        tst.l   d2
        bne.s   .emit
        tst.l   d4
        bne.s   .emit
        bra.s   .next_power             ; suppress leading zeroes

.emit:
        moveq   #1,d4
        move.l  d0,d5                   ; keep remainder
        move.l  d2,d0
        add.b   #48,d0
        bsr     buf_putc
        move.l  d5,d0
        bra.s   .next_power
.out:
        movem.l (sp)+,d0-d5/a2
        rts

; ---------------------------------------------------------------------------
; print_all: the five line types, identical for screen and serial
; ---------------------------------------------------------------------------
print_all:
        bsr     buf_reset
        lea     str_begin,a0
        bsr     buf_puts
        lea     id_string,a0
        bsr     buf_puts
        bsr     buf_crlf
        bsr     emit_line

        bsr     buf_reset
        lea     str_baseline,a0
        bsr     buf_puts
        move.l  cycle_buffer,d0
        bsr     buf_u32
        bsr     buf_crlf
        bsr     emit_line

        lea     cycle_buffer+4,a3
        moveq   #1,d4
        moveq   #NUM_RUNS-1,d3
.res_loop:
        bsr     buf_reset
        lea     str_res,a0
        bsr     buf_puts
        move.l  d4,d0
        bsr     buf_u32
        lea     str_semi,a0
        bsr     buf_puts
        move.l  (a3)+,d0
        bsr     buf_u32
        bsr     buf_crlf
        bsr     emit_line
        addq.l  #1,d4
        dbra    d3,.res_loop

        bsr     buf_reset
        lea     str_summary,a0
        bsr     buf_puts
        move.l  stat_min,d0
        bsr     buf_u32
        lea     str_semi,a0
        bsr     buf_puts
        move.l  stat_max,d0
        bsr     buf_u32
        lea     str_semi,a0
        bsr     buf_puts
        move.l  stat_avg,d0
        bsr     buf_u32
        bsr     buf_crlf
        bsr     emit_line

        bsr     buf_reset
        lea     str_end,a0
        bsr     buf_puts
        lea     id_string,a0
        bsr     buf_puts
        bsr     buf_crlf
        bsr     emit_line
        rts

; ---------------------------------------------------------------------------
; data
; ---------------------------------------------------------------------------
        section data,data
id_string:      dc.b    "PAT-004",0
dos_name:       dc.b    "dos.library",0
str_begin:      dc.b    "BEGIN;",0
str_baseline:   dc.b    "BASELINE;",0
str_res:        dc.b    "RES;",0
str_semi:       dc.b    ";",0
str_summary:    dc.b    "SUMMARY;",0
str_end:        dc.b    "END;",0

pow10_table:
        dc.l    1000000000,100000000,10000000,1000000,100000
        dc.l    10000,1000,100,10,1,0

; ---------------------------------------------------------------------------
; bss
; ---------------------------------------------------------------------------
        section bss,bss
        align   8
exec_base:      ds.l    1
dos_base:       ds.l    1
stdout_fh:      ds.l    1
serial_ok:      ds.l    1
stat_min:       ds.l    1
stat_max:       ds.l    1
stat_sum:       ds.l    1
stat_avg:       ds.l    1
line_len:       ds.l    1
cycle_buffer:   ds.l    1+NUM_RUNS
line_buf:       ds.b    192
        align   8
scratch_a:      ds.b    4096
scratch_b:      ds.b    4096
scratch_c:      ds.b    4096
scratch_d:      ds.b    4096
scratch_e:      ds.b    4096
        even
