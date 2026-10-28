# 12 – Matrix-Multiplikation (FPU Single Precision)

**Dateien:** [`seq/MAT-001.i`](seq/MAT-001.i), [`seq/MAT-002.i`](seq/MAT-002.i), [`seq/MAT-003.i`](seq/MAT-003.i).

**Ziel:** Maximal pipelined 68080 FPU-Routinen für 2x2, 3x3 und 4x4 Single-Precision-Gleitkomma-Matrixmultiplikationen ($C = A \cdot B$).

> **Messstatus:** Alle drei Einträge wurden mit **Harness v5** auf der **Vampire V4 Hardware** gemessen (100 Läufe unter `Forbid()`, Warmup, CCC-Hardwaretaktzähler, IEP-Pipeslot-Erfassung). Alle Läufe sind zu 100 % deterministisch (MIN = MAX = AVG).

---

## 1. Hardware-Architektur der 68080 FPU & Optimierungsmechanismus

Laut **Apollo 68080 PRM (S. 5, 93, 94)**:
1. **Fully Pipelined Execution:** Die 68080 FPU kann **in jedem einzelnen Takt** eine neue Gleitkomma-Instruktion (`FMUL`, `FADD`, `FSUB`) annehmen.
2. **6-Takt-Latenz:** Eine Instruktion (`FMUL`, `FADD`) benötigt **6 Taktzyklen**, bis ihr Ergebnis im Zielregister zur Verfügung steht.
3. **Pipeline-Hazard (Stalls):** Wird das Ergebnis einer `FMUL` unmittelbar in der nächsten Instruktion als Operand gebraucht (z. B. `fmul ..., fp0` gefolgt von `fadd ..., fp0`), **blockiert die CPU für bis zu 5–6 Takte**.
4. **Optimierungslösung: Concurrent Multi-Column Accumulator Interleaving:**
   - Mehrere Spaltenelemente derselben Zeile werden **parallel akkumuliert**.
   - Die Multiplikationen für Col 0, Col 1, Col 2, Col 3 werden in aufeinanderfolgenden Takten in die Pipeline geschickt.
   - Wenn die Additionen beginnen, sind die Multiplikationsergebnisse der vorherigen Spalten bereits fertig berechnet – **die Latenz ist zu 100 % maskiert**!

---

## 2. Vergleich: Vorher vs. Nachher auf Vampire V4

| ID | Matrix-Größe | Vorher (Standard ASM) | **Jetzt (Fully Pipelined)** | Ersparnis | Speedup vs C-Ref |
|:---|:---:|:---:|:---:|:---:|:---:|
| **`MAT-001`** | **2x2** | 60 Takte | **36 Takte** | **-40.0 %** (-24 Takte) | **3.08x** (C: 111 Takte) |
| **`MAT-002`** | **3x3** | 203 Takte | **124 Takte** | **-38.9 %** (-79 Takte) | **2.43x** (C: 301 Takte) |
| **`MAT-003`** | **4x4** | 476 Takte | **295 Takte** | **-38.0 %** (-181 Takte) | **2.03x** (C: 599 Takte) |

---

## 3. Detail-Analysen

### MAT-001: 2x2 Matrixmultiplikation (36 Takte)
- **Registerbelegung:** Matrix $B$ wird einmalig in `fp4..fp7` geladen (nur 4 statt 8 Speicherladungen).
- **Pipelining:** Multiplikationen von Zeile 0 werden zuerst abgesetzt; die Latenz bis zum `fadd` wird durch das Laden von Zeile 1 in die Integer-Register `d0, d1` (Zweitpipe, 1 Takt) vollständig überbrückt.
- Hardware-Messung V4: **36 Takte** (100/100 Läufe stabil 36, IEP1: 31 Slots, IEP2: 1 Slot).

### MAT-002: 3x3 Matrixmultiplikation (124 Takte)
- **Registerbelegung:** 3 parallele Spaltenakkumulatoren in `fp0` (Col 0), `fp1` (Col 1), `fp5` (Col 2).
- **Interleaving:** Term 0 ($a_{i0} \cdot b_{0x}$) setzt alle 3 Multiplikationen ab. Term 1 ($a_{i1} \cdot b_{1x}$) multipliziert in `fp6, fp7`, während Term 0 addiert wird. Term 2 schließt die Akkumulation ab.
- Hardware-Messung V4: **124 Takte** (100/100 Läufe stabil 124, IEP1: 92 Slots).

### MAT-003: 4x4 Matrixmultiplikation (295 Takte)
- **Registerbelegung:** 4 parallele Spaltenakkumulatoren in `fp0, fp1, fp2, fp3`.
- **Interleaving:** Für jeden Term $k \in \{0, 1, 2, 3\}$ wird $a_{ik}$ einmal in `fp4` geladen und auf alle 4 Spalten von $B$ multipliziert. Die Multiplikationslatenz wird durch die nächsten Spaltenladungen und die Akkumulation vorheriger Terme komplett absorbiert.
- Hardware-Messung V4: **295 Takte** (100/100 Läufe stabil 295, IEP1: 210 Slots).
