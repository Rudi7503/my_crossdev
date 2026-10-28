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
            move.l  a2,-(sp)                ; a2 sichern (Preserved Register)
            move.l  8(sp),a0                ; a0 = pointer A
            move.l  12(sp),a1               ; a1 = pointer B
            move.l  16(sp),a2               ; a2 = pointer Out (C)

            ; --- Zeile 0, Spalte 0: c00 ---
            fmove.s (a0),fp0                ; a00
            fmul.s (a1),fp0                ; a00 * b00
            fmove.s 4(a0),fp1               ; a01
            fmul.s 8(a1),fp1               ; a01 * b10
            fadd    fp1,fp0
            fmove.s fp0,(a2)                ; c00 speichern

            ; --- Zeile 0, Spalte 1: c01 ---
            fmove.s (a0),fp0                ; a00
            fmul.s 4(a1),fp0               ; a00 * b01
            fmove.s 4(a0),fp1               ; a01
            fmul.s 12(a1),fp1              ; a01 * b11
            fadd    fp1,fp0
            fmove.s fp0,4(a2)               ; c01 speichern

            ; --- Zeile 1, Spalte 0: c10 ---
            fmove.s 8(a0),fp0               ; a10
            fmul.s (a1),fp0                ; a10 * b00
            fmove.s 12(a0),fp1              ; a11
            fmul.s 8(a1),fp1               ; a11 * b10
            fadd    fp1,fp0
            fmove.s fp0,8(a2)               ; c10 speichern

            ; --- Zeile 1, Spalte 1: c11 ---
            fmove.s 8(a0),fp0               ; a10
            fmul.s 4(a1),fp0               ; a10 * b01
            fmove.s 12(a0),fp1              ; a11
            fmul.s 12(a1),fp1              ; a11 * b11
            fadd    fp1,fp0
            fmove.s fp0,12(a2)              ; c11 speichern

            move.l  (sp)+,a2                ; a2 wiederherstellen
            rts

; ==============================================================================
; 2. MATMUL 3x3 (9 Floats Output)
; Lädt pro Zeile i (i=0..2) die 3 Faktoren a_i0, a_i1, a_i2 in fp2, fp3, fp4
; und multipliziert sie mit den 3 Spalten von B.
; ==============================================================================
_matmul_3x3_asm:
            move.l  a2,-(sp)                ; a2 sichern
            move.l  8(sp),a0                ; a0 = pointer A
            move.l  12(sp),a1               ; a1 = pointer B
            move.l  16(sp),a2               ; a2 = pointer Out (C)
            fmovem.x fp2-fp5,-(sp)          ; 4 FPU-Register sichern

            ; -----------------------------------------------------------
            ; ZEILE 0
            ; -----------------------------------------------------------
            fmove.s (a0)+,fp2               ; fp2 = a00
            fmove.s (a0)+,fp3               ; fp3 = a01
            fmove.s (a0)+,fp4               ; fp4 = a02

            ; c00 = a00*b00 + a01*b10 + a02*b20
            fmove.x fp2,fp0
            fmul.s (a1),fp0
            fmove.x fp3,fp1
            fmul.s 12(a1),fp1
            fadd    fp1,fp0
            fmove.x fp4,fp1
            fmul.s 24(a1),fp1
            fadd    fp1,fp0
            fmove.s fp0,(a2)+

            ; c01 = a00*b01 + a01*b11 + a02*b21
            fmove.x fp2,fp0
            fmul.s 4(a1),fp0
            fmove.x fp3,fp1
            fmul.s 16(a1),fp1
            fadd    fp1,fp0
            fmove.x fp4,fp1
            fmul.s 28(a1),fp1
            fadd    fp1,fp0
            fmove.s fp0,(a2)+

            ; c02 = a00*b02 + a01*b12 + a02*b22
            fmove.x fp2,fp0
            fmul.s 8(a1),fp0
            fmove.x fp3,fp1
            fmul.s 20(a1),fp1
            fadd    fp1,fp0
            fmove.x fp4,fp1
            fmul.s 32(a1),fp1
            fadd    fp1,fp0
            fmove.s fp0,(a2)+

            ; -----------------------------------------------------------
            ; ZEILE 1
            ; -----------------------------------------------------------
            fmove.s (a0)+,fp2               ; fp2 = a10
            fmove.s (a0)+,fp3               ; fp3 = a11
            fmove.s (a0)+,fp4               ; fp4 = a12

            ; c10
            fmove.x fp2,fp0
            fmul.s (a1),fp0
            fmove.x fp3,fp1
            fmul.s 12(a1),fp1
            fadd    fp1,fp0
            fmove.x fp4,fp1
            fmul.s 24(a1),fp1
            fadd    fp1,fp0
            fmove.s fp0,(a2)+

            ; c11
            fmove.x fp2,fp0
            fmul.s 4(a1),fp0
            fmove.x fp3,fp1
            fmul.s 16(a1),fp1
            fadd    fp1,fp0
            fmove.x fp4,fp1
            fmul.s 28(a1),fp1
            fadd    fp1,fp0
            fmove.s fp0,(a2)+

            ; c12
            fmove.x fp2,fp0
            fmul.s 8(a1),fp0
            fmove.x fp3,fp1
            fmul.s 20(a1),fp1
            fadd    fp1,fp0
            fmove.x fp4,fp1
            fmul.s 32(a1),fp1
            fadd    fp1,fp0
            fmove.s fp0,(a2)+

            ; -----------------------------------------------------------
            ; ZEILE 2
            ; -----------------------------------------------------------
            fmove.s (a0)+,fp2               ; fp2 = a20
            fmove.s (a0)+,fp3               ; fp3 = a21
            fmove.s (a0)+,fp4               ; fp4 = a22

            ; c20
            fmove.x fp2,fp0
            fmul.s (a1),fp0
            fmove.x fp3,fp1
            fmul.s 12(a1),fp1
            fadd    fp1,fp0
            fmove.x fp4,fp1
            fmul.s 24(a1),fp1
            fadd    fp1,fp0
            fmove.s fp0,(a2)+

            ; c21
            fmove.x fp2,fp0
            fmul.s 4(a1),fp0
            fmove.x fp3,fp1
            fmul.s 16(a1),fp1
            fadd    fp1,fp0
            fmove.x fp4,fp1
            fmul.s 28(a1),fp1
            fadd    fp1,fp0
            fmove.s fp0,(a2)+

            ; c22
            fmove.x fp2,fp0
            fmul.s 8(a1),fp0
            fmove.x fp3,fp1
            fmul.s 20(a1),fp1
            fadd    fp1,fp0
            fmove.x fp4,fp1
            fmul.s 32(a1),fp1
            fadd    fp1,fp0
            fmove.s fp0,(a2)+

            fmovem.x (a7)+,fp2-fp5          ; FPU-Register wiederherstellen
            move.l  (sp)+,a2                ; a2 wiederherstellen
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
            movem.l d2/a2,-(sp)             ; d2 und a2 sichern
            move.l  12(sp),a0               ; a0 = pointer A
            move.l  16(sp),a1               ; a1 = pointer B
            move.l  20(sp),a2               ; a2 = pointer Out (C)
            fmovem.x fp2-fp7,-(sp)          ; 6 FPU-Register sichern

            moveq   #3,d2                   ; 4 Zeilen (3 bis 0)

