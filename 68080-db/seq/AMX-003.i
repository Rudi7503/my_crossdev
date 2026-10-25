; seq/AMX-003.i - saturating unsigned byte add with PADDUSB
; Uses: e0, e1, e2, a0, a1, a2
; MUST NOT touch: d5, d6, d7, a5, a6, a7
        load    (a0)+,e0
        load    (a1)+,e1
        paddusb e0,e1,e2
        store   e2,(a2)+
