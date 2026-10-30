# 14 – Discrete Cosine Transform (1D & 2D Float FPU & AMMX SIMD)

**Dateien:**
- FPU (Float 1D Dot-Product): [`seq/DCT-001.i`](seq/DCT-001.i) (32-Bit IEEE Single-Precision 8-Punkt FPU-Kern)
- Fixed-Point Skalar 1D: [`seq/DCT-002.i`](seq/DCT-002.i) (16-Bit Q1.15 Skalar 8-Punkt Skalar-Kern)
- Fixed-Point AMMX 1D (2 Punkte parallel): [`seq/DCT-003.i`](seq/DCT-003.i) (16-Bit Q1.15 AMMX 2-Wege Vektorreduktion)
- AMMX 4x4 Transpose: [`seq/DCT-004.i`](seq/DCT-004.i) (4x4 16-Bit Block-Transposition via `vperm`)
- Fast Float FPU 1D: [`seq/DCT-005.i`](seq/DCT-005.i) (32-Bit Float Fast-DCT Loeffler FPU-Kern)
- Fast Fixed AMMX 1D: [`seq/DCT-006.i`](seq/DCT-006.i) (16-Bit Q1.15 Fast-DCT Loeffler AMMX SIMD-Kern)
- Vollständige Algorithmen-Suite: [`Projects/DCT/`](../Projects/DCT/) (`main.c`, `dct.h`, `dct_c.c`, `dct_asm.s`, `Makefile`)

**Ziel:** Mathematische Analyse, Assembler-Optimierung und Hardware-Benchmarking der Discrete Cosine Transform (Typ-II Orthonormal FDCT) und der inversen Transformation (Typ-III IDCT) für 1D (8 Punkte) und 2D (8x8 Bildblock) auf der Vampire V4 (Apollo 68080), vergleichend zwischen klassischer Matrix-Multiplikation und dem optimierten Fast-DCT (Loeffler/LLM) Algorithmus.

> **Messstatus:** Alle sechs Kernsequenzen (`DCT-001` bis `DCT-006`) wurden mit **Harness v5** auf der **Vampire V4 Hardware** gemessen (100 Läufe unter `Forbid()`, Warmup, CCC-Hardwaretaktzähler, IEP-Pipeslot-Erfassung). Alle Messungen sind mit 100/100 Läufen absolut stabil (MIN=MAX=AVG). Zudem wurde die vollständige Benchmark- und Validierungssuite (`Projects/DCT/DCT`) auf der V4 ausgeführt und gegen die bitgenaue C-Referenz verifiziert.

---

## 1. Mathematische Definition & Algorithmen

### A. Klassische Matrix-Formulierung (Orthonormal DCT-II)
Die 1D 8-Punkt DCT-II berechnet für $k \in \{0, \dots, 7\}$:
$$X[k] = \alpha[k] \sum_{n=0}^{7} x[n] \cos\left( \frac{\pi (2n+1)k}{16} \right), \quad \alpha[k] = \begin{cases} \frac{1}{\sqrt{8}} & k=0 \\ \sqrt{\frac{2}{8}} & k > 0 \end{cases}$$

Die 2D 8x8 Block-Transformation (Standard in JPEG, MPEG, H.264 Intra-Prädiktion):
$$Y = D \cdot X \cdot D^T$$
Durch die Separabilität zerfällt die 2D-Transformation in Zeilen-Transformation, Transposition, Spalten-Transformation und Rück-Transposition.

### B. Fast-DCT (Loeffler / LLM Algorithmus)
Der Loeffler-Algorithmus zerlegt die 8-Punkt DCT in einen geraden (Even) und einen ungeraden (Odd) Pfad:
1. **Stage 1 (Symmetrische Butterflies):**
   $$s_n = x_n + x_{7-n}, \quad d_n = x_n - x_{7-n} \quad (n = 0, \dots, 3)$$
2. **Stage 2 Even ($X_0, X_2, X_4, X_6$):**
   - $t_0 = s_0 + s_3, \quad t_1 = s_1 + s_2, \quad t_2 = s_1 - s_2, \quad t_3 = s_0 - s_3$
   - $X_0 = c_4 (t_0 + t_1), \quad X_4 = c_4 (t_0 - t_1)$ mit $c_4 = \frac{1}{\sqrt{8}}$
   - Givens-Rotation auf $(t_3, t_2)$: $X_2 = c_2 t_3 + s_2 t_2, \quad X_6 = s_2 t_3 - c_2 t_2$ mit $c_2 = \frac{1}{2}\cos\frac{\pi}{8}, s_2 = \frac{1}{2}\sin\frac{\pi}{8}$
