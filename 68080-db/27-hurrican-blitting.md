# 27 – AMMX-Muster & Performance-Kernels aus dem Hurrican-Port (Harness v5)

**Herkunft:** Post-Mortem & Best Practices aus dem nativen *Hurrican*-Port für Vampire V4 (V1.0, Nov. 2026; tommysammy / Thomas).  
**Hardware-Target:** Vampire V4 Standalone (Apollo 68080 PCR 1088/1 / Apollo Core 11350 @ 92 MHz, ApolloOS 9.5).  
**Messmethodik:** **Harness v5** (`harness-v5.asm`, 10 × 100 Messläufe unter `Forbid()`, Clock Cycle Counter CCC, Pipeline-Slots IEP1/IEP2/IEP3, D-Cache Hit Rate, Baseline-Kompensation).

---

## 1. Übersicht & Motivation

Im Post-Mortem des *Hurrican*-V4-Ports wurden mehrere hochoptimierte AMMX-Praxisroutinen beschrieben, die in der Spiele-Engine kritische Flaschenhälse (Ruckler/Spitzen über 33,3 ms) beseitigt haben. Diese Muster wurden als eigenständige Testfälle isoliert, mit **Harness v5** assembliert und auf der physischen Vampire V4 exakt vermessen:

1. **`BLT-006` – AMMX Tail-Store mit `storeilm`:**  
   Schreiben der restlichen 1 bis 3 Pixel am Zeilenende ohne langsame bedingte Verzweigungen oder Sonderfall-Schleifen über vorberechnete Lane-Masken.
2. **`BLT-007` – Flashlight Fast Darkening ($3/8$-Skalierung):**  
   Schnelles Abdunkeln von 4× RGB565-Pixeln mit 64-Bit-Shifts (`lsrq`), Maskierung (`pand`) und Vektor-Addition (`paddw`).
3. **`BLT-008` – `MSCALE` RGB565 4-Pixel Kanal-Skalierung:**  
   Vollständige Farbkanal-Skalierung über `pand`, Festkomma-Multiplikation (`pmul88`) und Rekonstruktion (`por`).

---

## 2. BLT-006: AMMX Tail-Store mit `storeilm`

### Problemstellung
Beim Vektor-Blitting in Einheiten von 4 Pixeln (64 Bit) bleibt am Zeilenende oft ein Rest von 1, 2 oder 3 Pixeln übrig. Klassischer Code verzweigt hier in skalare Einzelfall-Schleifen, was Branch-Vorhersagen stört und Pipeline-Stalls erzeugt.

### AMMX-Lösung
Die überzähligen Pixel werden maskiert: Eine 64-Bit-Maske wird geladen (`$0000` = Pixel bleibt unverändert erhalten, `$FFFF` = Pixel wird aus dem Vektorregister geschrieben) und mit `storeilm` direkt in den Framebuffer übertragen.

```asm
; seq/BLT-006.i - AMMX tail store using STOREILM with lane mask
; Uses: e2, e6, a0, a1, a2
; MUST NOT touch: d5, d6, d7, a5, a6, a7
        load    (a0)+,e2
        load    (a2)+,e6
        storeilm e2,e6,(a1)+
```

### V4-Hardware-Messergebnisse (Harness v5)
* 10 Läufe à 100 Iterationen auf Vampire V4 Standalone:
* **MIN:** **4 Takte**
* **MAX:** **4 Takte**
* **AVG:** **4.0 Takte**
* **IEP-Slots:** IEP1 = 5, IEP2 = 0, IEP3 = 0
* **Rohlogs:** `measurements/v5-files/BLT-006.run01.log` bis `run10.log`

> **Fazit:** 4 Takte für das komplette Laden der Pixel, Laden der Maske und maskierte Schreiben in den Framebuffer – absolut branchless und ohne Speicherlesezugriff auf das Ziel.

---

## 3. BLT-007: Hurrican Flashlight Fast Darkening ($3/8$-Fading)

### Problemstellung
Für den Taschenlampen-Effekt müssen Kacheln und Sprites um feste Stufen abgedunkelt werden. Die Formel für $3/8$ Helligkeit lautet:
$$\text{Pixel} \times \frac{3}{8} = (\text{Pixel} \gg 2 \ \& \ \$39E7) + (\text{Pixel} \gg 3 \ \& \ \$18E3)$$
In 68k-Skalarcodes benötigt dies 16 Befehle für 4 Pixel (4× Shiften, 4× Maskieren, 4× Addieren, Speichern).

