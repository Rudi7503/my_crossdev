CIABPRA = $BFD000
I2CDAT = $DE0080

SCLH MACRO
	bset		#1,d7
	move.w	d7,(a1)
	ENDM
SCLL MACRO
	bclr		#1,d7
	move.w	d7,(a1)
	ENDM
SDAH MACRO
	bset		#0,d7
	move.w	d7,(a1)
	ENDM
SDAL MACRO
	bclr		#0,d7
	move.w	d7,(a1)
	ENDM
SDAtest MACRO
	btst		#0,1(a1)
	ENDM
IDLEREAD MACRO
	tst.b 	(a0)
	ENDM

bsrx macro
	lea	__L_\1_\@,a7
	bra	\1
__L_\1_\@:
endm

rtsx macro
	jmp (a7)
endm

bsrx2 macro
	lea	__L_\1_\@,a6
	bra	\1
__L_\1_\@:
endm

rtsx2 macro
	jmp (a6)
endm

bsrx3 macro
	lea	__L_\1_\@,a5
	bra	\1
__L_\1_\@:
endm

rtsx3 macro
	jmp (a5)
endm

I2CreadNVRAM:
	lea		CIABPRA,a0
	lea		I2CDAT,a1
	SCLH
	IDLEREAD
	SDAH
	IDLEREAD

	SDAL					; I2C start
	IDLEREAD
	SCLL
	IDLEREAD
	move.w	#$d0FF,d0
	bsrx		I2Crwbyte
	move.w	#$08FF,d0	; NVRAM/ALARM1 starts at addr 0x07
	bsrx		I2Crwbyte
	SDAL					; I2C stop
	IDLEREAD
	SCLH
	IDLEREAD
	SDAH
	IDLEREAD

	SDAL					; I2C start
	IDLEREAD
	SCLL
	IDLEREAD
	move.w	#$d1FF,d0
	bsrx		I2Crwbyte
	moveq		#2,d3
I2Creadloop:
	move.w	#$ff00,d0
	bsrx		I2Crwbyte
	dbra		d3,I2Creadloop
	move.w	#$ffFF,d0
	bsrx		I2Crwbyte
	SDAL					; I2C stop
	IDLEREAD
	SCLH
	IDLEREAD
	SDAH
	move.l	d2,d4
	swap		d4
	not.w		d4
	cmp.w		d2,d4
	bne		I2Cfail
I2Csuccess:
	; our data is in d2
I2Cfail:

; d0 byte to write and FF when write (wait for ACK) / 00 when read (ACK)
; a0 address of CIA-register
; a1 address of IO-register
; d1 trashed
; d2 result byte (high bytes 3, 2, 1)
; d7 trashed
I2Crwbyte:
	moveq		#8,d1
I2Crwloop:
	clr.w		d7
	lsl.w		#1,d0
	roxl.w	#1,d7
	move.w	d7,(a1)
	IDLEREAD
	SCLH
	lsl.l		#1,d2
	or.w		(a1),d2
	IDLEREAD
	SCLL
	IDLEREAD
	dbra		d1,I2Crwloop
	roxr.l	#1,d2
	rtsx
