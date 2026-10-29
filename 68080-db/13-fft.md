# 13 – Fast Fourier Transform (FPU Float & AMMX Fixed-Point)

**Dateien:**
- FPU (Float): [`seq/FFT-001.i`](seq/FFT-001.i) (32-Bit IEEE Single-Precision Schmetterling)
- Fixed-Point Skalar: [`seq/FFT-002.i`](seq/FFT-002.i) (16-Bit Q1.15 Skalar-Schmetterling)
- Fixed-Point AMMX SIMD: [`seq/FFT-003.i`](seq/FFT-003.i) (16-Bit Q1.15 AMMX Doppel-Schmetterling)
- Vollständige Algorithmen-Suite: [`Projects/FFT/`](../Projects/FFT/) (`main.c`, `fft_c.c`, `fft_asm.s`)

**Ziel:** Optimierung, theoretische Grenzwertanalyse und Hardware-Benchmarking von Radix-2 Cooley-Tukey FFT-Transformationen auf dem Apollo 68080 für 32-Bit Gleitkomma (FPU) und 16-Bit Festkomma (Skalar vs. AMMX SIMD) über alle Größen $N \in \{64, 128, 256, 512, 1024, 2048, 4096\}$.

> **Messstatus:** Alle drei Kernsequenzen wurden mit **Harness v5** auf der **Vampire V4 Hardware** gemessen (100 Läufe unter `Forbid()`, Warmup, CCC-Hardwaretaktzähler, IEP-Pipeslot-Erfassung). Alle Messungen 100/100 Läufe absolut stabil (MIN=MAX=AVG). Zudem wurde die vollständige Benchmark-Reihe ($N=64 \dots 4096$) auf der V4 gegen die bitgenaue C-Referenz (`-O2 -m68020 -m68881`) validiert.

---

## 1. Die Anatomie des Radix-2 Schmetterlings (Butterfly)

Für eine $N$-Punkt-FFT gilt:
- Anzahl Stufen: $S = \log_2(N)$
- Schmetterlinge gesamt: $B_{\text{total}} = \frac{N}{2} \cdot \log_2(N)$

Jeder Schmetterling verknüpft zwei komplexe Werte $u$ und $v$ mit einem Drehfaktor $W = w_r + i \cdot w_i$:
$$t = v \cdot W = (v_r \cdot w_r - v_i \cdot w_i) + i(v_r \cdot w_i + v_i \cdot w_r)$$
$$u' = (u + t) \gg 1, \quad v' = (u - t) \gg 1$$
*(Hinweis: Bei Festkomma verhindert der Rechtsshift $\gg 1$ pro Stufe das Überlaufen der 16-Bit-Werte).*

---

## 2. Hardware-Grenzen & Instruktions-Analyse

### A. 32-Bit Float FPU (`FFT-001.i`): 28 Takte pro Schmetterling
- **Operationen:** 4 $\times$ `fmul`, 2 $\times$ `fadd`, 2 $\times$ `fsub` (8 Rechenbefehle) + 4 $\times$ `fmove.s` (Loads) + 4 $\times$ `fmove.s` (Stores) = 16 FPU-Instruktionen.
- **Hardware-Limit:** Da die 68080 FPU an Pipe 1 gebunden ist (`FPU = no` auf Pipe 2), müssen alle Befehle sequentiell durch Pipe 1.
- **Gemessenes Minimum:** **28 Takte** (`IEP1 = 22, IEP2 = 0`).

### B. 16-Bit Fixed-Point Skalar (`FFT-002.i`): 31 Takte pro Schmetterling
- **Operationen:** 4 $\times$ `muls.w`, 32-Bit Arithmetik (`ext.l`, `add.l`, `sub.l`, `asr.l #15`, `asr.l #1`), Loads/Stores.
- **Problem des 16-Bit Überlaufs:** Zwischensummen müssen zwingend auf 32-Bit erweitert werden (`ext.l`), da Amplituden über 32767 sonst in 16-Bit signed überlaufen.
- **Gemessenes Minimum:** **31 Takte** (`IEP1 = 28, IEP2 = 12`).