### AMMX-Lösung
Mit 64-Bit-AMMX-Befehlen werden alle 4 RGB565-Pixel simultan verarbeitet:

```asm
; seq/BLT-007.i - Hurrican Flashlight Fast Darkening 3/8 with LSRQ + PAND + PADDW
; Uses: e2, e3, e4, e5, d0, d1, a0, a1
; MUST NOT touch: d5, d6, d7, a5, a6, a7
; d0=2, d1=3, e2=4x$39E7, e3=4x$18E3
        load    (a0)+,e4
        lsrq    d0,e4,e5
        pand    e2,e5,e5
        lsrq    d1,e4,e4
        pand    e3,e4,e4
        paddw   e5,e4,e4
        store   e4,(a1)+
```

### V4-Hardware-Messergebnisse (Harness v5)
* 10 Läufe à 100 Iterationen auf Vampire V4 Standalone:
* **MIN:** **8 Takte**
* **MAX:** **8 Takte**
* **AVG:** **8.0 Takte**
* **IEP-Slots:** IEP1 = 9, IEP2 = 0, IEP3 = 0
* **Durchsatz:** **2.00 Takte pro Pixel** (8 Takte für 4 Pixel)!
* **Rohlogs:** `measurements/v5-files/BLT-007.run01.log` bis `run10.log`

> **Fazit:** Extrem stabiler Durchsatz von 2 Takten pro Pixel inklusive Load und Store. Im 640×480 Framebuffer (307.200 Pixel) benötigt dieser Kernel netto nur 6,67 ms Rechenzeit bei 92 MHz!

---

## 4. BLT-008: Hurrican `MSCALE` 4-Pixel RGB565 Skalierung

### Problemstellung
Variables Sprite- und Kachel-Fading erfordert kontinuierliche Helligkeitsstufen (Faktor 0.0 bis 1.0). Hier reicht kein einfacher Shift aus, sondern die drei Kanäle (R, G, B) müssen isoliert, mit einem 8.8-Festkomma-Faktor multipliziert und wieder zusammengesetzt werden.

### AMMX-Lösung
Der `MSCALE`-Kernel isoliert mit Masken `e10` ($F800), `e13` ($07E0) und `e14` ($001F) die drei Farbkanäle, skaliert Rot/Grün/Blau unabhängig mit `pmul88` und bündelt sie über `por` verlustfrei zurück:

```asm
; seq/BLT-008.i - Hurrican MSCALE RGB565 4-Pixel Scale with PMUL88 + POR
; Uses: e0, e3, e4, e5, e8, e9, e10, e11, e12, e13, e14, a0, a1
; MUST NOT touch: d5, d6, d7, a5, a6, a7
        load    (a0)+,e0
        pand    e10,e0,e3
        pand    e13,e0,e4
        pand    e14,e0,e5
        pmul88  e11,e3,e3
        pand    e12,e3,e3
        pmul88  e9,e3,e3
        pmul88  e9,e4,e4
        pmul88  e9,e5,e5
        pmull   e8,e3,e3
        pand    e10,e3,e3
        pand    e13,e4,e4
        pand    e14,e5,e5
        por     e4,e3,e3
        por     e5,e3,e0
        store   e0,(a1)+
```

### V4-Hardware-Messergebnisse (Harness v5)
* 10 Läufe à 100 Iterationen auf Vampire V4 Standalone:
* **MIN:** **21 Takte**
* **MAX:** **21 Takte**
* **AVG:** **21.0 Takte**
* **IEP-Slots:** IEP1 = 18, IEP2 = 0, IEP3 = 0
* **Durchsatz:** **5.25 Takte pro Pixel** (21 Takte für 4 Pixel) für vollständiges 3-Kanal 8.8 Scaling.
* **Rohlogs:** `measurements/v5-files/BLT-008.run01.log` bis `run10.log`

---

## 5. Optimierung: Load-Parallelisierung, Unrolling & Interleaving

In linearen AMMX-Sequenzen leidet die Pipeline oft unter **RAW-Datenabhängigkeiten** (Read-After-Write Latency): Ein Vektor wird geladen (`load`), und der unmittelbar folgende Rechenbefehl (`lsrq` oder `pand`) muss warten, bis die Daten aus dem Cache/Register eintreffen.

