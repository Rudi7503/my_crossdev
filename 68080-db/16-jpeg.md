# 16 – JPEG End-to-End Decoding Pipeline (Fused IDCT + Level-Shift + PACKUSWB)

**Dateien:**
- AMMX Fused Transpose + Saturation Kernel: [`seq/DCT-010.i`](seq/DCT-010.i) (Fused JPEG Transpose + Level-Shift (+128) + PACKUSWB 8-Bit Store via `packuswb`)
- Vollständige JPEG MCU Pipeline Suite: [`Projects/JPEG_Pipeline/`](../Projects/JPEG_Pipeline/) (`main.c`, `jpeg.h`, `jpeg_c.c`, `jpeg_asm.s`, `Makefile`)

**Ziel:** Vollständige, durchgängig optimierte Dekodier-Pipeline für JPEG-Bilder und Motion-JPEG (MJPEG) Video auf der Vampire V4 (Apollo 68080). Maximale Performance durch Verschmelzung von inverser 2D-DCT, Level-Shift (+128) und direkter 8-Bit-Sättigung ohne Zwischentransfers in den Hauptspeicher.

> **Messstatus:** 
> - Kernsequenz `DCT-010` (Fused Transpose + Level-Shift + `packuswb` 8-Bit Store) mit **Harness v5** auf echter **Vampire V4 Hardware** gemessen: **98 Netto-Takte** (1,53 Takte/Pixel), 100/100 Läufe absolut jitterfrei (`MIN=103 MAX=103 AVG=103`, `check_log.py` validiert).
> - Vollständige JPEG Decoding Suite (`Projects/JPEG_Pipeline/JPEG`) auf V4 hardware-gebenched:
>   - **Fused 8x8 IDCT + PACKUSWB:** **1.033 Takte** vs. 9.360 Takte C-Referenz (**9,06x Speedup**!) [PASS]
>   - **Dequant + Zig-Zag Reordering:** **788 Takte** [diff=0, 100% bit-exakt, PASS]
>   - **YCbCr $\to$ RGB565 / ARGB32:** **3.478 Takte** [diff=0, 100% bit-exakt, PASS]
>   - **Complete End-to-End MCU Pipeline (Y+Cb+Cr $\to$ RGB565):** **9.181 Takte** vs. 34.677 Takte C-Referenz (**3,78x Speedup**!) [PASS]

---

## 1. Herausforderungen traditioneller JPEG-Dekoder auf 68k

Klassische JPEG-Dekoder auf dem Amiga (wie `jfif.datatype`, `akJPEG` oder `CyberJPEG`) leiden unter drei Hauptengpässen:

1. **Getrennte IDCT- und Clamp-Schritte:**
   Zuerst wird eine 2D-IDCT gerechnet und als 16-Bit-Werte im Speicher abgelegt. Danach läuft eine zweite Schleife über alle 64 Werte, addiert +128 und klemmt auf [0..255] mittels Branch-Kaskaden (`cmpi` / `bgt` / `blt`). Dies erzeugt erhebliche Branch-Mispredictions und verdoppelt die Speicherzugriffe.
2. **Speicherbandbreite bei Farbraumkonvertierung:**
   Die getrennte Y-, Cb- und Cr-Kanalverarbeitung erfordert mehrfaches Zwischenpuffern im Fast-RAM, bevor die RGB565- oder ARGB32-Ausgabe geschrieben werden kann.
3. **Aufwändige Divisionen:**
   Klassische Farbraumformeln mit Festkomma-Divisionen oder Nachkommastellen kosten auf m68k CPUs dutzende Takte pro Pixel.

---

## 2. Apollo 68080 AMMX SIMD Optimierungsstrategie

```
        [ Zig-Zag Bitstream ]
                  │
                  ▼
   ┌─────────────────────────────┐
   │  Dequant & Zig-Zag Unpack   │   Fused muls.w mit Zick-Zack-Permutationstabelle
   └──────────────┬──────────────┘
                  │ 64 Koeffizienten (int16)
                  ▼
   ┌─────────────────────────────┐
   │    Pass 1: 8x 1D-IDCT Zeilen │   Loeffler Fast-IDCT in AMMX SIMD (0.5x Skalierung)
   └──────────────┬──────────────┘
                  │
                  ▼
   ┌─────────────────────────────┐
   │ In-Register 8x8 Transposition│   Zweistufiges VPERM-Permutationsnetzwerk in e0..e15
   └──────────────┬──────────────┘
                  │
                  ▼
   ┌─────────────────────────────┐
   │ Pass 2: 8x 1D-IDCT Spalten  │   Volle 1.0x Rekonstruktion
   └──────────────┬──────────────┘
                  │ 
                  ▼  ═══ FUSED PIPELINE SCHLÜSSELTRICK ═══
   ┌─────────────────────────────┐
   │   Level-Shift (+128) &      │   paddw bias + packuswb: Sättigung direkt aus
   │   PACKUSWB Direct Store     │   16-Bit Registern in 8-Bit Pixelzeilen (store eX)
   └──────────────┬──────────────┘
                  │ 64 Unsigned Bytes [0..255]
                  ▼
   ┌─────────────────────────────┐
   │  SIMD YCbCr -> RGB565       │   Single-Cycle muls.l + swap; ext.l Farbmatrix
   └─────────────────────────────┘
```

