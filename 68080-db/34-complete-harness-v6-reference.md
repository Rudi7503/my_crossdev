# Gesamtreferenz: Apollo 68080 Benchmark-Suite (Harness v6)

**Hardware-Target**: Vampire V4 Standalone  
**CPU-Core**: Apollo 68080 Core 11350 (PCR 1088/1) @ 92,0 MHz  
**Messharness**: Harness v6 (10-fach unrolled innere Schleife, 100 Messungen, Forbid-Isolation)  
**Baseline-Schleifenoverhead**: `BASE_10 = 30` Takte (3,0 Takte / Schleifendurchlauf)  
**Hardware-Counter**: `CCC` (Taktzyklen), `IEP1` (Decoder Pipe 1 / Anton), `IEP2` (Decoder Pipe 2 / Bruno), `IEP3` (Pipe 3 / Cesar)

---

## 1. NOP-Kalibrierung & Pipeline-Verhalten (1 bis 9 NOPs)

Kalibrierung zur Bestimmung von Schleifenüberlappung, Sprungvorhersage und Einzeltakt-Instruktionskosten.

| Test-ID | Instruktionsfolge | RAW_10 | Netto (10x) | Takte / Durchlauf | Takte / NOP | Pipe 1 (Anton) | Pipe 2 (Bruno) | Pipe 3 (Cesar) |
|:---|:---|:---:|:---:|:---:|:---:|:---:|:---:|:---:|
| **`NOP-001`** | `1x nop` | 86 | +56 | **5,6 Cyc** | **5,6 Cyc** | **100 %** | 0 % | 0 % |
| **`NOP-002`** | `2x nop` | 146 | +116 | **11,6 Cyc** | **5,8 Cyc** | **100 %** | 0 % | 0 % |
| **`NOP-003`** | `3x nop` | 206 | +176 | **17,6 Cyc** | **5,9 Cyc** | **100 %** | 0 % | 0 % |
| **`NOP-004`** | `4x nop` | 266 | +236 | **23,6 Cyc** | **5,9 Cyc** | **100 %** | 0 % | 0 % |
| **`NOP-005`** | `5x nop` | 326 | +296 | **29,6 Cyc** | **5,9 Cyc** | **100 %** | 0 % | 0 % |
| **`NOP-006`** | `6x nop` | 386 | +356 | **35,6 Cyc** | **5,9 Cyc** | **100 %** | 0 % | 0 % |
| **`NOP-007`** | `7x nop` | 446 | +416 | **41,6 Cyc** | **5,9 Cyc** | **100 %** | 0 % | 0 % |
| **`NOP-008`** | `8x nop` | 506 | +476 | **47,6 Cyc** | **6,0 Cyc** | **100 %** | 0 % | 0 % |
| **`NOP-009`** | `9x nop` | 566 | +536 | **53,6 Cyc** | **6,0 Cyc** | **100 %** | 0 % | 0 % |

> [!NOTE]
> Jeder zusätzliche `NOP` fügt der 10er-Schleife **exakt 60 Zyklen** hinzu ($+6,0$ Zyklen pro NOP). `NOP` fungiert im 68080 als Barrierebefehl und läuft zu 100% über Decoder Pipe 1 (Anton). Die $5,6$ Takte bei `NOP-001` resultieren aus der Überlappung des `dbra`-Sprungbefehls mit dem vorangegangenen NOP (2,6 Cyc statt 3,0 Cyc im leeren Baseline-Zustand).

---

## 2. AMMX Butterfly-Instruktionen (`bflyw` / `bflyb`)

Dedizierte SIMD-Butterfly-Operationen für IDCT, DCT, FFT und Wavelets:
* `bflyw <vea>, b, d:d+1`: Berechnet simultan $d = b + a$ und $d+1 = b - a$ für 4 Words (16-Bit).
* `bflyb <vea>, b, d:d+1`: Berechnet simultan $d = b + a$ und $d+1 = b - a$ für 8 Bytes (8-Bit).

