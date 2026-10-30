# 15 – Audio MDCT / IMDCT Engine (MP3 / AAC / Vorbis Subband Decoding)

**Dateien:**
- AMMX Overlap-Add Kernel: [`seq/DCT-009.i`](seq/DCT-009.i) (18-Sample 50% Overlap-Add Kernel via `paddw`)
- Vollständige MP3/AAC Audio-Engine: [`Projects/Audio_MDCT/`](../Projects/Audio_MDCT/) (`main.c`, `mdct.h`, `mdct_c.c`, `mdct_asm.s`, `Makefile`)

**Ziel:** Mathematische Analyse, AMMX-SIMD-Optimierung und Real-Hardware-Benchmarking der **Modified Discrete Cosine Transform (MDCT)** und inversen Transformation (**IMDCT**) für moderne Audio-Codecs (MP3, AAC, Ogg Vorbis, Opus) auf der Vampire V4 (Apollo 68080).

> **Messstatus:** 
> - Kernsequenz `DCT-009` (Overlap-Add) mit **Harness v5** auf echter **Vampire V4 Hardware** gemessen: **18 Netto-Takte** (1,00 Takte/Sample), 100/100 Läufe jitterfrei (`check_log.py` validiert).
> - Vollständige MP3 Subband Engine (`Projects/Audio_MDCT/MDCT`) auf V4 hardware-gebenched: **795 Takte** für kompletten 36-Punkt Windowed IMDCT + Overlap-Add (7,04x schneller als C, **nur 4,58% CPU-Last** für 44,1 kHz Stereo-Echtzeitdekodierung).

---

## 1. Mathematische Grundlagen: MDCT & TDAC

Die MDCT ist eine überlappende, lapped biorthogonale Transformation basierend auf der DCT-IV mit Time-Domain Aliasing Cancellation (TDAC). Sie vermeidet Blockartefakte an Teilbandgrenzen ohne Verdoppelung der Datenrate.

### A. Vorwärts-MDCT ($2N \to N$ Samples)
Für einen Audioblock von $2N$ Abtastwerten (MP3: $N=18$, $2N=36$; AAC Long: $N=1024$, $2N=2048$):
$$X_k = \sum_{n=0}^{2N-1} x_n w_n \cos\left[ \frac{\pi}{N} \left( n + \frac{1}{2} + \frac{N}{2} \right) \left( k + \frac{1}{2} \right) \right], \quad k = 0, \dots, N-1$$

### B. Inverse IMDCT ($N \to 2N$ Samples)
Die Rekonstruktion erzeugt $2N$ zeitliche Abtastwerte aus $N$ Frequenzkoeffizienten:
$$y_i = \sum_{k=0}^{N-1} X_k \cos\left[ \frac{\pi}{N} \left( i + \frac{1}{2} + \frac{N}{2} \right) \left( k + \frac{1}{2} \right) \right], \quad i = 0, \dots, 2N-1$$
gefolgt von der Fenstermultiplikation:
$$z_i = y_i \cdot w_i$$

### C. Princen-Bradley Bedingung & 50% Overlap-Add
Damit das Time-Domain Aliasing exakt zu Null ausgelöscht wird, muss die Fensterfunktion $w_i$ die Princen-Bradley-Bedingung erfüllen:
$$w_i^2 + w_{i+N}^2 = 1$$
Im Standard-MP3-Codec wird das Sinus-Fenster verwendet:
$$w_i = \sin\left[ \frac{\pi}{2N} \left( i + \frac{1}{2} \right) \right]$$
Das finale Audiosignal $s_n$ für Block $m$ ergibt sich durch 50% Overlap-Add:
$$s_n^{(m)} = z_n^{(m)} + z_{n+N}^{(m-1)}, \quad n = 0, \dots, N-1$$

---

## 2. Apollo 68080 AMMX SIMD Optimierungsstrategie

### A. Fused Windowed Cosine Table (Verschmelzung von IMDCT und Fenster)
Anstatt erst 36 Samples IMDCT zu berechnen und anschließend mit 36 Sinus-Werten zu multiplizieren (2 Durchläufe), werden Kosinus-Basismatrix und Sinus-Fenster vorab zu einer einzigen Matrix verschmolzen:
$$M_{i, k}^{\text{fused}} = w_i \cdot \cos\left[ \frac{\pi}{18} \left( i + \frac{19}{2} \right) \left( k + \frac{1}{2} \right) \right]$$
Dadurch entfallen alle separaten Multiplikationen für die Fensterung vollständig!

### B. 2-Zeilen Parallele Vektorreduktion
- 18 Frequenzkoeffizienten werden auf 20 Wörter (5 Quadwords) aufgefüllt und dauerhaft in AMMX-Registern `e16..e20` gehalten.
- Die Matrix berechnet jeweils **2 Zeilen simultan** ($2j$ und $2j+1$):
  - 5x `load` + 5x `pmulh` für Zeile $2j \to$ Summenvektor `e1`
  - 5x `load` + 5x `pmulh` für Zeile $2j+1 \to$ Summenvektor `e10`