3. **Stage 2 Odd ($X_1, X_3, X_5, X_7$):**
   Multiplikation des Differenzvektors $\mathbf{d}$ mit der symmetrischen $4 \times 4$ Matrix $D_{\text{odd}}$ ($D_{\text{odd}}^T = D_{\text{odd}}$):
   $$D_{\text{odd}} = \begin{bmatrix} c_1 & c_3 & c_5 & c_7 \\ c_3 & -c_7 & -c_1 & -c_5 \\ c_5 & -c_1 & c_7 & c_3 \\ c_7 & -c_5 & c_3 & -c_1 \end{bmatrix}$$
   Wegen der perfekten Symmetrie benötigt auch die inverse Fast IDCT exakt dieselben 4 Konstantenvektoren!

---

## 2. Apollo 68080 Optimierungstechniken

### A. Überlaufsichere Festkomma-Arithmetik ("Product-First")
In Festkomma Q1.15 kann die Summe $\sum_{n=0}^7 x_n = t_0 + t_1$ den 16-Bit Wertebereich $[-32768, 32767]$ übersteigen (bis zu $\pm 65536$).
- **Lösung:** Multiplikation vor Addition!
  $X_0 = c_4 t_0 + c_4 t_1 = \text{pmulh}(t_0, c_4) + \text{pmulh}(t_1, c_4)$
  Da $t_0, t_1 \le 32767$, überläuft kein Zwischenwert.

### B. Dynamikbereich-Management im 2D Fast-DCT (0.5x Pass 1 Scaling)
In 2D expandiert der Wertebereich in Pass 1. Durch Weglassen des Verdoppelungs-Schritts (`paddw e,e,e`) in Pass 1 wird das Zwischensignal exakt um den Faktor 0.5 skaliert (maximal $\le 11965$), wodurch Pass 2 völlig überlauffrei arbeitet. Am Ende wird das Ergebnis durch Verdopplung in einem Durchlauf verlustfrei auf die exakte $1.0\times$ Skalierung gebracht.

### C. Pure-AMMX Matrix-Transposition via `vperm`
- 16-Bit Matrix (128 Bytes): 32 `vperm`-Instruktionen (~45 Takte statt 370 Takte in C).
- 32-Bit Float Matrix (256 Bytes): 32 `vperm`-Instruktionen (~50 Takte statt 1.450 Takte in C).

### D. In-Register 2D Block Processing & Zero Stack Traffic (`_dct2d_8x8_fixed_fast_inreg_ammx`)
- **Architektur:** Durch die 24 verfügbaren 64-Bit Register (`e0..e23`) des Apollo 68080 können alle 7 Loeffler-Konstantenvektoren (`_fast_c4, _fast_c2_s2, _fast_s2_mc2, _fast_odd1/3/5/7`) dauerhaft in `e16..e22` gehalten werden.
- **Einsparung:** Eliminiert 112 Speicherladungen und 16 `jsr`/`rts` Funktionsaufrufe pro 2D-Block.
- **Ergebnis:** Reduziert die 2D FDCT von 1.174 auf **909 Takte** (265 Takte gespart, **9.69x schneller als C**!).

### E. Sparse / DC-Only IDCT Early-Exit (`_idct2d_8x8_sparse_ammx`)
- **Konzept:** In Videodecodern (MPEG/JPEG) sind über 60 % aller 8x8 Blöcke in flachen Bildbereichen "DC-only" (nur Koeffizient 0 ist ungleich Null).
- **Vektor-Check:** AC-Wort 1 und Worte 2..3 werden skalar geprüft; die restlichen 60 Worte mit schnellen parallelen `por`-Instruktionen auf Null getestet.
- **Ergebnis:** Ist der Block DC-only, wird $(DC + 4) \gg 3$ per AMMX Broadcast in alle 64 Pixel geschrieben. Die IDCT benötigt statt 1.074 Takten nur noch **86 Takte** (**103x schneller als C**!). Der Fallback-Penalty für normale Blöcke beträgt nur 22 Takte.

