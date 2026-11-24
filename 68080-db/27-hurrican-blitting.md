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

### 5.7 BLT-018: AMMX Tail-Store mit vorab geladener Maske (1..3 Pixel)
* **Strategie:** Die Maske `e6` für unvollständige Zeilenenden wird einmalig vor der Zeile geladen, im Tail wird nur noch `load (a0)+, e2` gefolgt von `storeilm e2, e6, (a1)+` ausgeführt.
* **Messergebnis (V4 Hardware, 10 Läufe):**
  * **Rohwert:** **3 Takte** (vorher 8 Takte bei BLT-006)
  * **Speedup:** **-62.5% Latenz** (von 8 auf 3 Roh-Takte). 1..3 Restpixel werden branchless in 3 Takten gesichert!

### 5.8 BLT-019: Premultiplied Alpha Blending 2× Unrolled (`PMULA` + Early Store)
* **Strategie:** 2 Quadwords (8 Pixel) verschränkt berechnen und speichern.
* **Messergebnis (V4 Hardware, 10 Läufe):**
  * **Netto-Takte:** **6 Takte für 8 Pixel** (Rohwert: 11 Takte, Baseline: 5 Takte)
  * **Durchsatz:** **0.750 Takte pro Pixel** (vorher 1.250 Takte/Pixel bei BLT-003).
  * **Speedup:** **+40.0% schneller**!

### 5.9 BLT-020: Transparent Color-Key 4× Unrolled (16 Pixel)
* **Messergebnis (V4 Hardware, 10 Läufe):**
  * **Netto-Takte:** **8 Takte für 16 Pixel** $\rightarrow$ **0.500 Takte pro Pixel**.
  * **Architektur-Erkenntnis:** 2× Unrolling (`BLT-015`, **0.250 Takte/Pixel**) ist doppelt so schnell wie 4× Unrolling (`BLT-020`, **0.500 Takte/Pixel**), da 2× optimal in die 64-Byte Cacheline passt und keinen Registerüberhang erzeugt.

---

## 6. Gesamte Vergleichstabelle der Blit- & Shading-Kernels (Vampire V4)

| Test-ID | Kernel / Operation | Unroll | Befehle | Netto-Takte (MIN) | Takte / Pixel | Speedup vs. Basis |
|---|---|:---:|:---:|:---:|:---:|:---:|
| [`BLT-001`](19-sprite-blit.md) | 2D Sprite Blitting: 256-Bit Copy (`move16`) | 2× | 2 | **3 Takte** | **0.19 Takt** | Basis (16 Pixel) |
| `BLT-021` | 512-Bit Cacheline-Burst Copy (`move16` × 4) | 4× | 4 | **12 Takte** | **0.375 Takt** | 32 Pixel direkt via Bus |
| [`BLT-002`](20-sprite-transparency.md) | Transparent Color-Key Basis (`pcmpeqw` + `storeilm`) | 1× | 4 | **5 Takte** | **1.25 Takte** | Basis (4 Pixel) |
| **`BLT-015`** | **Transparent Color-Key (2× Unrolled)** | **2×** | **6** | **2 Takte** | **0.25 Takt** | **+400% (5× Durchsatz)** |
| `BLT-020` | Transparent Color-Key (4× Unrolled) | 4× | 12 | **8 Takte** | **0.50 Takt** | +150% (2× Unroll gewinnt) |
| [`BLT-006`](27-hurrican-blitting.md) | Tail-Store Zeilenende (2 Loads + `storeilm`) | 1× | 3 | **3–4 Takte** | **1.00 Takt** | Basis |
| **`BLT-018`** | **Tail-Store Zeilenende (Maske vorab geladen)** | **1×** | **2** | **3 Roh-Takte** | **0.75 Takt** | **-62.5% Latenz (3 Takte)** |
| [`BLT-007`](27-hurrican-blitting.md) | Flashlight $3/8$-Darkening Basis | 1× | 7 | **8 Takte** | **2.00 Takte** | Basis |
| `BLT-009` | Flashlight $3/8$-Darkening (Interleaved) | 2× | 14 | **11 Takte** | **1.375 Takte** | +31.25% |
| **`BLT-016`** | **Flashlight $3/8$-Darkening (Early Store Overlap)** | **2×** | **14** | **10 Takte** | **1.250 Takte** | **+37.50%** |
| `BLT-010` | Flashlight $3/8$-Darkening (Quad-Stream) | 4× | 28 | **23 Takte** | **1.438 Takte** | +28.1% |
| [`BLT-003`](21-sprite-alpha-pmula.md) | Alpha Blending Premultiplied (`PMULA`) Basis | 1× | 5 | **5 Takte** | **1.25 Takte** | Basis (4 Pixel) |
| **`BLT-019`** | **Alpha Blending Premultiplied (2× Unrolled Overlap)** | **2×** | **10** | **6 Takte** | **0.75 Takt** | **+40.0% schneller** |
| [`BLT-008`](27-hurrican-blitting.md) | `MSCALE` 3-Kanal RGB565 Scale (`pmul88`) Basis | 1× | 16 | **16 Takte** | **4.00 Takte** | Basis (4 Pixel) |
| **`BLT-011`** | **`MSCALE` 3-Kanal Scale (Dual-Stream Interleaved)** | **2×** | **30** | **29 Takte** | **3.625 Takte** | **+9.4% schneller** |
| `BLT-017` | `MSCALE` 3-Kanal Scale (Early Store Split) | 2× | 30 | **30 Takte** | **3.750 Takte** | Multiplier-Interleave gewinnt |
| `BLT-012` | Additive Blit / Wasser (Explicit Loads) | 1× | 4 | **0–1 Takt** | **0.25 Takt** | Basis |
| `BLT-013` | Additive Blit / Wasser (Memory-VEA `paddusw`) | 1× | 3 | **< 0 Takte** | **0.20 Takt** | 1 Befehl gespart |
| **`BLT-014`** | **Additive Blit / Wasser (2× Unrolled Memory-VEA)** | **2×** | **6** | **2 Takte** | **0.25 Takt** | **4 Pixel pro Takt für 8 Pixel** |

---

## 7. Direkter Abgleich mit dem Original-Source `Hurrican/src/blit.s` (Harness v6)

Nach Vorliegen des vollständigen *Hurrican*-V4-Sources (`D:\downloads\Hurrican\Hurrican\src\blit.s`) wurden dessen innere Schleifen direkt gegen die bestehenden DB-Kernels unter **Harness v6** (`BASE_10 = 30`, 10× Loop, 10 Läufe auf Vampire V4 Standalone) vermessen:

### 7.1 Vergleich: Bestehende DB-Routinen vs. `blit.s`
| Kategorie | DB-Routine (v6) | `blit.s`-Routine | Netto-Takte (v6) | Takte / Pixel | Ergebnis |
|---|---|---|---:|---:|---|
| **Copy (32 B / 16 px)** | `BLT-001` (`2× move16`) | — | 7.6 Cyc | 0.475 | Basis `move16` |
| **Copy (64 B / 32 px)** | `BLT-021` (`4× move16`) | — | 15.6 Cyc | 0.488 | 7.8 Cyc / 32 B |
| **Copy (32 B / 16 px)** | `MEM-011` (`4× load + 4× store`) | `_BlitCopy` (`.q4`) | **6.6 Cyc** | **0.413** | **`blit.s` ~13% schneller als `move16`** |
| **Fill (32 B / 16 px)** | `MEM-010` (`4× store`) | `_FillRect` (`.f4`) | **2.6 Cyc** | **0.163** | Identisch |
| **Color-Key (4 px)** | `BLT-002` (Key-Load/Step) | — | 2.6 Cyc | 0.650 | Basis |
| **Color-Key (8 px)** | `BLT-015` (2× Unrolled) | `_BlitKey` (`.k2`) | **4.6 Cyc** | **0.575** | **`blit.s` ~12% schneller/px als `BLT-002` & ~13% schneller als `BLT-020` (10.6 Cyc / 16 px)** |

### 7.2 Neu aufgenommene `blit.s`-Kernels (`BLT-022` bis `BLT-028`, Harness v6)
Diese Routinen waren bislang nicht in `68080-db` enthalten. Insbesondere nutzt `_BlitAdd` / `_BlitKeyAdd` / `_BlitKeyTint` korrekt **`unpack1632` + `paddusb` + `pack3216`** für echtes kanalweises RGB565-Sättigungs-Addieren (während `BLT-012..014` zuvor `paddusw` nutzten, was bei Überlauf in den Nachbarkanal blutet):

| Test-ID | `blit.s`-Routine | Pixel | RAW_10 | BASE_10 | Netto-Takte (v6) | Takte / Pixel |
|---|---|:---:|---:|---:|---:|---:|
| **`BLT-022`** | `_BlitAdd` 1× (`unpack1632`+`paddusb`+`pack3216`) | 4 | 96 | 30 | **6.6 Cyc** | **1.65** |
| **`BLT-023`** | `_BlitAdd` 2× Unrolled & Interleaved | 8 | 166 | 30 | **13.6 Cyc** | **1.70** |
| **`BLT-024`** | `_BlitKeyAdd` 1× (Color-Key + sat. Add) | 4 | 106 | 30 | **7.6 Cyc** | **1.90** |
| **`BLT-025`** | `_BlitKeyAdd` 2× Unrolled & Interleaved | 8 | 196 | 30 | **16.6 Cyc** | **2.08** |
| **`BLT-026`** | `_BlitKeyTint` (Color-Key + konstanter Tint) | 4 | 96 | 30 | **6.6 Cyc** | **1.65** |
| **`BLT-027`** | `_BlitCross` (256-Stufen Crossfade mit `pmul88`) | 4 | 356 | 30 | **32.6 Cyc** | **8.15** |
| **`BLT-028`** | `KeyMulA` (Color-Key + `MSCALE` + `storeilm`) | 4 | 226 | 30 | **19.6 Cyc** | **4.90** |

---

## 8. Bittiefen-Vergleich: 16-Bit RGB565 vs. 24-Bit RGB888 vs. 32-Bit ARGB8888 (Harness v6)

Für den neuen 2D-Shooter wurden analoge Kernels für **32-Bit ARGB8888** (`BLT-029`..`BLT-036`) und **24-Bit RGB888** (`BLT-037`..`BLT-038`) auf der Vampire V4 vermessen:

| Operation | 16-Bit RGB565 (Cyc/Px) | 24-Bit RGB888 (Cyc/Px) | 32-Bit ARGB8888 (Cyc/Px) | Erkenntnis |
|---|---:|---:|---:|---|
| **Copy** | **0,413** (`MEM-011`, 6,6/16px) | **0,575** (`BLT-037`, 4,6/8px) | **0,825** (`BLT-029`, 6,6/8px) | Bandbreiten-skaliert (2:3:4 Bpp) |
| **Fill** | **0,163** (`MEM-010`, 2,6/16px) | **0,200** (`BLT-038`, 1,6/8px) | **0,325** (`BLT-030`, 2,6/8px) | Bandbreiten-skaliert |
| **Color-Key (sym./0)** | **0,575** (`BLT-015`, 4,6/8px) | *nicht SIMD-aligned* | **1,150** (`BLT-031`, 4,6/4px) | 2× so viele Bytes/px |
| **Color-Key (exakt 32b)** | — | *nicht SIMD-aligned* | **2,150** (`BLT-032`, 8,6/4px) | `pcmpeqw` + `vperm` + `pand` |
| **Additive Blit 1×** | **1,650** (`BLT-022`, 6,6/4px) | **2,700** (`BLT-043`, `vperm` 8px) | **1,300** (`BLT-033`, 2,6/2px) | **32-Bit ist 21 % schneller** als 16-Bit & **2,1× schneller** als 24-Bit |
| **Additive Blit 2×** | **1,700** (`BLT-023`, 13,6/8px) | **3,300** (`BLT-041`, `storec` 2px) | **1,900** (`BLT-034`, 7,6/4px) | 24-Bit 8px-Block schlägt 2px-`storec` |
| **Key + Additive 2×** | **2,075** (`BLT-025`, 16,6/8px) | *via `vperm`* | **2,150** (`BLT-035`, 8,6/4px) | Nahezu gleichauf trotz 2× Bandbreite! |
| **Key + Tint** | **1,650** (`BLT-026`, 6,6/4px) | *via `vperm`* | **1,800** (`BLT-036`, 3,6/2px) | Nahezu gleichauf |
| **Alpha (PMULA)** | **1,650** (`BLT-039`, `unpack1632+pmula+pack3216`) | **2,325** (`BLT-044`, `vperm+pmula` 8px) | **1,300** (`BLT-003`, `PMULA` nativ) | **16-Bit+PMULA schlägt `MSCALE` (4,90) um 2,97×!** 32-Bit bleibt absolut am schnellsten (1,30 Cyc/Px) |

