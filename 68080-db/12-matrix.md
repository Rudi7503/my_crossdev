# 12 – Matrix-Multiplikation (FPU Single Precision)

**Dateien:** [`seq/MAT-001.i`](seq/MAT-001.i), [`seq/MAT-002.i`](seq/MAT-002.i), [`seq/MAT-003.i`](seq/MAT-003.i).

**Ziel:** Vollständig ungerollte, optimierte 68080 FPU-Routinen für 2x2, 3x3 und 4x4 Single-Precision-Gleitkomma-Matrixmultiplikationen ($C = A \cdot B$).

> **Messstatus:** Alle drei Einträge wurden mit **Harness v5** auf der **Vampire V4** hardware-gemessen (100 Läufe unter `Forbid()`, Warmup, CCC-Zähler, IEP-Pipeslots). Alle Läufe waren zu 100 % deterministisch (MIN = MAX = AVG).

---

## MAT-001: 2x2 Single-Precision Matrixmultiplikation

**Kategorie:** FPU / 3D Pipeline  
**Status:** VERIFIZIERT (gemessen – Vampire V4)  
**Taktzyklen:** **60 Takte** (Baseline: 5 Takte)  
**IEP1-Slots:** 30 Instruktionen pro Durchlauf  

### Problem
Naiver C-Code mit verschachtelten Schleifen erzeugt Adressierungs- und Loop-Overhead sowie redundante Speicherladungen.

### 68080-Optimierung
Zeile $i$ von Matrix $A$ wird vorab in FPU-Register geladen (`fp2`, `fp3`). Die Multiplikationen mit den Spaltenelementen von $B$ erfolgen direkt, gefolgt von `fadd` und direktem Store des Ergebnisses in $C$.

```asm
; seq/MAT-001.i - 2x2 Single Precision MatMul (C = A * B)
; Inputs:  a0 = Matrix A (16 Bytes), a1 = Matrix B (16 Bytes)
; Output:  a2 = Matrix C (16 Bytes)
; Uses:    fp0, fp1, fp2, fp3, a0, a1, a2
; MUST NOT touch: d5, d6, d7, a5, a6, a7
        fmove.s (a0),fp2        ; a00
        fmove.s 4(a0),fp3       ; a01

        ; c00 = a00*b00 + a01*b10
        fmove.s (a1),fp0        ; b00
        fmul    fp2,fp0         ; a00 * b00
        fmove.s 8(a1),fp1       ; b10
        fmul    fp3,fp1         ; a01 * b10
        fadd    fp1,fp0
        fmove.s fp0,(a2)

        ; c01 = a00*b01 + a01*b11
        fmove.s 4(a1),fp0       ; b01
        fmul    fp2,fp0         ; a00 * b01
        fmove.s 12(a1),fp1      ; b11
        fmul    fp3,fp1         ; a01 * b11
        fadd    fp1,fp0
        fmove.s fp0,4(a2)

        fmove.s 8(a0),fp2       ; a10
        fmove.s 12(a0),fp3      ; a11

        ; c10 = a10*b00 + a11*b10
        fmove.s (a1),fp0        ; b00
        fmul    fp2,fp0         ; a10 * b00
        fmove.s 8(a1),fp1       ; b10
        fmul    fp3,fp1         ; a11 * b10
        fadd    fp1,fp0
        fmove.s fp0,8(a2)

        ; c11 = a10*b01 + a11*b11
        fmove.s 4(a1),fp0       ; b01
        fmul    fp2,fp0         ; a10 * b01
        fmove.s 12(a1),fp1      ; b11
        fmul    fp3,fp1         ; a11 * b11
        fadd    fp1,fp0
        fmove.s fp0,12(a2)
```

---

## MAT-002: 3x3 Single-Precision Matrixmultiplikation

**Kategorie:** FPU / 3D Pipeline  
**Status:** VERIFIZIERT (gemessen – Vampire V4)  
**Taktzyklen:** **203 Takte** (Baseline: 5 Takte)  
**IEP1-Slots:** 92 Instruktionen pro Durchlauf  

### 68080-Optimierung
Zeile $i$ von $A$ wird vorab in `fp2`, `fp3`, `fp4` geladen (3 Floats). Jede Spalte von $B$ wird akkumuliert (`fmul` + `fadd` + `fmul` + `fadd`) und direkt nach $C$ geschrieben.

---

## MAT-003: 4x4 Single-Precision Matrixmultiplikation

**Kategorie:** FPU / 3D Pipeline  
**Status:** VERIFIZIERT (gemessen – Vampire V4)  
**Taktzyklen:** **476 Takte** (Baseline: 5 Takte)  
**IEP1-Slots:** 210 Instruktionen pro Durchlauf  

### 68080-Optimierung
Zeile $i$ von $A$ wird vorab in `fp2`, `fp3`, `fp4`, `fp5` geladen (4 Floats). Berechnung aller 16 Elemente vollständig ohne Schleifen-Overhead.
