# ADPCM 68080 – Optimierungs-Referenz & Logbuch (Core 11350)

## Aktueller Produktivstand (Gesamtübersicht aller 18 Modi)

Alle produktiven Decoder (`decoder_2bit.s`, `decoder_3bit.s`, `decoder_4bit.s`, `decoder_5bit.s`, `decoder_6bit.s`, `decoder_3_2ms.s`, `decoder_4_2ms.s`, `decoder_5_3ms.s`) nutzen auf **Core 11350 (PCR 1088/1, VCoreRev 0)** vorberechnete **Signed-32-Bit-Delta-Tabellen** (`dc.l`) und Inline-Makros ohne `bsr`/`rts`-Overhead und ohne `exg d6,a2`.

### 100×-Repeat-Benchmark (1 Block = 15 Bytes, 100 Warmup + 100 Messläufe, MIN/Median in Takten)

| Decoder / Modus | Samples/Block | Baseline (MIN/Med) | **Produktiv (MIN/Med)** | Ersparnis | Golden (512 Blöcke) |
|---|---:|---:|---:|---:|---|
| **2-Bit Stereo** | 30 Paare | 2443 / 2443 | **1337 / 1337** | **−45,3 %** | PASS (778.351 cyc) |
| **2-Bit M/S** | 30 Paare | 2869 / 2869 | **1763 / 1763** | **−38,5 %** | PASS (1.002.541 cyc) |
| **2-Bit Mono** | 60 Samples | 2335 / 2335 | **1254 / 1254** | **−46,3 %** | PASS (771.152 cyc) |
| **3-Bit Stereo** | 20 Paare | 1941 / 1941 | **1092 / 1092** | **−43,7 %** | PASS (572.402 cyc) |
| **3-Bit M/S** | 20 Paare | 2229 / 2229 | **1380 / 1380** | **−38,1 %** | PASS (713.650 cyc) |
| **3-Bit Mono** | 40 Samples | 1805 / 1805 | **1007 / 1007** | **−44,2 %** | PASS (568.140 cyc) |
| **4-Bit Stereo (`Fresh15`)** | 15 Paare | 1546 / 1548 | **784 / 784** | **−49,3 %** | PASS (589.315 cyc) |
| **4-Bit M/S (`Fresh15`)** | 15 Paare | 1876 / 1878 | **920 / 920** | **−51,0 %** | PASS (616.910 cyc) |
| **4-Bit Mono (`Fresh15`)** | 30 Samples | 1624 / 1625 | **729 / 729** | **−55,1 %** | PASS (459.075 cyc) |
| **5-Bit Stereo** | 12 Paare | 1356 / 1356 | **538 / 538** | **−60,3 %** | PASS (379.266 cyc) |
| **5-Bit M/S** | 12 Paare | 1512 / 1512 | **699 / 699** | **−53,8 %** | PASS (470.141 cyc) |
| **5-Bit Mono** | 24 Samples | 1236 / 1236 | **503 / 503** | **−59,3 %** | PASS (349.885 cyc) |
| **6-Bit Stereo** | 10 Paare | 1331 / 1331 | **609 / 609** | **−54,2 %** | PASS (336.226 cyc) |
| **6-Bit M/S** | 10 Paare | 1457 / 1457 | **735 / 735** | **−49,6 %** | PASS (406.829 cyc) |
| **6-Bit Mono** | 20 Samples | 1373 / 1373 | **590 / 590** | **−57,0 %** | PASS (328.392 cyc) |
| **3/2-Bit Hybrid M/S** | 24 Paare | 2411 / 2411 | **1248 / 1248** | **−48,2 %** | PASS (872.075 cyc) |
| **4/2-Bit Hybrid M/S** | 20 Paare | 2254 / 2254 | **1358 / 1358** | **−39,8 %** | PASS (765.683 cyc) |
| **5/3-Bit Hybrid M/S** | 15 Paare | 1755 / 1755 | **958 / 958** | **−45,4 %** | PASS (909.672 cyc) |

---

## Chronologisches Optimierungs-Logbuch (4-Bit: Opt2 → Fresh15)

### Opt3 (historischer Zwischenstand)

