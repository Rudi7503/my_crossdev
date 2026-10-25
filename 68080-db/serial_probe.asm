; ---------------------------------------------------------------------------
; serial_probe.asm - direct SERDAT/SERPER diagnostic for the V4
; Build: vasmm68k_mot -Fhunkexe -m68080 -o bin/SERIAL-PROBE serial_probe.asm
;
; This bypasses serial.device and drives the Amiga UART registers directly,
; using the same registers as ApolloCrossDev_Debug.c (SERPER/SERDAT/SERDATR).
; It also writes progress messages to stdout, so screen vs. UART is separable.
; ---------------------------------------------------------------------------
SERDAT          equ     $dff030
SERDATR         equ     $dff018
SERPER          equ     $dff032
INTREQ          equ     $dff09c
CIAB_PRA        equ     $bfd000
CIAB_DDRA       equ     $bfd200

SERDATR_TBE     equ     13
SERDAT_STP8     equ     $0100
SERPER_115200   equ     30              ; SERPER_BAUD(3546895,115200)

_LVOCloseLibrary equ    -414
_LVOOpenLibrary equ    -552
_LVOWrite       equ     -48
_LVOOutput      equ     -60

        section code,code
_start:
        movem.l d0-d7/a0-a6,-(sp)
        move.l  4.w,a6
        move.l  a6,exec_base

        bsr     open_dos
        lea     msg_boot,a0
        bsr     screen_puts

        bsr     uart_init
        lea     msg_init,a0
        bsr     screen_puts

        lea     msg_s1,a0
        bsr     uart_puts
        lea     msg_s2,a0
        bsr     uart_puts
        lea     msg_s3,a0
        bsr     uart_puts
        bsr     uart_flush

        lea     msg_sent,a0
        bsr     screen_puts

        bsr     close_dos
        movem.l (sp)+,d0-d7/a0-a6
        moveq   #0,d0
        rts

; ---------------------------------------------------------------------------
; direct UART
; ---------------------------------------------------------------------------
uart_init:
        move.b  #$c0,CIAB_DDRA
        clr.b   CIAB_PRA
        move.w  #SERPER_115200,SERPER
        rts

; a0 = NUL-terminated string
uart_puts:
        move.b  (a0)+,d0
        beq.s   .done
        bsr     uart_putc
        bra.s   uart_puts
.done:
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
; stdout helpers
; ---------------------------------------------------------------------------
strlen:
        moveq   #0,d0
.loop:
        tst.b   (a0)+
        beq.s   .done
        addq.l  #1,d0
        bra.s   .loop
.done:
        rts

; a0 = NUL-terminated string, writes to stdout if available
screen_puts:
        tst.l   stdout_fh
        beq.s   .done
        move.l  a0,d2
        bsr     strlen
        move.l  d0,d1
        move.l  d2,d0
        bsr     stdout_write
.done:
        rts

; d0 = buffer, d1 = length
stdout_write:
        tst.l   stdout_fh
        beq.s   .done
        movem.l d2-d3,-(sp)
        move.l  d0,d2
        move.l  d1,d3
        move.l  stdout_fh,d1
        move.l  dos_base,a6
        jsr     _LVOWrite(a6)
        movem.l (sp)+,d2-d3
.done:
        rts

open_dos:
        clr.l   stdout_fh
        lea     dos_name,a0
        move.l  a0,d0
        moveq   #0,d1
        move.l  exec_base,a6
        jsr     _LVOOpenLibrary(a6)
        move.l  d0,dos_base
        beq.s   .done
        move.l  d0,a6
        jsr     _LVOOutput(a6)
        move.l  d0,stdout_fh
.done:
        rts

close_dos:
        move.l  dos_base,a1
        beq.s   .done
        move.l  exec_base,a6
        jsr     _LVOCloseLibrary(a6)
.done:
        rts

; ---------------------------------------------------------------------------
        section data,data
dos_name:       dc.b    "dos.library",0
msg_boot:       dc.b    "SERIALPROBE;START",13,10,0
msg_init:       dc.b    "SERIALPROBE;UART-INIT;115200;8N1",13,10,0
msg_s1:         dc.b    "SERIALPROBE;BEGIN",13,10,0
msg_s2:         dc.b    "SERIALPROBE;HELLO-FROM-V4",13,10,0
msg_s3:         dc.b    "SERIALPROBE;END",13,10,0
msg_sent:       dc.b    "SERIALPROBE;SENT",13,10,0

; ---------------------------------------------------------------------------
        section bss,bss
        align   8
exec_base:      ds.l    1
dos_base:       ds.l    1
stdout_fh:      ds.l    1
        even
