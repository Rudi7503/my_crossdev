; seq/BLT-018.i - AMMX Tail-Store with preloaded lane mask in e6
; Uses: e2, e6, a0, a1
; MUST NOT touch: d5, d6, d7, a5, a6, a7
; Loads remaining pixels into e2, writes valid lanes using preloaded mask e6
        load    (a0)+,e2
        storeilm e2,e6,(a1)+