`decode_primary` und `decode_secondary`
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

### Fresh-Asm-Verifikation (fehlgeschlagen)

Der korrigierte Fresh-Asm-Lauf (`d6`-High-Word gefixt) stürzt weiterhin ab:

```text
Software Failure
Task: RAM:golden_test_freshasm
Error: 80000005 - Division by zero
PC: 00009DC6
```

Das ist kein gültiger Kandidat; die unrolled Makro-/Kontrollflussstruktur
muss neu geprüft werden. Produktiv bleibt **Opt9**.

## Fresh-Asm – erfolgreich, neuer Produktivstand

Nach zwei Fixes läuft die unabhängige unrolled Asm-Version bit-exakt:

1. `d6` vor jedem Tabellenzugriff nullen (High-Word).
2. Chunk-Zähler nicht mit `subq.l #1,a2` + `bne` (Adressregister setzt keine
   Flags), sondern über `d6` mit `subq` und Rückkopie.

Golden **PASS**, 100× **PASS**:

| Modus | Opt9 MIN/Med | Fresh-Asm MIN/Med |
|---|---:|---:|
| Stereo | 1135 / 1135 | **997 / 997** |
| MS | 1401 / 1402 | **1237 / 1237** |
| Mono | 1196 / 1196 | **963 / 963** |

Golden 512: Stereo **589.315**, MS **715.725**, Mono **566.197**.

Gegenüber Baseline: Stereo ~35,5 %, MS ~34,1 %, Mono ~40,7 % schneller.
`decoder_4bit.s` = Fresh-Asm; Opt9-Backup: `experiments/decoder_4bit_opt9_prod.s`.

## Fresh2/Fresh3 – selektives Chunk-Unrolling

- **Fresh2:** alle Chunks unrolled → MS +15 %, Mono +15 %, Stereo kein Gewinn.
- **Fresh3:** Stereo bleibt geloopet, MS/Mono unrolled → bester Kompromiss.

| Modus | Fresh-Asm | Fresh3 |
|---|---:|---:|
| Stereo | 997 / 997 | 1001 / 1001 |
| MS | 1237 / 1237 | **1047 / 1047** |
| Mono | 963 / 963 | **816 / 816** |

Golden Fresh3: Stereo 674.513, MS 716.661, Mono 556.172.
`decoder_4bit.s` = Fresh3; Fresh-Asm-Backup: `experiments/decoder_4bit_fresh_prod.s`.

## Negativtests (nicht dokumentiert / nicht nutzbar)

- `PADDSW`/`PSUBSW`: vasm 2.0d kennt die Mnemonics nicht.
- `PACK` (generisches 32→16): illegale Operandentypen.
- `LEA (d2,d4.l),d0`: nicht encodierbar (Basis muss Adressregister sein).
- `MIN.L`/`MAX.L`: assembliert, aber die V4 resettet (nicht nutzbar).
- `subq.l #1,a2` + `bne`: Adressregister setzt keine Flags.
- Reiner C-Neubau: nicht bit-exakt (ABI/Alignment).

## Mikrobenchmark-Gate (Pflicht)

Nach jeder Änderung:

1. relevanten Mikrobenchmark (100×, MIN/Median, Baseline) laufen lassen
2. End-to-End Golden + 100×
3. bei besser/schlechter: **warum** in dieser Datei/DB notieren
4. committen und pushen

Erster neuer Mikrobenchmark: **AMX-007 `PADDW`** – MIN 5, Median 5,
Baseline 5, `check_log.py` OK. Kein Vorteil gegenüber `PADDB`; keine
signed Saturation, daher für ADPCM nur begrenzt nutzbar.

## Core-Version

Alle Ergebnisse in diesem Projekt beziehen sich auf:

- **Apollo Core 11350**
- PCR 1088 / Revision 1
- VCoreRev 0

`auto_opt.py` speichert `core-before.json`, `core-after.json` und
`core_before`/`core_after` in `summary.json`.

## Fresh5 – branchless Vorzeichen (verworfen)

Idee: `btst #3` + `beq/sub/bra/add` durch `btst/sne/ext/eor/sub/add` ersetzen.