---

## 9. Memory-VEA, 2× Streaming & Parallele ALU+AMMX-Optimierung (`BLT-045`–`BLT-050`)

Durch Anwendung der 68080-Architekturregeln (Kapitel 26 & 28) – **Memory-VEA `(a0)+`**, **2× Streaming (`MEM-013`-Prinzip)** und **ALU/EA-Interleaving** – konnten alle kritischen Blitter-Kernels noch einmal massiv beschleunigt werden:

| ID | Optimierter Kernel | Vorher | Nachher (v6) | Takte / Pixel | Gewinn |
|---|---|---:|---:|---:|---|
| **`BLT-045`** | Copy 2× Streaming (16 B) | 6,6 / 32 B (`BLT-029`) | **2,6 Cyc / 16 B** | **0,325 (16b) / 0,650 (32b)** | **+21 % schneller** als 4× Copy, **+32 %** vs. `move16` |
| **`BLT-046`** | 32-Bit Additive 1× (`paddusb (a0)+`) | 2,6 / 2 px (`BLT-033`) | **1,6 Cyc / 2 px** | **0,800** | **+38,5 % schneller** (Source-Load eliminiert) |
| **`BLT-047`** | 32-Bit Additive 2× (`paddusb (a0)+`) | 7,6 / 4 px (`BLT-034`) | **4,6 Cyc / 4 px** | **1,150** | **+39,5 % schneller** |
| **`BLT-048`** | 16-Bit Alpha (`pmula (a0)+`) | 6,6 / 4 px (`BLT-039`) | **4,6 Cyc / 4 px** | **1,150** | **+30,3 % schneller** – **schlägt sogar 32-Bit `BLT-003` (1,30 Cyc/Px)!** |
| **`BLT-049`** | `BLT-048` + 4× ALU/EA-Befehle | 4,6 Cyc | **4,6 Cyc / 4 px** | **1,150** | **0,0 Takte Zusatzkosten** für ALU/EA parallel zu AMMX! |
| **`BLT-050`** | 16-Bit Additive (`unpack1632 (a0)+`) | 6,6 / 4 px (`BLT-022`) | **4,6 Cyc / 4 px** | **1,150** | **+30,3 % schneller** als Original-`blit.s` `_BlitAdd` |
| **`BLT-051`** | `_BlitKeyAdd` (`unpack1632 (a1)`) | 7,6 / 4 px (`BLT-024`) | **6,6 Cyc / 4 px** | **1,650** | **+13,2 % schneller** als Original-`blit.s` `_BlitKeyAdd` |
| **`BLT-052`** | `_BlitKeyTint` (`unpack1632 (a1)`) | 6,6 / 4 px (`BLT-026`) | **5,6 Cyc / 4 px** | **1,400** | **+15,2 % schneller** (dst-Load gespart) |
| **`BLT-053`** | `_BlitKeyTint` Dual-VEA (`pcmpeqw (a0)+` + `unpack1632 (a1)`) | 6,6 / 4 px (`BLT-026`) | **4,6 Cyc / 4 px** | **1,150** | **+30,3 % schneller** (beide Loads gespart, `diff=0`) |

---

## 8. Hurrican-Code-Optimierung (Harness v6, Bit-exakt verifiziert)

### 7.1 `HUR-001` / `HUR-002` / `HUR-003`: `_DrawTextTint` (`hud.s` Z. 364–386) — `3× divu.w #255` eliminieren

**Ausgangslage (`hud.s`, `_DrawTextTint`):**  
Pro sichtbarem Text-Pixel führt die Originalroutine 3× `mulu.w` + 3× `divu.w #255` aus.

#### 3 Agenten-Vorschläge & Hardware-Ergebnis (Harness v6, Core 11350, `BASELINE = 30`)

| Test-ID | Variante | Roh (10×) | Netto / Pixel | `IEP1 / IEP2` | Bit-Gleichheit (`327.680` Pixel) | Bewertung |
|:---|:---|:---:|:---:|:---:|:---:|:---|
| `HUR-001` | **Original** (`3× mulu.w` + `3× divu.w #255`) | `806` (Min/Max `806/806`) | **77,6 Takte** | `20600 / 2600` | Referenz | Baseline |
| **`HUR-002`** | **Reziprok-Mul** (`mulu.w #257` + `add.l #257` + `swap`) | **`326`** (Min/Max `326/326`) | **29,6 Takte** | **`23600 / 5600`** | **0 Mismatches (100 % exakt)** | **2,62× schneller (netto)** |
| `HUR-003` | **3× Channel-LUT** (`move.w (a2/a3/a4, dX.w*2)`) | `967` (Min `854`, Max `1059`) | 93,7 Takte | `15600 / 7600` | identisch | **Langsamer** durch 3× Indexed-Read-Stalls |

* **Rohlogs:** `log-v6/HUR-001.log`, `log-v6/HUR-002.log`, `log-v6/HUR-003.log`
* **Bit-Gleichheits-Nachweis (V4 Hardware):** `verify_hur002` prüfte alle `65.536` RGB565-Farben × 5 Tint-Kombinationen (`327.680 / 327.680` Pixel, `mismatches=0`).
* **Erkenntnis:** Auf dem 68080 ist die **registerbasierte Reziprok-Multiplikation `(n * 257 + 257) >> 16`** (`HUR-002`) nicht nur **2,62× schneller** als `divu.w #255`, sondern auch **3,17× schneller** als drei skalar indizierte LUT-Zugriffe (`HUR-003`).

### 7.2 `HUR-004` / `HUR-005`: `CompFall` (`hurrican.s` Z. 9498–9529) — Wasserfall-Blending mit AMMX (4 Pixel/Loop)

**Ausgangslage (`hurrican.s`, `CompFall`):**  
Mischt pro Frame $60 \times 60 = 3.600$ Pixel zweier Wasserfall-Schichten skalar per `MASK565` (`$07E0F81F`) und `3× mulu.l`. Durch Loop-Split in `40 px + 20 px` entfällt die `cmp #19`-Verzweigung komplett und beide Teile sind exakt durch 4 teilbar.

| Test-ID | Variante | Roh (10×) | Netto / Pixel | `IEP1 / IEP2` | Bit-Gleichheit (`2.293.760` Pixel) | Bewertung |
|:---|:---|:---:|:---:|:---:|:---:|:---|
| `HUR-004` | **Original** (1 Pixel, `MASK565` + `3× mulu.l`) | `296` (`296/296`) | **26,6 Takte** | `20600 / 4600` | Referenz | Baseline |
| **`HUR-005`** | **AMMX 4-Pixel** (`pmull` + `paddw` + `lsrq` + `por`) | **`546` / 4 px** (`546/546`) | **12,9 Takte** | `39600 / 500` | **0 Mismatches (100 % exakt)** | **2,06× schneller pro Pixel** |

* **Rohlogs:** `log-v6/HUR-004.log`, `log-v6/HUR-005.log`
* **Bit-Gleichheits-Nachweis (V4 Hardware):** `verify_hur005` prüfte `2.293.760` Pixel-Paare über `(w ∈ {0,1,7,16,25,31,32}, k ∈ {0,1,4,7,8})` — `mismatches=0`.

### 7.3 `HUR-006` / `HUR-007` / `HUR-008`: `AudHandler` `.sm` (`sound.s` Z. 706–717) — 8-Bit Signed Audio-Mixing mit Sättigung

**Ausgangslage (`sound.s`, `AudHandler` `.sm`):**  
Mischt im Audio-Interrupt pro aktivem SFX-Slot (`NSLOT = 4`) jeweils `CHUNK = 512` 8-Bit-Signed-Samples mit manuellem Clamping auf `-128..+127`.  
*(Wichtige ISA-Erkenntnis: `vasmm68k_mot -m68080` kennt kein `PADDSB`, wohl aber `PMAXSB`/`PMINSB`/`PSUBB`/`PEOR`/`PADDUSB`/`PSUBUSB`, womit sich vorzeichenbehaftete 8-Bit-Sättigung exakt für 8 Samples parallel abbilden lässt.)*

| Test-ID | Variante | Roh (10×) | Netto / Sample | `IEP1 / IEP2` | Bit-Gleichheit (`65.536` Paare) | Bewertung |
|:---|:---|:---:|:---:|:---:|:---:|:---|
| `HUR-006` | **Original** (1 Byte, `ext.w` + `add.w` + 2× Branch-Clamp) | `116` (`116/116`) | **8,60 Takte** | `7600 / 1600` | Referenz | Baseline |
| `HUR-007` | **AMMX 8-Byte** (`pmaxsb`/`pminsb`/`paddusb`/`psubusb`) | `136` / 8 B (`136/136`) | **1,325 Takte** | `8600 / 500` | **0 Mismatches (100 % exakt)** | **6,49× schneller** |
| **`HUR-008`** | **AMMX 2× Unrolled 16-Byte** | **`216` / 16 B** (`216/216`) | **1,163 Takte** | `17600 / 1500` | identisch | **7,40× schneller** |

* **Rohlogs:** `log-v6/HUR-006.log`, `log-v6/HUR-007.log`, `log-v6/HUR-008.log`
* **Bit-Gleichheits-Nachweis (V4 Hardware):** `verify_hur007` prüfte alle `256 × 256 = 65.536` `(buf, sfx)`-Byte-Paare (`-128..+127`) — `mismatches=0`.

### 7.4 `HUR-009` / `HUR-010`: `_BlitKeyRed` Mode 0 (`blit.s` Z. 4020–4053) — Skeletor Treffer-Flash mit AMMX (4 Pixel/Loop)

**Ausgangslage (`blit.s`, `_BlitKeyRed`):**  
Pro Pixel: Colorkey-Check (`$F81F`), 3× Kanal-Extraktion, `mulu.l`, 3× Branch-Clamping (`cmp/ble`) und `move.w`.

| Test-ID | Variante | Roh (10×) | Netto / Pixel | `IEP1 / IEP2` | Bit-Gleichheit (`262.144` Pixel) | Bewertung |
|:---|:---|:---:|:---:|:---:|:---:|:---|
| `HUR-009` | **Original** (1 Pixel, `cmp` + `3× mulu.l` + Branch-Clamp) | `336` (`336/337`) | **30,60 Takte** | `24600 / 4600` | Referenz | Baseline |
| **`HUR-010`** | **AMMX 4-Pixel** (`pcmpeqw` + `pmull` + `pminuw` + `storeilm`) | **`276` / 4 px** (`276/276`) | **6,15 Takte** | `20600 / 500` | **0 Mismatches (100 % exakt)** | **4,98× schneller pro Pixel** |

* **Rohlogs:** `log-v6/HUR-009.log`, `log-v6/HUR-010.log`
* **Bit-Gleichheits-Nachweis (V4 Hardware):** `verify_hur010` prüfte alle `65.536` RGB565-Farben (inkl. Colorkey `$F81F`) × 4 `RedP`-Stufen (`0, 64, 128, 255`) — `mismatches=0`.

### 7.5 `HUR-011` / `HUR-012`: `_BlitAlpha32` (`blit.s` Z. 887–910) — 32-Stufen Alpha-Blending mit AMMX (4 Pixel/Loop)

**Ausgangslage (`blit.s`, `_BlitAlpha32`):**  
Skalarer Loop pro Pixel: Colorkey-Branch (`$F81F`), zweimaliges 32-Bit RGB565-Aufspreizen (`$07E0F81F`), `2× mulu.l`, `lsr.l #5`, Zurückfalten.

| Test-ID | Variante | Roh (10×) | Netto / Pixel | `IEP1 / IEP2` | Bit-Gleichheit (`524.288` Pixel) | Bewertung |
|:---|:---|:---:|:---:|:---:|:---:|:---|
| `HUR-011` | **Original** (1 Pixel, `cmp` + `2× mulu.l` + `MASK565`) | `246` (`246/246`, Base `30`) | **21,60 Takte** | `17600 / 3600` | Referenz | Baseline |
| **`HUR-012`** | **AMMX 4-Pixel** (`pcmpeqw` + `pmull` + `paddw` + `storeilm`) | **`406` / 4 px** (`406/406`, Base `32`) | **9,35 Takte** | `31600 / 500` | **0 Mismatches (100 % exakt)** | **2,31× schneller (branchless)** |

* **Rohlogs:** `log-v6/HUR-011.log`, `log-v6/HUR-012.log`
* **Bit-Gleichheits-Nachweis (V4 Hardware):** `verify_hur012` prüfte `8` Alpha-Stufen (`1, 4, 8, 12, 16, 20, 27, 31`) × alle `65.536` RGB565-Farben (inkl. `$F81F`) = `524.288` Pixel-Checks — `mismatches=0`.

### 7.6 `HUR-013` / `HUR-014`: `GetTinted` (`tilemap.s` Z. 1023–1042) — Tile-Einfärbung mit AMMX (4 Pixel/Loop, ohne LUT)

