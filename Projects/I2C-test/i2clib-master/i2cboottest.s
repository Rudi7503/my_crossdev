ExecBase                 EQU    4  ; Exec.Base()
OpenLibrary              EQU -552  ; D0:libBase = Exec.OpenLibrary(A1:libName,D0:version)
CloseLibrary             EQU -414  ; Exec.CloseLibrary(A1:libBase)
PutStr                   EQU -948  ; DOS.PutStr(D1:str)

START:
opendos:
	move.l	ExecBase,a6
	lea			DosLibrary,a1
	moveq		#0,d0
	jsr			OpenLibrary(a6)
	tst.l		d0
	beq.b			error
	move.l	d0,a6
 
	bsr.w			I2CreadNVRAM
	move.l		d2,d0
	move.l		d2,d1

	lea			DataNumberEnd,a4

	moveq		#1,d6
loop2:
	move.l	d1,d2
	andi.l	#$88888888,d2
	lsr.l		#3,d2

	move.l	d1,d3
	andi.l	#$44444444,d3
	lsr.l		#2,d3

	move.l	d1,d4
	andi.l	#$22222222,d4
	lsr.l		#1,d4

	or.l		d3,d4
	and.l		d2,d4

	mulu.l	#7,d4

	moveq		#3,d5

loop:
	unpk		d1,d3,#$3030
	unpk		d4,d2,#0
	add.w		d3,d2
	move.w	d2,-(a4)
	lsr.l		#8,d1
	lsr.l		#8,d4
	dbra		d5,loop

	move.l	d0,d1
	dbra		d6,loop2

print:
	move.l	#DataNumber,d1
	jsr			PutStr(a6)

closedos:
	move.l	a6,a1
	move.l	ExecBase,a6
	jsr			CloseLibrary(a6)
	moveq		#0,d0
	rts

error:
	moveq		#1,d0
	rts




DosLibrary:					dc.b "dos.library",0
DataNumber:				dc.b "RTC and NVRAM data is 0x"
DataNumberStart:	dc.b "0000000000000000"
DataNumberEnd:			dc.b $0A,0

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
	bsr		I2Crwbyte
	move.w	#$08FF,d0	; NVRAM/ALARM1 starts at addr 0x07
	bsr		I2Crwbyte
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
	bsr		I2Crwbyte
	moveq		#2,d3
I2Creadloop:
	move.w	#$ff00,d0
	bsr		I2Crwbyte
	dbra		d3,I2Creadloop
	move.w	#$ffFF,d0
	bsr		I2Crwbyte
	SDAL					; I2C stop
	IDLEREAD
	SCLH
	IDLEREAD
	SDAH
	IDLEREAD
	IDLEREAD
;	move.l	d2,d4
;	swap		d4
;	not.w		d4
;	cmp.w		d2,d4
;	bne		I2Cfail
;I2Csuccess:
;	; our data is in d2
;I2Cfail:
	rts

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
	rts