| Modus | Fresh3 | Fresh5 |
|---|---:|---:|
| Stereo | 1001 | 1047 |
| MS | 1047 | 1112 |
| Mono | 816 | 900 |

**Schlechter** (4,6 % / 6,2 % / 10,3 %). Die Maske kostet 3–4 zusätzliche
Instruktionen; der 68080-Branch-Predictor verkraftet das 50/50-Vorzeichen
besser als erwartet. **Nicht übernommen.**

## Fresh6 – cmpa-Chunk-Zähler (übernommen)

`subq.l #1,a2` setzt keine Flags; bisher wurde über `d6` zurückkopiert.
Jetzt:

```asm
subq.l  #1,a2
cmpa.l  #0,a2
bne     loop
```

Eine Instruktion weniger pro Chunk. Ergebnis:

| Modus | Fresh3 | Fresh6 |
|---|---:|---:|
| Stereo | 1001 | **980** |
| MS | 1047 | 1046 |
| Mono | 816 | 816 |

Golden Fresh6: Stereo 596.528, MS 713.774, Mono 565.790.
Produktiv: **Fresh6**; Fresh3-Backup: `experiments/decoder_4bit_fresh3_prod.s`.

## Fresh7 – 16-Bit-Overflow-Matrix (übernommen)

MS-Matrix ohne 32-Bit-Clamp-Ketten:

```asm
move.w  d2,d0
add.w   d4,d0
bvc.s   .mlok\@
tst.w   d2
bge.s   .mlmax\@
move.w  #-32768,d0
bra.s   .mlok\@
.mlmax\@:
move.w  #32767,d0
.mlok\@:
```

Bei Überlauf entscheidet das Vorzeichen von `mid` über Min/Max. Golden/Repeat
PASS.

| Modus | Fresh6 | Fresh7 |
|---|---:|---:|
| Stereo | 980 | **978** |
| MS | 1046 | **1027** |
| Mono | 816 | 819 |

Golden Fresh7: MS **698.715**, Mono **551.802**.
Produktiv: **Fresh7**; Fresh6-Backup: `experiments/decoder_4bit_fresh6_prod.s`.

## Fresh8 – 16-Bit-Overflow-PCM (verworfen, Crash)

Versuch: PCM-`add.l`/`sub.l` + zwei Clamps durch `add.w`/`sub.w` + `bvc`
und `ext.l` ersetzen. Golden-Test stürzt reproduzierbar mit **Line 1010
(8000000A)** ab (auch nach Neustart). **Nicht übernommen.**

Fresh7 bleibt Produktivstand. Die Ursache ist nicht isoliert; der Ansatz darf
erst nach einem separaten Minimal-Harness-Test wiederverwendet werden.


## PCM-Overflow-Mikrotest (offen)

Isolierter Test `tests/pcm_ovf.s` + `pcm_ovf_test.c`; C-`int` ist auf
m68k-amigaos **16 Bit**, daher `int32_t` und Long-Argumente. Der große Test
läuft noch nicht stabil durch; V4 verliert während des Laufs das Netz.
Fresh7 bleibt Produktivstand.


Minimaltest PCM-Overflow auf V4: **PASS** (`ONE32 0 32767 -32768`).
Der große Flächentest ist wegen C-Referenz-/Loop-Problemen noch nicht
belastbar; keine Produktivänderung.

## PCM-Overflow: reiner ASM-Test PASS

`pcm_asm_test` (7 Deltas × 2 Vorzeichen × 65536 PCM-Werte) gegen 32-Bit-ASM-
Referenz: **917504/917504 PASS**. Die Operation ist korrekt; Fresh8 bleibt
wegen des Integrationsfehlers verworfen.

## Fresh8-Diagnose

- Frischer ASM-Test: PCM-Operation `add.w/sub.w + bvc` **917504/917504 PASS**.
- Fresh8 **nur im Mono-Makro** (`F_MONO_NIB`) → reproduzierbar **Line 1010**.
- Also liegt der Fehler in der **Mono-Integration/Makro**, nicht in der Arithmetik.
- Ein `ext.l`-only-Gegenversuch konnte wegen V4/ASH-Blockade nicht sauber laufen.
- **Kein Fresh8-Performancegewinn nachweisbar**; Fresh7 bleibt Produktivstand.

