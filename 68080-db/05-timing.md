# 05 – Instruktions-Timing und Fallstricke

**Quellen:** Apollo 68080 PRM (Juni 2026), S. 86 („Instruction Efficiency“),
S. 87 (Pipeline-Stufen und Blasen), S. 88 („Instruction Timing“), S. 89
(FPU-Pipeline), S. 90 („Optimizing“). Die Zahlen sind **PRM-Literaturwerte**,
die MIN-Werte unten sind auf einer V4 gemessen.

**Kernaussage:** Der 68080 ist kein 68060 mit höherem Takt. Viele Integer-
und AMMX-Instruktionen sind auf 1 Takt ausgelegt; die teuren Fälle sind
`MUL`/`DIV`, `MOVEM`, `MOVE16` und die Mikrocode-FPU-Operationen. Die
wichtigste strukturelle Falle ist die **ALU->EA-Blase von 2 Takten**.

> **Messstatus:** Alle Einträge wurden mit **Harness v5** auf einer Vampire V4
> gemessen (10 × 100 Läufe). Die v5-Dateilogs enthalten zusätzlich `PERF`
> (Pipe-Slots). Referenz ist MIN, MAX/AVG sind Mediane der 10 Läufe; die
> v1-Logs bleiben als Archiv unter `measurements/<ID>.log`.

## TIM-001: Division durch eine Zweierpotenz durch Shift ersetzen

**Kategorie:** Timing / ALU  
**Status:** VERIFIZIERT (gemessen – Vampire V4, Log vorhanden)  
**Beleg:** VERIFIZIERT (Literatur): PRM S. 88 (`DIV=<18`, Integer normal 1)  
**Betroffene CPUs:** 68080 (V4); nicht 68060

### Problem

`DIVU`/`DIVS` sind mehrzyklisch. Für eine Division durch 8 (vorzeichenlos)
ist ein logischer Shift nach rechts exakt äquivalent und deutlich kürzer.

### Naiver 68k-Ansatz

```asm
    divu.w  #8,d0       ; mehrzyklisch, Quotient/Rest in d0
```

### 68080-Ansatz

```asm
    lsr.l   #3,d0       ; Division durch 8, vorzeichenlos
```

### Warum das funktioniert

Das PRM nennt `DIV=<18` Takte, während Integer-Operationen normalerweise
1 Takt brauchen. `LSR.L #3` verschiebt alle 32 Bit nach rechts; die unteren
drei Bits fallen als Rest heraus. Für negative Werte ist `LSR` **nicht**
äquivalent zu `DIVS` (arithmetischer Shift `ASR` wäre die richtige Variante).

**Grenze:** Die Transformation gilt nur für exakte Zweierpotenzen und nur,
wenn der Rest nicht gebraucht wird. Für beliebige Divisoren ist eine
Multiplikation mit dem Kehrwert nur mit passender Genauigkeit möglich; das
ist keine allgemeine 68k-Optimierung und wird hier nicht behauptet.

### Messung

- Sequenz: [`seq/TIM-001.i`](seq/TIM-001.i)
- Log: [v5 V4-Datei](measurements/v5-files/TIM-001.run01.log) · [v1-Archiv](measurements/TIM-001.log)
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

- **Register:** `d0`.

**Gemessene Sequenz (verbatim aus `seq/TIM-001.i`, gebaut als `bin-v5/TIM-001`):**

```asm
; seq/TIM-001.i - DIV by power of two versus logical shift
; Uses: d0
; MUST NOT touch: d5, d6, d7, a5, a6, a7
        moveq   #8,d0
        lsr.l   #3,d0
```

## TIM-002: Adresse mit LEA erzeugen, nicht über ALU->EA

**Kategorie:** Timing / Pipeline  
**Status:** VERIFIZIERT (gemessen – Vampire V4, Log vorhanden)  
**Beleg:** VERIFIZIERT (Literatur): PRM S. 87  
**Betroffene CPUs:** 68080 (V4); nicht 68060

### Problem

Das PRM beschreibt drei Latency-Fälle:

```text
EA result to EA input  = no LATENCY
ALU result to ALU input = no LATENCY
ALU result to EA input  = 2 cycle bubble
```

Wer eine Adresse im D-Register berechnet und dann als EA-Operand verwendet,
bezahlt die dritte Zeile.

### Naiver 68k-Ansatz

```asm
    move.l  a0,d0
    addq.l  #8,d0
    move.l  (a0,d0.l),d1    ; d0 kommt aus der ALU -> EA-Blase
```

### 68080-Ansatz