.row_loop:
            ; -----------------------------------------------------------
            ; Term 0: a_i0 * Row_0(B)
            ; -----------------------------------------------------------
            fmove.s (a0)+,fp0               ; fp0 = a_i0
            fmove.x fp0,fp4
            fmul.s (a1),fp4                ; fp4 = a_i0 * b00
            fmove.x fp0,fp5
            fmul.s 4(a1),fp5               ; fp5 = a_i0 * b01
            fmove.x fp0,fp6
            fmul.s 8(a1),fp6               ; fp6 = a_i0 * b02
            fmove.x fp0,fp7
            fmul.s 12(a1),fp7              ; fp7 = a_i0 * b03

            ; -----------------------------------------------------------
            ; Term 1: + a_i1 * Row_1(B)
            ; -----------------------------------------------------------
            fmove.s (a0)+,fp0               ; fp0 = a_i1
            fmove.x fp0,fp1
            fmul.s 16(a1),fp1              ; a_i1 * b10
            fadd    fp1,fp4
            fmove.x fp0,fp2
            fmul.s 20(a1),fp2              ; a_i1 * b11
            fadd    fp2,fp5
            fmove.x fp0,fp3
            fmul.s 24(a1),fp3              ; a_i1 * b12
            fadd    fp3,fp6
            fmul.s 28(a1),fp0              ; a_i1 * b13
            fadd    fp0,fp7

            ; -----------------------------------------------------------
            ; Term 2: + a_i2 * Row_2(B)
            ; -----------------------------------------------------------
            fmove.s (a0)+,fp0               ; fp0 = a_i2
            fmove.x fp0,fp1
            fmul.s 32(a1),fp1              ; a_i2 * b20
            fadd    fp1,fp4
            fmove.x fp0,fp2
            fmul.s 36(a1),fp2              ; a_i2 * b21
            fadd    fp2,fp5
            fmove.x fp0,fp3
            fmul.s 40(a1),fp3              ; a_i2 * b22
            fadd    fp3,fp6
            fmul.s 44(a1),fp0              ; a_i2 * b23
            fadd    fp0,fp7

            ; -----------------------------------------------------------
            ; Term 3: + a_i3 * Row_3(B)
            ; -----------------------------------------------------------
            fmove.s (a0)+,fp0               ; fp0 = a_i3
            fmove.x fp0,fp1
            fmul.s 48(a1),fp1              ; a_i3 * b30
            fadd    fp1,fp4
            fmove.x fp0,fp2
            fmul.s 52(a1),fp2              ; a_i3 * b31
            fadd    fp2,fp5
            fmove.x fp0,fp3
            fmul.s 56(a1),fp3              ; a_i3 * b32
            fadd    fp3,fp6
            fmul.s 60(a1),fp0              ; a_i3 * b33
            fadd    fp0,fp7

            ; -----------------------------------------------------------
            ; Zeile i speichern (4 Floats = 16 Bytes)
            ; -----------------------------------------------------------
            fmove.s fp4,(a2)+
            fmove.s fp5,(a2)+
            fmove.s fp6,(a2)+
            fmove.s fp7,(a2)+

            dbf     d2,.row_loop

            fmovem.x (sp)+,fp2-fp7
            movem.l (sp)+,d2/a2
            rts
