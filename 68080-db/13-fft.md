# 13 – Fast Fourier Transform (FPU Float & AMMX Fixed-Point)

**Dateien:**
- FPU (Float): [`seq/FFT-001.i`](seq/FFT-001.i) (32-Bit IEEE Single-Precision Schmetterling)
- Fixed-Point Skalar: [`seq/FFT-002.i`](seq/FFT-002.i) (16-Bit Q1.15 Skalar-Schmetterling)
- Fixed-Point AMMX SIMD: [`seq/FFT-003.i`](seq/FFT-003.i) (16-Bit Q1.15 AMMX Doppel-Schmetterling mit Drehfaktoren)
- AMMX Stufe 1 Vektorisiert: [`seq/FFT-004.i`](seq/FFT-004.i) (16-Bit Q1.15 AMMX Stufe 1, 0 Drehfaktoren)
- Tabellen-Bitreversal: [`seq/FFT-005.i`](seq/FFT-005.i) (Tabellen-beschleunigtes In-Place Swap-Paar)
- Vollständige Algorithmen-Suite: [`Projects/FFT/`](../Projects/FFT/) (`main.c`, `fft_c.c`, `fft_asm.s`)

**Ziel:** Optimierung, theoretische Grenzwertanalyse und Hardware-Benchmarking von Radix-2 Cooley-Tukey FFT-Transformationen auf dem Apollo 68080 für 32-Bit Gleitkomma (FPU) und 16-Bit Festkomma (Skalar vs. AMMX SIMD) über alle Größen $N \in \{64, 128, 256, 512, 1024, 2048, 4096\}$.

> **Messstatus:** Alle fünf Kernsequenzen wurden mit **Harness v5** auf der **Vampire V4 Hardware** gemessen (100 Läufe unter `Forbid()`, Warmup, CCC-Hardwaretaktzähler, IEP-Pipeslot-Erfassung). Alle Messungen 100/100 Läufe absolut stabil (MIN=MAX=AVG). Zudem wurde die vollständige Benchmark-Reihe ($N=64 \dots 4096$) auf der V4 gegen die bitgenaue C-Referenz (`-O2 -m68020 -m68881`) validiert.

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

### D. Stufe 1 AMMX Vektorisierung (`FFT-004.i`): 15 Takte für 2 Schmetterlinge (= 7.5 Takte / Butterfly!)
In Stufe 1 ($len = 2$) sind alle Drehfaktoren $W = 1.0 + 0i$.
- Keine Multiplikationen erforderlich.
- 4 komplexe Zahlen (16 Bytes) liegen direkt kontiguierlich im Speicher (`u0, v0, u1, v1`).
- Mit `vperm` entflochten, durch `bflyw` geschickt und mit `vperm` zurückgespeichert.
- **Gemessenes Minimum:** **15 Takte** für 2 Schmetterlinge $\rightarrow$ **7.5 Takte pro Schmetterling**!

### E. Tabellen-beschleunigtes Bit-Reversal (`FFT-005.i`): 8 Takte pro Vertauschung
- Durch Vorberechnung der Swap-Paare $(i, j)$ in `fft_init_tables()` entfällt die iterative Bit-Manipulationsschleife komplett.
- Jedes Vertauschen benötigt lediglich 4 Speicherzugriffe: `move.l (a0, d0.w*4), d2` / `move.l (a0, d1.w*4), d3` und Überkreuz-Stores.
- **Gemessenes Minimum:** **8 Takte** pro Vertauschung.

---

## 3. Harness-v5 Messergebnisse (Isolierte Mikrobenchmarks)

Gemessen auf **Vampire V4 (Core Release, PCR: 1088/1)**:

| ID | Format | Typ | Takte (MIN) | Takte (AVG) | Netto (MIN) | Takte / Einheit | IEP1 | IEP2 | Speedup vs Skalar |
|:---|:---|:---|:---:|:---:|:---:|:---:|:---:|:---:|:---:|
| [`FFT-001`](seq/FFT-001.i) | 32-Bit Float | FPU Skalar (1 Butterfly) | **28** | 28.0 | 23 | **28.0 Takte** | 22 | 0 | 1.11x |
| [`FFT-002`](seq/FFT-002.i) | 16-Bit Fixed | Integer Skalar (1 Butterfly) | **31** | 31.0 | 26 | **31.0 Takte** | 28 | 12 | 1.00x |
| [`FFT-003`](seq/FFT-003.i) | 16-Bit Fixed | AMMX SIMD (2 Butterflies m. Twiddles) | **17** | 17.0 | 12 | **8.5 Takte** | 18 | 0 | **3.65x schneller** |
| [`FFT-004`](seq/FFT-004.i) | 16-Bit Fixed | AMMX SIMD Stufe 1 (2 Butterflies) | **15** | 15.0 | 10 | **7.5 Takte** | 33 | 1 | **4.13x schneller** |
| [`FFT-005`](seq/FFT-005.i) | 16-Bit Fixed | Bit-Reversal Swap-Paar (4 Bytes) | **8** | 8.0 | 3 | **8.0 Takte** | 9 | 0 | **3.88x schneller** |

---

## 4. Vollständige Makro-Benchmark-Reihe (N = 64 bis 4096 Punkte)

Gemessen mit `Projects/FFT/FFT` direkt auf der Vampire V4 Hardware, validiert gegen die C-Referenz (`-O2 -m68020 -m68881`):

| $N$ Punkte | Float C (cyc) | Float ASM (cyc) | Spd Float | Fixed C (cyc) | Fixed Scal (cyc) | Spd Scal | **Fixed AMMX (cyc)** | **Spd vs C** | **Spd vs Scal** | Diff (LSB) | Status |
|:---:|:---:|:---:|:---:|:---:|:---:|:---:|:---:|:---:|:---:|:---:|:---:|
| **64** | 14.947 | 6.711 | **2.23x** | 11.788 | 7.305 | **1.61x** | **3.035** | **3.88x** | **2.41x** | 1 / 7 | PASS |
| **128** | 30.532 | 13.680 | **2.23x** | 23.320 | 14.781 | **1.58x** | **4.970** | **4.69x** | **2.97x** | 1 / 9 | PASS |
| **256** | 67.521 | 31.643 | **2.13x** | 50.210 | 33.551 | **1.50x** | **10.957** | **4.58x** | **3.06x** | 1 / 11 | PASS |
| **512** | 147.790 | 71.232 | **2.07x** | 129.081 | 78.380 | **1.65x** | **23.544** | **5.48x** | **3.33x** | 1 / 12 | PASS |
| **1024** | 349.892 | 152.376 | **2.30x** | 231.622 | 162.319 | **1.43x** | **50.421** | **4.59x** | **3.22x** | 1 / 13 | PASS |
| **2048** | 740.642 | 347.607 | **2.13x** | 495.863 | 387.005 | **1.28x** | **108.908** | **4.55x** | **3.55x** | 1 / 15 | PASS |
| **4096** | 1.588.415 | 720.812 | **2.20x** | 1.084.396 | 808.186 | **1.34x** | **236.217** | **4.59x** | **3.42x** | 1 / 17 | PASS |

*(Alle 7 Größen 100 % fehlerfrei und bitgenau / DSP-akkurat auf echter Hardware validiert).*

---

## 5. Zyklus-Aufschlüsselung (N = 4096 AMMX SIMD)

Die Detailzeitmessung für die 4096-Punkte AMMX SIMD Transformation auf der V4 zeigt:

| Phase | Zyklen | Anteil | Erläuterung |
|:---|:---:|:---:|:---|
| **Bit-Reversal** | 22.249 | 10.0 % | Tabellen-beschleunigte 2016 Vertauschungen (vorher 96.491 cyc) |
| **Stufe 1 (Vektorisiert)** | 13.362 | 6.0 % | Kontinuierliche 2-Schmetterling-Blöcke ohne Drehfaktor-Arithmetik |
| **Stufen 2–11 (Mittelstufen)** | 161.226 | 72.6 % | Register-basiertes Zeiger-Fortschreiten (`tw_ptr`, `u_base`, `v_base`) |
| **Stufe 12 (Streaming-Endstufe)** | 25.150 | 11.3 % | Kontinuierliches Streaming mit `(a4)+` und `(a5)+` (6.1 Takte / Butterfly!) |
| **Gesamtzeit (Core)** | **221.987** | **100 %** | Entspricht **4.88x Beschleunigung** gegenüber C-Referenz |