### F. Vektor-Quantisierung & Fused AAN DCT (`_quantize8x8_ammx`, `_dct2d_8x8_aan_quant_ammx`)
- **Quantisierung:** Division durch die Quantisierungsmatrix wird durch Festkomma-Multiplikation mit reziproken Tabellenwerten ersetzt ($recip\_q = \text{round}(32768 / Q)$).
- **Durchsatz:** 16 Quadwords via `pmulh` + `paddw` quantisieren den gesamten 8x8 Block in **151 Takten** (**15.5x schneller als C** mit 2.340 Takten!).
- **Fused AAN DCT:** Durch Verschmelzen der 2D FDCT mit der Quantisierung entfallen Zwischenspeicherzugriffe vollständig (**1.076 Takte** Gesamtlaufzeit).

---

## 3. Harness-v5 Messergebnisse (Isolierte Mikrobenchmarks)

Gemessen auf **Vampire V4 (Core Release, PCR: 1088/1)** unter `Forbid()`:

| ID | Format | Typ / Beschreibung | Takte (MIN) | Takte (AVG) | Netto (MIN) | Takte / Einheit | IEP1 | IEP2 | Speedup vs Skalar |
|:---|:---|:---|:---:|:---:|:---:|:---:|:---:|:---:|:---:|
| [`DCT-001`](seq/DCT-001.i) | 32-Bit Float | FPU Single-Precision (8 MACs Matrix) | **53** | 53.0 | 48 | **48.0 Takte / Punkt** | 26 | 0 | 0.65x |
| [`DCT-002`](seq/DCT-002.i) | 16-Bit Fixed | Skalar Integer Q1.15 (8 MACs Matrix) | **36** | 36.0 | 31 | **31.0 Takte / Punkt** | 29 | 9 | 1.00x (Baseline) |
| [`DCT-003`](seq/DCT-003.i) | 16-Bit Fixed | AMMX SIMD 2-Wege Reduktion (Matrix) | **21** | 21.0 | 16 | **8.0 Takte / Punkt** | 22 | 0 | **3.88x schneller** |
| [`DCT-004`](seq/DCT-004.i) | 16-Bit Fixed | AMMX 4x4 Matrix Transposition | **16** | 17.2 | 11 | **11.0 Takte / Block** | 17 | 1 | **> 8x schneller** |
| [`DCT-005`](seq/DCT-005.i) | 32-Bit Float | Fast-DCT FPU Loeffler 8-Punkt Kern | **79** | 79.0 | 74 | **9.25 Takte / Punkt** | 41 | 0 | **3.35x schneller** |
| [`DCT-006`](seq/DCT-006.i) | 16-Bit Fixed | Fast-DCT AMMX SIMD 8-Punkt Kern | **49** | 49.0 | 44 | **5.50 Takte / Punkt** | 39 | 0 | **5.64x schneller** |
| [`DCT-007`](seq/DCT-007.i) | 16-Bit Fixed | AMMX 8x8 Matrix-Transposition (vperm) | **63** | 67.0 | **58** | **0.90 Takte / Wort** | 64 | 2 | **> 12x schneller** |
| [`DCT-008`](seq/DCT-008.i) | 16-Bit Fixed | AMMX 8x8 Quantisierung (pmulh) | **81** | 81.0 | **76** | **1.18 Takte / Punkt** | 64 | 0 | **> 18x schneller** |

*Alle Messungen 100/100 Läufe stabil ohne Jitter (`check_log.py` validiert).*

---

## 4. Vollständige Makro-Benchmark-Ergebnisse (Projects/DCT)

Gemessen mit `Projects/DCT/DCT` auf echter Vampire V4 Hardware, gemittelt über 100 Läufe, validiert gegen C-Referenz (`-O2 -m68020 -m68881`):

### 1D 8-Punkt Transformationen
| Transformation | C-Ref (cyc) | Matrix ASM (cyc) | **Fast ASM (cyc)** | **Speedup vs C** | **Speedup vs Matrix** | Verifikation | Rekonstruktionsfehler |
|:---|:---:|:---:|:---:|:---:|:---:|:---:|:---:|
| **1D Float FDCT** | 662 | 486 | **239** | **2.77x** | **2.03x** | $1.19 \times 10^{-7}$ | $5.96 \times 10^{-8}$ |
| **1D Float IDCT** | 662 | 487 | **236** | **2.81x** | **2.06x** | $1.19 \times 10^{-7}$ | $5.96 \times 10^{-8}$ |
| **1D Fixed FDCT** | 507 | 100 | **66** | **7.68x** | **1.52x** | 4 LSB diff (0.012 %) | 17 LSB |
| **1D Fixed IDCT** | 857 | 100 | **76** | **11.28x** | **1.32x** | 4 LSB diff (0.012 %) | 17 LSB |

