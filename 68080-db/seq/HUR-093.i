; seq/HUR-093.i - hurrican.s DrawWater: Optimized Inlined CovCell (row-hoisted pointer)
; Row invariant (y=10, row_offset = 6 * 34 = 204) computed once outside cell loop
; Uses: d0-d4, a0-a3. MUST NOT touch: d5, d6, d7, a5, a6, a7
        ifnd    HUR093_MAC
HUR093_MAC equ 1
HUR093_BODY macro
        ; Hoisted row setup once per row:
        moveq   #10-4,d1        ; y - CovTY = 6
        mulu.l  #34,d1          ; 204
        lea     (a4,d1.l),a0    ; a0 = row pointer in CovF

        ; Cell 1 (x=5)
        moveq   #5-2,d1         ; x - CovTX = 3
        move.b  (a0,d1.l),d4

        ; Cell 2 (x=6)
        moveq   #6-2,d1         ; 4
        add.b   (a0,d1.l),d4

        ; Cell 3 (x=7)
        moveq   #7-2,d1         ; 5
        add.b   (a0,d1.l),d4

        ; Cell 4 (x=8)
        moveq   #8-2,d1         ; 6
        add.b   (a0,d1.l),d4

        move.b  d4,d0
        endm
        endif
        HUR093_BODY