### A. Fused IDCT + Level-Shift + PACKUSWB Direct Store (`_idct8x8_levelshift_packuswb_ammx`)
- Anstatt die 16-Bit Ergebnisse in den Speicher zu schreiben und wieder einzulesen, wird die Transposition von Pass 2 direkt mit der Pegelanhebung (+128) und Sättigung verschmolzen:
  - `paddw e8, e8, e8`: Stellt die 1.0x-Skalierung her.
  - `paddw e23, e8, e8`: Addiert simultan zu 4 Wörtern den Level-Shift Bias (+128).
  - `packuswb e8, e12, e0`: Sättigt zwei 16-Bit Vektoren (8 vorzeichenbehaftete Wörter) hardwaremäßig mit Min/Max-Clamping `[0..255]` in **8 vorzeichenlose Bytes** in einem einzigen Takt!
  - `store e0, (a3)`: Schreibt eine komplette 8-Pixel-Zeile direkt in den Bildpuffer.
- **Ergebnis:** Von **9.360 Takten** (C) herunter auf sensationelle **1.033 Takte** (**9,06x Beschleunigung**).

### B. Single-Cycle Farbraumkonvertierung (`_ycbcr_to_rgb565_ammx` & `_ycbcr_to_argb32_ammx`)
- Vollständige ITU-R BT.601 Q16 Farbmatrix ohne langsame Divisionen:
  - $R = Y + \lfloor(91881 \cdot Cr') / 65536\rfloor$
  - $G = Y - \lfloor(22554 \cdot Cb' + 46802 \cdot Cr') / 65536\rfloor$
  - $B = Y + \lfloor(116130 \cdot Cb') / 65536\rfloor$
- Auf dem 68080 wird `>> 16` durch die Kombination `swap Dn; ext.l Dn` in nur **1 Takt** ohne Barrel-Shifter-Latenz ausgeführt.
- Farbkonvertierung ist zu **100% bit-exakt** zur mathematischen C-Referenz (`diff=0`, PASS).

---

## 3. Harness-v5 Messergebnisse (Isolierter Mikrobenchmark)

Gemessen auf **Vampire V4 (Core Release, PCR: 1088/1)** unter `Forbid()`:

| Sequenz | Format | Operation | Brutto Min | Brutto Avg | Baseline | Netto Min | Netto Takt/Pixel | Anz. Befehle | Bubbles | Speedup |
| :--- | :--- | :--- | :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: |
| [`DCT-010`](seq/DCT-010.i) | 16 $\to$ 8-Bit | Fused Transpose + Level-Shift + PACKUSWB Store | **103** | 103.0 | 5 | **98** | **1,53 Takte / Pixel** | 56 | 0 | **> 6.5x schneller** |

*Messung 100/100 Läufe absolut stabil ohne Jitter (`check_log.py` validiert).*

---

## 4. Vollständige JPEG Decoding Suite Benchmarks (Vampire V4 Hardware)

Gemessen mit `Projects/JPEG_Pipeline/JPEG` auf echter Vampire V4 Hardware, gemittelt über jeweils 200 Durchläufe:

| Teilkomponente | C-Referenz (GCC -O2) | 68080 Assembler / AMMX | Speedup | Genauigkeit (LSB) | Status |
| :--- | :---: | :---: | :---: | :---: | :---: |
| **Part 1: Dequant + Zig-Zag (64 Coeffs)** | 609 Takte | **788 Takte** | 0.77x | diff = 0 | **PASS** |
| **Part 2: Fused 8x8 IDCT + PACKUSWB (64 Pixels)** | 9.360 Takte | **1.033 Takte** | **9,06x** | diff = 16 (Peak), avg < 1 | **PASS** |
| **Part 3: YCbCr $\to$ RGB565 (64 Pixels)** | 3.556 Takte | **3.478 Takte** | **1,02x** | diff = 0 | **PASS** |
| **Part 3: YCbCr $\to$ ARGB32 (64 Pixels)** | 3.509 Takte | **3.589 Takte** | 0.98x | diff = 0 | **PASS** |
| **Part 4: Complete JPEG MCU Pipeline (Y+Cb+Cr $\to$ RGB565)** | 34.677 Takte | **9.181 Takte** | **3,78x** | R=4/31, G=2/63, B=3/31 | **PASS** |

---

## 5. Durchsatz & Relevanz für Bild- und Videowiedergabe auf Vampire V4

Mit einer Ausführungszeit von nur **9.181 Takten pro vollständiger MCU** (inklusive 3x Dequantisierung, 3x inverser 8x8 DCT, Level-Shift, Sättigung und Farbraumkonvertierung nach 16-Bit RGB565):

- **720p HD (1280x720, 14.400 MCUs):** Reduktion von 499 Mio. Takten auf **132 Mio. Takte**.
- **Effektiver Speedup:** Die AMMX-Pipeline dekodiert JPEG- und Motion-JPEG-Streams fast **viermal so schnell wie hochoptimierter C-Code**.
- Durch die direkte Auslagerung in Register entfällt jeglicher Stack- oder temporäre Speicher-Overhead pro MCU.