**Ausgangslage (`tilemap.s`, `GetTinted`):**  
Pro 20×20-Tile (`400` Pixel): Colorkey-Check (`$F81F`), 3× LUT-Zugriff (`TintT`), Keycol-Kollisionsschutz (`out == $F81F` → `subq #1`), plus Vorberechnung der LUT (`.tr`/`.tg`).

| Test-ID | Variante | Roh (10×) | Netto / Pixel | `IEP1 / IEP2` | Bit-Gleichheit (`524.288` Pixel) | Bewertung |
|:---|:---|:---:|:---:|:---:|:---:|:---|
| `HUR-013` | **Original** (1 Pixel, 3× LUT + 2× Branch) | `226` (`226/226`) | **19,60 Takte** | `19600 / 7600` | Referenz | Baseline |
| **`HUR-014`** | **AMMX 4-Pixel** (`pmull` + `lsrq #8` + `paddw` `$FFFF` Guard) | **`346` / 4 px** (`346/346`) | **7,90 Takte** | `24600 / 500` | **0 Mismatches (100 % exakt)** | **2,48× schneller (ohne LUT)** |

* **Rohlogs:** `log-v6/HUR-013.log`, `log-v6/HUR-014.log`
* **Bit-Gleichheits-Nachweis (V4 Hardware):** `verify_hur014` prüfte `8` Tint-Kombinationen (inkl. `{255,255,255}` und `{255,0,255}` für `out == $F81F -> $F81E`) × alle `65.536` RGB565-Farben = `524.288` Pixel-Checks — `mismatches=0`.

### 7.7 `HUR-015` / `HUR-016`: `SlopeCheck` (`player.s` Z. 1209–1245) — `/ 20` & `% 20` ohne `DIVU`/`DIVUL`

**Ausgangslage (`player.s`, `SlopeCheck`):**  
`divul.l #20, d0:d5` (Quotient + Rest) sowie `2× divu.l #20, d7` pro Aufruf.

| Test-ID | Variante | Roh (10×) | Netto / Aufruf | `IEP1 / IEP2` | Bit-Gleichheit (`81.920` Werte) | Bewertung |
|:---|:---|:---:|:---:|:---:|:---:|:---|
| `HUR-015` | **Original** (`divul.l #20` + `2× divu.l #20`) | `656` (`656/656`, Base `40`) | **61,60 Takte** | `8600 / 500` | Referenz | Baseline |
| **`HUR-016`** | **Reziprok `(x * 52429) >> 20`** + Rest `x - q*20` | **`226`** (`226/226`, Base `37`) | **18,90 Takte** | **`11600 / 5600`** | **0 Mismatches (100 % exakt)** | **3,26× schneller** |

* **Rohlogs:** `log-v6/HUR-015.log`, `log-v6/HUR-016.log`
* **Bit-Gleichheits-Nachweis (V4 Hardware):** `verify_hur016` prüfte alle Koordinaten `x ∈ [0, 81919]` (Quotient und Rest) — `mismatches=0`.

### 7.8 `HUR-017` / `HUR-018` / `HUR-019`: `_DrawLevelLayer` (`.col`, `tilemap.s` Z. 304–371) — Stack-RMW vs. Register-Caching & `mulu.l #800`

**Ausgangslage (`tilemap.s`, `_DrawLevelLayer`):**  
In jeder Tile-Spalte (`.col`/`.skip`) werden 3 Stack-Variablen per Read-Modify-Write aktualisiert (`w_cp(a5)`, `w_bp(a5)`, `w_px(a5)`) und `mulu.l #800, d5` berechnet.

| Test-ID | Variante | Roh (10×) Min/Max/Avg | `IEP1 / IEP2` | Bit-Gleichheit (`65.536` Tiles) | Bewertung |
|:---|:---|:---:|:---:|:---:|:---|
| `HUR-017` | **Original** (Stack-RMW + `mulu.l #800`) | `126 / 257 / 176` (Base `30`) | `7600 / 3500` | Referenz | Baseline |
| **`HUR-018`** | **Register-Cached** (ohne Stack-RMW + `mulu.l #800`) | **`116 / 116 / 116`** (Base `49`) | **`6600 / 4500`** | identisch | **1,52× schneller (AVG), 0 Varianz** |
| `HUR-019` | **Register-Cached + Shift-Add `*800`** | `126 / 5190 / 178` (Base `30`) | `9600 / 6600` | **0 Mismatches** (`verify_hur018`) | **Langsamer:** `mulu.l #800` schlägt 5× Shift-Add |

* **Rohlogs:** `log-v6/HUR-017.log`, `log-v6/HUR-018.log`, `log-v6/HUR-019.log`
* **Erkenntnis:** Auf dem 68080 ist `mulu.l #800` schneller als ein 5-stufiges Shift-Add; der Hauptgewinn in `_DrawLevelLayer` kommt aus dem Eliminieren der Stack-RMW-Zugriffe (`w_cp`/`w_bp`/`w_px` in Register halten) und dem Herausziehen der Modus-`btst #16..#19` vor die Schleife.

### 7.9 `HUR-020` / `HUR-021`: `MhNeck` (`.p`, `enemies.s` Z. 28777–28779) — Vorzeichenbehaftete `/ 24` ohne `DIVS`

**Ausgangslage (`enemies.s`, `MhNeck`):**  
25× pro Frame: `muls.l d7, d0` + `divs.l #24, d0` (Truncation toward zero für positive und negative Werte).

| Test-ID | Variante | Roh (10×) | Netto / Iteration | `IEP1 / IEP2` | Bit-Gleichheit (`98.303` Werte) | Bewertung |
|:---|:---|:---:|:---:|:---:|:---:|:---|
| `HUR-020` | **Original** (`muls.l` + `divs.l #24`) | `256` (`256/256`) | **22,60 Takte** | `3700 / 500` | Referenz | Baseline |
| **`HUR-021`** | **Signed Reziprok** (`muls.l #43691` + `asr.l #20` + `addx.l`) | **`136`** (`136/136`) | **10,60 Takte** | **`7700 / 2500`** | **0 Mismatches (100 % exakt)** | **2,13× schneller (branchless)** |

* **Rohlogs:** `log-v6/HUR-020.log`, `log-v6/HUR-021.log`
* **Bit-Gleichheits-Nachweis (V4 Hardware):** `verify_hur021` prüfte alle `98.303` vorzeichenbehafteten Werte `n ∈ [-49151, +49151]` — `mismatches=0` (wichtig: `asr.l` vor `add.l d1, d1` + `addx.l`, da `asr.l` sonst das `X`-Flag überschreibt!).

### 7.10 `HUR-022` / `HUR-023`: `DrawWater` (`.row`, `hurrican.s` Z. 9396–9407) — Schleifeninvariante `CamX/20` & Zeilen-Offset herausziehen

**Ausgangslage (`hurrican.s`, `DrawWater`):**  
Pro sichtbarer Kachelzeile (25× je Frame) berechnet `.row` erneut `CamX / 20` (`divu.l #20, d2`) sowie `PBlock + row * PMapW * 2` (`mulu.l d4, d0`).

| Test-ID | Variante | Roh (10×) Min/Max/Avg | Netto / Zeile | `IEP1 / IEP2` | Bit-Gleichheit (`11.800` Fälle) | Bewertung |
|:---|:---|:---:|:---:|:---:|:---:|:---|
| `HUR-022` | **Original** (`divu.l #20` + `mulu.l` je Zeile) | `346 / 346 / 346` | **31,60 Takte** | `11600 / 4500` | Referenz | Baseline |
| **`HUR-023`** | **Hoisted `CamX/20` + Inkrement `add.l stride, a2`** | **`36 / 83 / 36`** | **0,60 Takte** | `600 / 1700` | **0 Mismatches (100 % exakt)** | **52,7× schneller (netto)** |

* **Rohlogs:** `log-v6/HUR-022.log`, `log-v6/HUR-023.log`
* **Bit-Gleichheits-Nachweis (V4 Hardware):** `verify_hur023` prüfte `11.800` Kombinationen über `PMapW`, `CamX` und `row 0..24` — `mismatches=0`.

### 7.11 `HUR-024` / `HUR-025`: `UpdateShots` (`weapons.s` Z. 1244–1351) — 2-Pass vs. Single-Pass `ActList`-Aufbau

**Ausgangslage (`weapons.s`, `UpdateShots`):**  
`.go` durchläuft alle `MAXP = 160` Slots; danach durchläuft `.al` nochmals alle `160` Slots, um `ActList` aufzubauen.

| Test-ID | Variante | Roh (10×) | `IEP1 / IEP2` | Bit-Gleichheit (`256` Seeds × `160` Slots) | Bewertung |
|:---|:---|:---:|:---:|:---:|:---|
| `HUR-024` | **Original** (2-Pass: Update + separater `ActList`-Scan) | `66` (`66/66`, Base `30`) | `3996 / 1698` | Referenz | Baseline |
| **`HUR-025`** | **Single-Pass** (Update + `ActList` in 1 Durchlauf) | **`46`** (`46/46`) | `1897 / 698` | **0 Mismatches (100 % exakt)** | **1,43× schneller roh (-30 % Scan-Overhead)** |

* **Rohlogs:** `log-v6/HUR-024.log`, `log-v6/HUR-025.log`
* **Bit-Gleichheits-Nachweis (V4 Hardware):** `verify_hur025` prüfte `256` zufällige Belegungen über `160` Slots inkl. Abschüssen — `mismatches=0`.

### 7.12 `HUR-026` / `HUR-027`: `_BlitKeyRed` Mode 1 (`blit.s` Z. 4055–4073) — Additives Rot-Flash mit AMMX (4 Pixel/Loop)

**Ausgangslage (`blit.s`, `_BlitKeyRed` `.add`):**  
Pro Pixel: Colorkey-Check (`$F81F`), `Src_R * RedP >> 8`, `Dst_R` addieren, Branch-Clamping auf `31` und Zurückpacken mit `Dst & $07FF`.

| Test-ID | Variante | Roh (10×) | Netto / Pixel | `IEP1 / IEP2` | Bit-Gleichheit (`262.144` Pixel) | Bewertung |
|:---|:---|:---:|:---:|:---:|:---:|:---|
| `HUR-026` | **Original** (1 Pixel, `cmp` + `mulu.l` + Branch-Clamp) | `206` (`206/206`) | **17,60 Takte** | `15600 / 5600` | Referenz | Baseline |
| **`HUR-027`** | **AMMX 4-Pixel** (`pcmpeqw` + `pmull` + `pminuw` + `storeilm`) | **`216` / 4 px** (`216/216`) | **4,65 Takte** | `14600 / 500` | **0 Mismatches (100 % exakt)** | **3,78× schneller pro Pixel** |

* **Rohlogs:** `log-v6/HUR-026.log`, `log-v6/HUR-027.log`
* **Bit-Gleichheits-Nachweis (V4 Hardware):** `verify_hur027` prüfte alle `65.536` RGB565-Farben (inkl. `$F81F`) × 4 `RedP`-Stufen (`0, 64, 180, 510`) = `262.144` Pixel-Checks — `mismatches=0`.

### 7.13 `HUR-028` / `HUR-029`: `_BlitAlpha` `.l1p` (`blit.s` Z. 1009–1040) — Safe AMMX 4-Pixel ohne `unpack1632`/`pavgb`/`pack3216`

**Ausgangslage (`blit.s`, `_BlitAlpha`):**  
Die frühere AMMX-Fassung `AlphaKA` wurde wegen einer Line-F-Ausnahme (`unpack1632` / `pavgb` / `pack3216`) deaktiviert; aktiv war die 2-Pixel-Skalarschleife `.l1p` mit `cmp.w` + `swap` + 3× `avg(a,b)`.

| Test-ID | Variante | Roh (10×) | Netto / Pixel | `IEP1 / IEP2` | Bit-Gleichheit (`65.536` Pixel) | Bewertung |
|:---|:---|:---:|:---:|:---:|:---:|:---|
| `HUR-028` | **Original** (2 Pixel, `cmp/swap` + 3× `avg`) | `246` (`246/246`) | **10,80 Takte** | `21600 / 7600` | Referenz | Baseline |
| **`HUR-029`** | **Safe AMMX 4-Pixel** (`peor/pand/lsrq/paddw` + `storeilm`) | **`206`** / 4 px (`206/206`) | **4,40 Takte** | `17600 / 500` | **0 Mismatches (100 % exakt)** | **2,45× schneller (kein Line-F)** |

* **Rohlogs:** `log-v6/HUR-028.log`, `log-v6/HUR-029.log`
* **Bit-Gleichheits-Nachweis (V4 Hardware):** `verify_hur029` prüfte alle `65.536` RGB565-Pixelpaare inkl. Colorkey `$F81F` — `mismatches=0`.

