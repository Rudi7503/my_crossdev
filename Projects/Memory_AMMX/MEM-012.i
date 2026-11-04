; seq/MEM-012.i - AMMX Polymorphic Tail-Store (1..7 remaining bytes via STOREILM)
; Uses: e0, e6, a0, a1, a2
; MUST NOT touch: d5, d6, d7, a5, a6, a7
; e6 loaded from dynamic mask table in a2
        load    (a0)+,e0
        load    (a2)+,e6
        storeilm e0,e6,(a1)+
