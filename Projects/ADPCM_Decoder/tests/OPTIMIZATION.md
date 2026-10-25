# ADPCM 4-bit – Optimierung (final: Opt3)

## Produktivstand

`decoder_4bit.s` ist **Opt3**: `decode_primary` und `decode_secondary`
berechnen das Delta über eine exakte **89×8-Tabelle**:

```text
delta = delta_table[index*8 + (nibble & 7)]
```

`a4` zeigt direkt auf `delta_table`; das frühere `add.w #89` entfällt.
Die Tabelle enthält für jeden Step-Index und jede Amplitude (0..7) exakt:

```text
(step>>3) + bit0*(step>>2) + bit1*(step>>1) + bit2*step
```

Damit bleiben die einzelnen Floors erhalten. Der frühere
`step*mult>>3`-Ansatz war off-by-one und wurde verworfen.
Das Original liegt unter `experiments/decoder_4bit_orig.s`,
Opt2 unter `experiments/decoder_4bit_opt2.s`.

## Verifikation

- `golden_test_opt3`: Stereo, MS, Mono **PASS** (PCM/State/Guard,
  `unwritten=0`).
- `repeat_test_opt3`: 100 Warmup + 100 Messungen, State-Reset, C-Referenz:
  **PASS**.
- Hauptprojekt `make cleanbefore build` baut sauber.

## 100×-Vergleich (State-Reset, MIN/Median)

| Modus | Baseline MIN/Med | Opt2 MIN/Med | Opt3 MIN/Med |
|---|---:|---:|---:|
| Stereo | 1546 / 1548 | 1272 / 1428 | **1242 / 1243** |
| MS | 1876 / 1878 | 1557 / 1702 | **1526 / 1528** |
| Mono | 1624 / 1625 | 1286 / 1299 | **1256 / 1257** |

Opt3 ist gegenüber der Baseline etwa **19–23 % schneller** (Median).

## Dateien

- `tests/ADPCM_GOLDEN-opt3.txt`, `tests/ADPCM_REPEAT-opt3.txt`
- `tests/ADPCM_GOLDEN-opt2.txt`, `tests/ADPCM_REPEAT-opt2.txt`
- `tests/ADPCM_GOLDEN.txt`, `tests/ADPCM_REPEAT.txt` (Baseline)

## Opt4 (Index-Add) – neutral, nicht übernommen

`add.w (a6,d1.w*2),d3/d5` statt `move.w ...; add.w` spart eine Instruktion
pro Nibble, bringt im 100×-Lauf aber keinen messbaren Gewinn:

| Modus | Opt3 MIN/Med | Opt4 MIN/Med |
|---|---:|---:|
| Stereo | 1242 / 1243 | 1243 / 1244 |
| MS | 1526 / 1528 | 1526 / 1526 |
| Mono | 1256 / 1257 | 1256 / 1256 |

Opt4 bleibt in `experiments/`; produktiv bleibt **Opt3**.

## Opt5 (MIN.L/MAX.L) – verworfen

Die 68080-Assemblierung akzeptiert `MIN.L`/`MAX.L` mit Immediate, aber auf der
getesteten V4 führt der Opt5-Lauf zu einem **Reset/Crash** (kein serieller
Marker, `Ram Disk:` geleert). Die Instruktionen sind auf diesem Core/Setup
offenbar nicht nutzbar. Opt5 wird **nicht** übernommen; produktiv bleibt Opt3.

## Opt6 (MS-Matrix als Subroutine) – nicht übernommen

Gemeinsame `ms_finish`-Subroutine für Long- und Byte-Pfad: Golden **PASS**,
aber MS 512 Blöcke **861.059** statt Opt3 **845.599** (+1,8 %). Der
`bsr/rts`-Overhead pro Paar frisst den I-Cache-Vorteil. Opt6 bleibt in
`experiments/`; produktiv bleibt **Opt3**.

## Opt3a + IEP-Pipe-Analyse

Der tote Step-Load (`move.w (a4,...),d6; ext.l d6`) wurde entfernt.
Golden und 100×-Repeat: **PASS**.

IEP-Zähler (512 Blöcke, 128 Warmup):

| Modus | cycles | IEP1 | IEP2 | IEP3 | IEP1/2/3 pro Block |
|---|---:|---:|---:|---:|---:|
| Stereo | 673.378 | 557.339 | 176.968 | 0 | 1088 / 345 / 0 |
| MS | 835.503 | 657.763 | 210.688 | 0 | 1284 / 411 / 0 |
| Mono | 681.100 | 546.940 | 180.874 | 0 | 1068 / 353 / 0 |

