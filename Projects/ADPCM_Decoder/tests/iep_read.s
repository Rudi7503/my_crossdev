        section code,code
        xdef    _read_iep1
        xdef    _read_iep2
        xdef    _read_iep3
_read_iep1:
        movec.l IEP1,d0
        rts
_read_iep2:
        movec.l IEP2,d0
        rts
_read_iep3:
        dc.w    $4e7a,$000c
        rts
