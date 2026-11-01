; seq/REG-002.i - B address register: vasm-2.0f-compatible Dn<->Bn forms
; Uses: d0, d1, a0, b0
; MUST NOT touch: d5, d6, d7, a5, a6, a7
; vasm 2.0f intentionally rejects move.l Am,Bn and Bm,An.
; Load B0 through a data register and read it back through D1.
        move.l  a0,d0
        move.l  d0,b0
        move.l  b0,d1
