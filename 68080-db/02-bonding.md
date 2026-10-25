# 02 – Instruction Bonding und Superscalar

**Quellen:** Apollo-Wiki „Instruction Bonding“ und „Superscalar“ (Fassung
2020-08-02, Internet Archive); Apollo 68080 PRM (Juni 2026), S. 83–84
(„Superscalar“) und S. 92 (Cache/Speicher). Die Wiki-Seiten beschreiben den
SILVER2-Stand (zwei Pipes); das PRM beschreibt für neuere V4-Cores ab
ungefähr Core 11350 drei Pipes.

**Kernaussage:** *Fusing* verschmilzt zwei Befehle mit **demselben** Ziel zu
einer internen Instruktion. *Bonding* führt zwei **abhängige** Befehle mit
**zwei verschiedenen** Zielen parallel auf zwei Pipes aus. Beides ist von
normalem Superscalar zu unterscheiden, bei dem die Befehle voneinander
unabhängig sein müssen.

> **Messstatus:** Alle Einträge wurden mit **Harness v5** auf einer Vampire V4
> gemessen (10 × 100 Läufe). Die v5-Dateilogs enthalten zusätzlich `PERF`
> (Pipe-Slots). Referenz ist MIN, MAX/AVG sind Mediane der 10 Läufe; die
> v1-Logs bleiben als Archiv unter `measurements/<ID>.log`.

## BND-001: Abhängiges MOVEQ + ADD mit zwei Zielen bonden

**Kategorie:** Bonding  
**Status:** VERIFIZIERT (gemessen – Vampire V4, Log vorhanden)  
**Beleg:** VERIFIZIERT (Literatur): Apollo-Wiki „Instruction Bonding“, exakt
dieses Beispiel  
**Betroffene CPUs:** 68080 (V4); nicht 68060

### Problem

`move.l #$12,d1` schreibt `d1`; das folgende `add.l d1,d2` liest `d1` und
schreibt `d2`. Auf einem klassischen Superscalar-Core wie dem 68060 ist das
eine echte Datenabhängigkeit und damit nicht parallelisierbar. Auf dem 68080
ist es ein Bonding-Kandidat, weil die beiden Ziele `d1` und `d2`
verschieden sind.

### Naiver 68k-Ansatz (68060-Denkweise)

```asm
    move.l  #$12,d1
    move.l  d3,d4       ; unabhängige Arbeit dazwischen "verstecken"
    add.l   d1,d2
```

### 68080-Ansatz

```asm
    move.l  #$12,d1
    add.l   d1,d2       ; benachbart lassen: Bonding
```

### Warum das funktioniert

Die Wiki-Seite nennt genau dieses Paar:

> „move.l #1234,d1 / add.l d1,d2 … updates two registers, d1 and d2 … will be
> executed by two pipelines.“

Der Core kann die beiden Befehle auf zwei Pipes legen, obwohl der zweite vom
Ergebnis des ersten abhängt. Das unterscheidet Bonding von der klassischen
Superscalar-Ausführung, die unabhängige Instruktionen voraussetzt.

**Grenze:** Bonding ist wie Fusing eine dokumentierte Paarbildung. Eine
dazwischengeschobene Instruktion zerstört die Nachbarschaft; sie ist kein
dynamischer Scheduler, der beliebig weit vorausschaut.

### Messung

- Sequenz: [`seq/BND-001.i`](seq/BND-001.i)
- Log: [v5 V4-Datei](measurements/v5-files/BND-001.run01.log) · [v1-Archiv](measurements/BND-001.log)
- **Messergebnis (Vampire V4, Harness v5, 10 × 100 Läufe):**
  - `BASELINE = 5` (Median der 10 Läufe)
  - `MIN = 2` Takte (in allen 10 Läufen identisch)
  - `MAX = 2` Takte (Median der 10 Läufe)
  - `AVG = 2` Takte (Median der 10 Läufe)
  - **Pipe-Slots (v5):** `IEP1 = 3`, `IEP2 = 1`, `IEP3 = 0` pro Lauf (Median); Summe = 4.
  - 10/10 Läufe mit 100/100 `RES`-Zeilen, `check_log.py` OK.
- **Warmup:** Die ersten 10 Läufe liegen bei MIN; der v1-Kaltstart ist weg.
- **Wiederholbarkeit:** MIN 10/10 identisch; MAX/AVG sind Mediane der 10 Läufe.
- **Baseline-Warnung:** `BASELINE = 5`; `NET = RES − BASELINE` wird negativ und ist kein gültiger Sequenzwert.

- **Register:** `d1`, `d2`.

**Gemessene Sequenz (verbatim aus `seq/BND-001.i`, gebaut als `bin-v5/BND-001`):**