Durch **Loop Unrolling (2× bzw. 4×)** und **verschachteltes Scheduling (Interleaving)** können zwei oder mehr unabhängige Pixelströme (Quadwords) parallel in den Dual-/Triple-Decodern und Ausführungseinheiten des 68080 abgearbeitet werden.

### 5.1 BLT-009: Flashlight 3/8 Darkening 2× Unrolled (8 Pixel)
* **Strategie:** Laden beider Quadwords (`e4`, `e6`) zu Beginn, anschliessend paarweise unabhängige Shifter, Masken und Additionen.
* **Code:**
```asm
; seq/BLT-009.i - Hurrican Flashlight Fast Darkening 3/8 (2x Unrolled & Interleaved)
; Uses: e2, e3, e4, e5, e6, e7, d0, d1, a0, a1
; MUST NOT touch: d5, d6, d7, a5, a6, a7
        load    (a0)+,e4
        load    (a0)+,e6
        lsrq    d0,e4,e5
        lsrq    d0,e6,e7
        pand    e2,e5,e5
        pand    e2,e7,e7
        lsrq    d1,e4,e4
        lsrq    d1,e6,e6
        pand    e3,e4,e4
        pand    e3,e6,e6
        paddw   e5,e4,e4
        paddw   e7,e6,e6
        store   e4,(a1)+
        store   e6,(a1)+
```
* **Messergebnis (V4 Hardware, 10 Läufe):**
  * **Netto-Takte:** **11 Takte** für 8 Pixel (Rohwert: 16 Takte, Baseline: 5 Takte)
  * **Durchsatz:** **1.375 Takte pro Pixel** (vorher 2.000 Takte/Pixel bei BLT-007)
  * **Speedup:** **+31.25% schneller**!
  * **Hardware-Counter:** `IEP2 = 1` belegt echte Parallelausführung auf Pipe 2!
  * **Cache-Effizienz:** Die 14 Befehle (56 Bytes) passen exakt in eine einzige 64-Byte-Cacheline.

### 5.2 BLT-010: Flashlight 3/8 Darkening 4× Unrolled (16 Pixel)
* **Strategie:** 4 parallele Streams (`e4`, `e6`, `e8`, `e10`).
* **Messergebnis (V4 Hardware, 10 Läufe):**
  * **Netto-Takte:** **23 Takte** für 16 Pixel (Rohwert: 28 Takte)
  * **Durchsatz:** **1.4375 Takte pro Pixel**.
  * **Erkenntnis:** 4× Unrolling (112 Bytes) überschreitet die 64-Byte Cacheline-Grenze. 2× Unrolling (`BLT-009`, 56 Bytes) ist kompakter und mit **1.375 Takten/Pixel** messbar schneller!

### 5.3 BLT-011: Hurrican MSCALE RGB565 2× Unrolled Dual-Stream (8 Pixel)
* **Strategie:** 2 Quadwords parallel skalieren, um die Multiplikator-Latenzen (`pmul88`) des R-, G- und B-Kanals zu überlappen.
* **Messergebnis (V4 Hardware, 10 Läufe):**
  * **Netto-Takte:** **29 Takte** für 8 Pixel (Rohwert: 34 Takte)
  * **Durchsatz:** **3.625 Takte pro Pixel** (vorher 4.000 net / 5.25 raw Takte/Pixel bei BLT-008).
  * **Speedup:** **+9.4% schneller** auf rechenintensivem 3-Kanal 8.8 Scaling.

### 5.4 BLT-012 vs. BLT-013 vs. BLT-014: Additives Shading & Memory-VEA (Wasser/Energie)
* **Strategie:** Vergleich von expliziten Vektor-Loads gegen direkte Speicher-Operanden (`paddusw (a0)+, e1, e2`) und 2× Unrolling.
* **Messergebnisse (V4 Hardware, 10 Läufe):**
  * **`BLT-012` (1× Explizit):** 4 Befehle, **0-1 Netto-Takt** (Rohwert 5 Takte, Baseline 5).
  * **`BLT-013` (1× Memory-VEA):** 3 Befehle, **Rohwert 4 Takte** (spart 1 Befehlsslot und schlägt den Baseline-Loop).
  * **`BLT-014` (2× Unrolled Memory-VEA):** 6 Befehle, **2 Netto-Takte für 8 Pixel** = **0.25 Takte pro Pixel** (4 Pixel/Takt)!

