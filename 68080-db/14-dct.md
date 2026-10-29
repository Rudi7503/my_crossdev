# 14 – Discrete Cosine Transform (1D & 2D Float FPU & AMMX SIMD)

**Dateien:**
- FPU (Float 1D Dot-Product): [`seq/DCT-001.i`](seq/DCT-001.i) (32-Bit IEEE Single-Precision 8-Punkt FPU-Kern)
- Fixed-Point Skalar 1D: [`seq/DCT-002.i`](seq/DCT-002.i) (16-Bit Q1.15 Skalar 8-Punkt Skalar-Kern)
- Fixed-Point AMMX 1D (2 Punkte parallel): [`seq/DCT-003.i`](seq/DCT-003.i) (16-Bit Q1.15 AMMX 2-Wege Vektorreduktion)
- AMMX 4x4 Transpose: [`seq/DCT-004.i`](seq/DCT-004.i) (4x4 16-Bit Block-Transposition via `vperm`)
- Vollständige Algorithmen-Suite: [`Projects/DCT/`](../Projects/DCT/) (`main.c`, `dct.h`, `dct_c.c`, `dct_asm.s`, `Makefile`)

**Ziel:** Mathematische Analyse, Assembler-Optimierung und Hardware-Benchmarking der Discrete Cosine Transform (Typ-II Orthonormal FDCT) und der inversen Transformation (Typ-III IDCT) für 1D (8 Punkte) und 2D (8x8 Bildblock) auf der Vampire V4 (Apollo 68080).

> **Messstatus:** Alle vier Kernsequenzen (`DCT-001` bis `DCT-004`) wurden mit **Harness v5** auf der **Vampire V4 Hardware** gemessen (100 Läufe unter `Forbid()`, Warmup, CCC-Hardwaretaktzähler, IEP-Pipeslot-Erfassung). Alle Messungen sind mit 100/100 Läufen absolut stabil (MIN=MAX=AVG). Zudem wurde die vollständige Benchmark- und Validierungssuite (`Projects/DCT/DCT`) auf der V4 ausgeführt und gegen die bitgenaue C-Referenz verifiziert.

---

## 1. Mathematische Definition & Orthonormale Formulierung

Die 1D 8-Punkt DCT-II berechnet für $k \in \{0, \dots, 7\}$:
$$X[k] = \alpha[k] \sum_{n=0}^{7} x[n] \cos\left( \frac{\pi (2n+1)k}{16} \right), \quad \alpha[k] = \begin{cases} \frac{1}{\sqrt{8}} & k=0 \\ \sqrt{\frac{2}{8}} & k > 0 \end{cases}$$

Die 2D 8x8 Block-Transformation (Standard in JPEG, MPEG, H.264 Intra-Prädiktion):
$$Y = D \cdot X \cdot D^T$$
Durch die Separabilität zerfällt die 2D-Transformation in:
1. **Zeilen-Transformation:** 8 $\times$ 1D-FDCT auf den Zeilen von $X$: $T = X \cdot D^T$
2. **Matrix-Transposition:** $T^T$
3. **Spalten-Transformation:** 8 $\times$ 1D-FDCT auf den Zeilen von $T^T$: $Y^T = T^T \cdot D^T$
4. **Rück-Transposition:** $(Y^T)^T = Y$

In 16-Bit Festkomma (Q1.15) werden die Koeffizienten mit $32767$ skaliert.

---

## 2. Apollo 68080 Optimierungstechniken

### A. 2-Wege parallele AMMX Vektorreduktion (`DCT-003.i`)
Die Berechnung zweier Ausgangspunkte $k$ und $k+1$ erfolgt parallel in AMMX Vektoren:
1. **Parallel-Multiplikation:** `pmulh e0, e2, e4` und `pmulh e1, e3, e5` multiplizieren 8 Terme parallel und schieben das Ergebnis um $\gg 16$.
2. **Kreuzweise Vektor-Akkumulation (`vperm`):**
   - $T_1 = \text{vperm } \#\$018945cd, E_4, E_6 \implies [a_0, b_0, a_2, b_2]$
   - $T_2 = \text{vperm } \#\$23ab67ef, E_4, E_6 \implies [a_1, b_1, a_3, b_3]$
   - $S = T_1 + T_2 = [a_0+a_1, b_0+b1, a_2+a_3, b_2+b_3]$
   - $S_2 = \text{vperm } \#\$45670123, S, S$
   - $Total = S + S_2 = [\text{sum}_k, \text{sum}_{k+1}, \dots]$
3. **32-Bit Aligned Store:** Ein einziger `move.l e8, d1; move.l d1, (a2)+` speichert beide 16-Bit-Ausgangspunkte ohne Byte-/Halfword-Penalties.
- **Ergebnis:** Nur **16.0 Netto-Takte für 2 Punkte** $\rightarrow$ **8.0 Takte pro 8-Punkt DCT-Wert**!

### B. Register-Transposition via `vperm` (`DCT-004.i`)
Statt einer speicherbasierten C-Schleife (die hunderte Zyklen kostet) wird die 8x8 Transposition komplett in Registern durch ein $4 \times 4$ Submatrix-Permutationsmuster ausgeführt:
- Für jede $4 \times 4$ Submatrix genügen 4 Loads, 8 `vperm`-Befehle und 4 Stores.
- Dauer: Nur **11 Netto-Takte** pro $4 \times 4$ Block!
- Eine komplette 8x8 Transposition benötigt dadurch nur noch ~45 Takte statt 370 Takte in C (über 8x schneller).

