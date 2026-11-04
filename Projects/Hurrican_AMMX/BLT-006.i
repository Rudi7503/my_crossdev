; seq/BLT-006.i - AMMX tail store using STOREILM with lane mask
; Uses: e2, e6, a0, a1, a2
; MUST NOT touch: d5, d6, d7, a5, a6, a7
; Loads 4 pixels into e2, tail mask into e6, writes valid lanes to (a1)+
        load    (a0)+,e2
        load    (a2)+,e6
        storeilm e2,e6,(a1)+
