# 21 – Sprite-Alpha-Blending mit PMULA

**Kanonische Sequenz:** [`seq/BLT-003.i`](seq/BLT-003.i)  
**Hardware:** Vampire V4, Apollo Core **11350**, PCR 1088/1, VCoreRev 0, 92 MHz  
**Harness:** v5, 100 Warmup + 100 Messläufe, 10 Wiederholungen

```asm
; e0 = premultiplied sprite (alpha,R,G,B | alpha,R,G,B)
; e1 = background RGB
        load    (a0)+,e0
        load    (a2)+,e1
        pmula   e0,e1,e2
        store   e2,(a1)+
```

## PMULA-Semantik

Laut PRM:

```text
d = (alpha == 255) ? b
  : (alpha == 0)   ? a
  : min(a + ((alpha * b) >> 8), 255)
```

`PMULA` verarbeitet **8 Byte = zwei 32-Bit-Pixel** pro Instruktion. Das Alpha-
Byte wird aus `a` gelesen; die RGB-Bytes von `b` werden damit gewichtet und
zu den **premultiplied** RGB-Werten in `a` addiert. Das Ergebnis sättigt pro
Byte auf 255.

**Voraussetzung:** Die Sprite-Quelle muss premultiplied Alpha verwenden
(RGB bereits mit Alpha multipliziert). Für nicht-premultiplied RGBA ist ein
zusätzlicher Multiplikationsschritt nötig; `PMULA` allein reicht dann nicht.

## Messung

- **MIN:** 5 Takte
- **Median MIN:** 5 Takte
- **MAX:** 57 Takte (Interrupt-Ausreißer)
- 10/10 Läufe, 100/100 RES, `check_log.py` OK
- **PERF:** `IEP1` dominiert; `IEP2/IEP3` bleiben 0 in dieser Einzelsequenz

## Rohlogs (Harness v5)

| ID | run01 … run10 |
|---|---|
| `BLT-003` | [01](cores/core-11350/logs/BLT-003.run01.log) [02](cores/core-11350/logs/BLT-003.run02.log) [03](cores/core-11350/logs/BLT-003.run03.log) [04](cores/core-11350/logs/BLT-003.run04.log) [05](cores/core-11350/logs/BLT-003.run05.log) [06](cores/core-11350/logs/BLT-003.run06.log) [07](cores/core-11350/logs/BLT-003.run07.log) [08](cores/core-11350/logs/BLT-003.run08.log) [09](cores/core-11350/logs/BLT-003.run09.log) [10](cores/core-11350/logs/BLT-003.run10.log) |

> Die Sequenz misst die **PMULA-Instruktion** auf Scratch-Daten. Sie ist kein
> vollständiger Alpha-Blend-Suite-Test; ein pixelgenauer C-/ASM-Golden-Test
> für beliebige Alpha-Werte steht noch aus.

**Bit-exakter Golden-Test:** [`22-sprite-alpha-pmula-golden.md`](22-sprite-alpha-pmula-golden.md) – 256 Alpha-Werte, 10.000 Zufallsvektoren, 16/32/64-Sprites, `diff=0`.