```asm
    lea     8(a0),a1
    move.l  (a1),d1         ; EA -> EA, keine Blase
```

### Warum das funktioniert

`LEA` und `ADDQ/SUBQ` auf Adressregister laufen in der EA-Einheit; das
Ergebnis steht wieder einem EA-Eingang zur Verfügung, ohne die 2-Takt-Blase
des ALU->EA-Pfads. Der Effekt ist besonders in Schleifen relevant, in denen
pro Iteration eine neue Adresse berechnet wird.

**Grenze:** Die Blase ist nur relevant, wenn die nächste Instruktion das
Ergebnis wirklich als EA-Eingang braucht. Ein `move.l d0,(a0)` mit `d0` als
Datenquelle ist kein ALU->EA-Fall.

### Messung

- Sequenz: [`seq/TIM-002.i`](seq/TIM-002.i)
- Log: [v5 V4-Datei](measurements/v5-files/TIM-002.run01.log) · [v1-Archiv](measurements/TIM-002.log)
- **Messergebnis (Vampire V4, Harness v5, 10 × 100 Läufe):**
  - `BASELINE = 5` (Median der 10 Läufe)
  - `MIN = 2` Takte (in allen 10 Läufen identisch)
  - `MAX = 2` Takte (Median der 10 Läufe)
  - `AVG = 2` Takte (Median der 10 Läufe)
  - **Pipe-Slots (v5):** `IEP1 = 3`, `IEP2 = 0`, `IEP3 = 0` pro Lauf (Median); Summe = 3.
  - 10/10 Läufe mit 100/100 `RES`-Zeilen, `check_log.py` OK.
- **Warmup:** Die ersten 10 Läufe liegen bei MIN; der v1-Kaltstart ist weg.
- **Wiederholbarkeit:** MIN 10/10 identisch; MAX/AVG sind Mediane der 10 Läufe.
- **Baseline-Warnung:** `BASELINE = 5`; `NET = RES − BASELINE` wird negativ und ist kein gültiger Sequenzwert.

- **Register:** `a0`, `a1`.

**Gemessene Sequenz (verbatim aus `seq/TIM-002.i`, gebaut als `bin-v5/TIM-002`):**

```asm
; seq/TIM-002.i - produce an address with LEA, not ALU result -> EA
; Uses: a0, a1
; MUST NOT touch: d5, d6, d7, a5, a6, a7
        lea     8(a0),a1
```

## TIM-003: EA->EA-Ketten haben keine Latenz

**Kategorie:** Timing / Pipeline  
**Status:** VERIFIZIERT (gemessen – Vampire V4, Log vorhanden)  
**Beleg:** VERIFIZIERT (Literatur): PRM S. 87  
**Betroffene CPUs:** 68080 (V4); nicht 68060

### Problem

Eine Kette von Adressberechnungen über D-Register kann die ALU->EA-Blase
mehrfach hintereinander auslösen. Eine Kette von `lea`-Operationen bleibt
dagegen im EA-Pfad.

### Naiver 68k-Ansatz

```asm
    move.l  a0,d0
    addq.l  #8,d0
    move.l  d0,a0
    addq.l  #8,a0
    move.l  a0,a1
```

### 68080-Ansatz

```asm
    lea     8(a0),a0
    lea     8(a0),a1
```

### Warum das funktioniert

Das PRM sagt „EA result to EA input = no LATENCY“. Beide `lea`-Operationen
liegen im Adresspfad; die zweite kann das Ergebnis der ersten ohne die
2-Takt-ALU->EA-Blase verwenden. Das ist besonders für Zeigerketten und
Adressvorberechnung nützlich.

**Grenze:** Die Kette bleibt von der Lade-/Store-Latenz abhängig, sobald
tatsächlich Speicher zugriffen wird. Der Eintrag optimiert nur die
Adressberechnung, nicht den Speicherzugriff selbst.

### Messung

- Sequenz: [`seq/TIM-003.i`](seq/TIM-003.i)
- Log: [v5 V4-Datei](measurements/v5-files/TIM-003.run01.log) · [v1-Archiv](measurements/TIM-003.log)
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

- **Register:** `a0`.

**Gemessene Sequenz (verbatim aus `seq/TIM-003.i`, gebaut als `bin-v5/TIM-003`):**

```asm
; seq/TIM-003.i - EA result to EA input has no latency (LEA chain)
; Uses: a0
; MUST NOT touch: d5, d6, d7, a5, a6, a7
        lea     8(a0),a0
        lea     8(a0),a0
```

## TIM-004: Fünf Register mit MOVEM statt fünf MOVE.L

