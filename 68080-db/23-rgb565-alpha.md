# 23 – RGB565-Alpha-Blending (exakter Skalar-Test)

**Status:** **PASS**  
**Hardware:** Vampire V4, Apollo Core **11350**, PCR 1088/1, VCoreRev 0, 92 MHz  
**Projekt:** [`Projects/Sprite_Alpha_PMULA/`](../Projects/Sprite_Alpha_PMULA/)  
**Rohlog:** [`cores/core-11350/golden/rgb565_alpha.log`](cores/core-11350/golden/rgb565_alpha.log)

## Warum PMULA hier nicht passt

`PMULA` arbeitet auf **8-Bit-Kanälen** (ARGB32). RGB565 hat **5/6/5 Bit** pro
Kanal. Die Alpha-Mischung muss daher pro Kanal separat berechnet werden:

```text
out = (src * alpha + dst * (255 - alpha) + 127) / 255
```

Getestet wird die exakte Ganzzahlformel für R5, G6 und B5.

## Ergebnis

```text
RGB565 ALPHA GOLDEN TEST
CORE;AC68080;11350;PCR1088/1;VCoreRev0;92MHz
ALPHA_SWEEP;256;fails=0
RANDOM;20000;fails=0
SPRITE;16x16;diff=0
SPRITE;32x32;diff=0
SPRITE;64x64;diff=0
EDGE;alpha=0;src=1234;dst=abcd;out=abcd
EDGE;alpha=128;src=1234;dst=abcd;out=5af1
EDGE;alpha=255;src=1234;dst=abcd;out=1234
RESULT;PASS;fails=0
```

- **256 Alpha-Werte**: 0 Fehler
- **20.000 Zufallspixel**: 0 Fehler
- **16×16 / 32×32 / 64×64**: `diff=0`
- Alpha 0 → Ziel, Alpha 255 → Quelle

## Abgrenzung

- `bsel` ist ein **Color-Key**-Verfahren, kein Alpha-Blending; es ist kein
  direkter Vergleichsalgorithmus.
- Die Suite ist ein **skalarer Referenztest**; eine AMMX-/PMULA-Variante für
  RGB565 existiert nicht direkt.
- Der Test ist nicht Teil der 75 kanonischen v5-Sequenzen; er ist ein
  separater Golden-/Semantiktest.
