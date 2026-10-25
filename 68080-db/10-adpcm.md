# 10 – ADPCM-4-Bit Case Study (verifiziert)

**Projekt:** `Projects/ADPCM_Decoder`  
**CPU:** Apollo 68080 / Vampire V4 / Core **11350** (ApolloControl), PCR 1088/1, VCoreRev 0  
**Messung:** 100 Warmup + 100 Läufe, State-Reset, C-Referenz; MIN/Median.

## Ergebnis

| Modus | Baseline | Opt9 | Fresh3 |
|---|---:|---:|---:|
| Stereo | 1546 / 1548 | 1135 / 1135 | 1001 / 1001 |
| MS | 1876 / 1878 | 1401 / 1402 | **1047 / 1047** |
| Mono | 1624 / 1625 | 1196 / 1196 | **816 / 816** |

Fresh3 = **ca. 35–50 %** schneller als Baseline; alle Golden/Repeat PASS.

## Gut (übernommen)

- **ADP-001 – Exakte 89×8-Delta-Tabelle:**  
  `(step>>3)+b0*(step>>2)+b1*(step>>1)+b2*step` exakt vorberechnen.
  Ersetzt drei Bit-Test-/Branch-Ketten durch einen Tabellenzugriff.
- **ADP-002 – `moveq #0,d6` vor Wort-Tabellenzugriff:**  
  `move.w (a4,...),d6` lässt das High-Word stehen; danach `add.l/sub.l d6`.
  High-Word vorher nullen.
- **ADP-003 – 4 Paare pro MOVEX unrolled:**  
  Kein `bsr` pro Kanal, weniger Loop-Overhead. Besonders stark bei MS/Mono.
- **ADP-004 – Selektives Unrolling:**  
  Stereo bleibt geloopt (Codegröße/I-Cache), MS/Mono unrolled.
  Nicht blind alle Modi unrollen.
- **ADP-005 – D-Register statt E2–E5-Scratch:**  
  Der schnellste ADPCM-Pfad nutzt `d0/d1` als Nibbles und `d6/d7` als Temps;
  E-Register nur für MOVEX/CCC. E-Scratch war nicht schneller.

## Schlecht (verworfen)

- **ADP-101 – `step*mult >> 3`:**  
  Falsch, weil die Originalrechnung **jeden Shift einzeln abrundet**.
  Off-by-one bei PCM/State; bit-exakte Tests decken das auf.
- **ADP-102 – `subq.l #1,a2` + `bne`:**  
  Adressregister-`subq` setzt **keine Flags** → Endlosschleife/Pufferüberlauf.
  Zähler über D-Register führen oder `cmpa`.
- **ADP-103 – `MIN.L`/`MAX.L` als Clamp:**  
  Vasm akzeptiert es, die V4 stürzt damit ab (Reset). Nicht verwenden.
- **ADP-104 – Reiner C-Neubau:**  
  Frischer C-Ansatz war nicht bit-exakt (State/ABI/Alignment); kein valider
  Performance-Vergleich.
- **ADP-105 – E2–E5-Scratch als Selbstzweck:**  
  Mehr E-Moves statt D-Register brachten keinen Vorteil; der D-Register-
  Unroll-Pfad ist schneller.

## Noch offen

- AMMX-SIMD (`PADDW`, `PACK3216`, `PCMPGTB/BSEL`) ist **nicht** eingesetzt.
- Pipe 3 = 0 in der IEP-Messung; Pipe 2 ca. 24–31 %.
- Fusing/Bonding nicht gezielt genutzt.

**Fazit:** Die größten Gewinne kamen aus exakter Tabelle, weniger Branches,
Unrolling und Registerwahl – nicht aus AMMX. AMMX bleibt der nächste
ernsthafte Kandidat, braucht aber einen anderen Datenfluss.

## Negativtests (neu getestet)

- `PADDSW`/`PSUBSW`: vasm 2.0d akzeptiert die Mnemonics nicht; kein
  signed-saturating AMMX-Pfad über diesen Assembler.
- `PACK` (generisch): illegale Operandentypen; `PACK3216` ist RGB565-spezifisch.
- `LEA (d2,d4.l),d0`: nicht encodierbar (Basis muss ein Adressregister sein).
  FUS move+add bleibt der Weg.
- `MIN.L`/`MAX.L`: assembliert, aber V4-Reset; nicht verwenden.
- `subq.l #1,a2` + `bne`: Adressregister setzt keine Flags; Zähler über D.

Siehe [11-adpcm-candidates.md](11-adpcm-candidates.md) für die
Instruktions-/Schätzungsmatrix (gemessen vs. Hypothese).

- `PCMPGTW`/`BSEL`: 4×16-Bit-Max in 7 Takten vs 33 Skalar (4,7×), aber kein 32-Bit-`PCMPGTL`; ADPCM-PCM/MS-Clamps bleiben 32-Bit.

## Fresh5 – branchless Sign (verworfen)

`btst/sne/ext/eor/sub/add` statt `btst/beq/sub/add`: 1047/1112/900 statt
1001/1047/816. Mehr Instruktionen schlagen den gesparten Branch.
Lehre: **Branchless ist nicht automatisch schneller**; bei 68080 erst messen.

## Fresh6 – `subq` + `cmpa` statt D-Rückkopie

Adressregister-`subq` setzt keine Flags. `cmpa.l #0,An` setzt sie explizit
und spart die Rückkopie über `d6`:

```asm
subq.l  #1,a2
cmpa.l  #0,a2
bne     loop
```

Gemessen: Stereo 1001 → **980**, MS 1047 → 1046, Mono 816 → 816.
**Gut** für Adressregister-Zähler; ein `cmpa` ist billiger als drei Moves.

## Fresh7 – 16-Bit-Overflow-Matrix

MS-Matrix mit `add.w`/`sub.w` + `bvc`; nur bei Überlauf wird anhand des
Vorzeichens von `mid` auf ±32767/−32768 geklemmt. Kein 32-Bit-Clamp-Pfad.

Gemessen: MS **1046 → 1027**, Stereo 980 → 978, Mono 819 (Rauschen).
Golden/Repeat PASS. **Gut:** weniger Instruktionen im häufigen In-Range-Fall.

## Fresh8 – PCM-Word-Overflow (verworfen)

`add.w`/`sub.w` + `bvc` + `ext.l` für PCM-Clamps: reproduzierbarer
**Line-1010-Crash** im Golden-Test. Nicht sicher; Fresh7 bleibt Produktiv.
Lehre: 16-Bit-Overflow-Clamping für den 32-Bit-PCM-State erst isoliert
verifizieren, nicht direkt in den Decoder patchen.