## Fresh8 – kein Performancegewinn, keine stabile Integration

- `ext.l`-only: Golden/Repeat PASS, aber Mono **833** statt 819 → teurer.
- `add.w/sub.w`-only im Mono-Makro: Crash **Line 1111 F**.
- Fresh8 gesamt: Crash **Line 1010**.
- Operation isoliert: 917504/917504 PASS.
- **Fazit:** Fresh8 nicht übernehmen; Fresh7 bleibt Produktivstand.

## Fresh9 – Signed-32-Bit-Delta-Tabelle (übernommen)

Die Tabelle enthält jetzt pro Index **16 Einträge** (Nibble 0–15) als
**signed 32-Bit-Deltas** (`dc.l`, 1424 Einträge). Dafür entfallen:

- `btst/beq/sub/bra/add` (Vorzeichenbranch)
- `and.l #7,d7` (Nibble-Maske)
- `ext.l d6`

Lookup:

```asm
lsl.w   #4,d6
move.l  (a4,d7.w*4),d6
add.l   d6,d2
```

| Modus | Fresh7 | **Fresh9** |
|---|---:|---:|
| Stereo | 978 | **848** |
| MS | 1027 | **922** |
| Mono | 819 | **732** |

Golden + 100× PASS. Golden 512: MS **614.851**, Mono **475.884**.
Produktiv: **Fresh9**; Fresh7-Backup: `experiments/decoder_4bit_fresh7_prod.s`.

## Fresh10–Fresh14 – Stereo-Unroll und Alignment

- **Fresh10:** Stereo-Byte-Rest unrolled → Stereo **826**, aber Mono **845**
  (Layout-/I-Cache-Effekt).
- **Fresh11:** + 32-Byte-Alignment Mono → **829 / 917 / 738**.
- **Fresh12:** zusätzlich MS-Alignment → **MS 1390, Mono 1342** (verworfen).
- **Fresh13:** Fresh9 + Mono-Alignment → **848 / 919 / 733** (kein Gewinn).
- **Fresh14:** Fresh11 + **64-Byte-Mono-Alignment** → **829 / 918 / 732**
  → **übernommen**.

Golden + 100× PASS. Golden 512: MS **656.143**, Mono **477.321**.
Produktiv: **Fresh14**; Fresh9-Backup: `experiments/decoder_4bit_fresh9_prod.s`.

## Fresh15 – Stereo voll unrolled (übernommen)

Auch der 3×MOVEX-Chunk-Loop ist entfernt; Stereo ist komplett unrolled.
Alignment Mono bleibt 64 Byte.

| Modus | Fresh14 | **Fresh15** |
|---|---:|---:|
| Stereo | 829 | **784** |
| MS | 918 | 920 |
| Mono | 732 | 729 |

Golden + 100× PASS. Golden 512: MS **616.910**, Mono **459.075**.
Produktiv: **Fresh15**; Fresh14-Backup: `experiments/decoder_4bit_fresh14_prod.s`.

## Fresh16/Fresh17 – Layout-Varianten (verworfen)

- **Fresh16:** Delta-Tabelle auf 64 Byte ausgerichtet → Stereo **897** (statt 784), MS 919, Mono 730. **Verworfen.**
- **Fresh17:** Stereo-Funktion auf 64 Byte ausgerichtet → Stereo **898**, MS 919, Mono 730. **Verworfen.**

**Fresh15 bleibt Produktivstand (4-Bit): 784 / 920 / 729.**
Layout-Änderungen sind auf dem 68080 extrem empfindlich; nur mit vollständigem
100×-Lauf übernehmen.

---

## 2-/3-/5-/6-Bit & Hybrid-M/S (`3/2`, `4/2`, `5/3`): Signed-32-Bit-Delta-Tabellen (2026-10-26, übernommen)

Das Erfolgsrezept aus `Fresh9` (vorberechnete `dc.l`-Tabelle mit vorzeichenbehafteten 32-Bit-Deltas + Inline-Makros ohne `bsr`/`rts` und ohne `exg d6,a2`, mit `d1` als einzigem Scratch-Register) wurde auf alle übrigen Decoder-Dateien übertragen:

- **Backups der Originale:** `experiments/decoder_{2bit,3bit,5bit,6bit,3_2ms,4_2ms,5_3ms}_orig.s`
- **Neue Produktiv-Implementierungen:** `decoder_{2bit,3bit,5bit,6bit,3_2ms,4_2ms,5_3ms}.s` (Kandidaten unter `experiments/decoder_*_signed.s`)
- **Wichtiger Bugfix in `decoder_2bit_signed.s`:** Ursprünglich nutzten `DEC_L`/`DEC_R`/`DEC_M` die Register `d6` und `d7` als Scratch, die jedoch gleichzeitig die `dbf`-Schleifenzähler waren. Umstellung auf `d1` als einziges Scratch-Register behebt die Kollision und spart zudem Instruktionen.
- **Kritischer Compiler-Befund (`m68k-amigaos-gcc -m68080 -O2`):**
  - Die scheinbaren C-Referenz-Abweichungen (`PCM = -65536` statt `-32768`) und Abstürze beim Start von C-Test-Binaries (`debug2`, `repeat3`) lagen an fehlerhafter Code-Generierung im `-m68080`-Backend von GCC 6.5.0 bei `-O2`:
    1. `moviw.l #-32768, d0` (Opcode `0x303d 0x8000`) schreibt auf Core 11350 `0xFFFF0000` (`-65536`).
    2. In `rng()`-Initialisierungsschleifen berechnet GCC um `moviw.l #7679, d2` (`0x343d 0x1dff`) eine ungerade Sprungdistanz (`dbf d2, <ungerade_adresse>`, `51ca ffe7`), was sofort einen Address-Error-Guru auslöst.
  - **Lösung:** Alle C-Test-Harnesse werden mit `-m68020 -O2` kompiliert und gegen die `vasm -m68080`-Objekte gelinkt.
- **Wichtiger ABI-Bugfix in `decoder_5_3ms.s`:** `ADPCM_Decoder.c` (Zeile 696) übergibt `block_cnt = 1` für einen 15-Byte-Block (15 M/S-Paare). Das ursprüngliche `decoder_5_3ms.s` interpretierte `block_cnt` jedoch als Einzelbyte-Paarzähler (sodass `1` nur 1 Paar statt 15 Paaren dekodierte). In `decoder_5_3ms.s` (und `experiments/decoder_5_3ms_signed.s`) verarbeitet jede `block_cnt`-Iteration nun wie bei allen anderen Decodern einen kompletten 15-Byte-Block (3× `movex.l` + 3× `move.b` = 15 Paare).
- **Makefile-Fix:** `C_OPTIONS` im Haupt-`Makefile` wurde von `-m68080` auf `-m68020` umgestellt (während `vasm` weiterhin `-m68080` nutzt), um den GCC-6.5.0-Codegen-Bug im Hauptprogramm auszuschließen.
- **Ergebnis:** Alle 18 Modi in `golden_all` (je 512 Blöcke) **PASS**, alle 100×-Repeat-Tests (`repeat2`, `repeat3`, `repeat5`, `repeat6`, `repeat_hybrid`) **PASS** mit **38,1 % bis 60,3 % weniger Taktzyklen** (siehe Gesamttabelle oben). Alle 17 Real-Audio-Hörtests (je 10,00 s auf der V4) **Sound OK** (siehe `tests/AUDIO_COMPARE.md`).


## Korrigierte Original-vs-Signed-Messung (2026-10-30)

Die Skripte verglichen zuvor die signierte Produktion als „orig“. Nach Fix auf
`experiments/decoder_*_orig.s`: 2-Bit 38–46 %, 3-Bit 38–44 %, 5-Bit 51–59 %,
6-Bit 50–57 %, Hybrid 39,7–48,2 % weniger Zyklen. Alle 15 Golden-Modi PASS.
Details: `tests/ORIGINAL_VS_SIGNED.md`. 5-Bit-Signed-Ausreißer (650 statt 538)
ist als offene Layout-/Messfrage dokumentiert.