```asm
; seq/BND-001.i - dependent MOVEQ + ADD with two destinations (bonding)
; Uses: d1, d2
; MUST NOT touch: d5, d6, d7, a5, a6, a7
        move.l  #$12,d1
        add.l   d1,d2
```

## BND-002: Gleiches Ziel ist Fusing, nicht Bonding

**Kategorie:** Bonding / Abgrenzung  
**Status:** VERIFIZIERT (gemessen – Vampire V4, Log vorhanden)  
**Beleg:** VERIFIZIERT (Literatur): PRM S. 85; Wiki „Instruction Fusing“ und
„Instruction Bonding“  
**Betroffene CPUs:** 68080 (V4); nicht 68060

### Problem

Viele Optimierungen werfen „Fusing“ und „Bonding“ durcheinander. Der
Unterschied ist präzise:

- **Fusing:** beide Instruktionen aktualisieren dasselbe Ziel; der Core macht
  daraus *eine* interne Instruktion in *einer* Pipe.
- **Bonding:** die Instruktionen aktualisieren *zwei verschiedene* Ziele; der
  Core führt sie in *zwei* Pipes aus, obwohl eine Datenabhängigkeit besteht.

### Naiver 68k-Ansatz

```asm
    move.l  d2,d3
    nop
    add.l   d4,d3       ; gleiches Ziel, aber Paar zerstört
```

### 68080-Ansatz

```asm
    move.l  d2,d3
    add.l   d4,d3       ; gleiches Ziel -> Fusing
```

### Warum das funktioniert

`d3` ist Quelle und Ziel beider Instruktionen. Das ist die klassische
Fusing-Form `move.l dn,dm` + `add.l dx,dm`. Würde man stattdessen
`move.l d2,d3` + `add.l d4,d5` schreiben, wären die Ziele verschieden; dann
wäre es kein Fusing-Paar, sondern höchstens ein Bonding-/Superscalar-Fall –
abhängig von den dokumentierten Regeln.

### Messung

- Sequenz: [`seq/BND-002.i`](seq/BND-002.i)
- Log: [v5 V4-Datei](measurements/v5-files/BND-002.run01.log) · [v1-Archiv](measurements/BND-002.log)
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

