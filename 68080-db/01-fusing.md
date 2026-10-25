# 01 – Instruction Fusing

**Quellen:** Apollo 68080 PRM (Juni 2026), S. 85 „Instruction Fusing“ und
S. 86 „Instruction Efficiency“; Apollo-Wiki „Instruction Fusing“ (Fassung
2020-08-02, Internet Archive). Die Wiki-Seite nennt die hier verwendeten Paare
für den SILVER2-Core; das PRM führt dieselben Kombinationen für die V4-Cores.

**Kernaussage:** Der 68080 kann zwei *benachbarte* 68k-Instruktionen, die
dasselbe Zielregister aktualisieren, zu einer internen 3-Operanden-
Instruktion verschmelzen. Das ist **kein** normales Superscalar und **keine
Quelltext-Syntax**: Man schreibt weiterhin zwei 68k-Befehle, der Core erkennt
das Paar. Ein eingeschobener `nop` oder eine unabhängige Instruktion zerstört
die Nachbarschaft und damit die dokumentierte Paarbildung.

> **Messstatus:** Alle Einträge wurden mit **Harness v5** auf einer Vampire V4
> gemessen (10 × 100 Läufe). Die v5-Dateilogs enthalten zusätzlich `PERF`
> (Pipe-Slots). Referenz ist MIN, MAX/AVG sind Mediane der 10 Läufe; die
> v1-Logs bleiben als Archiv unter `measurements/<ID>.log`.

## FUS-001: MOVE.L + ADD.L zur 3-Operanden-Fusion

**Kategorie:** Fusing  
**Status:** VERIFIZIERT (gemessen – Vampire V4, Log vorhanden)  
**Beleg:** VERIFIZIERT (Literatur): PRM S. 85; Apollo-Wiki „Instruction Fusing“  
**Betroffene CPUs:** 68080 (V4); nicht 68060

### Problem

Auf dem 68060 ist es sinnvoll, abhängige Instruktionen zu trennen oder mit
unabhängiger Arbeit zu füllen. Auf dem 68080 verhindert genau das die Fusion:
`move.l d2,d3` und `add.l d4,d3` sind ein dokumentiertes Paar; ein `nop`
dazwischen macht daraus zwei getrennte Instruktionen.

### Naiver 68k-Ansatz (langsam auf 68080)

```asm
    move.l  d2,d3
    nop                 ; "Pipeline füllen" – hier kontraproduktiv
    add.l   d4,d3
```

### 68080-Ansatz

```asm
    move.l  d2,d3
    add.l   d4,d3       ; benachbart lassen: der Core fused
```

### Warum das funktioniert

Die Apollo-ALU ist intern 3-operandig. Aus dem Paar wird intern
`add.l d2,d4,d3` (C: `d3 = d2 + d4`). Das PRM listet genau diese Kombination
als Fusing-Paar. Die Wiki-Seite beschreibt für dieses Beispiel, dass beide
68k-Instruktionen zusammen in einem Takt ausgeführt werden; das PRM sagt
allgemein „Integer CPU & AMMX instructions normally are 1 cycle“.

**Wichtig:** `add.l d2,d4,d3` ist **keine** vom Assembler akzeptierte
Quelltext-Form. vasm 2.0d meldet dafür „illegal operand types“. Die
3-Operanden-Form ist eine interne Repräsentation, keine neue 68k-Syntax.

### Messung

- Sequenz: [`seq/FUS-001.i`](seq/FUS-001.i)
- Log: [v5 V4-Datei](measurements/v5-files/FUS-001.run01.log) · [v1-Archiv](measurements/FUS-001.log)
- **Messergebnis (Vampire V4, Harness v5, 10 × 100 Läufe):**
  - `BASELINE = 5` (Median der 10 Läufe)
  - `MIN = 3` Takte (in allen 10 Läufen identisch)
  - `MAX = 3` Takte (Median der 10 Läufe)
  - `AVG = 3` Takte (Median der 10 Läufe)
  - **Pipe-Slots (v5):** `IEP1 = 4`, `IEP2 = 0`, `IEP3 = 0` pro Lauf (Median); Summe = 4.
  - 10/10 Läufe mit 100/100 `RES`-Zeilen, `check_log.py` OK.
