# 11 – ADPCM Instruction Candidates (gemessen vs. Schätzung)

**Regel:** Eine Zahl ist nur ein Messwert, wenn ein 100×-Mikrobenchmark und
ein Log vorliegen. Alles andere ist ausdrücklich **Schätzung/Hypothese** und
muss vor einer Übernahme gemessen werden.

## Gemessen

| Instruktion | Einsatz | Messwert | Bewertung |
|---|---|---:|---|
| `MOVEX.L` | 4 Byte/Nibbles laden | in Fresh3 genutzt | **gut**, unverzichtbar |
| `PADDB` | 8×8-Bit-Adds | MIN 5, Median 5 | gut für Byte-Lanes |
| `PADDW` | 4×16-Bit-Adds | MIN 5, Median 5 | gleicher Preis wie PADDB, breitere Lanes |
| `PACK3216` | 2×2 32-Bit → RGB565 | MIN 5 | **für ADPCM ungeeignet** (RGB-spezifisch) |
| `MIN.L`/`MAX.L` | Clamp | — | **V4-Reset**, nicht nutzbar |
| `PADDSW`/`PSUBSW` | signed saturation | — | vasm 2.0d kennt Mnemonics nicht |
| `LEA (d2,d4.l)` | move+add ersetzen | — | nicht encodierbar (Basis muss An sein) |
| `subq.l #1,An` + `bne` | Loop-Zähler | — | **keine Flags**, Endlosschleife |

## Schätzungen (noch nicht gemessen)

| Kandidat | erwarteter Nutzen | geschätzte Beschleunigung | Status |
|---|---|---:|---|
| `PADDW` + anschließendes Clamping | 4×16-Bit-Lanes, aber Clamp bleibt skalar | **0–3 %** (MS-Matrix) | nur bei 4-Lane-Batch sinnvoll; für 2 Lanes (L/R) kein Vorteil |
| `PADDSW`/`PSUBSW` (falls verfügbar) | Add+Clamp in einer Instruktion | **5–12 %** MS, **3–8 %** PCM | Tool/CPU nicht verfügbar; nicht verwenden |
| `PCMPGTB`/`BSEL` | branchless Clamp/Select | **2–6 %** | braucht eigenen 100×-Mikrobenchmark |
| `PACK3216` | Packen L/R | **0 %** | falsches Format (RGB565) |
| explizites Fusing `move+add` | Matrix | **0–2 %** | eventuell schon CPU-seitig fusioniert |
| Pipe-3-Scheduling | unabhängige L/R-Arbeit | **0–5 %** | IEP3 war 0; erst Pipe-Messung |

## Konsequenz für ADPCM

1. **PADDW mit Clamping** ist kein klarer Gewinn: Der Clamp bleibt vier
   Vergleiche/Branches; bei nur zwei PCM-Lanes ist der Packing-/Extract-
   Overhead größer als der Nutzen. Erst 4-Lane-Batching messen.
2. **PADDSW/PSUBSW** wären ideal, sind mit vasm 2.0d aber nicht nutzbar.
3. **PCMPGTB/BSEL** ist der nächste sinnvolle, messbare Kandidat für
   branchless Clamping.
4. **PACK3216** ist für ADPCM-PCM **falsch**; nicht weiter verfolgen.

## Nächster Mikrobenchmark

- `PCMPGTB + BSEL` Clamp-Sequenz gegen `cmp/ble/bge`-Clamp.
- Danach erst entscheiden, ob ein Fresh4-Umbau der PCM-/Matrix-Clamps lohnt.