| Test-ID | Instruktionsfolge | RAW_10 | Netto-Takte / Durchlauf | Pipe 1 (Anton) | Pipe 2 (Bruno) | Vorteil / Architektonische Bedeutung |
|:---|:---|:---:|:---:|:---:|:---:|:---|
| **`BFLY-001`** | **`bflyw e2, e8, e0:e1`** | **26** | **2,6 Cyc** (inc. Loop) | 0 % | **100 %** | **-1,0 Takt schneller** vs. getrenntes Add+Sub |
| `BFLY-002` | `paddw e2, e8, e0` + `psubw e2, e8, e1` | 36 | 3,6 Cyc (inc. Loop) | **50 %** | **50 %** | Referenz: Perfektes Dual-Issue Pairing |
| **`BFLY-003`** | **`bflyw (a0), e8, e0:e1`** | **26** | **2,6 Cyc** (inc. Loop) | 0 % | **100 %** | **-2,0 Takte schneller**: Speicher-Load wird mit 0 Takten gefaltet! |
| `BFLY-004` | `load (a0), e2` + `paddw` + `psubw` | 46 | 4,6 Cyc (inc. Loop) | 76 % | 24 % | Getrenntes Laden und Berechnen |
| **`BFLY-005`** | **`bflyb e2, e8, e0:e1`** | **26** | **2,6 Cyc** (inc. Loop) | 0 % | **100 %** | 8 parallele 8-Bit-Butterflies in 1 Takt |
| `BFLY-006` | `paddb` + `psubb` | 36 | 3,6 Cyc (inc. Loop) | **50 %** | **50 %** | Getrennte Byte-Vektor-Arithmetik |

---

## 3. Speicher-Durchsatz & Bandbreite (`MEM`)

Gemessen auf physikalischem FastRAM bei 92,0 MHz CPU-Takt.

| Test-ID | Operation | Blockgröße / Durchlauf | Netto-Takte | Takte / Byte | Nutzdaten-Durchsatz | Bus-Transferrate |
|:---|:---|:---:|:---:|:---:|:---:|:---:|
| **`MEM-010`** | Fast MEMSET (Fill, 4x `store`) | 32 Bytes | 2,6 Cyc | **0,081 c/B** | **1.132,3 MByte/s** | **1.132,3 MByte/s** |
| **`MEM-011`** | Fast MEMCPY (Copy, 4x `load` + 4x `store`) | 32 Bytes | 6,6 Cyc | **0,206 c/B** | **446,1 MByte/s** | **892,1 MByte/s** |
| **`MEM-012`** | Tail Mask-Store (`storeilm`) | 8 Bytes | 1,6 Cyc | **0,200 c/B** | **460,0 MByte/s** | **920,0 MByte/s** |
| **`MEM-013`** | MEMMOVE Streaming (`2x load` + `2x store`) | 16 Bytes | 2,6 Cyc | **0,163 c/B** | **566,2 MByte/s** | **1.132,3 MByte/s** |
| **`MEM-020`** | Cacheline Burst MEMSET (8x `store`) | 64 Bytes | 6,6 Cyc | **0,103 c/B** | **892,1 MByte/s** | **892,1 MByte/s** |
| **`MEM-021`** | Cacheline Burst MEMCPY (8x `load` + 8x `store`)| 64 Bytes | 14,6 Cyc | **0,228 c/B** | **403,3 MByte/s** | **806,6 MByte/s** |

---

## 4. Branchless Binary Search Trees (`BST`)

Vergleich von Pointer-basierten Suchbäumen gegen AMMX SIMD und flache Eytzinger-Arraybäume.

| Test-ID | Implementierung | Netto-Takte | **Takte / Suchschritt** | Speedup | Vorteil |
|:---|:---|:---:|:---:|:---:|:---|
| `BST-001` | Klassischer Branchless Pointer-Tree | 75 | **7,5 Cyc / Schritt** | 1,00× | Referenz |
| `BST-002` | Base-Displacement Trick `4(a0, d2.w)` | 55 | **5,5 Cyc / Schritt** | **1,36×** | Spart Adress-Add |
| `BST-003` | Dual-EA Parallel Load | 96 | **9,6 Cyc / Schritt** | 0,78× | Bus-Kollision |
| `BST-004` | AMMX 4-Way SIMD B-Tree Node | 16 | **0,4 Cyc / Key** | **4,69×** | 4 Vergleiche parallel in 1,6 Cyc |
| `BST-005` | Ultra-Compact `subx.w` + Scaler `*4` | 55 | **5,5 Cyc / Schritt** | **1,36×** | Nur 4 Befehle, 12 Bytes Code |
| **`BST-006`** | **Eytzinger Implicit Array-Tree** | 25 | **2,5 Cyc / Schritt** | **3,00×** | **Keine Pointer, perfekte Cache-Lokalität** |

---

## 5. String- & Textverarbeitung (`STR`)

8-Byte parallele Vektorverarbeitung mit AMMX.