- **Horizontale Kreuzaddition via `vperm`:**
  - `vperm #$018945cd, e1, e10, e11` mischt Wörter aus beiden Zeilen
  - `vperm #$23ab67ef, e1, e10, e12`
  - `paddw e12, e11, e11`
  - `vperm #$45670123, e11, e11, e13`
  - `paddw e13, e11, e11` $\to$ `e11 = [sum_a, sum_b, sum_a, sum_b]`
- **Zero-Shift 32-Bit Extraktion:** `move.l e11, d0` liest direkt die beiden fertigen 16-Bit Abtastwerte `[sum_a, sum_b]`!

### C. Fused Zero-Buffer Pipeline
Die ersten 18 Samples werden sofort mit dem `prev18`-Puffer addiert und direkt in den Ausgabepuffer `out18` geschrieben. Die zweiten 18 Samples werden direkt in den `prev18`-Puffer für den nächsten Block gespeichert. Es wird **kein temporärer Speicher** im Fast-RAM benötigt.

---

## 3. Harness-v5 Messergebnisse (Isolierter Mikrobenchmark)

Gemessen auf **Vampire V4 (Core Release, PCR: 1088/1)** unter `Forbid()`:

| Sequenz | Format | Operation | Brutto Min | Brutto Avg | Baseline | Netto Min | Netto Takt/Sample | Anz. Befehle | Bubbles | Speedup |
| :--- | :--- | :--- | :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: |
| [`DCT-009`](seq/DCT-009.i) | 16-Bit Fixed | AMMX 18-Sample 50% Overlap-Add | **23** | 25.0 | 5 | **18** | **1.00 Takte / Sample** | 10 | 0 | **> 4.5x schneller** |

*Messung 100/100 Läufe stabil (`check_log.py` validiert).*

---

## 4. Vollständige Audio-Engine Benchmarks (Vampire V4 Hardware)

Gemessen mit `Projects/Audio_MDCT/MDCT` auf echter Vampire V4 Hardware, gemittelt über jeweils 200 Durchläufe:

| Teilkomponente | C-Referenz (GCC -O2) | 68080 Assembler / AMMX | Speedup | Genauigkeit (LSB) | Status |
| :--- | :---: | :---: | :---: | :---: | :---: |
| **Part 1: 32-Bit Float IMDCT-36** | 5.877 Takte | **5.688 Takte** (FPU) | 1.03x | 0.000000e+00 | **PASS** |
| **Part 2: 16-Bit Fixed IMDCT-36** | 5.415 Takte | **740 Takte** (AMMX) | **7.32x** | max 26 LSB | **PASS** |
| **Part 3: 50% Overlap-Add (18 Samples)** | 86 Takte | **41 Takte** (AMMX) | **2.10x** | 26 LSB | **PASS** |
| **Part 4: Fused Complete MP3 Engine** | 5.593 Takte | **795 Takte** (Fused AMMX) | **7.04x** | max 50 LSB | **PASS** |

### Pipeline-Auslastung & Superskalarität (Harness v5: IEP1, IEP2, IEP3)
Gemessen auf **Vampire V4 Hardware (Core 1088/1 @ 85 MHz)** mit den Zählern `IEP1` (Anton), `IEP2` (Bruno), `IEP3` (Cesar) und DCH/DCM:

| MDCT Teilkomponente | Takte (CCC) | Befehle gesamt | IPC | Anton (`IEP1`) | Bruno (`IEP2`) | Cesar (`IEP3`) | D-Cache Hit |
|---|---|---|---|---|---|---|---|
| **36-pt IMDCT Float (FPU ASM)** | 5.827 | 2.899 | **0,50** | 75,0 % | **25,0 %** | 0,0 % | 100,0 % |
| **36-pt IMDCT Fixed (AMMX PMULLW)** | 741 | 727 | **0,98** | 97,1 % | 2,9 % | 0,0 % | 100,0 % |
| **Overlap-Add (AMMX PADDW)** | 42 | 34 | **0,81** | 100,0 % | 0,0 % | 0,0 % | 100,0 % |
| **Fused Windowed IMDCT+OLA (AMMX)** | 796 | 804 | **1,01** | 93,9 % | **6,1 %** | 0,0 % | 100,0 % |

### Real-Time CPU-Last Budget für 44.1 kHz Stereo MP3:
- **Audio-Budget:** 1 Sekunde MP3 Stereo benötigt $44.100 / 1152 \times 128 = \mathbf{4.900\text{ IMDCTs/Sekunde}}$.
- Bei ~85 MHz effektiver 68080-Taktung:
  - **C-Referenz:** $4.900 \times 5.593 \text{ Takte} \approx 27.4\text{ MIPS} \implies \mathbf{32.24\% \text{ CPU-Last}}$
  - **Fused AMMX:** $4.900 \times 795 \text{ Takte} \approx 3.9\text{ MIPS} \implies \mathbf{4.58\% \text{ CPU-Last}}$
- **Ergebnis:** Die CPU-Last für die rechenintensivste Stufe des MP3-Decoders sinkt von einem Drittel des gesamten Rechners auf praktisch unmerkliche **4.58%**!