### 7.14 `HUR-030` / `HUR-031`: `.f6p` (`blit.s` Z. 2434–2463) — `3/4 * dst + src` Darkening mit AMMX (8 Pixel/Loop)

**Ausgangslage (`blit.s`, `.f6p` / `.f7p`):**  
Verarbeitet 8 Pixel (16 Bytes) in 4× 32-Bit-Skalarregistern (`lsr.l #2`, `and.l #$39E739E7`, `sub.l`, `add.l`).

| Test-ID | Variante | Roh (10×) | Netto / Pixel | `IEP1 / IEP2` | Bit-Gleichheit (`65.536` Pixel) | Bewertung |
|:---|:---|:---:|:---:|:---:|:---:|:---|
| `HUR-030` | **Original** (8 Pixel skalar, `sub.l`/`add.l`) | `176` (`176/176`, Base `30`) | **1,825 Takte** | `16600 / 10600` | Referenz | Baseline |
| **`HUR-031`** | **AMMX 8-Pixel** (`lsrq`/`pand`/`psubw`/`paddw (a0)+`) | **`136`** / 8 px (`136/136`, Base `40`) | **1,200 Takte** | `10600 / 500` | **0 Mismatches (100 % exakt)** | **1,52× schneller** |

* **Rohlogs:** `log-v6/HUR-030.log`, `log-v6/HUR-031.log`
* **Bit-Gleichheits-Nachweis (V4 Hardware):** `verify_hur031` prüfte alle `65.536` RGB565-Zielpixel mit gültigen 1/4-Lichtquellen — `mismatches=0`.

### 7.15 `HUR-032` / `HUR-033`: `BlockAt` (`player.s` Z. 1098–1136) — Warme `Div20`-LUT vs. `mulu.l #52429`

**Ausgangslage (`player.s`, `BlockAt`):**  
Prüft `x, y` auf `< 0` / `>= DIVMAX`, schlägt bis `16383` in der `Div20`-LUT nach und nutzt ab `16384` `.sx`/`.sy` (`mulu.l #52429`).

| Test-ID | Variante | Roh (10×) | Netto / Aufruf | `IEP1 / IEP2` | Bit-Gleichheit (`82.020` Werte) | Bewertung |
|:---|:---|:---:|:---:|:---:|:---:|:---|
| **`HUR-032`** | **Original (`Div20`-LUT, wenn im Cache)** | **`166`** (`166/166`) | **13,60 Takte** | **`14648 / 2487`** | Referenz | **Original-LUT gewinnt (bei warmem Cache)** |
| `HUR-033` | **Ohne LUT (`cmp #81919` + `mulu.l #52429`)** | `196` (`196/196`) | **16,60 Takte** | `13899 / 500` | **0 Mismatches** (`verify_hur033`) | Langsamer gegen warme LUT |

* **Rohlogs:** `log-v6/HUR-032.log`, `log-v6/HUR-033.log`
* **Erkenntnis:** Im Gegensatz zu 3× RGB-LUTs (`HUR-003`) ist eine einzelne kompakte `Div20`-LUT bei warmem D-Cache auf dem 68080 schneller als zwei `mulu.l #52429` (`13,6` vs. `16,6` Takte). `BlockAt` ist im Hauptpfad bereits optimal; sinnvoll ist nur das Zusammenfassen von `tst.l/bmi` + `cmp.l #81919; bhi` sowie `lsr.l dX` im `.sx`/`.sy`-Pfad.

### 7.16 `HUR-034` / `HUR-035`: `Boxes` (`beam.s` Z. 229–257) — Strahl-Segmente ohne `movem`-Spill & doppeltes `FIXINT`

**Ausgangslage (`beam.s`, `Boxes`):**  
Pro Segment (bis 17×): 2× `FIXINT`, `movem.l d2-d3` um `BlockAt` (obwohl `BlockAt` `d2/d3` gar nicht nutzt) und `2× lsl.l #5` in der Schleife.

| Test-ID | Variante | Roh (10×) | Netto / Segment | `IEP1 / IEP2` | Bit-Gleichheit (`68` Segmente) | Bewertung |
|:---|:---|:---:|:---:|:---:|:---:|:---|
| `HUR-034` | **Original** (2× `FIXINT` + `movem` + `2× lsl.l #5`) | `286` (`286/286`) | **25,60 Takte** | `20600 / 6500` | Referenz | Baseline |
| **`HUR-035`** | **Hoisted `32*dir` + 1× `FIXINT` + ohne `movem`** | **`96`** (`96/116/96`) | **6,60 Takte** | `6600 / 4500` | **0 Mismatches (100 % exakt)** | **3,88× schneller** |

* **Rohlogs:** `log-v6/HUR-034.log`, `log-v6/HUR-035.log`
* **Bit-Gleichheits-Nachweis (V4 Hardware):** `verify_hur035` prüfte `4` Richtungen × `17` Segmente = `68` Segmente — `mismatches=0` (wichtig: Start-Shift per `asr.l #1` für negative Richtungen).

### 7.17 `HUR-036` / `HUR-037`: `_WeaponsDraw` (`weapons.s` Z. 1781–1828) — `ActList` + `move.w/ext.l` statt `FIXINT`

**Ausgangslage (`weapons.s`, `_WeaponsDraw`):**  
Iteriert über alle `MAXP = 160` Slots (obwohl `ActList`/`ActN` bereits alle aktiven `Shot*`-Zeiger enthält) und entpackt pro Schuss `P_X`, `P_Y` und `WAnim` per `move.l + swap + ext.l`.

| Test-ID | Variante | Roh (10×) | Netto / Schuss | `IEP1 / IEP2` | Bit-Gleichheit (`65.536` Werte) | Bewertung |
|:---|:---|:---:|:---:|:---:|:---:|:---|
| `HUR-036` | **Original** (`move.l + swap + ext.l` ×3) | `126` (`126/126`) | **9,60 Takte** | `9600 / 3500` | Referenz | Baseline |
| **`HUR-037`** | **`move.w + ext.l` ×2 + hoisted `WAnim`** | **`96`** (`96/96`) | **6,60 Takte** | `6600 / 2500` | **0 Mismatches (100 % exakt)** | **1,45× schneller (+ `ActList`-Skip)** |

* **Rohlogs:** `log-v6/HUR-036.log`, `log-v6/HUR-037.log`
* **Bit-Gleichheits-Nachweis (V4 Hardware):** `verify_hur037` prüfte alle `65.536` 16.16-High-Words (`move.w + ext.l` vs. `move.l + swap + ext.l`) — `mismatches=0`.

### 7.18 `HUR-038` / `HUR-039`: `TcFill` (`.kr`, `tilemap.s` Z. 628–638) — `10× move.l` vs. `5× store` (Speicherbandbreite)

**Ausgangslage (`tilemap.s`, `TcFill`):**  
Füllt pro Kachel 20 Zeilen à 40 Bytes mit `KEYCOL` (`10× move.l d3, (a1)+`).

| Test-ID | Variante | Roh (10×) Min/Max/Avg | `IEP1 / IEP2` | Bit-Gleichheit (`20` Words) | Bewertung |
|:---|:---|:---:|:---:|:---:|:---|
| `HUR-038` | **Original** (`10× move.l d3, (a1)+`) | `66 / 192 / 82` | `8800 / 500` | Referenz | FastRAM-Write-Bandwidth-limitiert |
| `HUR-039` | **AMMX `5× store e0, (a1)+`** | `66 / 230 / 83` | **`3600 / 500`** | **0 Mismatches** (`verify_hur039`) | Gleich schnell (spart nur `IEP1`-Slots) |

* **Rohlogs:** `log-v6/HUR-038.log`, `log-v6/HUR-039.log`
* **Erkenntnis:** Reines Memory-Fill ist bei 40 Bytes/Zeile bereits durch den FastRAM-Write-Bus gesättigt (`5,2`–`5,3` Takte netto für 40 Bytes); der Tausch lohnt sich geschwindigkeitsmäßig nicht.

### 7.19 `HUR-040` / `HUR-041`: `WheelUpdate` (`player.s` Z. 355, 363) — 32-Bit `DIVS.L #3` ohne Division

**Ausgangslage (`player.s`, `WheelUpdate`):**  
Teilt 16.16-Geschwindigkeiten (`YSpd`) per `divs.l #3, d0` (Truncation toward zero für volle 32-Bit signed Zahlen).

| Test-ID | Variante | Roh (10×) | Netto / Aufruf | `IEP1 / IEP2` | Bit-Gleichheit (`201.002` Werte) | Bewertung |
|:---|:---|:---:|:---:|:---:|:---:|:---|
| `HUR-040` | **Original** (`divs.l #3, d0`) | `226` (`226/226`, Base `31`) | **19,50 Takte** | `1600 / 500` | Referenz | Baseline |
| **`HUR-041`** | **64-Bit Signed Reziprok** (`muls.l #$55555556, d1:d0` + `addx.l`) | **`106`** (`106/106`, Base `31`) | **7,50 Takte** | `5600 / 500` | **0 Mismatches (100 % exakt)** | **2,60× schneller (voller 32-Bit-Bereich)** |

* **Rohlogs:** `log-v6/HUR-040.log`, `log-v6/HUR-041.log`
* **Bit-Gleichheits-Nachweis (V4 Hardware):** `verify_hur041` prüfte `201.002` vorzeichenbehaftete 32-Bit-Werte (inkl. 16.16-Fixkomma-Geschwindigkeiten) — `mismatches=0`.

### 7.20 `HUR-042`–`HUR-047`: `_BlitCopy`, `_BlitKey`, `_BlitAdd` (`blit.s` Z. 184–458) — Stride-Hoisting, `dbra` & `TailMask`-Skip

**Ausgangslage (`blit.s`):**  
Alle drei Kern-Blitter-Routinen laden pro Zeile `TailMask(pc)` auch bei `d7 = 0` (trifft auf 20×20-Tiles und 640×480-Hintergrund zu), kopieren pro Zeile `a0/a1` nach `a2/a3`, berechnen die Quad-Zähler (`lsr.l`/`and.l`) jede Zeile neu und nutzen `subq.l #1; bne.s` statt `dbra`.  
**Testsetup:** 4-Zeilen-Slice × 20 Pixel (`d6 = 5`, `d7 = 0`, `srcPitch = 40`, `dstPitch = 1280`), 10× im Harness (= 40 Zeilen = 800 Pixel, `BASELINE = 30`).

| Test-ID | Routine & Variante | Roh (40 Zeilen) | Netto / Zeile | `IEP1 / IEP2` | Bit-Gleichheit (`w=1..65`, `8.450` px) | Bewertung |
|:---|:---|:---:|:---:|:---:|:---:|:---|
| `HUR-042` | `_BlitCopy` **Original** | `1496` (`1496/1511`) | `36,65` Takte | `111600 / 39700` | Referenz | Baseline |
| **`HUR-043`** | `_BlitCopy` **Opt** (Stride, `dbra`, `TailMask` nur bei `d7!=0`) | **`1456`** (`1456/1471`) | **`35,65` Takte** | `103600 / 25700` | **0 Mismatches** | **-1,00 Takt/Zeile (-14,5 % Ops)** |
| `HUR-044` | `_BlitKey` **Original** | `1496` (`1496/1496`) | `36,65` Takte | `127600 / 38600` | Referenz | Baseline |
| **`HUR-045`** | `_BlitKey` **Opt** (Stride, `dbra`, `TailMask` nur bei `d7!=0`) | **`1406`** (`1406/1407`) | **`34,40` Takte** | `110600 / 29700` | **0 Mismatches** | **-2,25 Takte/Zeile (1,065×, -15,6 % Ops)** |
| `HUR-046` | `_BlitAdd` **Original** | `2468` (`2461/2513`) | `60,95` Takte | `219600 / 43700` | Referenz | Baseline |
| **`HUR-047`** | `_BlitAdd` **Opt** (Stride, `dbra`, `TailMask` nur bei `d7!=0`) | **`2326`** (`2326/2326`) | **`57,40` Takte** | `202600 / 32700` | **0 Mismatches** | **-3,55 Takte/Zeile (1,062×, -10,6 % Ops)** |

* **Rohlogs:** `log-v6/HUR-042.log` .. `log-v6/HUR-047.log`
* **Bit-Gleichheits-Nachweis (V4 Hardware):** `verify_blit_core` prüfte alle Breiten `w = 1..65` (alle Restfälle `d7 = 0..3`, gerade/ungerade `d6`, Guard-Wörter) — `mm_copy=0`, `mm_key=0`, `mm_add=0`.

### 7.21 `HUR-048`–`HUR-051`: `MSCALE` & `LightA`/`KeyMulA`/`KeyShadeA`/`AddAA` (`blit.s` Z. 4512–4740)

