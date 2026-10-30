; seq/BLT-002.i - transparent sprite color-key store with PCMEQW + STOREILM
; Uses: e0, e4, e7, a0, a1, a2
; MUST NOT touch: d5, d6, d7, a5, a6, a7
        load    (a0)+,e0
        load    (a2)+,e7
        pcmpeqw e7,e0,e4
        storeilm e0,e4,(a1)+