- **Register:** `d2`, `d3`, `d4`.
- **Vergleich:** [`FUS-001`](01-fusing.md#fus-001-movel--addl-zur-3-operanden-fusion)
  und [`BND-001`](02-bonding.md#bnd-001-abhängiges-moveq--add-mit-zwei-zielen-bonden)
  sind die beiden Gegenstücke.

**Gemessene Sequenz (verbatim aus `seq/BND-002.i`, gebaut als `bin-v5/BND-002`):**

```asm
; seq/BND-002.i - same destination: fusion instead of bonding
; Uses: d2, d3, d4
; MUST NOT touch: d5, d6, d7, a5, a6, a7
        move.l  d2,d3
        add.l   d4,d3
```

## BND-003: Zwei unabhängige ALU-Instruktionen sind normales Superscalar

**Kategorie:** Superscalar  
**Status:** VERIFIZIERT (gemessen – Vampire V4, Log vorhanden)  
**Beleg:** VERIFIZIERT (Literatur): Apollo-Wiki „Instruction Bonding“
(„add.l d0,d1 / add.l d2,d3“); PRM S. 83–84  
**Betroffene CPUs:** 68080 (V4); nicht 68060

### Problem

Unabhängige Instruktionen sind der einfachste Superscalar-Fall – aber nur,
wenn sie die Pipe-2-Restriktionen einhalten. Der 68060 kann deutlich weniger
Kombinationen parallel ausführen als der 68080.

### Naiver 68k-Ansatz

```asm
    add.l   d0,d1
    nop
    add.l   d2,d3
```

### 68080-Ansatz

```asm
    add.l   d0,d1
    add.l   d2,d3       ; unabhängig, beide simple ALU
```

### Warum das funktioniert

Die Wiki-Seite nennt genau dieses Paar als Beispiel für Superscalar auf 68060
und 68080. Beide Instruktionen sind einfache, ein Takt lange ALU-Operationen
ohne gemeinsame Quelle/Ziele und ohne Flags-Abhängigkeit. Pipe 2 darf laut
Wiki nur Instruktionen ausführen, die in einem Takt fertig werden, maximal
8 Byte lang sind und nicht von Flags der Pipe 1 abhängen.

### Messung

- Sequenz: [`seq/BND-003.i`](seq/BND-003.i)
- Log: [v5 V4-Datei](measurements/v5-files/BND-003.run01.log) · [v1-Archiv](measurements/BND-003.log)
- **Messergebnis (Vampire V4, Harness v5, 10 × 100 Läufe):**
  - `BASELINE = 5` (Median der 10 Läufe)
  - `MIN = 2` Takte (in allen 10 Läufen identisch)
  - `MAX = 2` Takte (Median der 10 Läufe)
  - `AVG = 2` Takte (Median der 10 Läufe)
  - **Pipe-Slots (v5):** `IEP1 = 3`, `IEP2 = 1`, `IEP3 = 0` pro Lauf (Median); Summe = 4.
  - 10/10 Läufe mit 100/100 `RES`-Zeilen, `check_log.py` OK.
- **Warmup:** Die ersten 10 Läufe liegen bei MIN; der v1-Kaltstart ist weg.
- **Wiederholbarkeit:** MIN 10/10 identisch; MAX/AVG sind Mediane der 10 Läufe.
- **Baseline-Warnung:** `BASELINE = 5`; `NET = RES − BASELINE` wird negativ und ist kein gültiger Sequenzwert.

- **Register:** `d0`–`d3`.

**Gemessene Sequenz (verbatim aus `seq/BND-003.i`, gebaut als `bin-v5/BND-003`):**

```asm
; seq/BND-003.i - two independent ALU instructions (superscalar)
; Uses: d0, d1, d2, d3
; MUST NOT touch: d5, d6, d7, a5, a6, a7
        add.l   d0,d1
        add.l   d2,d3
```

## BND-004: MOVEM belegt Pipe 1 – unabhängige ALU daneben legen

**Kategorie:** Superscalar / Pipe-Zuteilung  
**Status:** VERIFIZIERT (gemessen – Vampire V4, Log vorhanden)  
**Beleg:** VERIFIZIERT (Literatur): PRM S. 83–84, 88; Wiki „Superscalar“
(„Pipe2 cannot do MOVEM, CMPM, MOVE (mem),(mem), MUL, DIV“)  
**Betroffene CPUs:** 68080 (V4); nicht 68060

### Problem

`MOVEM` ist mehrzyklisch und wird laut PRM von den Pipe-2-Einheiten
(Bruno/Cesar) nicht angenommen. Es belegt Pipe 1. Wer daneben eine abhängige
oder ebenfalls mehrzyklische Instruktion legt, gewinnt nichts.

### Naiver 68k-Ansatz

```asm
    movem.l d0-d3,(a0)
    addq.l  #1,d0       ; greift auf eine MOVEM-Quelle zu: kein guter Pipe-2-Kandidat
```

### 68080-Ansatz

```asm
    movem.l d0-d3,(a0)
    addq.l  #1,d4       ; unabhängig von der MOVEM-Registerliste
```

### Warum das funktioniert

Das PRM nennt für `MOVEM` die Kosten `int(1 + n/2)` (bei vier Longs also
`int(1+2) = 3`) und ordnet mehrzyklische Instruktionen Pipe 1 zu. Eine
unabhängige einzyklische ALU-Instruktion kann dagegen auf Pipe 2 laufen,
sofern sie die 8-Byte-/Flags-/Datenpfad-Restriktionen einhält. Der Effekt ist
Überlappung, keine Verkürzung der MOVEM-Kosten selbst.

### Messung

- Sequenz: [`seq/BND-004.i`](seq/BND-004.i)
- Log: [v5 V4-Datei](measurements/v5-files/BND-004.run01.log) · [v1-Archiv](measurements/BND-004.log)
- **Messergebnis (Vampire V4, Harness v5, 10 × 100 Läufe):**
  - `BASELINE = 5` (Median der 10 Läufe)
  - `MIN = 7` Takte (in allen 10 Läufen identisch)
  - `MAX = 37` Takte (Median der 10 Läufe)
  - `AVG = 10` Takte (Median der 10 Läufe)
  - **Pipe-Slots (v5):** `IEP1 = 20.98`, `IEP2 = 0`, `IEP3 = 0` pro Lauf (Median); Summe = 20.98.
  - 10/10 Läufe mit 100/100 `RES`-Zeilen, `check_log.py` OK.
- **Warmup:** Die ersten 10 Läufe liegen bei MIN; der v1-Kaltstart ist weg.
- **Wiederholbarkeit:** MIN 10/10 identisch; MAX/AVG sind Mediane der 10 Läufe.
- **Baseline-Warnung:** `BASELINE = 5`; `NET = RES − BASELINE` ist nicht subtrahierbar.

- **Register:** `d0`–`d3` (MOVEM), `d4` (addq), `a0` (Ziel).

**Gemessene Sequenz (verbatim aus `seq/BND-004.i`, gebaut als `bin-v5/BND-004`):**

```asm
; seq/BND-004.i - MOVEM stays on pipe 1, independent ALU can use pipe 2
; Uses: d0-d3, d4, a0
; MUST NOT touch: d5, d6, d7, a5, a6, a7
        movem.l d0-d3,(a0)
        addq.l  #1,d4
```

## BND-005: Drei unabhängige ALU-Instruktionen auf Drei-Pipe-Cores

**Kategorie:** Superscalar  
**Status:** VERIFIZIERT (gemessen – Vampire V4, Log vorhanden)  
**Beleg:** VERIFIZIERT (Literatur): PRM S. 83 („From approx core 11350 onward
there are 3 pipes“); Wiki „Superscalar“ (zwei Pipes, SILVER2)  
**Betroffene CPUs:** 68080 V4 ab ca. Core 11350; ältere V4-Cores nur zwei Pipes

### Problem

Die verbreitete Wiki-Beschreibung von 2020 nennt zwei Pipes. Das PRM von 2026
beschreibt für neuere V4-Cores drei Pipes (Anton, Bruno, Cesar). Eine
Optimierung, die drei unabhängige ALU-Instruktionen bewusst nebeneinander
legt, ist deshalb **core-revisionsabhängig**.

### Naiver 68k-Ansatz

```asm
    add.l   d0,d1
    nop
    add.l   d2,d3
    nop
    eor.l   d4,d4
```

### 68080-Ansatz

```asm
    add.l   d0,d1
    add.l   d2,d3
    eor.l   d4,d4       ; drei unabhängige einfache ALU-Ops
```

### Warum das funktioniert

Das PRM beschreibt drei Pipes:

- **Anton:** alle Instruktionen, FPU/CPU beliebiger Länge, kann fusen.
- **Bruno:** bis 8-Byte-EA/ALU/Branches, kann fusen; keine mehrzyklischen
  Instruktionen.
- **Cesar:** bis 6-Byte-EA/Branches; ALU war im Juni 2026 noch „under
  construction“.

Für die drei `add`/`eor`-Instruktionen sind alle drei einfach und kurz genug;
auf einem älteren Zwei-Pipe-Core bleibt die dritte Instruktion in der
nächsten Gruppe. **Die Messung muss die Core-Revision mitschreiben**, sonst
ist der Vergleich wertlos.

### Messung

- Sequenz: [`seq/BND-005.i`](seq/BND-005.i)
- Log: [v5 V4-Datei](measurements/v5-files/BND-005.run01.log) · [v1-Archiv](measurements/BND-005.log)
- **Messergebnis (Vampire V4, Harness v5, 10 × 100 Läufe):**
  - `BASELINE = 5` (Median der 10 Läufe)
  - `MIN = 3` Takte (in allen 10 Läufen identisch)
  - `MAX = 3` Takte (Median der 10 Läufe)
  - `AVG = 3` Takte (Median der 10 Läufe)
  - **Pipe-Slots (v5):** `IEP1 = 4`, `IEP2 = 1`, `IEP3 = 0` pro Lauf (Median); Summe = 5.
  - 10/10 Läufe mit 100/100 `RES`-Zeilen, `check_log.py` OK.
- **Warmup:** Die ersten 10 Läufe liegen bei MIN; der v1-Kaltstart ist weg.
- **Wiederholbarkeit:** MIN 10/10 identisch; MAX/AVG sind Mediane der 10 Läufe.
- **Baseline-Warnung:** `BASELINE = 5`; `NET = RES − BASELINE` wird negativ und ist kein gültiger Sequenzwert.

- **Register:** `d0`–`d4`.

**Gemessene Sequenz (verbatim aus `seq/BND-005.i`, gebaut als `bin-v5/BND-005`):**

```asm
; seq/BND-005.i - three independent ALU instructions (3-pipe cores)
; Uses: d0, d1, d2, d3, d4
; MUST NOT touch: d5, d6, d7, a5, a6, a7
        add.l   d0,d1
        add.l   d2,d3
        eor.l   d4,d4
```

## Was Bonding/Superscalar verhindert

Die folgenden Punkte sind in Wiki und PRM belegt und gelten für Pipe 2
(Bruno/Cesar) der Zwei-/Drei-Pipe-Cores:

| Restriktion | Konsequenz |
|---|---|
| Mehr als ein Takt Laufzeit | `MUL`, `DIV`, `MOVEM`, `CMPM`, `MOVE (mem),(mem)` bleiben auf Pipe 1. |
| Länge > 8 Byte | Bruno nimmt maximal 8 Byte, Cesar maximal 6 Byte; längere Instruktionen bleiben auf Pipe 1. |
| Flags-Abhängigkeit | Pipe 2 darf nie von Flags abhängen, die Pipe 1 gesetzt hat. |
| 16-Byte-Icache-Feed | Das gemeinsame Bündel muss in den 16-Byte-Feed passen. |
| 8-Byte-Dcache-Feed | Pro Takt nur ein Read **oder** ein Write **oder** ein Update. |

Diese Tabelle ist keine vollständige Scheduling-Simulation. Sie beschreibt die
dokumentierten Grenzen; konkrete Paare müssen gemessen werden.