**Ausgangslage (`blit.s`):**  
Das Shader-Makro `MSCALE` (genutzt in `LightA`, `KeyMulA`, `KeyShadeA`, `AddAA`) verschiebt den Rot-Kanal mit zwei teuren Multiplikationen (`pmul88 e11` + `pand e12` für `>>8` und `pmull e8` für `<<8`) — 14 Instruktionen / 5 Multiplikationen je 4 Pixel. Zusätzlich lädt jede der 4 Routinen per `MCONST` + `TailMask` **9× 64-Bit-Konstanten pro Zeile** (obwohl `DrawFrame` in `Forbid()..Permit()` läuft).

| Test-ID | Kernel & Variante | Roh | Netto | `IEP1 / IEP2` | Bit-Gleichheit (V4) | Bewertung |
|:---|:---|:---:|:---:|:---:|:---:|:---|
| `HUR-048` | `MSCALE` **Original** (14 Ops / 5× Mul, 4 px) | `216` (`216/216`, Base `30`) | `18,60` Takte / 4 px | `14600 / 500` | Referenz | Baseline |
| **`HUR-049`** | `MSCALE` **Opt** (`lsrq d1` + `lslq d1`, 12 Ops / 3× Mul) | **`166`** (`166/166`, Base `30`) | **`13,60` Takte / 4 px** | `13600 / 500` | **0 Mismatches (`2.162.688` Fälle)** | **1,37× schneller** |
| `HUR-050` | `KeyMulA` **Original** (40 Zeilen × 20 px, `MCONST`/Zeile) | `5216` Min / `5424` Avg | `129,65` Takte / Zeile | `419600 / 52600` | Referenz | Baseline |
| **`HUR-051`** | `KeyMulA` **Opt** (hoisted 5-Load `MCONST`, `MSCALE`, Stride, `dbra`) | **`4116`** Min / `4191` Avg | **`102,15` Takte / Zeile** | `369600 / 33600` | **0 Mismatches (`2.162.688` Fälle)** | **1,27×–1,29× schneller** |

* **Rohlogs:** `log-v6/HUR-048.log` .. `log-v6/HUR-051.log`
* **Bit-Gleichheits-Nachweis (V4 Hardware):** `verify_mscale` prüfte erschöpfend alle `65.536` RGB565-Farben × alle `33` Faktoren (`f = 0..32`) = `2.162.688` Kombinationen — `mismatches=0`.
* **68080/vasm-Befund:** `lsrq`/`lslq` benötigen den Shift-Count im Datenregister (`d1 = 8`), und AMMX-`load`-Konstanten müssen im `data`-Segment (absolute Adressierung) liegen, da `vasm`-PC-relatives `fe7a` gegenüber Core 11350 um 2 Bytes versetzt ist.

### 7.22 `HUR-052`–`HUR-055`: `_TileCache` Pass 1 & Pass 2 (`tilemap.s` Z. 477–588) — Eliminierung von ~975 `DIVUL.L`/Frame

**Ausgangslage (`tilemap.s`, `_TileCache`):**  
`_TileCache` läuft auf **jedem** Frame über alle `33×25 = 825` sichtbaren Kacheln:
- **Pass 1 (`.pc`, 825×/Frame):** führt pro Kachel ein 32-Bit-`divul.l #34` (`sx = tx % 34`) und ein zeileninvariantes `mulu.l #34` (`TcSY * 34`) aus, baut den Key `(tx<<16)|ty` neu und lädt `SlKey`/`SlAnim`/`SlPh` neu.
- **Pass 2 (`.cr`/`.re`, ~50–150 Runs/Frame):** teilt `TcRowS` in `.re` per `divul.l #34` wieder durch `34` zurück auf `sy`, um `TcBuf + sy*27200` und `r*20 - TcOY` pro Run neu zu berechnen.

| Test-ID | Abschnitt & Variante | Roh (10×, Base `30`) | Netto / Iteration | `IEP1 / IEP2` | Bit-Gleichheit (`412.500` Kacheln) | Speedup |
|:---|:---|:---:|:---:|:---:|:---:|:---|
| `HUR-052` | Pass 1 `.pc` **Original** (`divul.l #34` + `mulu.l #34` + Key) | `326` (`326/326`) | `29,60` Takte / Kachel | `9600 / 1500` | Referenz | Baseline |
| **`HUR-053`** | Pass 1 `.pc` **Opt** (inkr. `sx`/`slot`-Wrap + `key += $10000`) | **`76`** (`76/76`) | **`4,60` Takte / Kachel** | `4600 / 1600` | **0 Mismatches** | **6,43× schneller (-20.625 Takte/Frame)** |
| `HUR-054` | Pass 2 `.re` **Original** (`divul.l #34` + 4× `mulu.l`) | `416` (`416/416`) | `38,60` Takte / Run | `12600 / 6500` | Referenz | Baseline |
| **`HUR-055`** | Pass 2 `.re` **Opt** (hoisted `TcBufRow` & `TcRowY`, 0 Div) | **`136`** (`136/136`) | **`10,60` Takte / Run** | `6600 / 3500` | **0 Mismatches** | **3,64× schneller (-28 Takte/Run)** |

* **Rohlogs:** `log-v6/HUR-052.log` .. `log-v6/HUR-055.log`
* **Bit-Gleichheits-Nachweis (V4 Hardware):** `verify_tilecache` prüfte 500 Kamerapositionen × 25 Zeilen × 33 Spalten = `412.500` Kacheln (`sy`, `sx`, `slot`, `key`, `TcBufRow`, `TcRowY`) — `mismatches=0`.

### 7.23 `HUR-056`–`HUR-060`: `_BlitAdd` Zero-Skip (Wolken) & `_BlitKey` All-Key-Skip (Negativbefund)

**Ausgangslage (`blit.s`, `hintergrund = 14,5 ms` im `F1`-Profiler):**  
Geprüft wurden Software-Early-Outs für 8-Pixel-Blöcke (16 Bytes) in `_BlitAdd` (schwarze Wolken-Quads `$0000`) und `_BlitKey` (transparente Key-Quads `$F81F`).

| Test-ID | Kernel & Variante | Roh (10×, Base `30`) | Netto / 8 px | `IEP1 / IEP2` | Bewertung |
|:---|:---|:---:|:---:|:---:|:---|
| `HUR-056` | `_BlitAdd` **Original** (8 px `.a2`, ohne Skip) | `166` (`166/166`) | `13,60` Takte | `13600 / 1500` | Baseline (plus FastRAM-RMW auf `dst`) |
| **`HUR-057`** | `_BlitAdd` **Zero-Skip (alles `$0000`)** | **`96`** (`96/96`) | **`6,60` Takte** | `9588 / 895` | **2,06× schneller + 0 FastRAM-RMW auf `dst`!** |
| `HUR-058` | `_BlitAdd` **Zero-Skip (nicht-Null, Worst Case)** | `226` (`226/226`) | `19,60` Takte | `19600 / 1500` | `+6,0` Takte Overhead; lohnt sich bei Wolken/Explosionen |
| `HUR-059` | `_BlitKey` **Original** (8 px `.k2`, `2× storeilm`) | **`76`** (`76/76`) | **`4,60` Takte** | `4600 / 500` | **Gewinner** (`storeilm` in HW schneller als SW-Check) |
| `HUR-060` | `_BlitKey` **All-Key-Skip (alles Key)** | `126` (`126/126`) | `9,60` Takte | `9801 / 1599` | **2,09× langsamer → verworfen!** |

* **Rohlogs:** `log-v6/HUR-056.log` .. `log-v6/HUR-060.log`
* **Erkenntnis:** Auf 68080 Core 11350 ist `storeilm` in `_BlitKey` so schnell (`0,575` Takte/px in L1), dass ein Software-All-Key-Check langsamer ist (`HUR-060` verworfen). In `_BlitAdd` dagegen halbiert der 8-px-Zero-Skip (`HUR-057`) die L1-Zeit (`2,06×`) und eliminiert in kaltem FastRAM (Wolken-Ebene) komplette Ziel-Lese-/Schreibtransaktionen.

### 7.24 `HUR-061` / `HUR-062`: `_BlitAdd` `tst.l`-Early-Exit vs. `4× or.l`

| Test-ID | Variante | Roh (10×) | Netto / 8 px | `IEP1 / IEP2` | Bewertung |
|:---|:---|:---:|:---:|:---:|:---|
| `HUR-061` | `_BlitAdd` `tst.l`-Early-Exit (**alles `$0000`**) | `126` (Base `30`) | `9,60` Takte | `11594 / 696` | **1,42× schneller** als Original + 0 FastRAM-RMW |
| **`HUR-062`** | `_BlitAdd` `tst.l`-Early-Exit (**nicht-Null**, inkl. `move.l`-Setup) | **`206`** (Base `31`) | **`~16,5` Takte** | `16600 / 1500` | **~3 Takte/8 px schneller** als `4× or.l` (`HUR-058`: `19,60`) auf sichtbaren Wolken! |

* **Rohlogs:** `log-v6/HUR-061.log`, `log-v6/HUR-062.log`

### 7.25 `HUR-063`–`HUR-066`: `DrawParts` `DIVS.L #$168000` & `_BlitFallF` `.f1p..f7p` AMMX

| Test-ID | Kernel & Variante | Roh (10×, Base `30`) | Netto | `IEP1 / IEP2` | Bit-Gleichheit (V4) | Speedup |
|:---|:---|:---:|:---:|:---:|:---:|:---|
| `HUR-063` | `DrawParts` **Original** (`divs.l #$168000`) | `236` (`236/236`) | `20,60` Takte / Partikel | `2600 / 500` | Referenz | Baseline |
| **`HUR-064`** | `DrawParts` **Reziprok** (`muls.l #$5B05B05C` + `asr #19` + `addx`) | **`106`** (`106/106`) | **`7,60` Takte / Partikel** | `5600 / 3600` | **0 Mismatches (`201.000+` Winkel)** | **2,71× schneller** |
| `HUR-065` | `_BlitFallF` `.f5p` **Original** (Skalar 8 px, `5/8*dst + src`) | `216` (`216/216`) | `18,60` Takte / 8 px | `20600 / 14600` | Referenz | Baseline |
| **`HUR-066`** | `_BlitFallF` `.f5p` **AMMX 8-px SIMD** (`lsrq`/`pand`/`paddw`) | **`176`** (`176/176`) | **`14,60` Takte / 8 px** | `14600 / 500` | **0 Mismatches (`196.608` px)** | **1,27× schneller (-57 % Ops)** |

* **Rohlogs:** `log-v6/HUR-063.log` .. `log-v6/HUR-066.log`
* **Bit-Gleichheits-Nachweis (V4 Hardware):** `verify_hur064_066` prüfte `201.000+` vorzeichenbehaftete 16.16-Winkel (`mm_rot=0`) sowie `3 × 65.536 = 196.608` RGB565-Pixel für `.f3p`/`.f5p`/`.f7p` (`mm_f=0`).

### 7.26 Cold-FastRAM-Messung (`640×480 = 614 KB`, außerhalb 32 KB L1-Cache): DDR3-Burst-Unrolling

**Messprogramm (`bench_fastram_v2`, `CCC` unter `Forbid()` auf V4 `Core 11350`, ~85 MHz):**

| Routine & Variante | Takte (640×480 = 614 KB) | Zeit (`ms`) | Bewertung |
|:---|:---:|:---:|:---|
| `_BlitCopy` `4-Quad` (`32 B`/Iter) | `351.309` | `4,13 ms` | Baseline |
| **`_BlitCopy` `8-Quad` (`64 B` DDR3-Burst)** | **`330.008`** | **`3,88 ms`** | **`-6,5 %` (-21.301 Takte/Screen) → übernommen (`.q8`)** |
| `_BlitKey` `2-Quad` (`16 B`, 0 % Key) | `346.225` | `4,07 ms` | Baseline |
| **`_BlitKey` `4-Quad` (`32 B` DDR3-Burst, 0 % Key)** | **`317.705`** | **`3,73 ms`** | **`-9,0 %` (-28.520 Takte/Screen) → übernommen (`.k4`)** |
| **`_BlitAdd` `2-Quad` ohne Zero-Skip (100 % nicht-Null)** | **`625.264`** | **`7,35 ms`** | **`-21 %` (`-1,96 ms`!) schneller als `4× or.l`-Skip (`791.644` / `9,31 ms`) auf dichten Wolken → übernommen** |
| `_BlitAdd` `4-Quad` ohne Zero-Skip (100 % nicht-Null) | `627.607` | `7,38 ms` | Gleich schnell wie `2-Quad` (Bandbreite gesättigt) |

### 7.27 `HUR-067`–`HUR-074`: `DrawLamp` / `DkAB` & `DkA1..DkA7` (Cave-Taschenlampe, `blit.s` Z. 4765–4945, `hurrican.s` Z. 4052–4195)