- **Warmup:** Die ersten 10 Läufe liegen bei MIN; der v1-Kaltstart ist weg.
- **Wiederholbarkeit:** MIN 10/10 identisch; MAX/AVG sind Mediane der 10 Läufe.
- **Baseline-Warnung:** `BASELINE = 5`; `NET = RES − BASELINE` wird negativ und ist kein gültiger Sequenzwert.

- **Register:** `d2`, `d3`, `d4`; reservierte Register unberührt.
- **Vergleich:** Der Anti-Pattern-Lauf [`ANTI-001`](08-antipatterns.md#anti-001-nop-zwischen-move-und-abhängigem-add)
  misst dasselbe Paar mit `nop`: MIN = 9 Takte gegen MIN = 3 Takte hier.
  Die Gegenprobe bestätigt den Fusing-Nachteil des NOPs in diesem Test.

**Gemessene Sequenz (verbatim aus `seq/FUS-001.i`, gebaut als `bin-v5/FUS-001`):**

```asm
; seq/FUS-001.i - MOVE.L + ADD.L (3-operand fusion)
; Uses: d2, d3, d4
; MUST NOT touch: d5, d6, d7, a5, a6, a7
        move.l  d2,d3
        add.l   d4,d3
```

## FUS-002: MOVE.L + NEG.L

**Kategorie:** Fusing  
**Status:** VERIFIZIERT (gemessen – Vampire V4, Log vorhanden)  
**Beleg:** VERIFIZIERT (Literatur): PRM S. 85 (`not.b/w/l dm & neg, addq, subq`)  
**Betroffene CPUs:** 68080 (V4); nicht 68060

### Problem

Eine Kopie plus Vorzeichenwechsel wird oft als zwei abhängige Schritte
geschrieben. Auf dem 68060 bleibt das eine serielle Kette; auf dem 68080 ist
es ein Fusing-Paar.

### Naiver 68k-Ansatz

```asm
    move.l  d2,d3
    nop
    neg.l   d3
```

### 68080-Ansatz

```asm
    move.l  d2,d3
    neg.l   d3          ; move+neg als ein internes Read-Modify-Write
```

### Warum das funktioniert

Das PRM führt `move.l dn,dm` gefolgt von `neg` (und `not`, `addq`, `subq`) als
Fusionspaar. Beide Instruktionen aktualisieren dasselbe Zielregister `d3`;
der Core kann sie deshalb in einem internen 32-Bit-Register-Update
zusammenfassen. Die sichtbare Semantik bleibt „kopiere, dann negiere“,
inklusive der von `NEG` gesetzten Condition Codes.

### Messung

- Sequenz: [`seq/FUS-002.i`](seq/FUS-002.i)
- Log: [v5 V4-Datei](measurements/v5-files/FUS-002.run01.log) · [v1-Archiv](measurements/FUS-002.log)
- **Messergebnis (Vampire V4, Harness v5, 10 × 100 Läufe):**
  - `BASELINE = 5` (Median der 10 Läufe)
  - `MIN = 3` Takte (in allen 10 Läufen identisch)
  - `MAX = 3` Takte (Median der 10 Läufe)
  - `AVG = 3` Takte (Median der 10 Läufe)
  - **Pipe-Slots (v5):** `IEP1 = 4`, `IEP2 = 0`, `IEP3 = 0` pro Lauf (Median); Summe = 4.
  - 10/10 Läufe mit 100/100 `RES`-Zeilen, `check_log.py` OK.
- **Warmup:** Die ersten 10 Läufe liegen bei MIN; der v1-Kaltstart ist weg.
- **Wiederholbarkeit:** MIN 10/10 identisch; MAX/AVG sind Mediane der 10 Läufe.
- **Baseline-Warnung:** `BASELINE = 5`; `NET = RES − BASELINE` wird negativ und ist kein gültiger Sequenzwert.

- **Register:** `d2`, `d3`.

**Gemessene Sequenz (verbatim aus `seq/FUS-002.i`, gebaut als `bin-v5/FUS-002`):**

```asm
; seq/FUS-002.i - MOVE.L + NEG.L (fused read-modify-write)
; Uses: d2, d3
; MUST NOT touch: d5, d6, d7, a5, a6, a7
        move.l  d2,d3
        neg.l   d3
```

## FUS-003: MOVEQ + OR.L

**Kategorie:** Fusing  
**Status:** VERIFIZIERT (gemessen – Vampire V4, Log vorhanden)  
**Beleg:** VERIFIZIERT (Literatur): PRM S. 85 (`moveq #,dn and.(b/w/l) dx,dn & or`)  
**Betroffene CPUs:** 68080 (V4); nicht 68060

### Problem

Konstanten werden oft in ein Register geladen und sofort mit einem anderen
Register verknüpft. Auf vielen 68k-Cores ist das eine serielle Abhängigkeit;
auf dem 68080 ist `moveq` + `or`/`and` ein Fusionspaar.

### Naiver 68k-Ansatz

```asm
    moveq   #$12,d0
    nop
    or.l    d1,d0
```

### 68080-Ansatz

```asm
    moveq   #$12,d0
    or.l    d1,d0       ; moveq+or als ein internes Update
```

### Warum das funktioniert

Das PRM listet `moveq #,dn` zusammen mit `and.(b/w/l) dx,dn` und `or` als
Fusionskombination. Der `moveq`-Immediate wird in die interne Operation
eingebettet; das Zielregister ist in beiden Fällen `d0`.

**Grenze:** Die Fusion gilt für die dokumentierten `moveq`+`and/or`-Paare,
nicht für beliebige Immediate-Operationen. `add.l #...` ist ein eigener Fall
und sollte nicht aus dieser Regel abgeleitet werden.

### Messung

- Sequenz: [`seq/FUS-003.i`](seq/FUS-003.i)
- Log: [v5 V4-Datei](measurements/v5-files/FUS-003.run01.log) · [v1-Archiv](measurements/FUS-003.log)
- **Messergebnis (Vampire V4, Harness v5, 10 × 100 Läufe):**
  - `BASELINE = 26` (Median der 10 Läufe)
  - `MIN = 3` Takte (in allen 10 Läufen identisch)
  - `MAX = 3` Takte (Median der 10 Läufe)
  - `AVG = 3` Takte (Median der 10 Läufe)
  - **Pipe-Slots (v5):** `IEP1 = 4`, `IEP2 = 0`, `IEP3 = 0` pro Lauf (Median); Summe = 4.
  - 10/10 Läufe mit 100/100 `RES`-Zeilen, `check_log.py` OK.
- **Warmup:** Die ersten 10 Läufe liegen bei MIN; der v1-Kaltstart ist weg.
- **Wiederholbarkeit:** MIN 10/10 identisch; MAX/AVG sind Mediane der 10 Läufe.
- **Baseline-Warnung:** `BASELINE = 26`; `NET = RES − BASELINE` wird negativ und ist kein gültiger Sequenzwert.

- **Register:** `d0`, `d1`.

**Gemessene Sequenz (verbatim aus `seq/FUS-003.i`, gebaut als `bin-v5/FUS-003`):**

```asm
; seq/FUS-003.i - MOVEQ + OR.L (fused immediate/register op)
; Uses: d0, d1
; MUST NOT touch: d5, d6, d7, a5, a6, a7
        moveq   #$12,d0
        or.l    d1,d0
```

## FUS-004: Zwei MOVE.L (An)+,(Am)+ werden zu einem 64-Bit-Move

**Kategorie:** Fusing  
**Status:** VERIFIZIERT (gemessen – Vampire V4, Log vorhanden)  
**Beleg:** VERIFIZIERT (Literatur): PRM S. 85; Wiki „Instruction Fusing“ (Paar 1)  
**Betroffene CPUs:** 68080 (V4); nicht 68060

### Problem

Eine Kopierschleife mit einem 32-Bit-Move pro Iteration verschenkt die
64-Bit-Breite des 68080-Speicherpfads. Zwei benachbarte `move.l (a0)+,(a1)+`
werden dagegen als ein 64-Bit-Move ausgeführt.

### Naiver 68k-Ansatz

```asm
.loop:
    move.l  (a0)+,(a1)+
    subq.l  #1,d0
    bne.s   .loop
```

### 68080-Ansatz

```asm
    move.l  (a0)+,(a1)+
    move.l  (a0)+,(a1)+     ; Paar -> ein 64-Bit-Move
```

Im echten Kopierloop wird das Paar pro Iteration wiederholt; die
Schleifensteuerung bleibt außen vor. Die Sequenz misst genau das Paar, damit
der Effekt nicht mit Schleifen-Overhead vermischt wird.

### Warum das funktioniert

Das PRM listet:
`move.l (an)+,(am)+` + `move.l (an)+,(am)+` → „quad move“. Beide Moves lesen
und schreiben benachbarte 32-Bit-Worte; der 68080 hat einen 64-Bit-Speicherbus
und kann sie als einen 64-Bit-Zugriff behandeln. Die Wiki-Seite nennt das
explizit „executes them together in a single clock cycle“.

**Wichtig:** Es gibt dafür **keinen** portablen Quelltext-Mnemonic `move.q`
im getesteten vasm. `move.q` wird als „illegal opcode extension“ abgelehnt.
Die Fusion entsteht nur durch das benachbarte Paar.

### Messung

- Sequenz: [`seq/FUS-004.i`](seq/FUS-004.i)
- Log: [v5 V4-Datei](measurements/v5-files/FUS-004.run01.log) · [v1-Archiv](measurements/FUS-004.log)
- **Messergebnis (Vampire V4, Harness v5, 10 × 100 Läufe):**
  - `BASELINE = 5` (Median der 10 Läufe)
  - `MIN = 3` Takte (in allen 10 Läufen identisch)
  - `MAX = 13.5` Takte (Median der 10 Läufe)
  - `AVG = 3` Takte (Median der 10 Läufe)
  - **Pipe-Slots (v5):** `IEP1 = 4`, `IEP2 = 0`, `IEP3 = 0` pro Lauf (Median); Summe = 4.
  - 10/10 Läufe mit 100/100 `RES`-Zeilen, `check_log.py` OK.
- **Warmup:** Die ersten 10 Läufe liegen bei MIN; der v1-Kaltstart ist weg.
- **Wiederholbarkeit:** MIN 10/10 identisch; MAX/AVG sind Mediane der 10 Läufe.
- **Baseline-Warnung:** `BASELINE = 5`; `NET = RES − BASELINE` wird negativ und ist kein gültiger Sequenzwert.

- **Register:** `a0`, `a1` (beide werden um 8 Byte pro Lauf erhöht).

**Gemessene Sequenz (verbatim aus `seq/FUS-004.i`, gebaut als `bin-v5/FUS-004`):**

```asm
; seq/FUS-004.i - two MOVE.L (An)+,(Am)+ -> one 64-bit move
; Uses: a0, a1
; MUST NOT touch: d5, d6, d7, a5, a6, a7
        move.l  (a0)+,(a1)+
        move.l  (a0)+,(a1)+
```

## FUS-005: Zwei CLR.L (An)+ werden zu einem Quad-Clear

**Kategorie:** Fusing  
**Status:** VERIFIZIERT (gemessen – Vampire V4, Log vorhanden)  
**Beleg:** VERIFIZIERT (Literatur): PRM S. 85 (`clr.l (an)+` + `clr.l (an)+`)  
**Betroffene CPUs:** 68080 (V4); nicht 68060

### Problem

Ein Clear-Loop mit einem Long pro Iteration nutzt nur 32 Bit pro Schreibzugriff.
Zwei benachbarte `clr.l (a0)+` füllen dagegen ein 64-Bit-Fenster.

### Naiver 68k-Ansatz

```asm
.loop:
    clr.l   (a0)+
    subq.l  #1,d0
    bne.s   .loop
```

### 68080-Ansatz

```asm
    clr.l   (a0)+
    clr.l   (a0)+       ; Paar -> Quad-Clear
```

### Warum das funktioniert

Das PRM führt das Paar `clr.l (an)+` + `clr.l (an)+` als „quad clr“. Der Core
erkennt die beiden aufeinanderfolgenden Nullschreibzugriffe und fasst sie zu
einem 64-Bit-Schreibvorgang zusammen.

**Nicht verwechseln:** `clr.q` ist eine LineA-Instruktion, die laut PRM nur auf
den Scores 10280–10904 verfügbar war und ausdrücklich „Do not use“ ist. Das
benachbarte `clr.l`-Paar ist der portable Weg für V4-Cores.

### Messung

- Sequenz: [`seq/FUS-005.i`](seq/FUS-005.i)
- Log: [v5 V4-Datei](measurements/v5-files/FUS-005.run01.log) · [v1-Archiv](measurements/FUS-005.log)
- **Messergebnis (Vampire V4, Harness v5, 10 × 100 Läufe):**
  - `BASELINE = 26` (Median der 10 Läufe)
  - `MIN = 2` Takte (in allen 10 Läufen identisch)
  - `MAX = 13` Takte (Median der 10 Läufe)
  - `AVG = 2` Takte (Median der 10 Läufe)
  - **Pipe-Slots (v5):** `IEP1 = 21`, `IEP2 = 0`, `IEP3 = 0` pro Lauf (Median); Summe = 21.
  - 10/10 Läufe mit 100/100 `RES`-Zeilen, `check_log.py` OK.
- **Warmup:** Die ersten 10 Läufe liegen bei MIN; der v1-Kaltstart ist weg.
- **Wiederholbarkeit:** MIN 10/10 identisch; MAX/AVG sind Mediane der 10 Läufe.
- **Baseline-Warnung:** `BASELINE = 26`; `NET = RES − BASELINE` wird negativ und ist kein gültiger Sequenzwert.

- **Register:** `a0` (läuft 8 Byte pro Lauf weiter).

**Gemessene Sequenz (verbatim aus `seq/FUS-005.i`, gebaut als `bin-v5/FUS-005`):**

```asm
; seq/FUS-005.i - two CLR.L (An)+ -> one quad clear
; Uses: a0
; MUST NOT touch: d5, d6, d7, a5, a6, a7
        clr.l   (a0)+
        clr.l   (a0)+
```

## Grenzen der Fusion

- **Nur benachbarte Paare.** Ein `nop`, eine unabhängige Instruktion oder ein
  Label dazwischen verhindert die dokumentierte Paarbildung. Deshalb dürfen
  Sequenzen keine Labels enthalten: Sie werden 20× in den Codestrom
  eingebettet (zehnmal pro `MEASURE10`-Expansion, zwei Expansions) und 200×
  ausgeführt (100 Warmup + 100 Messung).
- **Nur dokumentierte Kombinationen.** Die Tabellen in PRM S. 85 und in der
  Wiki-Seite sind die Referenz. Es gibt keine allgemeine Regel „zwei beliebige
  Befehle auf dasselbe Ziel fusionieren“.
- **Keine Quelltext-3-Operanden-Syntax.** `add.l d2,d4,d3` ist intern, nicht
  assemblerfähig. Wer das schreibt, bekommt einen Build-Fehler.
- **Fusing ist nicht Bonding.** Fusing aktualisiert *ein* Ziel; Bonding
  aktualisiert *zwei* Ziele in zwei Pipes. Siehe [`02-bonding.md`](02-bonding.md).
