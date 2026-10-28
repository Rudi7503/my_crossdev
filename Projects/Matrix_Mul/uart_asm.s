        section code,code
        xdef    _uart_init
        xdef    _uart_puts
        xdef    _uart_flush
_uart_init:
        move.b  #$c0,$bfd200
        clr.b   $bfd000
        move.w  #30,$dff032
        rts
_uart_puts:
        move.l  4(sp),a0
.loop:
        move.b  (a0)+,d0
        beq.s   .done
        bsr.s   uart_putc
        bra.s   .loop
.done:
        rts
uart_putc:
        cmp.b   #10,d0
        bne.s   .raw
        move.l  d0,-(sp)
        moveq   #13,d0
        bsr.s   .raw
        move.l  (sp)+,d0
.raw:
        move.w  $dff018,d1
        btst    #13,d1
        beq.s   .raw
        move.w  #1,$dff09c
        and.l   #$ff,d0
        or.l    #$100,d0
        move.w  d0,$dff030
        rts
_uart_flush:
.fl:
        move.w  $dff018,d1
        btst    #12,d1
        beq.s   .fl
        rts
