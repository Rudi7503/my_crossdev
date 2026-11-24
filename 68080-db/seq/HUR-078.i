; seq/HUR-078.i - Hurrican _EnemiesDraw dispatch: Jump table (move.l (a4,d4.l*4),d0)
; Benchmarks dispatch cost for same 2 enemies using direct table lookup via a4 (scratch)
; Uses: d0-d4, a0-a3. MUST NOT touch: d5, d6, d7, a5, a6, a7
        ifnd    HUR078_MAC
HUR078_MAC equ 1
HUR078_BODY macro
        ; Test ID 5 (Walker)
        moveq   #5,d4
        move.l  (a4,d4.l*4),d0

        ; Test ID 48 (Pflanze)
        moveq   #48,d4
        move.l  (a4,d4.l*4),d1

        add.l   d1,d0
        endm
        endif
        HUR078_BODY