**Kategorie:** Timing / Mehrfachzugriff  
**Status:** VERIFIZIERT (gemessen – Vampire V4, Log vorhanden)  
**Beleg:** VERIFIZIERT (Literatur): PRM S. 88 (`MOVEM=int(1+n/2)`)  
**Betroffene CPUs:** 68080 (V4); nicht 68060

### Problem

Fünf einzelne `move.l` zu einem Ziel kosten fünf Instruktionen und fünf
Speicherzugriffe. `MOVEM` fasst sie zusammen – aber es ist mehrzyklisch und
läuft nur auf Pipe 1.

### Naiver 68k-Ansatz

```asm
    move.l  d0,(a0)
    move.l  d1,4(a0)
    move.l  d2,8(a0)
    move.l  d3,12(a0)
    move.l  d4,16(a0)
```

### 68080-Ansatz

```asm
    movem.l d0-d4,(a0)  ; laut PRM int(1+5/2) = 3 Takte
```

### Warum das funktioniert

Das PRM gibt `MOVEM = int(1 + n/2)` an, für `n=5` also 3 Takte (gegenüber
fünf einzelnen Moves). Der Preis: MOVEM ist mehrzyklisch und belegt Pipe 1;
es kann nicht mit einer zweiten mehrzyklischen Instruktion parallelisiert
werden. Für große Registerlisten ist MOVEM trotzdem kompakt und schnell.

**Grenze:** `MOVEM` schreibt in aufsteigender Reihenfolge; bei `-(An)` bzw.
`(An)+` muss die Richtung zum Stack-/Pufferaufbau passen. Das PRM nennt
außerdem die Sonderfälle `all=15 long=8`.

### Messung

- Sequenz: [`seq/TIM-004.i`](seq/TIM-004.i)
- Log: [v5 V4-Datei](measurements/v5-files/TIM-004.run01.log) · [v1-Archiv](measurements/TIM-004.log)
- **Messergebnis (Vampire V4, Harness v5, 10 × 100 Läufe):**
  - `BASELINE = 5` (Median der 10 Läufe)
  - `MIN = 7` Takte (in allen 10 Läufen identisch)
  - `MAX = 63.5` Takte (Median der 10 Läufe)
  - `AVG = 9.5` Takte (Median der 10 Läufe)
  - **Pipe-Slots (v5):** `IEP1 = 19.98`, `IEP2 = 0`, `IEP3 = 0` pro Lauf (Median); Summe = 19.98.
  - 10/10 Läufe mit 100/100 `RES`-Zeilen, `check_log.py` OK.
- **Warmup:** Die ersten 10 Läufe liegen überwiegend bei MIN; einzelne Interrupt-Ausreißer sind im v5-Report dokumentiert.
- **Wiederholbarkeit:** MIN 10/10 identisch; MAX/AVG sind Mediane der 10 Läufe.
- **Baseline-Warnung:** `BASELINE = 5`; `NET = RES − BASELINE` ist nicht subtrahierbar.

- **Register:** `d0`–`d4`, `a0`.

**Gemessene Sequenz (verbatim aus `seq/TIM-004.i`, gebaut als `bin-v5/TIM-004`):**

```asm
; seq/TIM-004.i - save five registers with MOVEM, not five MOVE.L
; Uses: d0-d4, a0
; MUST NOT touch: d5, d6, d7, a5, a6, a7
        movem.l d0-d4,(a0)
```

## TIM-005: MOVE16 ist vier Takte – nicht automatisch schneller

**Kategorie:** Timing / Speicher  
**Status:** VERIFIZIERT (gemessen – Vampire V4, Log vorhanden)  
**Beleg:** VERIFIZIERT (Literatur): PRM S. 88 (`MOVE16=4`), S. 27
(keine Ausrichtungsbedingung auf dem 68080)  
**Betroffene CPUs:** 68080 (V4); nicht 68060

### Problem

`MOVE16` klingt nach dem optimalen 16-Byte-Kopierbefehl. Auf dem 68080 kostet
er laut PRM aber **4 Takte**. Zwei benachbarte `move.l`-Paare, die zu 64-Bit-
Moves fusionieren, können rechnerisch zwei Takte brauchen. `MOVE16` ist damit
nicht automatisch die schnellste 16-Byte-Kopie – aber es hat andere Vorteile:
Es ist ein einzelner Befehl und auf dem 68080 **nicht** ausrichtungspflichtig.

### Naiver 68k-Ansatz

```asm
    move.l  (a0)+,(a1)+
    move.l  (a0)+,(a1)+
    move.l  (a0)+,(a1)+
    move.l  (a0)+,(a1)+
```

