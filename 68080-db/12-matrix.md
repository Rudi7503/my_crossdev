# 12 – Matrix-Multiplikation (FPU Float & AMMX Fixed-Point)

**Dateien:**
- FPU (Float): [`seq/MAT-001.i`](seq/MAT-001.i), [`seq/MAT-002.i`](seq/MAT-002.i), [`seq/MAT-003.i`](seq/MAT-003.i).
- Fixed-Point (AMMX): [`seq/FIX-001.i`](seq/FIX-001.i), [`seq/FIX-002.i`](seq/FIX-002.i), [`seq/FIX-003.i`](seq/FIX-003.i).

**Ziel:** Vergleich und Benchmark von 32-Bit IEEE Single-Precision FPU gegen 16-Bit Fixed-Point AMMX SIMD für 2x2, 3x3 und 4x4 Matrizen.

> **Messstatus:** Alle sechs Einträge wurden mit **Harness v5** auf der **Vampire V4 Hardware** gemessen (100 Läufe unter `Forbid()`, Warmup, CCC-Hardwaretaktzähler, IEP-Pipeslot-Erfassung). Alle Messungen 100/100 Läufe absolut stabil.

---

## 1. Warum 118 Takte das theoretische Minimum für 4x4 FPU sind

Ein 4x4 MatMul erfordert:
- 16 Dotprodukte von 4 Elementen = **64 Multiplikationen (`fmul`)**
- 16 Summen von 4 Elementen = **48 Additionen (`fadd`)**
- Gesamte Arithmetik = **112 Gleitkomma-Instruktionen**

### Die Hardware-Grenzen des 68080 (PRM S. 91 & 93):
1. **Keine FPU auf Pipe 2:** `FPU = no` auf der zweiten superskalaren Pipe. Alle FPU-Befehle müssen über Pipe 1.
2. **Issue-Limit:** Pipe 1 kann maximal 1 FPU-Befehl pro Takt starten = **mindestens 112 Takte für die Math**.
3. **Pipeline-Drain:** Die letzte Addition benötigt 6 Takte Latenz vor dem Store = **118 Takte absolute Untergrenze der FPU**.
4. **Speicher-Overhead:**
   - 16 Loads für Matrix $B$
   - 16 Loads für Matrix $A$
   - 16 Stores für Matrix $C$
   - Summe = **48 Speicherinstruktionen (`fmove.s`)**.
   - Da `fmove.s` ein Line-F (FPU) Befehl ist, kann er ebenfalls nicht auf Pipe 2 laufen.
   - Gesamte Instruktionsanzahl = $112 + 48 = 160$ Befehle (+ 2 Harness-Instruktionen = 162).
   - Bei 1 Befehl pro Takt + Prefetch-Grenzen über 6-Byte-Bank-Präfixe resultiert das gemessene Hardware-Limit von **184 Takten**.

---

## 2. 16-Bit Fixed-Point AMMX SIMD: Durchbruch auf 57 Takte

Da die 68080 FPU an das 1-Befehl/Takt-Limit gebunden ist, bietet AMMX SIMD (64-Bit Vektor) die Möglichkeit für massive Parallelisierung:
- **Ein 64-Bit-Register (`e0`) fasst 4 Wörter (16-Bit):** Eine komplette Zeile der 4x4-Matrix passt in **ein einziges Register**!
- **Outer-Product Vektorisierung:**
  Jedes Element der Zeile wird via `vperm #$..., e0, e0, eX` in 1 Takt auf alle 4 Lanes gesplattet und via `pmulh` mit der Zeile von $B$ multipliziert.
- Ein `pmulh` berechnet **vier 16-Bit-Multiplikationen gleichzeitig in 1 Takt**!
- Ein `paddw` addiert **vier 16-Bit-Werte gleichzeitig in 1 Takt**!
- Eine ganze Matrix $A$ (32 Bytes) wird mit 4 Quad-Loads geladen.
- Eine ganze Matrix $C$ (32 Bytes) wird mit 4 Quad-Stores gespeichert.

---

## 3. Hardware-Messergebnisse auf der Vampire V4

| Matrix | GCC -O2 C-Ref | Float FPU Baseline | **Float FPU (Opt)** | **AMMX 16-Bit Fixed-Point** | **Speedup vs C** |
|:---|:---:|:---:|:---:|:---:|:---:|
| **2x2** | 111 Takte | 60 Takte | **27 Takte** | **17 Takte** | **6.53x schneller** |
| **3x3** | 301 Takte | 203 Takte | **87 Takte** | **34 Takte** | **8.85x schneller** |
| **4x4** | 599 Takte | 476 Takte | **184 Takte** | **54 Takte** | **11.09x schneller** |

*(Alle Messungen 100/100 Läufe stabil auf Vampire V4 gemessen).*