### 5.5 BLT-015: Transparent Sprite Color-Key 2× Unrolled (8 Pixel)
* **Strategie:** Vorladen des Farb-Schlüssels in `e7`, 2× Unrolling mit `pcmpeqw` und `storeilm`.
* **Messergebnis (V4 Hardware, 10 Läufe):**
  * **Netto-Takte:** **2 Takte für 8 Pixel** (Rohwert: 7 Takte, Baseline: 5 Takte)
  * **Durchsatz:** **0.25 Takte pro Pixel** (vorher 1.25 Takte/Pixel bei BLT-002)
  * **Speedup:** **500% Durchsatzsteigerung (5× schneller)** gegenüber einfachem 1× Color-Keying!

### 5.6 BLT-016: Flashlight 3/8 Darkening mit vorgezogenem Store (Early Store Overlap)
* **Strategie:** Der erste `store e4, (a1)+` wird sofort nach Abschluss von Vektor A abgesetzt, während Vektor B noch addiert (`paddw e7, e6, e6`).
* **Messergebnis (V4 Hardware, 10 Läufe):**
  * **Netto-Takte:** **10 Takte für 8 Pixel** (Rohwert: 15 Takte, Baseline: 5 Takte)
  * **Durchsatz:** **1.250 Takte pro Pixel** (vorher 1.375 bei BLT-009 und 2.000 bei BLT-007).
  * **Speedup:** Weiterer Takt eingespart (**+37.5% schneller** als Basis BLT-007).

---

## 6. Gesamte Vergleichstabelle der Blit- & Shading-Kernels (Vampire V4)

| Test-ID | Kernel / Operation | Unroll | Befehle | Netto-Takte (MIN) | Takte / Pixel | Speedup vs. Basis |
|---|---|:---:|:---:|:---:|:---:|:---:|
| [`BLT-001`](19-sprite-blit.md) | 2D Sprite Blitting: 256-Bit Copy (`move16`) | 2× | 2 | **3 Takte** | **0.19 Takt** | Basis |
| [`BLT-002`](20-sprite-transparency.md) | Transparent Color-Key (`pcmpeqw` + `storeilm`) | 1× | 4 | **5 Takte** | **1.25 Takte** | Basis |
| **`BLT-015`** | **Transparent Color-Key (2× Unrolled)** | **2×** | **6** | **2 Takte** | **0.25 Takt** | **+400% (5× Durchsatz)** |
| [`BLT-006`](27-hurrican-blitting.md) | Tail-Store Zeilenende (`storeilm` + Mask) | 1× | 3 | **3-4 Takte** | **1.00 Takt** | Basis |
| [`BLT-007`](27-hurrican-blitting.md) | Flashlight $3/8$-Darkening (`lsrq` + `paddw`) | 1× | 7 | **8 Takte** | **2.00 Takte** | Basis |
| `BLT-009` | Flashlight $3/8$-Darkening (Interleaved) | 2× | 14 | **11 Takte** | **1.375 Takte** | +31.25% |
| **`BLT-016`** | **Flashlight $3/8$-Darkening (Early Store Overlap)** | **2×** | **14** | **10 Takte** | **1.250 Takte** | **+37.50%** |
| `BLT-010` | Flashlight $3/8$-Darkening (Quad-Stream) | 4× | 28 | **23 Takte** | **1.438 Takte** | +28.1% |
| [`BLT-003`](21-sprite-alpha-pmula.md) | Alpha Blending Premultiplied (`PMULA`) | 1× | 5 | **5 Takte** | **1.25 Takte** | Basis |
| [`BLT-008`](27-hurrican-blitting.md) | `MSCALE` 3-Kanal RGB565 Scale (`pmul88`) | 1× | 16 | **16 Takte** | **4.00 Takte** | Basis |
| **`BLT-011`** | **`MSCALE` 3-Kanal RGB565 Scale (Dual-Stream)** | **2×** | **30** | **29 Takte** | **3.625 Takte** | **+9.4%** |
| `BLT-012` | Additive Blit / Wasser (Explicit Loads) | 1× | 4 | **0-1 Takt** | **0.25 Takt** | Basis |
| `BLT-013` | Additive Blit / Wasser (Memory-VEA `paddusw`) | 1× | 3 | **<0 Takt** | **0.20 Takt** | -1 Takt Rohzeit |
| **`BLT-014`** | **Additive Blit / Wasser (2× Unrolled VEA)** | **2×** | **6** | **2 Takte** | **0.25 Takt** | **4 Pixel / Takt** |