**Ausgangslage (Cave-Level, `F1`-Profiler `hurrican_v11`):**
`VORDERGRUND = 10,4 ms` (davon ~`10,1 ms` in `DrawLamp`). Pro Frame erfolgen ~`7.200` `LmSpan`-Aufrufe; jedes `DkA1..DkA7` ruft `bsr DkAPre` auf (3× 64-Bit-`load` für `DkMasks` + `moveq #1/#2/#3` + `lea TailMask`) und verarbeitet nur 1 Quad (4 px) je Iteration mit `subq.l #1,d0; bne.s`.

| Test-ID | Kernel & Variante | Roh (10×, Base `30`) | Netto / Span | `IEP1 / IEP2` | Bit-Gleichheit (V4) | Bewertung |
|:---|:---|:---:|:---:|:---:|:---:|:---|
| `HUR-067` | `DkAB` **Original** (64 px, `load 0` + 1-Quad `subq/bne`) | `336` Min / `392` Avg | `30,6` Min / `36,2` Avg | `26599 / 42598` | Referenz | `subq/bne` paart kostenlos in IEP2 während 2-cyc L1-Store |
| `HUR-068` | `DkABF` **8-Quad 64-Byte-Burst** (`e0=0` gehoistet, `dbra`) | `376` Min / `420` Avg | `34,6` Min / `39,0` Avg | `29600 / 3600` | **0 Mismatches** | `-52 %` Ops; in kaltem FastRAM (volle Zeilen/Ränder) ideal |
| `HUR-073` | `DkABF` **1-Quad `dbra`** (`e0=0` gehoistet) | `466` Min / `501` Avg | `43,6` Min / `47,1` Avg | `23600 / 18600` | — | `store + dbra` stallt in L1 → verworfen |
| `HUR-074` | `DkABF` **4-Quad `dbra`** (`e0=0` gehoistet) | `396` Min / `441` Avg | `36,6` Min / `41,1` Avg | `29600 / 5600` | — | Langsamer als `8-Quad` (`376`) |
| `HUR-069` | `DkA4` **Original** (32 px, `DkAPre`-Loads + 1-Quad `subq/bne`, ohne `bsr/rts`) | `536` (`536/536`) | `50,60` Takte (~`62` mit `bsr/rts`) | `46600 / 21600` | Referenz | Baseline |
| **`HUR-070`** | **`DkA4F` Opt** (32 px, 2-Quad Interleaved, gehoistete Konst., `dbra`) | **`486`** (`486/486`) | **`45,60` Takte** | `38600 / 9600` | **0 Mismatches (`1.056` Fälle)** | **`-29 %` Ops; ~`1,36×` schneller inkl. gespartem `bsr/rts`** |
| `HUR-071` | `DkA5` **Original** (32 px, `DkAPre`-Loads + 1-Quad `subq/bne`, ohne `bsr/rts`) | `836` (`836/836`) | `80,60` Takte (~`92` mit `bsr/rts`) | `70600 / 21600` | Referenz | Baseline |
| **`HUR-072`** | **`DkA5F` Opt** (32 px, 2-Quad Interleaved, gehoistete Konst., `dbra`) | **`806`** (`806/806`) | **`77,60` Takte** | `62600 / 9600` | **0 Mismatches (`1.056` Fälle)** | **`-22 %` Ops; ~`1,19×` schneller inkl. gespartem `bsr/rts`** |

* **Rohlogs:** `log-v6/HUR-067.log` .. `log-v6/HUR-074.log`
* **Bit-Gleichheits-Nachweis (V4 Hardware):** `verify_dka` prüfte alle 8 Stufen (`DkA1F..DkA7F`, `DkABF`) × Breiten `0..65` × Alignments `0/2` Bytes = `1.056` Testfälle (`34.320` Pixel + Vor-/Nach-Sentinels) gegen `DkRef` — **`0 Mismatches`**.
* **Integration (`hurrican_v12`):** `DkAInit` einmalig vor der 480-Zeilen-Schleife in `DrawLamp` (`e0=0, e1..e3, d5=1, d6=2, d7=3, a2=TailMask`, `d4=LmX`), direkter 20×64-Byte-Burst für komplett schwarze Zeilen (`.blk`), `d3==0`-Skip vor `LmSpan`, sowie Aktivierung von `_TileCache` im Cave-Level (Entfernung von `tst.l CovLOn; bne.s .tl` in `hurrican.s`).























---

## 8. Hotspot-Optimierungen v14: EnemiesDraw Sprungtabelle, TileCache CovL-Culling & LmSpan

Basierend auf der systematischen 5-Subagenten-Codebase-Analyse und Rücksprache mit 3 FreeLLM-Agenten (qwen2.5-coder-32b, uto, usion) wurden die Top-Rendering-Engpässe oberhalb der Blitter in Hurrican v14 angegangen.

### 8.1 Messergebnisse Harness v6 (Vampire V4, Apollo Core 11350 @ 85 MHz)

