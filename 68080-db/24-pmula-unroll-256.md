# 24 – PMULA 4×-Unrolling & 256×256-Alpha-Sprite-Last

**Status:** **PASS** (bit-exakt)  
**Hardware:** Vampire V4, Apollo Core **11350**, PCR 1088/1, VCoreRev 0, 92 MHz  
**Projekt:** [`Projects/Sprite_Alpha_PMULA/`](../Projects/Sprite_Alpha_PMULA/)  
**Golden-Test:** [`22-sprite-alpha-pmula-golden.md`](22-sprite-alpha-pmula-golden.md)

## Kanonische Sequenzen

| ID | Variante | Takte / 32 Byte | Takte/Pixel |
|---|---|---:|---:|
| BLT-003 | 1× PMULA, 2 Loads | 5 / 8 Byte | 2,50 |
| BLT-004 | 4× PMULA, 8 Loads + 4 Stores | **16** | 2,00 |
| BLT-005 | 4× PMULA **Memory-VEA**, 4 Loads + 4 Stores | **13** | **1,625** |

**BLT-005 spart pro 32 Byte vier explizite `load`-Instruktionen**, weil der
A-Operand direkt als Memory-VEA in `pmula (a0)+,e1,e2` steht.

## 256×256 echte Alpha-Sprite-Last

```text
SPRITE;256x256;diff1=0;diff4=0;diffmem=0
BENCH256;1x;min=526742;cyc_px_x1000=8037
BENCH256;4x;min=558884;cyc_px_x1000=8527
BENCH256;4xmem;min=159298;cyc_px_x1000=2430
```

- **1×:** 8,037 Takte/Pixel
- **4× Register:** 8,527 Takte/Pixel (langsamer)
- **4× Memory-VEA:** **2,430 Takte/Pixel**
- **Memory-VEA ist 3,31× schneller als 1×** auf 256×256
- **Alle drei Varianten bit-exakt:** `diff=0`

## Pipe-Auslastung (v5)

| ID | CCC | IEP1 | IEP2 | IEP3 | IEP2% |
|---|---:|---:|---:|---:|---:|
| BLT-004 | 16 | 17 | 1 | 0 | 5,6 % |
| BLT-005 | 13 | 14 | 0 | 0 | 0,0 % |

**Antwort auf „Pipe 2/3 besser auslasten?“:** Mit PMULA nicht. PMULA bleibt
auf **IEP1**; die 4×-Registervariante gewinnt nur minimal 5,6 % IEP2, die
Memory-VEA-Variante gar nichts. Der echte Gewinn kommt aus der **eingesparten
Load-Instruktion**, nicht aus Pipe-2/3-Parallelität.

## Warum 4× mit Registern langsamer wird

Acht explizite `load`-Instruktionen plus vier `pmula` plus vier `store`
erzeugen einen breiteren Instruktionsstrom, ohne dass PMULA auf eine zweite
Pipeline ausweichen kann. Memory-VEA entfernt vier Instruktionen und damit
auch vier Issue-Slots pro 32 Byte.

## Rohlogs

| ID | run01 … run10 |
|---|---|
| `BLT-004` | [01](cores/core-11350/logs/BLT-004.run01.log) [02](cores/core-11350/logs/BLT-004.run02.log) [03](cores/core-11350/logs/BLT-004.run03.log) [04](cores/core-11350/logs/BLT-004.run04.log) [05](cores/core-11350/logs/BLT-004.run05.log) [06](cores/core-11350/logs/BLT-004.run06.log) [07](cores/core-11350/logs/BLT-004.run07.log) [08](cores/core-11350/logs/BLT-004.run08.log) [09](cores/core-11350/logs/BLT-004.run09.log) [10](cores/core-11350/logs/BLT-004.run10.log) |
| `BLT-005` | [01](cores/core-11350/logs/BLT-005.run01.log) [02](cores/core-11350/logs/BLT-005.run02.log) [03](cores/core-11350/logs/BLT-005.run03.log) [04](cores/core-11350/logs/BLT-005.run04.log) [05](cores/core-11350/logs/BLT-005.run05.log) [06](cores/core-11350/logs/BLT-005.run06.log) [07](cores/core-11350/logs/BLT-005.run07.log) [08](cores/core-11350/logs/BLT-005.run08.log) [09](cores/core-11350/logs/BLT-005.run09.log) [10](cores/core-11350/logs/BLT-005.run10.log) |
