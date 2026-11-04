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

## 5. Vergleichstabelle der Blit- & Shading-Kernels (Vampire V4)

| Test-ID | Kernel / Operation | Befehle | Netto-Takte (MIN) | Takte / Pixel | Status (Harness v5) |
|---|---|:---:|:---:|:---:|:---:|
| [`BLT-001`](19-sprite-blit.md) | 2D Sprite Blitting: 256-Bit Copy (`move16`) | 2 | **3 Takte** | **0.19 Takt** | Gemessen (V4) |
| [`BLT-002`](20-sprite-transparency.md) | Transparent Color-Key (`pcmpeqw` + `storeilm`) | 4 | **5 Takte** | **1.25 Takte** | Gemessen (V4) |
| [`BLT-006`](27-hurrican-blitting.md) | Tail-Store Zeilenende (`storeilm` + Mask) | 3 | **4 Takte** | **1.00 Takt** | Gemessen (V4) |
| [`BLT-007`](27-hurrican-blitting.md) | Flashlight $3/8$-Darkening (`lsrq` + `paddw`) | 7 | **8 Takte** | **2.00 Takte** | Gemessen (V4) |
| [`BLT-003`](21-sprite-alpha-pmula.md) | Alpha Blending Premultiplied (`PMULA`) | 5 | **5 Takte** | **1.25 Takte** | Gemessen (V4) |
| [`BLT-008`](27-hurrican-blitting.md) | `MSCALE` 3-Kanal RGB565 Scale (`pmul88` + `por`) | 16 | **21 Takte** | **5.25 Takte** | Gemessen (V4) |