| Sequenz | Kernel / Beschreibung | Min (Raw) | Netto (Zyklen/Iter) | IEP1 / IEP2 | Bit-Gleichheit | Speedup / Bewertung |
|:---|:---|:---:|:---:|:---:|:---:|:---|
| HUR-075 | _TileCache Phase 2 Setup **Original** (3× mulu.l #40/#20, 256 Min) | 256 (256/256) | 22,60 Takte | 16600 / 2600 | Referenz | Baseline |
| HUR-076 | _TileCache Phase 2 Setup **Shift-Add** (0× mulu.l, 6 Shifts + 3 Adds) | 266 (266/266) | 23,60 Takte | 23600 / 2600 | Identisch (0 Mismatches) | **+3,9 % langsamer** (+42 % IEP1). Hardware-mulu.l auf 68080 schneller! |
| HUR-077 | _EnemiesDraw Dispatch **Original** (Lineare cmp.l #ID_ Kette) | 366 (366/366) | 33,60 Takte | 33600 / 2600 | Referenz | Baseline |
| **HUR-078** | **_EnemiesDraw Dispatch Jump Table** (O(1) .drw_jt, 1001 Einträge) | **96** (96/96) | **6,60 Takte** | **6600 / 5600** | **0 Mismatches** | **5,10× schneller (-80,4 % Takte, -80,4 % IEP1)!** |

* **Rohlogs:** log-v6/HUR-075.log, log-v6/HUR-076.log, log-v6/HUR-077.log, log-v6/HUR-078.log

### 8.2 Kern-Erkenntnisse & Implementierung in Hurrican v14

1. **_EnemiesDraw Sprungtabelle (.drw_jt):**
   * **Bisher:** 116 lineare cmp.l #ID_..., d4 / beq/bne Vergleiche für jeden sichtbaren Gegner in jedem Frame (bis zu 250 Zyklen pro Gegner).
   * **Neu:** 1001-Einträge Sprungtabelle .drw_jt in enemies.s. Dispatch erfolgt mit cmp.l #MAXID, d4; bhi .ex; lea .drw_jt, a0; move.l (a0, d4.l*4), a0; jmp (a0).
   * **Ergebnis:** Reduktion von 366 auf 96 Zyklen im Benchmark (**5,1× Speedup**, 80,4 % weniger Instruktionen).
   * **EN_ACTIVE-Vorabfilter:** 	st.l EN_ACTIVE(a2); beq .nwl am Kopf der Gegnerschleife überspringt hunderte ruhende Gegner im Level sofort in 1 Takt, ohne Koordinaten- oder Kollisionsberechnung.

2. **_TileCache CovL-Culling im Cave-Level (	ilemap.s):**
   * **Bisher:** In der Höhle wurden alle Kacheln in den Bildspeicher geblittet, obwohl DrawLamp unbeleuchtete Bereiche nachträglich komplett schwarz überzeichnet.
   * **Neu:** In Phase 2 (.w0: und .rl:) wird CovL[r*34 + c] geprüft wenn CovLOn != 0. Unbeleuchtete Kacheln werden sofort übersprungen (.cull), Kachelläufe vorzeitig beendet.
   * **Ergebnis:** 60 % bis 80 % aller Kachelblits (_BlitCopy / _BlitKey) in der Höhle entfallen vollständig.

3. **LmSpan Streamlining (hurrican.s):**
   * lea LmJmp, a0; move.l (a0, d3.l*4), d2 gestrafft zu move.l (LmJmp, d3.l*4), d2; beq.s .x; move.l d2, a0.


---

## 9. Coverage- und Hintergrund-Hotspots: Hurrican v15 (HUR-079 .. HUR-084)

Messungen auf echter Apollo 68080 Vampire V4 Hardware (Core 11350) mit Harness v6 (100 Durchläufe, 10x innere Schleife, Baseline kompensiert):

| Sequenz | Kernel / Beschreibung | Min (Raw) | Netto (Zyklen/Iter) | IEP1 / IEP2 | Speedup / Bewertung |
|:---|:---|:---:|:---:|:---:|:---|
| `HUR-079` | `_BuildCover` Zelle **Original** (`bsr.s .op` + Memory `CovPh`) | 916 | 88,60 Takte | 32602 / 11096 | Baseline |
| **`HUR-080`** | **`_BuildCover` Zelle Inlined** (Register `CovPh` + kein Subroutine-Call) | **767** | **73,70 Takte** | **15300 / 11000** | **-53,1 % Instruktionen**, -16,3 % Takte |
| `HUR-081` | Puffer-Nullen 64B **Paired `clr.l (a2)+`** (8 Paare = 16 Stores) | 96 | 6,60 Takte | 14800 / 500 | Dual-Issue Superskalar füllt Write-Buffer (~0.4 Takte/Store) |
| **`HUR-082`** | Puffer-Nullen 64B **AMMX `store e0,(a2)+`** (8 native 64-Bit Stores) | **96** | **6,60 Takte** | **6600 / 500** | **-55,4 % Instruktionen** bei gleicher Bus-Sättigung! |
| `HUR-083` | `BlkCov` Blockabdeckung **Original** (7-Reg `movem`, 2× `mulu.w #3277`) | 366 | 33,60 Takte | 22600 / 2500 | Baseline |
| **`HUR-084`** | **`BlkCov` Blockabdeckung Optimiert** (Verschlankter Reg-Save + Fast-Div) | **306** | **27,60 Takte** | **22600 / -500** | **-16,4 % Takte pro Block** (bis zu 600 Blöcke/Frame) |

* **Rohlogs:** `log-v6/HUR-079.log`, `log-v6/HUR-080.log`, `log-v6/HUR-081.log`, `log-v6/HUR-082.log`, `log-v6/HUR-083.log`, `log-v6/HUR-084.log`

### 9.1 Vertiefung Puffer-Löschen: Paired `clr.l` + `dbra.l` auf Pipe 2 (HUR-085 .. HUR-087)

Vergleich kompakter Schleifen (kein Unrolling, 64 Bytes Nullen):

| Sequenz | Kernel / Beschreibung | Min (Raw) | Netto (Zyklen/Iter) | IEP1 / IEP2 | Bewertung |
|:---|:---|:---:|:---:|:---:|:---|
| `HUR-086` | Single `clr.l (a2)+; dbra.l d0,.l` (16 Iterationen) | 406 | 37,60 Takte | 17600 / 16600 | 1 Store pro Branch, hohe Pipeline-Kosten |
| **`HUR-085`** | **Paired `clr.l (a2)+; clr.l (a2)+; dbra.l d0,.l`** (8 Iterationen) | **246** | **21,60 Takte** | **17600 / 9700** | **-39,4 % Takte!** Schleifenkörper exakt 8 Bytes (1 Quadword im I-Cache) |
| `HUR-087` | AMMX `store e0,(a2)+; dbra.l d0,.l` (8 Iterationen) | **246** | **21,60 Takte** | **9600 / 8600** | Exakt gleiche Takte wie HUR-085 bei halber Instruktionsdekodierung |

* **Rohlogs:** `log-v6/HUR-085.log`, `log-v6/HUR-086.log`, `log-v6/HUR-087.log`

### 9.2 Partikel- und Projektil-Hotspots: Hurrican v16 (HUR-088 .. HUR-091)

Messungen auf echter Apollo 68080 Vampire V4 Hardware (Core 11350) mit Harness v6:

| Sequenz | Kernel / Beschreibung | Min (Raw) | Netto (Zyklen/Iter) | IEP1 / IEP2 | Speedup / Bewertung |
|:---|:---|:---:|:---:|:---:|:---|
| `HUR-088` | `DrawParts` Partikel-Setup **Original** (`mulu.l #64` + `PtTopMode`-Verzweigung) | 556 | 52,60 Takte | 44600 / 11600 | Baseline |
| **`HUR-089`** | **`DrawParts` Partikel-Setup Optimiert** (`lsl.l #6` + direktes `btst #19`) | **356** | **32,60 Takte** | **32600 / 7500** | **-36,0 % Takte**, bitgleich (0 Mismatches) |
| `HUR-090` | `_WeaponsDraw` Sprite-Info **Original** (`bsr SprInfo` mit Stack-Push/Pop) | 686 | 65,60 Takte | 56600 / 8700 | Baseline |
| **`HUR-091`** | **`_WeaponsDraw` Sprite-Info Inlined** (kein Subroutine-Call, kein Stack-Spill) | **346** | **31,60 Takte** | **31600 / 7600** | **1,98× schneller (`-49,6 %` Takte)**, bitgleich |

* **Rohlogs:** `log-v6/HUR-088.log`, `log-v6/HUR-089.log`, `log-v6/HUR-090.log`, `log-v6/HUR-091.log`

### 9.3 Wasser- und Deckungs-Hotspots: Hurrican v17 (HUR-092 .. HUR-093)

Messungen auf echter Apollo 68080 Vampire V4 Hardware (Core 11350) mit Harness v6:

| Sequenz | Kernel / Beschreibung | Min (Raw) | Netto (Zyklen/Iter) | IEP1 / IEP2 | Speedup / Bewertung |
|:---|:---|:---:|:---:|:---:|:---|
| `HUR-092` | `DrawWater` `CovCell` **Original** (`bsr CovCell` + `mulu.l #34` je Zelle) | 836 | 80,60 Takte | 72600 / 20600 | Baseline |
| **`HUR-093`** | **`DrawWater` `CovCell` Inlined Zeilenzeiger** (Zeile einmal vorberechnet) | **216** | **18,60 Takte** | **16600 / 12500** | **3,87× schneller (`-74,2 %` Takte)**, bitgleich! |

* **Rohlogs:** `log-v6/HUR-092.log`, `log-v6/HUR-093.log`

### 9.4 Wellensimulation und Wasserfall-Rendering: Hurrican v18 (HUR-094 .. HUR-097)

Messungen auf echter Apollo 68080 Vampire V4 Hardware (Core 11350) mit Harness v6 (100 Durchläufe, 10x innere Schleife, Baseline kompensiert):

| Sequenz | Kernel / Beschreibung | Min (Raw) | Netto (Zyklen/Iter) | IEP1 / IEP2 | Speedup / Bewertung |
|:---|:---|:---:|:---:|:---:|:---|
| `HUR-094` | `WaterSurf` Wellen-Indexierung **Original** (`mulu.l #3`, `lsl.l #4`, `swap/ext`, per-tile `lea`) | 1456 | 142,60 Takte | 123600 / 26600 | Baseline (8 Oberflächenkacheln) |
| **`HUR-095`** | **`WaterSurf` Wellen-Indexierung Invariant Hoisting** (`+3 mod 64`, `extb.l`, Shift-Add) | **686** | **65,60 Takte** | **54600 / 20600** | **2,12× schneller (`-52,9 %` Takte)**, **-55,8 % Instruktionen**, bitgleich! |
| `HUR-096` | `DrawFalls` `FallCov` **Original** (`bsr FallCov` + `CovCell` `mulu.l #34` je Kachel) | 1912 | 188,20 Takte | 150800 / 37500 | Baseline (8 Kacheln Zeilenscan) |
| **`HUR-097`** | **`DrawFalls` Coverage Inlined Zeilen-Zeiger** (Zeile einmal vorberechnet, kein `bsr/mulu`) | **1416** | **138,60 Takte** | **102600 / 15500** | **-25,9 % Takte (`-496` Zyklen)**, **-32,0 % Instruktionen**, bitgleich! |

* **Rohlogs:** `log-v6/HUR-094.log`, `log-v6/HUR-095.log`, `log-v6/HUR-096.log`, `log-v6/HUR-097.log`

#### Technische Details der Optimierungen in Hurrican v18:
1. **`WaterSurf` Wellenberechnung:**
   - In der Originalfassung berechnete die innere Kachelschleife `.t:` für jede Kachel den Index der Sinustabelle mit `swap/ext.l`, `mulu.l #3, d1`, `lsl.l #4, d1`, `and.l #63, d0` und lud die Tabelle jedes Mal neu via `lea WaveTab(pc), a0`.
   - Da sich $x$ pro Durchlauf genau um 1 erhöht, erhöht sich der Sinus-Tabellenindex exakt um 3 (modulo 64). Der Basisindex wird einmalig vor der Schleife mittels Shift-Add (`add.l d1, d1; add.l d4, d1`) berechnet. In der Schleife genügt `addq.l #3, d3; and.l #63, d3`.
   - Das Vorzeichen der Tabellenwerte wird mit dem nativen 68080-Befehl `extb.l` in 1 Takt erweitert (spart `ext.w` + `ext.l`).
   - Die Kachelanzahl-Division `divu.l #20, d6` wurde durch eine reziproke Festkomma-Multiplikation (`mulu.l #52429, d6; lsr.l #20, d6`) ersetzt.
   - Ergebnis auf V4: **686 statt 1456 Takte (2.12× Speedup)**.

2. **`DrawFalls` und `FallCov` Wasserfall-Rendering:**
   - Analog zu `DrawWater` (`HUR-093`) wurde die Berechnung des Zeilenzeigers für den Deckungspuffer `CovF` aus der Spaltenschleife herausgezogen (`mulu.l #34, d0` nur noch 1× pro Kachelzeile statt für jede Wasserfall-Kachel).
   - Die Kachelprüfung wurde direkt in `.col:` und `.re:` geinlined: Kacheln ohne `WAND` (1) oder `VERDECKEN` (64) werden in 2 Befehlen (`and.w #65, d0; beq.s`) direkt als sichtbar erkannt, ohne Funktionsaufruf oder Puffertest.
   - In `FallRun` wurde die teure 64-Bit-Ganzzahldivision `divul.l #3, d1:d0` für den Blockversatz vor die Schleife verlegt, da alle nachfolgenden Chunks garantiert an 3-Kachel-Blöcken ausgerichtet sind (`d1 = 0`).
   - Ergebnis auf V4: **1416 statt 1912 Takte (-25.9 % Takte, -32.0 % IEP1)** bei 100 % bitgleicher Ausgabe.

### 9.5 Parallax-Culling und Hintergrund-Optimierung: Hurrican v19 (HUR-098 .. HUR-099)

Messungen auf echter Apollo 68080 Vampire V4 Hardware (Core 11350) mit Harness v6 (100 Durchläufe, 10x innere Schleife, Baseline kompensiert):

| Sequenz | Kernel / Beschreibung | Min (Raw) | Netto (Zyklen/Iter) | IEP1 / IEP2 | Speedup / Bewertung |
|:---|:---|:---:|:---:|:---:|:---|
| `HUR-098` | `tilemap.s` `ParBCov` **Original** (pro Block: y-Clipping, `asr.l #4`, `mulu.w #20`, 7-Reg `movem`) | 4242 | 421,20 Takte | 390800 / 57500 | Baseline (8 Parallax-Blöcke) |
| **`HUR-099`** | **`tilemap.s` `ParBCov` mit `ParBCovRow`** (Zeilenzeiger `ParBPtr` 1× vorberechnet) | **1876** | **184,60 Takte** | **158600 / 30500** | **2,26× schneller (`-55,8 %` Takte)**, **-59,4 % Instruktionen**, bitgleich! |

* **Rohlogs:** `log-v6/HUR-098.log`, `log-v6/HUR-099.log`

#### Technische Details der Optimierungen in Hurrican v19:
1. **`ParBCov` Zeilen-Invarianten Hoisting (`ParBCovRow`):**
   - In `_DrawParallaxBlocks` / `PBGo` prüfte `ParBCov` für jeden einzelnen 32×16-Block (bis zu 20 pro Zeile × 30 Zeilen = 600 Blöcke pro Frame) die Verdeckung durch darüberliegende Parallax-B-Blöcke.
   - Dabei sicherte `ParBCov` 7 Register auf dem Stack (28 Bytes), clippte $y_0 = \max(y, 0)$ und $y_1 = \min(y+15, 479)$ jedes Mal neu, berechnete $by_0 = (y_0 - parBY) \gg 4$ und $by_1 = (y_1 - parBY) \gg 4$ via arithmetischem Shift und führte eine 32-Bit-Hardware-Multiplikation `mulu.w #20, d3` für den Zeilenstart in `ParBMap` aus.
   - Da die vertikale Koordinate $y$ für alle 20 Blöcke einer Zeile identisch ist, wurde die neue Routine `ParBCovRow` eingeführt, die diese Invarianten genau 1× pro Blockzeile berechnet und den Zeilenzeiger `ParBPtr` sowie `ParBRows` ablegt.
   - `ParBCov` sichert nun nur noch 3 Register, testet `ParBPtr` (bei Deaktivierung oder wenn außerhalb des Bildschirms: sofortiger Return in 4 Takten) und führt pro Block nur noch das $x$-Clipping und den Speichertest durch.
   - Ergebnis auf V4: **1876 statt 4242 Takte (2.26× Speedup, -59.4 % Instruction Fetch)** bei 100 % bitgleichem Culling-Ergebnis.

2. **`_DrawParallax` Blit-Funktionszeiger Hoisting:**
   - In `_DrawParallax` wurde die Fallunterscheidung für den Kachelmodus (Modus 0 = `_BlitCopy`, Modus 1 = `_BlitKey`, Modus 2 = `_BlitAdd`) bisher in der horizontalen Bildschirmschleife `.loop:` für jede Kachel neu ausgeführt.
   - Der Funktionszeiger wird nun einmalig vor der Schleife in Adressregister `a4` aufgelöst (`lea _BlitCopy, a4` / `_BlitKey` / `_BlitAdd`), und in der Schleife direkt via `jsr (a4)` aufgerufen.

### 9.6 Strahl-Waffen und Segment-Berechnung: Hurrican v20 (HUR-100 .. HUR-101)

Messungen auf echter Apollo 68080 Vampire V4 Hardware (Core 11350) mit Harness v6 (100 Durchläufe, 10x innere Schleife, Baseline kompensiert):

| Sequenz | Kernel / Beschreibung | Min (Raw) | Netto (Zyklen/Iter) | IEP1 / IEP2 | Speedup / Bewertung |
|:---|:---|:---:|:---:|:---:|:---|
| `HUR-100` | `beam.s` `Boxes` Segment-Schleife **Original** (pro Segment: 3× Push, 1× Pop, 2× Read, `addq.l #8,sp`) | 1466 | 143,60 Takte | 116600 / 59700 | Baseline (8 Segmente) |
| **`HUR-101`** | **`beam.s` `Boxes` Spekulativer Store** (Zero-Stack Segment-Commit via `addq.l #8,a2`) | **1066** | **103,60 Takte** | **92600 / 51700** | **1,38× schneller (`-27,3 %` Takte)**, **-20,6 % Instruktionen**, bitgleich! |

* **Rohlogs:** `log-v6/HUR-100.log`, `log-v6/HUR-101.log`

#### Technische Details der Optimierungen in Hurrican v20:
1. **`Boxes` Spekulativer Puffer-Store (Zero-Stack-Architektur):**
   - In der Originalfassung sicherte die Segment-Schleife `.lp:` für jedes einzelne Strahlsegment (bis zu 17 Segmente) $x_{int}$ und $y_{int}$ auf dem Stack (3x Push, 1x Pop, 2x Read, 1x Stack-Bereinigung).
   - In `HUR-101` werden $x_{int}$ und $y_{int}$ direkt und spekulativ in den Zielpuffer `(a2)` bzw. `4(a2)` geschrieben. Trifft der Strahl eine Wand, beendet ein einfacher Sprung `bne.s .end` die Schleife, ohne dass Daten korrumpiert werden oder der Stack bereinigt werden muss. Ist das Segment frei, genügt ein einziger Befehl `addq.l #8, a2`, um das Segment zu bestätigen.
   - Ergebnis auf V4: **1066 statt 1466 Takte (-27.3 % Takte, -20.6 % IEP1)** bei 100 % bitgleichem Ergebnis.

2. **`Frame` Division-Eliminierung (Winkelpose):**
   - In `Frame` wurde zur Berechnung der 36 Spieler-Blickrichtungen bisher zweimal hintereinander eine 64-Bit-Ganzzahldivision `divul.l #10, d1:d0` und `divul.l #36, d1:d0` aufgerufen.
   - Da der dividierte Winkel maximal den Wert 53 annehmen kann ($53 < 2 \times 36$), wurde die zweite 64-Bit-Division vollständig durch einen simplen Vergleich `cmp.l #36, d0; blt.s ... sub.l #36, d0` ersetzt. Die Division durch 10 erfolgt über ein schnelles `divu.w #10, d0`.
   - Spart über 35 Takte pro Frame.

3. **`_BeamDraw` Inlined `AddSpr`:**
   - In `_BeamDraw` rief die Schleife `.sg:` für jedes Segment `bsr AddSpr` auf, welches redundanterweise `d5-d7` auf dem Stack sicherte. Da `_BlitAdd` diese Register ohnehin hardwareseitig erhält, wurde `AddSpr` direkt inlined.

| `HUR-102` | `beam.s` `Frame` Phasenberechnung mit `divu.w #10` (36 Winkel) | 11106 | 1107,60 Takte | 392600 / 42700 | Baseline (16-Bit-Division) |
| **`HUR-103`** | **`beam.s` `Frame` Reziproke Multiplikation `mulu.w #6554`** (`clr.w`, `swap`) | **5346** | **531,60 Takte** | **428600 / 42700** | **2,07× schneller (`-51,9 %` Takte)**, bitgleich! |

* **Rohlogs:** `log-v6/HUR-102.log`, `log-v6/HUR-103.log`

### 9.7 Spieler-Physik & Kollision: Hurrican v21 (HUR-104 .. HUR-105)

Messungen auf echter Apollo 68080 Vampire V4 Hardware (Core 11350) mit Harness v6 (100 Durchläufe, 10x innere Schleife, Baseline kompensiert):

| Sequenz | Kernel / Beschreibung | Min (Raw) | Netto (Zyklen/Iter) | IEP1 / IEP2 | Speedup / Bewertung |
|:---|:---|:---:|:---:|:---:|:---|
| `HUR-104` | `player.s` `BlockAt` Tile-Lookup **Original** (8 Lookups: getrennte `tst.l`/`cmp.l`, 2× `lea Div20, a0`) | 66957 | 6692,70 Takte | 6475600 / 1145700 | Baseline (inkl. Tabellen-Setup) |
| **`HUR-105`** | **`player.s` `BlockAt` Kombinierte Bereichsprüfung** (vorzeichenlose `cmp.l #DIVMAX-1`, nur 1× `lea Div20`, eliminierte Verzweigungen) | **66446** | **6641,60 Takte** | **6427600 / 1153700** | **-511 Takte (-64 Takte/Abfrage)**, bitgleich! |

* **Rohlogs:** `log-v6/HUR-104.log`, `log-v6/HUR-105.log`

#### Technische Details der Optimierungen in Hurrican v21:
1. **`BlockAt` Vorzeichenlose Bereichsprüfung (`cmp.l #DIVMAX-1`):**
   - In `player.s` führte `BlockAt` für $x$ und $y$ bisher jeweils getrennte Tests durch (`tst.l d1; bmi.s .air; tst.l d0; bmi.s .wall; cmp.l #DIVMAX-1, d0; bhi.s ...`).
   - Da im Zweierkomplement negative Werte `$80000000..$FFFFFFFF` bei vorzeichenlosem Vergleich (`bhi.s`) immer größer als `DIVMAX-1` ($16383$) sind, fasst ein einziges `cmp.l #DIVMAX-1, d0; bhi.s .wall` sowohl $x < 0$ als auch $x \ge 16384$ zusammen!
   - Da Koordinaten $\ge 16384$ außerhalb jeder existierenden Spielkarte liegen, verzweigen sie sofort direkt zu `.wall` (horizontal) bzw. `.air` (vertikal).
   - `lea Div20, a0` wird nur noch genau **einmal** geladen und bedient sowohl $x$ als auch $y$.

2. **`SolidBelow` Register-Bereinigung:**
   - Unnötiges Sichern und Wiederherstellen von Adressregister `a1` auf dem Stack eliminiert.

3. **`SlopeCheck` Zeilenzeiger Hoisting (`RowTab`):**
   - In `SlopeCheck` konvertierte die Schleife bisher die bereits bekannte Block-Zeile $d_5$ per `mulu.l #20` zurück in Pixel, um dann `BlockAt` aufzurufen (welches die Koordinate wiederum per `Div20` dividierte und `RowTab` las).
   - Dies wurde durch ein einmaliges Vorab-Laden des Zeilenzeigers `a0 = RowTab[d5]` pro Zeile ersetzt. Die Spalten-Schleifen greifen nun direkt in 1 Takt über `move.w (a0, d7.l*2), d0` auf die Kacheldaten zu – ohne Stack-Frame, Multiplikation oder Subroutinenaufruf.

### 9.8 Gegner-Kollision, Waffen-Draw & HUD: Hurrican v22 (HUR-106 .. HUR-111)

Messungen auf echter Apollo 68080 Vampire V4 Hardware (Core 11350) mit Harness v6 (100 Durchläufe, 10x innere Schleife, Baseline kompensiert):

| Sequenz | Kernel / Beschreibung | Min (Raw) | Netto (Zyklen/Iter) | IEP1 / IEP2 | Speedup / Bewertung |
|:---|:---|:---:|:---:|:---:|:---|
| `HUR-106` | `enemies.s` `ShotHits` Schleife **Original** (10 Schüsse: `movem.l d2-d5,-(sp)`, `jsr SprInfo`, `movem.l (sp)+,d2-d5`) | 6590 | 656,00 Takte | 483600 / 126600 | Baseline (10 Schüsse × Gegner) |
| **`HUR-107`** | **`enemies.s` `ShotHits` Zero-Stack & Early-X** (kein Stack-Spill, `SprInfo` inlined, Early-Reject auf X) | **5426** | **539,60 Takte** | **438600 / 111600** | **-1164 Takte (`-17,7 %` Takte)**, **-45.000 Instruktionen**, bitgleich! |
| `HUR-108` | `weapons.s` `_WeaponsDraw` Blit-Schleife **Original** (10 Schüsse: 5 Push, 5 Pop, `bsr`/`rts` je Schuss) | 4236 | 420,60 Takte | 346600 / 105700 | Baseline (Stack-Saves vor `DrawKeyed`) |
| **`HUR-109`** | **`weapons.s` `_WeaponsDraw` Direkter Blit-Call** (Zero Stack Traffic: `_BlitKey` erhält `d2-d7/a2-a4` hardwareseitig) | **3436** | **340,60 Takte** | **306600 / 125700** | **1,23× schneller (`-18,9 %` Takte)**, **-40.000 Instruktionen**, bitgleich! |
| `HUR-110` | `hud.s` `Number` Dezimalausgabe **Original** (4 Zahlen: Lives, Score, Time, Diamonds mit `divul.l #10, d1:d0`) | 3126 | 309,60 Takte | 73500 / 32500 | Baseline (12× 64-Bit-Hardware-Division) |
| **`HUR-111`** | **`hud.s` `Number` Reziproke Multiplikation** (4 Zahlen: `mulu.l #$CCCCCCCD` mit 3-Bit Shift, kein `divul.l`) | **1936** | **190,60 Takte** | **146500 / 32500** | **1,61× schneller (`-38,1 %` Takte)**, bitgleich! |

* **Rohlogs:** `log-v6/HUR-106.log` .. `log-v6/HUR-111.log`

#### Technische Details der Optimierungen in Hurrican v22:
1. **`ShotHits` Kollisionsschleife mit Zero-Stack und Early-X-Reject (`enemies.s`):**
   - In `ShotHits` prüfte die Schleife für jeden aktiven Schuss die Kollision mit dem Gegner. Dabei sicherte die Schleife bisher in jeder Iteration die Gegnerkoordinaten $d_2-d_5$ per `movem.l d2-d5, -(sp)` (16 Bytes), rief `SprInfo` per Subroutine auf, berechnete Schussmitte $x$ und $y$ und stellte $d_2-d_5$ wieder her – nur um danach festzustellen, dass der Schuss weit links oder rechts am Gegner vorbeifliegt.
   - Neu: `SprInfo` wurde inlined (5 Instruktionen), Schussmitte $x$ wird direkt ohne Stack-Spill berechnet. Liegt $x$ außerhalb des Gegners ($x < d_2$ oder $x \ge d_4$, was in >90 % aller Tests zutrifft), springt die Schleife **sofort** zur nächsten Iteration, ohne $y$ überhaupt anzufassen!
   - Ergebnis auf V4: **5426 statt 6590 Takte (-17.7 % Takte, -45.000 IEP1)** bei 100 % bitgleichem Trefferverhalten.

2. **`_WeaponsDraw` Direkter Blit-Call (`weapons.s`):**
   - Vor dem Blitten jedes Schusses rief `_WeaponsDraw` bisher `bsr DrawKeyed` auf und sicherte zuvor $a_3$ und $d_4$ auf dem Stack; `DrawKeyed` sicherte nochmals $d_5-d_7$ (insgesamt 48 Bytes Stack-Traffic pro Schuss).
   - Da `_BlitKey` und `_BlitAdd` in `blit.s` sämtliche Register $d_2-d_7$ und $a_2-a_4$ hardwareseitig vollständig sichern und restaurieren, wurden `DrawKeyed` inlined und sämtliche Stack-Operationen eliminiert.
   - Ergebnis auf V4: **3436 statt 4236 Takte (-18.9 % Takte, -40.000 IEP1)**.

3. **`hud.s` Reziproke Dezimalausgabe & Energie-Skalierung:**
   - In `Number` wurden pro Frame für Leben, Punkte, Zeit und Diamanten bis zu 12× 64-Bit-Ganzzahldivisionen `divul.l #10, d1:d0` (22 Takte je Division) aufgerufen.
   - Dies wurde durch vorzeichenlose reziproke 64-Bit-Multiplikation mit $M = \$CCCCCCCD$ ($q = (N \times M) \gg 35$) ersetzt.
   - Ergebnis auf V4: **1936 statt 3126 Takte (1.61× schneller, -38.1 % Takte)**.
   - Zudem wurden in `_HudDraw` die Divisionen durch 100 (Energiekugel) und 200 (Radkugel) durch exakte Festkomma-Multiplikationen `mulu.w #45220` ($69/100$) bzw. `mulu.w #22610` ($69/200$) ersetzt.

### 9.9 SAGA 60 Hz Triple-Buffering Pacer & CCC-Timing: Hurrican v24

#### 1. Problemstellung (v22/v23 Post-Mortem)
- **v22 (Ungebremst):** Hurrican lief auf V4 mit 100–140 FPS, da `WaitPace` nur `rts` war. Bei schnellen Renderzeiten (< 7 ms) überschrieb die CPU `BPLHPT` mehrfach innerhalb desselben 60-Hz-HDMI-Rasters -> Mikroruckler und Tearing.
- **v23 (Deadlock nach ~8s):** Der Versuch, mit `_LVOWaitTOF()` auf den VBL zu synchronisieren, fror das Spiel nach ca. 8 Sekunden (~480 Aufrufe) ein (Sound lief per DMA weiter). Unter `Forbid()` (Task-Pri 19) im Direct-SAGA-Modus verliert AROS/ApolloOS `WaitTOF()` gelegentlich Signale oder deadlockt im `Wait()`.

#### 2. Die v24-Lösung: Hardware-VBL-Server + Zero-Latency Spin-Wait & Time-Borrowing
1. **Hardware-VBL-Server (`INTB_VERTB` = 5 via `_LVOAddIntServer`):**
   ```asm
   VblServer:
       addq.l  #1,(a1)             ; VblCount erhöhen
       addq.l  #1,VblSincePoke     ; Delta-Zähler seit letztem BPLHPT-Poke
       movec.l ccc,d0              ; 68080 Clock Cycle Counter
       move.l  d0,VblTimestamp     ; Exakter VBL-Startzyklus
       moveq   #0,d0               ; Z-Flag = 1 -> Interrupt-Kette fortführen
       rts
   ```
2. **Zero-Latency Spin-Wait & Time-Borrowing (in `HwFrame`):**
   ```asm
   .wait_vbl:
       move.l  VblSincePoke,d1
       cmp.l   d0,d1
       bhs.s   .vbl_ready
       bra.s   .wait_vbl

   .vbl_ready:
       sub.l   d0,VblSincePoke     ; 1 VBL abziehen (Time-Borrowing)
       ble.s   .clr
       cmp.l   #1,VblSincePoke     ; Maximal 1 Frame Vorlauf (Triple-Buffer-Tiefe)
       ble.s   .poke
       move.l  #1,VblSincePoke
       bra.s   .poke
   .clr:
       clr.l   VblSincePoke
   .poke:
       move.l  Back,BPLHPT         ; Hardware-Latch beim nächsten Vertical Blank
   ```
3. **Behandlung des CCC-Counter-Overflows:**
   Der 68080 Clock Cycle Counter (`MOVEC CCC, Dn`) zählt mit Prozessortakt (~92 MHz) und läuft alle ~46,7 Sekunden über ($FFFFFFFF -> $00000000). Da 32-Bit-Zweierkomplement-Subtraktionen modulo $2^{32}$ rechnen, ergibt `current_ccc - vbl_timestamp` auch bei Überlauf über Null exakt die verstrichene Zyklendauer.

