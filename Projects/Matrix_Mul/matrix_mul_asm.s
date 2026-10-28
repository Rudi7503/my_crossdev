; ==============================================================================
; EXTREME OPTIMIZED 68080 MATRIX MULTIPLICATION (FPU / 3D PIPELINE)
; Enthält:
;   - _matmul_2x2_asm: 2x2 Matrix-Multiplikation (Single Precision Float)
;   - _matmul_3x3_asm: 3x3 Matrix-Multiplikation (Single Precision Float)
;   - _matmul_4x4_asm: 4x4 Matrix-Multiplikation (Single Precision Float)
;   - _get_ccc:        68080 Hardware Cycle Counter
;
; ABI & Aufrufkonvention:
;   void matmul_NxN_asm(const float *a, const float *b, float *out);
;   Argumente auf Stack:
;     4(sp)  = a   (Zeiger auf Matrix A, Row-Major)
;     8(sp)  = b   (Zeiger auf Matrix B, Row-Major)
;     12(sp) = out (Zeiger auf Ergebnis-Matrix C, Row-Major)
; ==============================================================================

            SECTION CODE,CODE

            XDEF    _matmul_2x2_asm
            XDEF    _matmul_3x3_asm
            XDEF    _matmul_4x4_asm
            XDEF    _get_ccc

; ==============================================================================
; 0. HARDWARE CYCLE COUNTER (CCC)
; ==============================================================================
_get_ccc:
            movec   ccc,d0
            rts

; ==============================================================================
; 1. MATMUL 2x2 (4 Floats Output)
; Vollständig ausgerollt, nutzt ausschließlich die Scratch-Register fp0 & fp1!
; Keine fmovem-Stackzugriffe für Register-Save/Restore nötig.
;
; c00 = a00*b00 + a01*b10
; c01 = a00*b01 + a01*b11
; c10 = a10*b00 + a11*b10
; c11 = a10*b01 + a11*b11
; ==============================================================================
_matmul_2x2_asm:
            move.l  4(a7),a0                ; a0 = pointer A
            move.l  8(a7),a1                ; a1 = pointer B
            move.l  12(a7),a2               ; a2 = pointer Out (C)

            ; --- Zeile 0, Spalte 0: c00 ---
            fmove.s (a0),fp0                ; a00
            fsmul.s (a1),fp0                ; a00 * b00
            fmove.s 4(a0),fp1               ; a01
            fsmul.s 8(a1),fp1               ; a01 * b10
            fsadd   fp1,fp0
            fmove.s fp0,(a2)                ; c00 speichern

            ; --- Zeile 0, Spalte 1: c01 ---
            fmove.s (a0),fp0                ; a00
            fsmul.s 4(a1),fp0               ; a00 * b01
            fmove.s 4(a0),fp1               ; a01
            fsmul.s 12(a1),fp1              ; a01 * b11
            fsadd   fp1,fp0
            fmove.s fp0,4(a2)               ; c01 speichern

            ; --- Zeile 1, Spalte 0: c10 ---
            fmove.s 8(a0),fp0               ; a10
            fsmul.s (a1),fp0                ; a10 * b00
            fmove.s 12(a0),fp1              ; a11
            fsmul.s 8(a1),fp1               ; a11 * b10
            fsadd   fp1,fp0
            fmove.s fp0,8(a2)               ; c10 speichern

            ; --- Zeile 1, Spalte 1: c11 ---
            fmove.s 8(a0),fp0               ; a10
            fsmul.s 4(a1),fp0               ; a10 * b01
            fmove.s 12(a0),fp1              ; a11
            fsmul.s 12(a1),fp1              ; a11 * b11
            fsadd   fp1,fp0
            fmove.s fp0,12(a2)              ; c11 speichern

            rts

