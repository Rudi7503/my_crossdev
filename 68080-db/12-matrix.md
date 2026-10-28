# 12 – Matrix-Multiplikation (FPU Single Precision)

**Dateien:** [`seq/MAT-001.i`](seq/MAT-001.i), [`seq/MAT-002.i`](seq/MAT-002.i), [`seq/MAT-003.i`](seq/MAT-003.i).

**Ziel:** Maximal pipelined 68080 FPU-Routinen für 2x2, 3x3 und 4x4 Single-Precision-Gleitkomma-Matrixmultiplikationen ($C = A \cdot B$) unter Nutzung des vollständigen 68080 32-Register-Modells.

> **Messstatus:** Alle drei Einträge wurden mit **Harness v5** auf der **Vampire V4 Hardware** gemessen (100 Läufe unter `Forbid()`, Warmup, CCC-Hardwaretaktzähler, IEP-Pipeslot-Erfassung). Alle Läufe sind zu 100 % deterministisch (MIN = MAX = AVG).

---

## 1. Hardware-Architektur der 68080 FPU & Optimierungsmechanismen

Aus dem Studium des **Apollo 68080 PRM (S. 4, 5, 9, 90, 91, 93, 94)**:
1. **32 FPU-Register (`FP0..7` & `E0..23`):**
   Laut PRM S. 5 hat die 68080 FPU Zugriff auf 4-mal so viele Float-Register über das BANK-Präfix. Sowohl `fmove.s` als auch `fmul` und `fadd` können direkt auf `e0..e23` zugreifen.
2. **3-Operanden-Instruktionen (`fmul src1, src2, dest`):**
   Laut PRM S. 5 & 90 ist die FPU eine echte 3-Operanden-Einheit. Damit entfallen sämtliche Zwischenkopien (`fmove`).
3. **Vollständiges Register-Preloading (Zero Redundant Loads):**
   - Bei 4x4 MatMul wird Matrix $B$ (16 Floats) **einmalig vollständig in `e0..e15` geladen**.
   - Dies eliminiert 48 redundante Speicherladungen über die 4 Zeilen hinweg.
4. **Zero-Stall Pipeline-Scheduling:**
   - Multiplikationen werden für alle Spalten/Terme kontinuierlich in aufeinanderfolgenden Takten in die FPU-Pipeline geschickt (1 Takt Durchsatz).
   - Nach 6 Takten sind die ersten Multiplikationsergebnisse fertig und können unmittelbar in aufeinanderfolgenden Takten addiert werden – ohne 1 einzigen Takt Wartezeit!

---

## 2. Hardware-Messergebnisse auf der Vampire V4

| ID | Matrix | C-Referenz (GCC -O2) | Baseline ASM | Pipelined (8 Regs) | **32-Reg 3-Op ASM (Final)** | **Gesamtersparnis** | Speedup vs C |
|:---|:---:|:---:|:---:|:---:|:---:|:---:|:---:|
| **`MAT-001`** | **2x2** | 111 Takte | 60 Takte | 36 Takte | **27 Takte** | **-55.0 %** (-33 Takte) | **4.11x** |
| **`MAT-002`** | **3x3** | 301 Takte | 203 Takte | 124 Takte | **87 Takte** | **-57.1 %** (-116 Takte) | **3.46x** |
| **`MAT-003`** | **4x4** | 599 Takte | 476 Takte | 295 Takte | **189 Takte** | **-60.3 %** (-287 Takte) | **3.17x** |

---

## 3. Detail-Analysen der Implementierung

### MAT-001: 2x2 Matrixmultiplikation (27 Takte)
- Matrix $A$ (4 Floats) in `e0..e3`, Matrix $B$ (4 Floats) in `e4..e7`.
- Alle 8 Multiplikationen werden in den Takten T0..T7 über 3-Operanden `fmul` abgesetzt.
- Ab T8 werden die 4 Additionen (`fadd`) abgesetzt.
- Ab T14 werden die 4 Ergebnisse ohne jeden Stall gespeichert.

### MAT-002: 3x3 Matrixmultiplikation (87 Takte)
- Matrix $A$ in `e0..e8` (9 Floats), Matrix $B$ in `e9..e17` (9 Floats).
- Matrix $B$ wird nur ein einziges Mal aus dem Speicher geladen (18 Loads gesamt statt 36).
- 3-Operanden `fmul` und `fadd` über `fp0..fp7` und `e18..e21`.

### MAT-003: 4x4 Matrixmultiplikation (189 Takte)
- Matrix $B$ (16 Floats) komplett in `e0..e15` vorgeladen.
- Für jede Zeile $i$ werden $a_{i0}..a_{i3}$ in `e16..e19` geladen.
- 4 parallele Akkumulatoren in `fp0..fp3`.
- Reduziert den Speicherverkehr um 48 Single-Precision-Loads und läuft mit 189 Takten auf der V4 Hardware.