| Test-ID | Operation / Code | Netto-Takte | Durchsatz (92 MHz) | Beschreibung |
|:---|:---|:---:|:---:|:---|
| **`STR-001`** | `paddb` / `pmaxub` To-Lower | **1,6 Cyc** | **460,0 MB/s** | Wandelt 8 ASCII-Zeichen parallel in Kleinbuchstaben |
| **`STR-002`** | `psubb` / `pminub` To-Upper | **1,6 Cyc** | **460,0 MB/s** | Wandelt 8 ASCII-Zeichen parallel in Großbuchstaben |
| **`STR-003`** | `pcmpeqb` Pattern Match | **3,6 Cyc** | **204,4 MB/s** | 8-Byte exakter Substring-Vergleich |
| **`STR-004`** | `pmaxub` Whitespace Trim | **3,6 Cyc** | **204,4 MB/s** | Whitespace- und Nullterminator-Prüfung |
| **`STR-011`** | `vperm` Hex-to-ASCII High | **4,6 Cyc** | **160,0 MB/s** | 8 High-Nibbles parallel in Hex-ASCII |
| **`STR-012`** | `vperm` Hex-to-ASCII Low | **4,6 Cyc** | **160,0 MB/s** | 8 Low-Nibbles parallel in Hex-ASCII |
| **`STR-014`** | UTF-8 Multi-Byte Decode | **9,6 Cyc** | **76,7 MB/s** | Parallele UTF-8 Maskenvalidierung |

---

## 6. Sortieralgorithmen & Branchless Min/Max (`SRT`)

| Test-ID | Operation | Netto-Takte | Kennwert | Pipeline (Anton / Bruno) |
|:---|:---|:---:|:---:|:---:|
| **`SRT-001`** | Skalar Branchless Min/Max (`sub.l`, `addx.l`) | **4,6 Cyc** | 4,6 Cyc / Paar | Pipe 1 (100%) |
| **`SRT-002`** | AMMX 8-Byte Vektor Min/Max (`pmaxub`, `pminub`) | **2,6 Cyc** | **0,325 Cyc / Byte** | Pipe 1 / Pipe 2 (Dual) |
| **`SRT-003`** | AMMX 4-Way 16-Bit Word Sort Kernel | **9,6 Cyc** | **2,4 Cyc / Word** | Pipe 1 / Pipe 2 (Dual) |
| **`SRT-013`** | AMMX 8-Way Bitonic Sorting Network | **19,6 Cyc** | **2,45 Cyc / Element** | Pipe 1 (92%) / Pipe 2 (8%) |

---

## 7. Diskrete Wavelet-Transformationen (`DWT`)

Signalkompression für JPEG 2000 und Subband-Audiocodierung.

| Test-ID | Algorithmus | Stufe | Netto-Takte | Durchsatz / Bemerkung |
|:---|:---|:---|:---:|:---|
| **`DWT-001`** | Haar Wavelet Forward | 1D Dekomposition | **10,6 Cyc** | 4 Koeffizientenpaare |
| **`DWT-002`** | Haar Wavelet 2D | Zeilen- + Spaltenpass | **19,6 Cyc** | 2D $4 \times 4$ Kachel |
| **`DWT-003`** | Haar Inverse Wavelet | Rekonstruktion | **20,6 Cyc** | Verlustfreie inverse Stufe |
| **`DWT-004`** | LeGall 5/3 Wavelet (JPEG 2000) | Predict-Step | **18,6 Cyc** | Verlustfreie Bildkompression |
| **`DWT-005`** | LeGall 5/3 Wavelet (JPEG 2000) | Update-Step | **26,6 Cyc** | Verlustfreie Bildkompression |
| **`DWT-011`** | Daubechies 9/7 Wavelet | Predict 1 | **22,6 Cyc** | Verlustbehaftete Hochpräzision |
| **`DWT-012`** | Daubechies 9/7 Wavelet | Update 1 | **35,6 Cyc** | Skalierte Festkomma-Filterung |
| **`DWT-013`** | Daubechies 9/7 Wavelet | Predict 2 | **43,6 Cyc** | Finale Rekonstruktionsstufe |
| **`DWT-014`** | Subband Multilevel Split | Oktavenzerlegung | **18,6 Cyc** | Pyramidenzerlegung |
| **`DWT-015`** | Wavelet Horizontal Transpose Pack | `vperm` / `pack` | **24,6 Cyc** | Transponierung im Register |

---

## 8. Corpus- & Signal-Optimierungen (`corpus_opt`)

Kernal-Optimierungen für JPEG, MPEG-Audio, FFT und Festkomma-Arithmetik.