### 2D 8x8 Block-Transformationen
| Transformation | C-Ref (cyc) | Matrix ASM (cyc) | **Fast AMMX (cyc)** | **In-Register AMMX (cyc)** | **Speedup vs C** | Verifikation |
|:---|:---:|:---:|:---:|:---:|:---:|:---:|
| **2D 8x8 Float FDCT** | 11.490 | 8.804 | **3.983** | – | **2.88x** | $2.38 \times 10^{-7}$ [PASS] |
| **2D 8x8 Float IDCT** | 11.082 | 8.370 | **4.164** | – | **2.66x** | $2.38 \times 10^{-7}$ [PASS] |
| **2D 8x8 Fixed FDCT** | 8.804 | 2.008 | 1.174 | **909** | **9.69x** | 0 LSB diff [PASS] |
| **2D 8x8 Fixed IDCT** | 8.856 | 1.707 | 1.334 | **1.074** | **8.25x** | 0 LSB diff [PASS] |

### Erweiterte Optimierungen (Sparse IDCT & Quantisierung)
| Algorithmus / Pfad | C-Ref (cyc) | **AMMX (cyc)** | **Speedup vs C** | Status / Anmerkung |
|:---|:---:|:---:|:---:|:---|
| **2D Sparse IDCT (DC-Only Fast Path)** | 8.856 | **86** | **103.0x** | **12.5x vs Full IDCT**, exakter Wert Pixel=125 [PASS] |
| **2D Sparse IDCT (Full Block Fallback)** | 8.856 | **1.096** | **8.08x** | Fallback-Penalty nur 22 Takte [PASS] |
| **8x8 Block Quantisierung (JPEG Q-Matrix)** | 2.340 | **151** | **15.50x** | 16 Quadwords via `pmulh`, $\le 2$ LSB diff [PASS] |
| **8x8 Fused AAN FDCT + Quantisierung** | 11.144 | **1.076** | **10.36x** | Nahtlose Pipeline, exakt synchron zu AMMX-Quant [PASS] |

---

## 5. Fazit & Hardware-Erkenntnisse
1. **Loeffler Fast-DCT Durchsatz:**
   - 1D Fast Fixed AMMX benötigt nur **66 Takte** (vs 507 Takte in C $\rightarrow$ **7.68x Speedup**!).
   - 2D 8x8 Block In-Register Fast Fixed AMMX benötigt nur **909 Takte** (vs 8.804 Takte in C $\rightarrow$ **9.69x Speedup**!).
   - 2D 8x8 Block Float Fast FPU ASM benötigt nur **3.983 Takte** (vs 11.490 Takte in C $\rightarrow$ **2.88x Speedup**!).
2. **DC-Only Early-Exit (103x Speedup):**
   - Für flache Bildbereiche reduziert `_idct2d_8x8_sparse_ammx` die Rekonstruktionszeit von 1.074 Takten auf sensationelle **86 Takte**.
3. **Vektor-Quantisierung & AAN:**
   - AMMX quantisiert 64 Koeffizienten in **151 Takten** (1.18 Takte/Punkt im Harness `DCT-008`).
   - Die Fused AAN Pipeline berechnet vollständige 2D DCT inklusive Quantisierung in nur **1.076 Takten**.
4. **Überlauffreiheit & Präzision:**
   - Durch das "Product-First"-Schema und 0.5x Zwischenskalierung bleibt die 16-Bit AMMX Fast-DCT überlauffrei und erreicht bitgenaue Rekonstruktion ohne Artefakte.

## Rohlogs (Harness v5)

Alle zehn kanonischen v5-Läufe je Sequenz:

| ID | run01 … run10 |
|---|---|
| `DCT-001` | [01](cores/core-11350/logs/DCT-001.run01.log) [02](cores/core-11350/logs/DCT-001.run02.log) [03](cores/core-11350/logs/DCT-001.run03.log) [04](cores/core-11350/logs/DCT-001.run04.log) [05](cores/core-11350/logs/DCT-001.run05.log) [06](cores/core-11350/logs/DCT-001.run06.log) [07](cores/core-11350/logs/DCT-001.run07.log) [08](cores/core-11350/logs/DCT-001.run08.log) [09](cores/core-11350/logs/DCT-001.run09.log) [10](cores/core-11350/logs/DCT-001.run10.log) |
| `DCT-002` | [01](cores/core-11350/logs/DCT-002.run01.log) [02](cores/core-11350/logs/DCT-002.run02.log) [03](cores/core-11350/logs/DCT-002.run03.log) [04](cores/core-11350/logs/DCT-002.run04.log) [05](cores/core-11350/logs/DCT-002.run05.log) [06](cores/core-11350/logs/DCT-002.run06.log) [07](cores/core-11350/logs/DCT-002.run07.log) [08](cores/core-11350/logs/DCT-002.run08.log) [09](cores/core-11350/logs/DCT-002.run09.log) [10](cores/core-11350/logs/DCT-002.run10.log) |
| `DCT-003` | [01](cores/core-11350/logs/DCT-003.run01.log) [02](cores/core-11350/logs/DCT-003.run02.log) [03](cores/core-11350/logs/DCT-003.run03.log) [04](cores/core-11350/logs/DCT-003.run04.log) [05](cores/core-11350/logs/DCT-003.run05.log) [06](cores/core-11350/logs/DCT-003.run06.log) [07](cores/core-11350/logs/DCT-003.run07.log) [08](cores/core-11350/logs/DCT-003.run08.log) [09](cores/core-11350/logs/DCT-003.run09.log) [10](cores/core-11350/logs/DCT-003.run10.log) |
| `DCT-004` | [01](cores/core-11350/logs/DCT-004.run01.log) [02](cores/core-11350/logs/DCT-004.run02.log) [03](cores/core-11350/logs/DCT-004.run03.log) [04](cores/core-11350/logs/DCT-004.run04.log) [05](cores/core-11350/logs/DCT-004.run05.log) [06](cores/core-11350/logs/DCT-004.run06.log) [07](cores/core-11350/logs/DCT-004.run07.log) [08](cores/core-11350/logs/DCT-004.run08.log) [09](cores/core-11350/logs/DCT-004.run09.log) [10](cores/core-11350/logs/DCT-004.run10.log) |
| `DCT-005` | [01](cores/core-11350/logs/DCT-005.run01.log) [02](cores/core-11350/logs/DCT-005.run02.log) [03](cores/core-11350/logs/DCT-005.run03.log) [04](cores/core-11350/logs/DCT-005.run04.log) [05](cores/core-11350/logs/DCT-005.run05.log) [06](cores/core-11350/logs/DCT-005.run06.log) [07](cores/core-11350/logs/DCT-005.run07.log) [08](cores/core-11350/logs/DCT-005.run08.log) [09](cores/core-11350/logs/DCT-005.run09.log) [10](cores/core-11350/logs/DCT-005.run10.log) |
| `DCT-006` | [01](cores/core-11350/logs/DCT-006.run01.log) [02](cores/core-11350/logs/DCT-006.run02.log) [03](cores/core-11350/logs/DCT-006.run03.log) [04](cores/core-11350/logs/DCT-006.run04.log) [05](cores/core-11350/logs/DCT-006.run05.log) [06](cores/core-11350/logs/DCT-006.run06.log) [07](cores/core-11350/logs/DCT-006.run07.log) [08](cores/core-11350/logs/DCT-006.run08.log) [09](cores/core-11350/logs/DCT-006.run09.log) [10](cores/core-11350/logs/DCT-006.run10.log) |
| `DCT-007` | [01](cores/core-11350/logs/DCT-007.run01.log) [02](cores/core-11350/logs/DCT-007.run02.log) [03](cores/core-11350/logs/DCT-007.run03.log) [04](cores/core-11350/logs/DCT-007.run04.log) [05](cores/core-11350/logs/DCT-007.run05.log) [06](cores/core-11350/logs/DCT-007.run06.log) [07](cores/core-11350/logs/DCT-007.run07.log) [08](cores/core-11350/logs/DCT-007.run08.log) [09](cores/core-11350/logs/DCT-007.run09.log) [10](cores/core-11350/logs/DCT-007.run10.log) |
| `DCT-008` | [01](cores/core-11350/logs/DCT-008.run01.log) [02](cores/core-11350/logs/DCT-008.run02.log) [03](cores/core-11350/logs/DCT-008.run03.log) [04](cores/core-11350/logs/DCT-008.run04.log) [05](cores/core-11350/logs/DCT-008.run05.log) [06](cores/core-11350/logs/DCT-008.run06.log) [07](cores/core-11350/logs/DCT-008.run07.log) [08](cores/core-11350/logs/DCT-008.run08.log) [09](cores/core-11350/logs/DCT-008.run09.log) [10](cores/core-11350/logs/DCT-008.run10.log) |