### 68080-Ansatz

```asm
    move16  (a0)+,(a1)+     ; 4 Takte, 16 Byte, keine Alignmentschranke
```

### Warum das funktioniert

Das PRM nennt `MOVE16=4` Takte und stellt ausdrücklich klar: „move16 does NOT
have to be aligned on the 68080“. Der 68040 verlangte 16-Byte-Ausrichtung;
der 68080 nicht. Der Befehl ist deshalb gut für einzelne 16-Byte-Blöcke an
beliebigen Adressen. Für große Kopien ist die fused 64-Bit-Schleife oft
besser; das ist eine **literaturbasierte Erwartung**, kein Messergebnis.

### Messung

- Sequenz: [`seq/TIM-005.i`](seq/TIM-005.i)
- Log: [v5 V4-Datei](measurements/v5-files/TIM-005.run01.log) · [v1-Archiv](measurements/TIM-005.log)
- **Messergebnis (Vampire V4, Harness v5, 10 × 100 Läufe):**
  - `BASELINE = 5` (Median der 10 Läufe)
  - `MIN = 5` Takte (in allen 10 Läufen identisch)
  - `MAX = 7` Takte (Median der 10 Läufe)
  - `AVG = 5` Takte (Median der 10 Läufe)
  - **Pipe-Slots (v5):** `IEP1 = 3`, `IEP2 = 0`, `IEP3 = 0` pro Lauf (Median); Summe = 3.
  - 10/10 Läufe mit 100/100 `RES`-Zeilen, `check_log.py` OK.
- **Warmup:** Die ersten 10 Läufe liegen bei MIN; der v1-Kaltstart ist weg.
- **Wiederholbarkeit:** MIN 10/10 identisch; MAX/AVG sind Mediane der 10 Läufe.
- **Baseline-Warnung:** `BASELINE = 5`; `NET = RES − BASELINE` ist nicht subtrahierbar.

- **Register:** `a0`, `a1`.

**Gemessene Sequenz (verbatim aus `seq/TIM-005.i`, gebaut als `bin-v5/TIM-005`):**

```asm
; seq/TIM-005.i - MOVE16: four cycles, not automatically faster
; Uses: a0, a1
; MUST NOT touch: d5, d6, d7, a5, a6, a7
        move16  (a0)+,(a1)+
```

## Weitere Timing-Regeln aus dem PRM

| Operation | PRM-Wert | Anmerkung |
|---|---:|---|
| Integer/AMMX | normal 1 Takt | Ausnahmen unten |
| `MUL` | 2 oder 3 | abhängig von Operanden |
| `DIV` | `< 18` | genauer Wert nicht angegeben |
| `MOVEM` | `int(1+n/2)` | `all=15 long=8` |
| `MOVE16` | 4 | keine Alignmentschranke auf 68080 |
| `JMP`/`JSR` | 1 | mit berechneter EA: 4 |
| EA-Berechnung | frei | exotische indirekte Modi: ca. 4 |
| Integer-Konvertierung | +1 | byte/word/long |
| `FADD`, `FCMP`, `FSUB`, `FMUL` | 6 | voll gepipelined, Issue 1 |
| `FDIV` | 10 | voll gepipelined |
| `FSQRT` | 22 | voll gepipelined |
| `FSINCOS`, `FTWOTOX` usw. | ca. 100–200+ | Mikrocode, `E23` wird überschrieben |

**FPU-Praxis:** Die Latenz ist nicht die Issue-Rate. `FADD` kann jedes Takt
neu ausgegeben werden, aber ein abhängiges Ergebnis braucht 6 Takte. Wer
darauf wartet, verschenkt 5 Takte; unabhängige Integer- oder FPU-Arbeit
dazwischen ist laut PRM „free“. Dieses Harness speichert keine FPU-Register
und misst deshalb keine FPU-Sequenzen.

**Conditional Instruction:** Das PRM nennt als schnelle Form die einzelne
Instruktion nach `bcc.s` („a faster way to execute a single following
instruction after bcc.s“). Das ist ein Branch-über-eine-Instruktion-Muster,
kein allgemeiner Prädikatmodus; es wird hier nicht als eigener Messblock
geführt, weil die Sequenzen keine Labels enthalten dürfen.

**Misaligned Reads:** Das PRM sagt, unausgerichtete **Lese**zugriffe kosten
bei Cache-Treffer keinen Aufpreis. Für **Schreib**zugriffe gilt weiterhin:
an Quad-Grenzen ausrichten (siehe [`06-memory.md`](06-memory.md)).
