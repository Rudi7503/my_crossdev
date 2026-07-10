;==========================================================================
; Some macro definitions to be included in "i2c.library.card.s", 
; NOT in "i2c.library.s"!
;==========================================================================

; a CIA address, needed for the delay loops:
CIABPRA = $BFD000
I2CDAT = $DE0080

; What I/O lines do we use?
; SCL out: bit1 \_ in the word (!) at the
; SDA out: bit0 /  base address of our board
; SDA in:  bit0  of the same address

INITPORT MACRO
    lea I2CDAT,a1
    move.w #3,(a1)
    ENDM
ALLOCPERCALL MACRO
    ENDM
RELEASEPERCALL MACRO
    ENDM

PREP4MACROS MACRO
    lea CIABPRA,a0
    lea I2CDAT,a1
    moveq #3,d7
    ENDM
SCLH MACRO
    bset #1,d7
    move.w d7,(a1)
    ENDM
SCLL MACRO
    bclr #1,d7
    move.w d7,(a1)
    ENDM
SDAH MACRO
    bset #0,d7
    move.w d7,(a1)
    ENDM
SDAL MACRO
    bclr #0,d7
    move.w d7,(a1)
    ENDM
SDAtest MACRO
    btst #0,1(a1)
    ENDM
IDLEREAD MACRO
    tst.b (a0)
    ENDM

; this will become the 2nd half of the Version String:
IDPART2 MACRO
    dc.b ' for Apollo Core boards',13,10,0
    ENDM