; ==============================================================================
; 2. MATMUL 3x3 (9 Floats Output)
; Lädt pro Zeile i (i=0..2) die 3 Faktoren a_i0, a_i1, a_i2 in fp2, fp3, fp4
; und multipliziert sie mit den 3 Spalten von B.
; ==============================================================================
_matmul_3x3_asm:
            fmovem.x fp2-fp5,-(a7)          ; 4 FPU-Register sichern (48 Bytes)
            move.l  52(a7),a0               ; a0 = pointer A (48 Bytes FPU + 4 Bytes PC)
            move.l  56(a7),a1               ; a1 = pointer B
            move.l  60(a7),a2               ; a2 = pointer Out (C)

            ; -----------------------------------------------------------
            ; ZEILE 0
            ; -----------------------------------------------------------
            fmove.s (a0)+,fp2               ; fp2 = a00
            fmove.s (a0)+,fp3               ; fp3 = a01
            fmove.s (a0)+,fp4               ; fp4 = a02

            ; c00 = a00*b00 + a01*b10 + a02*b20
            fmove.x fp2,fp0
            fsmul.s (a1),fp0
            fmove.x fp3,fp1
            fsmul.s 12(a1),fp1
            fsadd   fp1,fp0
            fmove.x fp4,fp1
            fsmul.s 24(a1),fp1
            fsadd   fp1,fp0
            fmove.s fp0,(a2)+

            ; c01 = a00*b01 + a01*b11 + a02*b21
            fmove.x fp2,fp0
            fsmul.s 4(a1),fp0
            fmove.x fp3,fp1
            fsmul.s 16(a1),fp1
            fsadd   fp1,fp0
            fmove.x fp4,fp1
            fsmul.s 28(a1),fp1
            fsadd   fp1,fp0
            fmove.s fp0,(a2)+

            ; c02 = a00*b02 + a01*b12 + a02*b22
            fmove.x fp2,fp0
            fsmul.s 8(a1),fp0
            fmove.x fp3,fp1
            fsmul.s 20(a1),fp1
            fsadd   fp1,fp0
            fmove.x fp4,fp1
            fsmul.s 32(a1),fp1
            fsadd   fp1,fp0
            fmove.s fp0,(a2)+

            ; -----------------------------------------------------------
            ; ZEILE 1
            ; -----------------------------------------------------------
            fmove.s (a0)+,fp2               ; fp2 = a10
            fmove.s (a0)+,fp3               ; fp3 = a11
            fmove.s (a0)+,fp4               ; fp4 = a12

            ; c10
            fmove.x fp2,fp0
            fsmul.s (a1),fp0
            fmove.x fp3,fp1
            fsmul.s 12(a1),fp1
            fsadd   fp1,fp0
            fmove.x fp4,fp1
            fsmul.s 24(a1),fp1
            fsadd   fp1,fp0
            fmove.s fp0,(a2)+

            ; c11
            fmove.x fp2,fp0
            fsmul.s 4(a1),fp0
            fmove.x fp3,fp1
            fsmul.s 16(a1),fp1
            fsadd   fp1,fp0
            fmove.x fp4,fp1
            fsmul.s 28(a1),fp1
            fsadd   fp1,fp0
            fmove.s fp0,(a2)+

            ; c12
            fmove.x fp2,fp0
            fsmul.s 8(a1),fp0
            fmove.x fp3,fp1
            fsmul.s 20(a1),fp1
            fsadd   fp1,fp0
            fmove.x fp4,fp1
            fsmul.s 32(a1),fp1
            fsadd   fp1,fp0
            fmove.s fp0,(a2)+

            ; -----------------------------------------------------------
            ; ZEILE 2
            ; -----------------------------------------------------------
            fmove.s (a0)+,fp2               ; fp2 = a20
            fmove.s (a0)+,fp3               ; fp3 = a21
            fmove.s (a0)+,fp4               ; fp4 = a22

            ; c20
            fmove.x fp2,fp0
            fsmul.s (a1),fp0
            fmove.x fp3,fp1
            fsmul.s 12(a1),fp1
            fsadd   fp1,fp0
            fmove.x fp4,fp1
            fsmul.s 24(a1),fp1
            fsadd   fp1,fp0
            fmove.s fp0,(a2)+

            ; c21
            fmove.x fp2,fp0
            fsmul.s 4(a1),fp0
            fmove.x fp3,fp1
            fsmul.s 16(a1),fp1
            fsadd   fp1,fp0
            fmove.x fp4,fp1
            fsmul.s 28(a1),fp1
            fsadd   fp1,fp0
            fmove.s fp0,(a2)+

            ; c22
            fmove.x fp2,fp0
            fsmul.s 8(a1),fp0
            fmove.x fp3,fp1
            fsmul.s 20(a1),fp1
            fsadd   fp1,fp0
            fmove.x fp4,fp1
            fsmul.s 32(a1),fp1
            fsadd   fp1,fp0
            fmove.s fp0,(a2)+

            fmovem.x (a7)+,fp2-fp5          ; FPU-Register wiederherstellen
            rts