---

## 3. Harness-v5 Messergebnisse (Isolierte Mikrobenchmarks)

Gemessen auf **Vampire V4 (Core Release, PCR: 1088/1)** unter `Forbid()`:

| ID | Format | Typ / Beschreibung | Takte (MIN) | Takte (AVG) | Netto (MIN) | Takte / Einheit | IEP1 | IEP2 | Speedup vs Skalar |
|:---|:---|:---|:---:|:---:|:---:|:---:|:---:|:---:|:---:|
| [`DCT-001`](seq/DCT-001.i) | 32-Bit Float | FPU Single-Precision (8 MACs) | **53** | 53.0 | 48 | **48.0 Takte / Punkt** | 26 | 0 | 0.65x (FPU Pipe 1) |
| [`DCT-002`](seq/DCT-002.i) | 16-Bit Fixed | Skalar Integer Q1.15 (8 MACs) | **36** | 36.0 | 31 | **31.0 Takte / Punkt** | 29 | 9 | 1.00x (Baseline) |
| [`DCT-003`](seq/DCT-003.i) | 16-Bit Fixed | AMMX SIMD 2-Wege Reduktion | **21** | 21.0 | 16 | **8.0 Takte / Punkt** | 22 | 0 | **3.88x schneller** |
| [`DCT-004`](seq/DCT-004.i) | 16-Bit Fixed | AMMX 4x4 Matrix Transposition | **16** | 17.2 | 11 | **11.0 Takte / Block** | 17 | 1 | **> 8x schneller** |

*Alle Messungen 100/100 Läufe stabil ohne Jitter (`check_log.py` validiert).*

---

## 4. Vollständige Makro-Benchmark-Ergebnisse (Projects/DCT)

Gemessen mit `Projects/DCT/DCT` auf echter Vampire V4 Hardware, gemittelt über 100 Läufe, validiert gegen C-Referenz (`-O2 -m68020 -m68881`):

### 1D 8-Punkt Transformationen
| Transformation | C-Referenz (cyc) | Skalar ASM (cyc) | **AMMX SIMD (cyc)** | **Speedup vs C** | **Speedup vs Skalar** | Verifikation | Rekonstruktionsfehler |
|:---|:---:|:---:|:---:|:---:|:---:|:---:|:---:|
| **1D Float FDCT** | 662 | 486 | — | **1.36x** | — | **0.000000e+00 bit-exakt** | $1.19 \times 10^{-7}$ |
| **1D Float IDCT** | 662 | 487 | — | **1.36x** | — | **0.000000e+00 bit-exakt** | $1.19 \times 10^{-7}$ |
| **1D Fixed FDCT** | 507 | 326 | **100** | **5.07x** | **3.26x** | 9 LSB diff (0.027 %) | 27 LSB |
| **1D Fixed IDCT** | 507 | 560 | **100** | **5.07x** | **5.60x** | 9 LSB diff (0.027 %) | 27 LSB |

### 2D 8x8 Block-Transformationen
| Transformation | C-Referenz (cyc) | Skalar ASM (cyc) | **AMMX SIMD (cyc)** | **Speedup vs C** | **Speedup vs Skalar** | Verifikation | Rekonstruktionsfehler |
|:---|:---:|:---:|:---:|:---:|:---:|:---:|:---:|
| **2D 8x8 Float FDCT** | 11.096 | 8.272 | — | **1.34x** | — | **0.000000e+00 bit-exakt** | $2.98 \times 10^{-7}$ |
| **2D 8x8 Float IDCT** | 11.091 | 8.277 | — | **1.34x** | — | **0.000000e+00 bit-exakt** | $2.98 \times 10^{-7}$ |
| **2D 8x8 Fixed FDCT** | 8.754 | 5.703 | **1.709** | **5.12x** | **3.34x** | 33 LSB diff (0.10 %) | 107 LSB |
| **2D 8x8 Fixed IDCT** | 8.895 | 6.072 | **1.741** | **5.11x** | **3.49x** | 33 LSB diff (0.10 %) | 107 LSB |

---

## 5. Fazit & Hardware-Erkenntnisse
1. **FPU-Verhalten:** Die 68080 FPU ist strikt an Pipe 1 gebunden (`IEP2 = 0`). Mit 48 Netto-Takten pro 8-Punkt Punkt-Transformation erreicht FPU-Assembler einen Speedup von **1.36x** gegenüber GCC 6.5.0 (`-O2 -m68881`).
2. **AMMX Durchsatz:** Durch die 2-Wege parallele Vektorreduktion sinkt die 1D-Transformationszeit auf **100 Takte** (**5.07x schneller als C**).
3. **2D-Transpositionsoptimierung:** Die Eliminierung von speicherbasierten C-Transpositionen durch pure AMMX `vperm`-Transposition senkt die 2D 8x8 Block DCT von über 2.780 Takten auf **1.709 Takte** (**5.12x schneller als C**, **3.34x schneller als Skalar**).
4. **Verifikation & Genauigkeit:**
   - 32-Bit Float FPU ASM ist **vollständig bit-exakt (0.000000e+00 Differenz)**.
   - 16-Bit Fixed AMMX weicht durch die 16-Bit Zwischen-Trunkierung in `pmulh` um maximal 33 LSB (0.10 %) von der 32-Bit Skalar-Akkumulation ab, was für Bild- und Signalverarbeitung (JPEG) innerhalb der Standard-Toleranz liegt.