**IEP3 = 0** in allen Modi: Pipe 3 wird von dieser Instruktionsmischung
nicht belegt (bzw. nicht gezählt). Pipe 2 trägt ca. 24–31 %.
Damit ist eine explizite Pipe-3-Optimierung nicht möglich; der einzige
Ansatz wäre mehr ILP (L/R-Interleaving bei Stereo/MS). Mono ist seriell.

## Opt8 – E-Register L/R-Pair (final, produktiv)

Stereo nutzt jetzt einen inline Pair-Body mit E-Scratch:

- E2/E3 = L/R-Nibble
- E4/E5 = L/R-Delta
- `d6/d7` bleiben Loop-Zähler
- keine `bsr decode_primary/secondary` pro Paar mehr

Golden **PASS**; 100× **PASS**:

| Modus | Opt3a MIN/Med | Opt8 MIN/Med |
|---|---:|---:|
| Stereo | 1242 / 1243 | **1136 / 1137** |
| MS | 1526 / 1526 | **1466 / 1467** |
| Mono | 1256 / 1256 | **1196 / 1196** |

MS/Mono-Quellcode ist unverändert; die Werte profitieren von Layout/Codegröße.
`decoder_4bit.s` = Opt8; Hauptdecoder gebaut.

## Opt9 – E-Register MS-Pair (final, produktiv)

MS nutzt jetzt denselben E-Scratch-Pair-Body wie Stereo, gefolgt von der
Mid/Side-Matrix:

- E2/E3 = Mid/Side-Nibble
- E4/E5 = Mid/Side-Delta
- `d6/d7` bleiben Loop-Zähler
- keine `bsr decode_primary/secondary` mehr im MS-Pfad

Golden **PASS**; 100× **PASS**:

| Modus | Opt8 MIN/Med | Opt9 MIN/Med |
|---|---:|---:|
| Stereo | 1136 / 1137 | 1135 / 1135 |
| MS | 1466 / 1467 | **1399 / 1399** |
| Mono | 1196 / 1196 | 1196 / 1197 |

MS-Golden 512: **746.255** statt 800.280 (~−6,7 %).
`decoder_4bit.s` = Opt9; Hauptdecoder gebaut.

## Unabhängige C-Neuimplementierung (verworfen)

`experiments/decoder_4bit_fresh.c` wurde ohne Asm-Vorlage als reine
C-Implementierung gebaut (`tests/build_fresh.sh`, `get_ccc.s`).

Ergebnis: **Golden FAIL**, u. a.:

```text
MONO;FAIL;blocks=512;stateL=-508755968/88;PCM
```

Die C-Version ist damit weder bit-exakt noch als Performance-Vergleich
verwertbar. Sie bleibt Experiment; produktiv ist weiterhin die Asm-Opt9.

## AMMX / Pipes / Fusing – Stand

- **AMMX:** nur `MOVEX.L` für den Bitstrom und `E0`–`E5` als Register/Scratch.
  Keine AMMX-SIMD-Arithmetik (`padd/psub/pmul` = 0 Vorkommen).
- **Parallele Pipes:** IEP-Messung (Opt3a): IEP1 ca. 76 %, IEP2 ca. 24–31 %,
  **IEP3 = 0**. Kein explizites Pipe-Scheduling; L/R wird nicht echt
  instruktionsparallel verwoben.
- **Fusing/Bonding:** keine expliziten Fuse-/Bond-Paare; der Code nutzt
  `EXG`, `BSR` und Tabellen, aber keine gezielte Fusing-Strategie.

Damit sind AMMX, Pipe 2/3 und Fusing **nicht vollständig ausgenutzt**.
Der Fresh-C-Versuch zeigt, dass ein reiner C-Neubau ohne ABI-/Alignment-
Korrektur nicht trägt; ein neuer Asm-Ansatz wäre der nächste sinnvolle
Vergleich.

## Frischer unrolled Asm-Entwurf (experimentell, noch nicht verifiziert)

`experiments/decoder_4bit_fresh_asm.s` ist eine unabhängige Neuimplementierung:

- 4 Paare pro MOVEX unrolled
- `d0/d1` = L/R-Nibbles, `d6/d7` = Temps
- `a2` = Chunk-Zähler
- keine E2–E5-Scratch, keine `bsr`-Kette

Erster Lauf: V4 nicht mehr erreichbar (Absturz/Reset). Ursache im Entwurf:
`d6` wurde nur als Wort aus der Delta-Tabelle geladen, danach aber als Long
verrechnet (High-Word-Müll). `moveq #0,d6` vor jedem Tabellenzugriff ist
eingebaut; **Verifikation steht noch aus**, weil die V4 neu gebootet werden muss.

Produktiv bleibt Opt9. Der Fresh-Asm-Kandidat ist ausdrücklich nicht
freigegeben.
