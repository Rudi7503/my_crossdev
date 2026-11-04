# 29. Diskrete Wavelet-Transformationen (DWT) auf der 68080 / AMMX

Dieses Kapitel dokumentiert die Premiere von **Diskreten Wavelet-Transformationen (DWT)** auf dem Apollo 68080 Prozessor.
Alle Messungen wurden auf der physikalischen **Vampire V4 Standalone** (Core 11350 @ 92 MHz, PCR 1088/1) mit **Harness v5** unter `Forbid()` (10 Läufe à 100 Iterationen) durchgeführt.

---

## 1. Übersicht & Mathematische Grundlagen

Wavelet-Transformationen zerlegen Signale und Bilder multiskalar in Approximations- (Tiefpass) und Detail-Koeffizienten (Hochpass). Im Gegensatz zur Fourier-Transformation (FFT/DCT) bieten Wavelets sowohl **Zeit- als auch Frequenzlokalisierung** und vermeiden die typischen Block-Artefakte der 8×8 DCT bei hoher Kompression.

### Die 5 implementierten Wavelet-Kerne

| Test-ID | Wavelet-Verfahren | Typ / Algorithmus | Multiplikationen | Einsatzbereich |
|---|---|---|:---:|---|
| **`DWT-001`** | **Haar 1D (8 Samples)** | 2-Tap Orthogonal Filter | **0** (nur Add/Sub/Shift) | LOD, Texturen, Kanten |
| **`DWT-002`** | **CDF 5/3 1D (LeGall)** | Reversible Integer Lifting | **0** (dyadisch: $\frac{1}{2}, \frac{1}{4}$) | Verlustfreies **JPEG 2000**, Dirac Video |
| **`DWT-003`** | **CDF 9/7 1D** | 4-Stage Fixed-Point Lifting | 4 (`pmulh` 16-Bit Q14/Q15) | Verlustbehaftetes **JPEG 2000**, Broadcast |
| **`DWT-004`** | **Daubechies-4 (Db4)** | 4-Tap Orthogonal FIR Filter | 4 (`pmulh` + `paddw`) | Audio-Denoising, Spektralanalyse |
| **`DWT-005`** | **2D Haar (4 Subbänder)** | 2D Mallat Block-Zerlegung | **0** (Horiz. + Vertikal) | Bildkachel-Kompression (**LL, LH, HL, HH**) |

---

## 2. Gemessene Ergebnisse auf Vampire V4 Hardware

Alle Messungen liefen über 10 Durchgänge mit 100% stabiler Wiederholgenauigkeit:

| Test-ID | Kernel / Operation | Befehle | Netto-Takte (MIN) | Takte / Sample | Durchsatz (92 MHz) |
|---|---|:---:|:---:|:---:|:---:|
| **`DWT-001`** | 1D Haar Wavelet Transform (8 Samples) | 12 | **8 Takte** | **1.00 Takt** | **92.0 MSamples / s** |
| **`DWT-002`** | 1D CDF 5/3 Integer Lifting (8 Samples) | 16 | **17 Takte** | **2.125 Takte** | **43.3 MSamples / s** |
| **`DWT-003`** | 1D CDF 9/7 Fixed-Point Lifting (8 Samples) | 20 | **18 Takte** | **2.250 Takte** | **40.9 MSamples / s** |
| **`DWT-004`** | 1D Daubechies-4 (Db4) FIR Filter (8 Samples) | 18 | **16 Takte** | **2.000 Takte** | **46.0 MSamples / s** |
| **`DWT-005`** | 2D Haar 4×4/8×8 Mallat-Zerlegung (16 Samples) | 24 | **24 Takte** | **1.500 Takte** | **61.3 MSamples / s** |

---

## 3. Technische Details & AMMX-Implementierung

### 3.1 DWT-001: 1D Haar Wavelet (1.00 Takt pro Sample)
* Durch `vperm #$014589cd` und `vperm #$2367abef` werden 8 aufeinanderfolgende 16-Bit-Werte in einem Takt in gerade (`x_even`) und ungerade (`x_odd`) Vektoren entflochten.
* `paddw` und `psubw` berechnen Summe und Differenz simultan auf Pipe 1 und Pipe 2.
* Mit **1.00 Takt pro Sample** erreicht der 68080 bei 92 MHz einen Durchsatz von **92 Millionen 16-Bit-Samples pro Sekunde**!

### 3.2 DWT-002: Reversibles CDF 5/3 Integer Lifting Scheme (JPEG 2000 Lossless)
Das Lifting-Schema berechnet die Wavelet-Koeffizienten „in place“ ohne Pufferüberhang:
* **Predict-Stufe (High-Pass / Details):**
  $$d[n] = x_{odd}[n] - \lfloor \frac{x_{even}[n] + x_{even}[n+1]}{2} \rfloor$$
* **Update-Stufe (Low-Pass / Approximation):**
  $$s[n] = x_{even}[n] + \lfloor \frac{d[n-1] + d[n] + 2}{4} \rfloor$$
* Da alle Teiler Zweierpotenzen sind, kommt die Routine komplett **ohne Multiplikationen** aus. Mit nur **17 Takten für 8 Samples** (2.125 Takte/Sample) ist dies der ideale verlustfreie Bild- und Audiokompressor für 68080-Systeme.

### 3.3 DWT-003: CDF 9/7 Biorthogonales Lifting Scheme (JPEG 2000 Lossy)
* Verwendet vier verschachtelte Lifting-Stufen mit den irrationalen Konstanten $\alpha \approx -1.5861$, $\beta \approx -0.05298$, $\gamma \approx 0.8829$, $\delta \approx 0.4435$.
* In 16-Bit-Festkomma berechnet `pmulh` die fraktionalen Produkte in 1 Takt.
* Mit **18 Takten für 8 Samples** arbeitet das hochkomplexe 9/7-Wavelet praktisch genauso schnell wie das einfachere 5/3-Wavelet.

### 3.4 DWT-005: 2D Mallat-Zerlegung in 4 Frequenz-Subbänder
* Transformiert zweidimensionale 16-Bit-Pixelblöcke zuerst horizontal und anschließend vertikal.
* Das Ergebnis liefert direkt die 4 JPEG-2000-Quadranten:
  * **LL (Low-Low):** Tiefpass-Vorschaubild mit halber Auflösung
  * **HL (High-Low):** Horizontale Kantenmerkmale
  * **LH (Low-High):** Vertikale Kantenmerkmale
  * **HH (High-High):** Diagonale Texturdetails
* Mit **24 Takten für 16 Pixel** (1.50 Takte/Pixel) können komplette 640×480 RGB565-Frames in **4.6 ms** zerlegt werden – weit unter dem 33.3-ms-Budget für 30 FPS!

---

## 4. Speicherorte & Reproduzierbarkeit

* **ASM-Sequenzen:** `Projects/DWT_AMMX/DWT-001.i` bis `DWT-005.i`
* **V4-Messprotokolle:** `cores/core-11350/suites/dwt_logs/DWT-001.run01.log` bis `DWT-005.run10.log` (50 Rohlogs)
* **Runner-Script:** `run_opt_suite.py dwt`
