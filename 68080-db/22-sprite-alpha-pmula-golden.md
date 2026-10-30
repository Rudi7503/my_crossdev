# 22 – PMULA-Alpha-Golden-Test (C-Referenz vs. Hardware)

**Status:** **PASS**  
**Hardware:** Vampire V4, Apollo Core **11350**, PCR 1088/1, VCoreRev 0, **92 MHz**  
**Projekt:** [`Projects/Sprite_Alpha_PMULA/`](../Projects/Sprite_Alpha_PMULA/)  
**Rohlog:** [`cores/core-11350/golden/pmula_golden.log`](cores/core-11350/golden/pmula_golden.log)  
**Mikrobenchmark:** [`seq/BLT-003.i`](seq/BLT-003.i) / [`21-sprite-alpha-pmula.md`](21-sprite-alpha-pmula.md)

## 1. Getestete PMULA-Semantik

PMULA verarbeitet **8 Byte = zwei ARGB32-Pixel**:

```text
a = [alpha0, R0, G0, B0, alpha1, R1, G1, B1]
b = [?,      R0, G0, B0, ?,      R1, G1, B1]
d = [0,      R0',G0',B0',0,      R1',G1',B1']
```

Pro Kanal:

```text
alpha == 255 ? bRGB
alpha == 0   ? aRGB
sonst        : min(aRGB + ((alpha * bRGB) >> 8), 255)
```

Die Alpha-Bytes im Ergebnis werden **0**. `b` liefert die Quell-RGB-Werte,
`a` den mit `(255-alpha)` vorbereiteten Hintergrund. Damit ergibt sich
Source-over:

```text
out = bg * (1 - alpha/255) + src * alpha/255
```

## 2. C-Referenz

Die Referenz in `pmula_test.c` bildet exakt diese Byte-Operationen nach –
inklusive Sättigung und der Null-Alpha-Bytes. Der Test vergleicht **jedes
Byte**, nicht nur RGB.

## 3. Ergebnisse auf der V4

```text
ALPHA_SWEEP;256;fails=0
RANDOM;10000;fails=0
VEC128;got=006e463700cbffe3;exp=006e463700cbffe3
SPRITE;16x16;diff=0
SPRITE;32x32;diff=0
SPRITE;64x64;diff=0
BENCH;PMULA;64x64;runs=100;min=8228;avg=10377;cycles_per_pixel_x1000=2008
BENCH;REF;64x64;runs=100;min=812492;avg=834968;cycles_per_pixel_x1000=198362
RESULT;PASS;fails=0
```

- **256 Alpha-Werte** (0…255), beide Pixel: **0 Fehler**
- **10.000 Zufallsvektoren**: **0 Fehler**
- **16×16, 32×32, 64×64 ARGB32-Sprites**: **diff=0**
- **PMULA:** 8.228 Takte / 4.096 Pixel = **2,008 Takte/Pixel** (MIN)
- **Skalares C:** 812.492 Takte = **198,362 Takte/Pixel**
- **Speedup:** ca. **98,8×**

## 4. Grenzen

- Der Test deckt **ARGB32/8-Bit-Kanäle** ab; PMULA ist keine RGB565-Instruktion.
- `bsel` ist ein **Color-Key**-Verfahren, kein Alpha-Blending; ein direkter
  PMULA-vs-`bsel`-Vergleich wäre kein gleicher Algorithmus.
- Der Test prüft **PMULA-Semantik und Source-over**; eine vollständige
  Alpha-Sprite-Suite über alle Farbtiefen/Größen bleibt ein separates Projekt.

## 5. Reproduktion

```bash
cd Projects/Sprite_Alpha_PMULA
make clean all
# PMULATest auf die V4 kopieren und ausführen
```

Der Rohlog ist unter `cores/core-11350/golden/pmula_golden.log` versioniert.

**Unrolling/VEA-Messung:** [`24-pmula-unroll-256.md`](24-pmula-unroll-256.md).
