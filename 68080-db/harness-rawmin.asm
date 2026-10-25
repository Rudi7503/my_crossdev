; harness-rawmin.asm - minimal one-sequence RAW counter test
; FUS-001 only; no Disable, no 100-run loop, no file log.
        section code,code
SERDAT          equ     $dff030
SERDATR         equ     $dff018
SERPER          equ     $dff032
INTREQ          equ     $dff09c
CIAB_PRA        equ     $bfd000
CIAB_DDRA       equ     $bfd200
SERDATR_TBE     equ     13
SERDAT_STP8     equ     $0100
SERPER_115200   equ     30
_LVOForbid      equ     -132
_LVOPermit      equ     -138

_start:
        movem.l d0-d7/a0-a6,-(sp)
        move.l  4.w,a6
        move.l  a6,exec_base
        ori     #$800,sr
        bsr     uart_init

        move.l  exec_base,a6
        jsr     _LVOForbid(a6)

        ; deterministic FUS-001 inputs
        moveq   #1,d2
        moveq   #2,d3
        moveq   #3,d4

        lea     str_begin,a0
        bsr     uart_puts

        ; raw start
        movec.l IEP1,d0
        move.l  d0,raw_i1s
        movec.l IEP2,d0
        move.l  d0,raw_i2s
        dc.w    $4e7a,$000c
        move.l  d0,raw_i3s

        ; one FUS-001 sequence, CCC directly around it
        movec.l CCC,d5
        include "seq/FUS-001.i"
        movec.l CCC,d6
        sub.l   d5,d6
        move.l  d6,raw_ccc

        ; raw end
        movec.l IEP1,d0
        move.l  d0,raw_i1e
        movec.l IEP2,d0
        move.l  d0,raw_i2e
        dc.w    $4e7a,$000c
        move.l  d0,raw_i3e

        move.l  exec_base,a6
        jsr     _LVOPermit(a6)

        lea     str_raw,a0
        bsr     uart_puts
        lea     str_iep1,a0
        bsr     uart_puts
        move.l  raw_i1s,d0
        bsr     hex32
        lea     str_semi,a0
        bsr     uart_puts
        move.l  raw_i1e,d0
        bsr     hex32
        lea     str_semi,a0
        bsr     uart_puts
        lea     str_iep2,a0
        bsr     uart_puts
        move.l  raw_i2s,d0
        bsr     hex32
        lea     str_semi,a0
        bsr     uart_puts
        move.l  raw_i2e,d0
        bsr     hex32
        lea     str_semi,a0
        bsr     uart_puts
        lea     str_iep3,a0
        bsr     uart_puts
        move.l  raw_i3s,d0
        bsr     hex32
        lea     str_semi,a0
        bsr     uart_puts
        move.l  raw_i3e,d0
        bsr     hex32
        lea     str_semi,a0
        bsr     uart_puts
        lea     str_ccc,a0
        bsr     uart_puts
        move.l  raw_ccc,d0
        bsr     hex32

        moveq   #10,d0
        bsr     uart_putc
        lea     str_end,a0
        bsr     uart_puts
        bsr     uart_flush

        movem.l (sp)+,d0-d7/a0-a6
        moveq   #0,d0
        rts

; ---------------------------------------------------------------------------
uart_init:
        move.b  #$c0,CIAB_DDRA
        clr.b   CIAB_PRA
        move.w  #SERPER_115200,SERPER
        rts

uart_puts:
        move.b  (a0)+,d0
        beq.s   .done
        bsr     uart_putc
        bra.s   uart_puts
.done:  rts

hex32:
        move.l  d0,d3
        moveq   #7,d4
.loop:
        rol.l   #4,d3
        move.l  d3,d0
        and.l   #$f,d0
        cmp.b   #10,d0
        blt.s   .digit
        add.b   #'A'-10,d0
        bra.s   .out
.digit:
        add.b   #'0',d0
.out:
        bsr     uart_putc
        dbra    d4,.loop
        rts

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
        btst    #12,d1
        beq.s   uart_flush
        rts

; ---------------------------------------------------------------------------
        section data,data
str_begin:      dc.b    "BEGIN;FUS-001-RAW",10,0
str_raw:        dc.b    "RAW;",0
str_iep1:       dc.b    "IEP1;",0
str_iep2:       dc.b    "IEP2;",0
str_iep3:       dc.b    "IEP3;",0
str_ccc:        dc.b    "CCC;",0
str_semi:       dc.b    ";",0
str_end:        dc.b    "END;FUS-001-RAW",10,0

; ---------------------------------------------------------------------------
        section bss,bss
exec_base:      ds.l    1
raw_i1s:        ds.l    1
raw_i1e:        ds.l    1
raw_i2s:        ds.l    1
raw_i2e:        ds.l    1
raw_i3s:        ds.l    1
raw_i3e:        ds.l    1
raw_ccc:        ds.l    1