; ==============================================================================
; 3. MATMUL 4x4 (16 Floats Output – 3D Transformations-Matrix)
; Algorithmus: Vektorisierte Row-Akkumulation:
;   Row_i(C) = a_i0 * Row_0(B) + a_i1 * Row_1(B) + a_i2 * Row_2(B) + a_i3 * Row_3(B)
;
; fp4, fp5, fp6, fp7 akkumulieren parallel die 4 Spaltenwerte der aktuellen Zeile.
; Alle Speicherzugriffe auf Matrix B erfolgen linear mit Schrittweite 1 (stride 1).
; Schleife läuft 4x für die 4 Zeilen von A (oder unrolled).
; ==============================================================================
_matmul_4x4_asm:
            fmovem.x fp2-fp7,-(a7)          ; 6 FPU-Register sichern (72 Bytes)
            move.l  d2,-(a7)                ; Schleifenzähler sichern (4 Bytes)
            ; Gesamt-Stack-Offset: 72 + 4 + 4 (PC) = 80 Bytes
            move.l  80(a7),a0               ; a0 = pointer A
            move.l  84(a7),a1               ; a1 = pointer B
            move.l  88(a7),a2               ; a2 = pointer Out (C)

            moveq   #3,d2                   ; 4 Zeilen (3 bis 0)

.row_loop:
            ; -----------------------------------------------------------
            ; Term 0: a_i0 * Row_0(B)
            ; -----------------------------------------------------------
            fmove.s (a0)+,fp0               ; fp0 = a_i0
            fmove.x fp0,fp4
            fsmul.s (a1),fp4                ; fp4 = a_i0 * b00
            fmove.x fp0,fp5
            fsmul.s 4(a1),fp5               ; fp5 = a_i0 * b01
            fmove.x fp0,fp6
            fsmul.s 8(a1),fp6               ; fp6 = a_i0 * b02
            fmove.x fp0,fp7
            fsmul.s 12(a1),fp7              ; fp7 = a_i0 * b03

            ; -----------------------------------------------------------
            ; Term 1: + a_i1 * Row_1(B)
            ; -----------------------------------------------------------
            fmove.s (a0)+,fp0               ; fp0 = a_i1
            fmove.x fp0,fp1
            fsmul.s 16(a1),fp1              ; a_i1 * b10
            fsadd   fp1,fp4
            fmove.x fp0,fp2
            fsmul.s 20(a1),fp2              ; a_i1 * b11
            fsadd   fp2,fp5
            fmove.x fp0,fp3
            fsmul.s 24(a1),fp3              ; a_i1 * b12
            fsadd   fp3,fp6
            fsmul.s 28(a1),fp0              ; a_i1 * b13
            fsadd   fp0,fp7

            ; -----------------------------------------------------------
            ; Term 2: + a_i2 * Row_2(B)
            ; -----------------------------------------------------------
            fmove.s (a0)+,fp0               ; fp0 = a_i2
            fmove.x fp0,fp1
            fsmul.s 32(a1),fp1              ; a_i2 * b20
            fsadd   fp1,fp4
            fmove.x fp0,fp2
            fsmul.s 36(a1),fp2              ; a_i2 * b21
            fsadd   fp2,fp5
            fmove.x fp0,fp3
            fsmul.s 40(a1),fp3              ; a_i2 * b22
            fsadd   fp3,fp6
            fsmul.s 44(a1),fp0              ; a_i2 * b23
            fsadd   fp0,fp7

            ; -----------------------------------------------------------
            ; Term 3: + a_i3 * Row_3(B)
            ; -----------------------------------------------------------
            fmove.s (a0)+,fp0               ; fp0 = a_i3
            fmove.x fp0,fp1
            fsmul.s 48(a1),fp1              ; a_i3 * b30
            fsadd   fp1,fp4
            fmove.x fp0,fp2
            fsmul.s 52(a1),fp2              ; a_i3 * b31
            fsadd   fp2,fp5
            fmove.x fp0,fp3
            fsmul.s 56(a1),fp3              ; a_i3 * b32
            fsadd   fp3,fp6
            fsmul.s 60(a1),fp0              ; a_i3 * b33
            fsadd   fp0,fp7

            ; -----------------------------------------------------------
            ; Zeile i speichern (4 Floats = 16 Bytes)
            ; -----------------------------------------------------------
            fmove.s fp4,(a2)+
            fmove.s fp5,(a2)+
            fmove.s fp6,(a2)+
            fmove.s fp7,(a2)+

            dbf     d2,.row_loop

            move.l  (a7)+,d2
            fmovem.x (a7)+,fp2-fp7
            rts