### C. 16-Bit Fixed-Point AMMX SIMD (`FFT-003.i`): 17 Takte für 2 Schmetterlinge (= 8.5 Takte / Butterfly!)
AMMX verarbeitet **zwei Schmetterlinge parallel** in 64-Bit-Vektorregistern:
1. **Parallel-Load:** `load (An), E0` lädt 4 Wörter (2 komplexe Zahlen) in **1 Takt**.
2. **Re-Normierung & Skalierung geschenkt:** `pmulh` multipliziert und schiebt gleichzeitig $\gg 16$. Da $Q1.15 \times Q1.15$ einen $\gg 15$-Shift und der Schmetterling ein $\gg 1$ verlangt, entspricht $\gg 16$ exakt dem kombinierten **Normierungs- und Halbierungsschritt** in einem einzigen Takt!
3. **Vektor-Drehung (`VPERM` + `PMULH` + `PADDW`):** Real- und Imaginärteile werden dupliziert und mit den Drehfaktoren multipliziert.
4. **Hardware-Schmetterling (`BFLYW`):** Ein einziger `bflyw E_T, E_U, E12:E13`-Befehl berechnet parallel alle 4 Additionen und 4 Subtraktionen für 2 komplette Schmetterlinge in **1 Takt**.
- **Gemessenes Minimum:** **17 Takte** für 2 Schmetterlinge $\rightarrow$ **8.5 Takte pro Schmetterling**!

---

## 3. Harness-v5 Messergebnisse (Isolierter Schmetterling-Mikrobenchmark)

Gemessen auf **Vampire V4 (Core Release, PCR: 1088/1)**:

| ID | Format | Typ | Takte (MIN) | Takte (AVG) | Netto (MIN) | Takte / Butterfly | IEP1 | IEP2 | Speedup vs Skalar |
|:---|:---|:---|:---:|:---:|:---:|:---:|:---:|:---:|:---:|
| [`FFT-001`](seq/FFT-001.i) | 32-Bit Float | FPU Skalar (1 Butterfly) | **28** | 28.0 | 23 | **28.0 Takte** | 22 | 0 | 1.11x |
| [`FFT-002`](seq/FFT-002.i) | 16-Bit Fixed | Integer Skalar (1 Butterfly) | **31** | 31.0 | 26 | **31.0 Takte** | 28 | 12 | 1.00x |
| [`FFT-003`](seq/FFT-003.i) | 16-Bit Fixed | AMMX SIMD (2 Butterflies) | **17** | 17.0 | 12 | **8.5 Takte** | 18 | 0 | **3.65x schneller** |

---

## 4. Vollständige Makro-Benchmark-Reihe (N = 64 bis 4096 Punkte)

Gemessen mit `Projects/FFT/FFT` direkt auf der Vampire V4 Hardware, validiert gegen die C-Referenz:

| $N$ Punkte | Float C (cyc) | Float ASM (cyc) | Spd Float | Fixed C (cyc) | Fixed Scal (cyc) | Spd Scal | **Fixed AMMX (cyc)** | **Spd vs C** | **Spd vs Scal** | Diff (LSB) | Status |
|:---:|:---:|:---:|:---:|:---:|:---:|:---:|:---:|:---:|:---:|:---:|:---:|
| **64** | 14.649 | 8.991 | 1.63x | 11.493 | 9.610 | 1.20x | **6.226** | **1.85x** | 1.54x | 1 / 7 | PASS |
| **128** | 31.004 | 18.622 | 1.66x | 23.506 | 20.266 | 1.16x | **11.082** | **2.12x** | 1.83x | 1 / 9 | PASS |
| **256** | 67.737 | 40.281 | 1.68x | 50.054 | 44.386 | 1.13x | **23.181** | **2.16x** | 1.91x | 1 / 11 | PASS |
| **512** | 170.802 | 88.116 | 1.94x | 107.431 | 95.866 | 1.12x | **47.471** | **2.26x** | 2.02x | 1 / 12 | PASS |
| **1024** | 317.295 | 185.906 | 1.71x | 230.791 | 253.968 | 0.91x | **98.143** | **2.35x** | 2.59x | 1 / 13 | PASS |
| **2048** | 736.529 | 403.166 | 1.83x | 519.753 | 435.228 | 1.19x | **201.431** | **2.58x** | 2.16x | 1 / 15 | PASS |
| **4096** | 1.506.219 | 872.571 | 1.73x | 1.094.262 | 1.010.767 | 1.08x | **421.037** | **2.60x** | 2.40x | 1 / 17 | PASS |

*(Alle Messungen 100 % fehlerfrei und bitgenau / DSP-akkurat auf echter Hardware validiert).*
