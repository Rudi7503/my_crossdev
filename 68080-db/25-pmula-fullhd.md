# 25 – PMULA Full-HD-Alpha-Last (1920×1080) und Load-Einsparung

**Status:** **PASS** (bit-exakt)  
**Hardware:** Vampire V4, Apollo Core **11350**, PCR 1088/1, VCoreRev 0, 92 MHz  
**Projekt:** [`Projects/Sprite_Alpha_PMULA/`](../Projects/Sprite_Alpha_PMULA/)  
**Rohlog:** [`cores/core-11350/golden/pmula_golden.log`](cores/core-11350/golden/pmula_golden.log)

## 1. Kann man einen weiteren Load einsparen?

Getestet wurde ein interleaved A/B-Layout mit 16-Byte-Records und `loadq`.
Ergebnis: **`loadq` existiert im vasm-2.0d-Mot-Syntax nicht**:

```text
error 2: unknown mnemonic <loadq>
```

AMMX `load` lädt **64 Bit**; ein 128-Bit-Register-Load für A+B ist damit nicht
verfügbar. Zwei 64-Bit-Loads aus einem interleaved Record sparen gegenüber
zwei separaten Streams keinen Load. Der Interleaved-Ansatz bringt daher
**keinen Vorteil**; der zusätzliche Kopier-/Layoutaufwand wäre kontraproduktiv.

Die einzige echte Load-Einsparung ist bereits **Memory-VEA** in `BLT-005`:

```asm
load    (a1)+,e1
pmula   (a0)+,e1,e2
store   e2,(a2)+
```

Damit entfällt der explizite A-Load; pro 32 Byte werden **4 Loads** gegenüber
der Registervariante gespart.

## 2. Unroll-Vergleich (v5, 32 Byte)

| ID | Variante | Takte | Takte/Pixel | IEP1 | IEP2 |
|---|---|---:|---:|---:|---:|
| BLT-003 | 1× | 5 / 8 B | 2,50 | 6 | 0 |
| BLT-004 | 4× Register | **16** | 2,00 | 17 | 1 (5,6 %) |
| BLT-005 | 4× Memory-VEA | **13** | **1,625** | 14 | 0 |

**Pipe 2/3:** PMULA bleibt auf **IEP1**; die Registervariante gewinnt nur
5,6 % IEP2 und wird insgesamt langsamer. Memory-VEA füllt Pipe 2/3 nicht,
spart aber die Load-Instruktionen.

## 3. 256×256

```text
SPRITE;256x256;diff1=0;diff4=0;diffmem=0
BENCH256;1x;min=532559;cyc_px_x1000=8126
BENCH256;4x;min=568632;cyc_px_x1000=8676
BENCH256;4xmem;min=174755;cyc_px_x1000=2666
```

- 1×: **8,126 Takte/Pixel**
- 4× Register: **8,676** (langsamer)
- 4× Memory-VEA: **2,666** → ca. **3,05× schneller** als 1×

## 4. Full-HD 1920×1080

```text
SPRITE;1920x1080;diff1=0;diff4mem=0
BENCH_FHD;1x;min=17928252;cyc_px_x1000=8645;MBps=43
BENCH_FHD;4xmem;min=6435035;cyc_px_x1000=3103;MBps=119
```

| Variante | Takte | Takte/Pixel | Durchsatz |
|---|---:|---:|---:|
| 1× | 17.928.252 | 8,645 | 43 MB/s |
| **4× Memory-VEA** | **6.435.035** | **3,103** | **119 MB/s** |

**2,79× schneller**, bit-exakt (`diff=0`). Die absoluten Werte sind
speicherbandbreitenbegrenzt; die 256×256-Werte liegen wegen besserer
Cache-Lokalität niedriger.

## 5. Fazit

- **Weiterer Load nicht sinnvoll einsparbar:** `loadq`/128-Bit-Load fehlt.
- **Memory-VEA ist die beste Variante:** 13 Takte/32 Byte, 3,103 Takte/Pixel Full-HD.
- **Pipe 2/3:** mit PMULA nicht auslastbar; PMULA bleibt IEP1-gebunden.