| Test-ID | Algorithmus | Netto-Takte | Bedeutung & Durchsatz |
|:---|:---|:---:|:---|
| **`AMX-102`** | AMMX 8-Byte Transpose Row 1 | **4,6 Cyc** | Transponierungs-Stufe für $8 \times 8$ Blöcke |
| **`AMX-103`** | AMMX 8-Byte Transpose Row 2 | **4,6 Cyc** | Zweite Transponierungs-Stufe |
| **`AMX-105`** | AMMX Parallel Add-Saturate | **4,6 Cyc** | Sättigungsarithmetik ohne Überlauf-Branch |
| **`AMX-106`** | AMMX Matrix Row Unpack & Blend | **6,6 Cyc** | Verflechtung zweier Bildzeilen |
| **`AMX-107`** | AMMX Quad Word Parallel Shift | **4,6 Cyc** | SIMD-Skalierung für 4 Festkommazahlen |
| **`AMX-108`** | AMMX 8×8 Matrix Transpose Stage 1 | **10,6 Cyc** | Vollständige Transponierung von 64 Koeffizienten |
| **`FIX-101`** | Q15 Fixed-Point Multiply Kernel | **14,6 Cyc** | 4 parallele 16-Bit Multiplikationen mit Shift |
| **`FIX-102`** | Q31 Fixed-Point Multiply & Accumulate | **32,6 Cyc** | 32-Bit Festkomma mit hoher Dynamik |
| **`DCT-109`** | **1D-DCT 8-Punkt AMMX Kernel** | **16,6 Cyc** | 8 Koeffizienten in nur 16,6 Takten! |
| **`DCT-101`** | **8×8 2D-DCT Kompletter Pass** | **56,6 Cyc** | Vollständiger 64-Pixel DCT-Block |
| **`FFT-101`** | **Radix-2 Complex FFT 8-Punkt** | **79,9 Cyc** | 8 komplexe Frequenzbänder |

---

## 9. Hurrican AMMX Blitting & Shading (`BLT`)

Hardwarenahe Grafikbeschleunigung für Spiele und Demoszenen.

| Test-ID | Blitter-Operation | Netto-Takte | Pixel-Durchsatz (92 MHz) | Beschreibung |
|:---|:---|:---:|:---:|:---|
| **`BLT-006`** | Alpha Blit Step 1 | **1,6 Cyc** | **460,0 Mpx/s** | Grundlegende Alpha-Modulation |
| **`BLT-007`** | Alpha Blit Dual Pixel | **5,6 Cyc** | **131,4 Mpx/s** | Dual-Pixel Alpha Blending |
| **`BLT-008`** | Additive Blending 4-Pixel | **18,6 Cyc** | **19,8 Mpx/s** | Licht- und Glüheffekte |
| **`BLT-009`** | Tint Shading Multi-Channel | **13,6 Cyc** | **27,1 Mpx/s** | Farbfilter-Einfärbung |
| **`BLT-010`** | Full Sprite Blend Row 1 | **25,6 Cyc** | **14,4 Mpx/s** | Vollständige Transparenz-Zeile |
| **`BLT-011`** | Full Sprite Blend Row 2 | **31,6 Cyc** | **11,6 Mpx/s** | Zweite Sprite-Zeile mit Schatten |
| **`BLT-012`** | Masked Qword Blit | **2,6 Cyc** | **283,1 Mpx/s** | Maskierter 64-Bit Transfer |
| **`BLT-013`** | Color Key Transparent Filter | **1,6 Cyc** | **460,0 Mpx/s** | 1-Takt Color-Keying für 8 Pixel |
| **`BLT-014`** | Subtractive Blend Step | **4,6 Cyc** | **160,0 Mpx/s** | Schatten- und Raucheffekte |
| **`BLT-015`** | Multiplicative Blend Step | **4,6 Cyc** | **160,0 Mpx/s** | Dunkle Modulation |
| **`BLT-016`** | Particle Shading 8-Pixel | **12,6 Cyc** | **58,4 Mpx/s** | 8 Partikel auf einmal gerendert |
| **`BLT-017`** | Lighting Map Modulation | **32,6 Cyc** | **11,3 Mpx/s** | 2D Dynamische Lichtquellen |
| **`BLT-018`** | **Ultra-Fast Single Qword Blend** | **0,6 Cyc** | **1.226,7 Mpx/s** | Fast-Path Qword Blit (>1,2 GPixel/s!) |
| **`BLT-019`** | Gouraud Span Shading Step 1 | **8,6 Cyc** | **42,8 Mpx/s** | Farbverlaufs-Interpolation |
| **`BLT-020`** | Gouraud Span Shading Step 2 | **10,6 Cyc** | **34,7 Mpx/s** | Gouraud Shading mit Dithering |
| **`BLT-021`** | Z-Buffer Comparison & Write | **15,6 Cyc** | **23,6 Mpx/s** | 3D Tiefenpuffer-Test |
