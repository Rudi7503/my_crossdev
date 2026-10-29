# 08 – Antipatterns: Was den 68080 bremst

**Quellen:** Apollo 68080 PRM (Juni 2026), S. 83–90; Apollo-Wiki
„Instruction Fusing“, „Instruction Bonding“, „Superscalar“;
`blitapollo.asm`.

**Kernaussage:** Viele dieser Muster sind auf dem 68060 sinnvoll oder
zumindest harmlos, auf dem 68080 aber kontraproduktiv, weil sie genau die
Features zerstören, die den Core schnell machen: benachbarte Fusing-Paare,
Bonding, 64-Bit-Zugriffe und die EA-Einheit.

> **Wichtige Einschränkung:** Es wurden **keine 68060-Vergleichsmessungen**
> durchgeführt. Die Überschrift „langsamer als auf dem 68060“ ist deshalb
> nicht als gemessene Aussage zu lesen. Die Einträge zeigen Muster, die den
> 68080 unter seinem dokumentierten Optimum halten; ein direkter 060-Vergleich
> bleibt offen (siehe [`README.md`](README.md#offene-punkte)).

> **Messstatus:** Alle Einträge wurden mit **Harness v5** auf einer Vampire V4
> gemessen (10 × 100 Läufe). Die v5-Dateilogs enthalten zusätzlich `PERF`
> (Pipe-Slots). Referenz ist MIN, MAX/AVG sind Mediane der 10 Läufe; die
> v1-Logs bleiben als Archiv unter `measurements/<ID>.log`.

## ANTI-001: NOP zwischen MOVE und abhängigem ADD

**Kategorie:** Antipattern / Fusing  
**Status:** VERIFIZIERT (gemessen – Vampire V4, Log vorhanden)  
**Beleg:** VERIFIZIERT (Literatur): PRM S. 85; Wiki „Instruction Fusing“  
**Betroffene CPUs:** 68080 (V4); nicht 68060

### Problem

Der `nop` soll auf alten Cores eine Pipeline-Blase füllen. Auf dem 68080
zerstört er die Nachbarschaft des Fusing-Paares `move.l` + `add.l`.

### Anti-Pattern (wird gemessen)

```asm
    move.l  d2,d3
    nop
    add.l   d4,d3
```

### Best Practice

```asm
    move.l  d2,d3
    add.l   d4,d3
```

### Warum das Anti-Pattern schadet

Die Fusion erkennt laut PRM und Wiki ein *benachbartes* Paar. Der `nop`
dazwischen verhindert die Paarbildung; es bleiben drei Instruktionen statt
eines internen 3-Operanden-Updates. Der `nop` fügt nicht nur eine Instruktion
hinzu, er nimmt dem Core auch die Möglichkeit, die beiden anderen
zusammenzufassen.

### Messung

- Sequenz: [`seq/ANTI-001.i`](seq/ANTI-001.i)
- Log: [v5 V4-Datei](measurements/v5-files/ANTI-001.run01.log) · [v1-Archiv](measurements/ANTI-001.log)
- **Messergebnis (Vampire V4, Harness v5, 10 × 100 Läufe):**
  - `BASELINE = 5` (Median der 10 Läufe)
  - `MIN = 9` Takte (in allen 10 Läufen identisch)
  - `MAX = 9` Takte (Median der 10 Läufe)
  - `AVG = 9` Takte (Median der 10 Läufe)
  - **Pipe-Slots (v5):** `IEP1 = 5`, `IEP2 = 0`, `IEP3 = 0` pro Lauf (Median); Summe = 5.
  - 10/10 Läufe mit 100/100 `RES`-Zeilen, `check_log.py` OK.
- **Warmup:** Die ersten 10 Läufe liegen bei MIN; der v1-Kaltstart ist weg.
- **Wiederholbarkeit:** MIN 10/10 identisch; MAX/AVG sind Mediane der 10 Läufe.
- **Baseline-Warnung:** `BASELINE = 5`; `NET = RES − BASELINE` ist nicht subtrahierbar.

- **Vergleich:** [`FUS-001`](01-fusing.md#fus-001-movel--addl-zur-3-operanden-fusion)
  ist die optimierte Fassung.

**Gemessene Sequenz (verbatim aus `seq/ANTI-001.i`, gebaut als `bin-v5/ANTI-001`):**

```asm
; seq/ANTI-001.i - ANTI-PATTERN: NOP between MOVE and dependent ADD
; Uses: d2, d3, d4
; MUST NOT touch: d5, d6, d7, a5, a6, a7
; NOTE: this sequence measures the anti-pattern on purpose.
        move.l  d2,d3
        nop
        add.l   d4,d3
```

## ANTI-002: 68060-Scheduling durch Einschub einer unabhängigen Instruktion

**Kategorie:** Antipattern / Fusing  
**Status:** VERIFIZIERT (gemessen – Vampire V4, Log vorhanden)  
**Beleg:** VERIFIZIERT (Literatur): Wiki „Instruction Fusing“; PRM S. 85  
**Betroffene CPUs:** 68080 (V4); nicht 68060

### Problem

Auf dem 68060 ist es üblich, eine abhängige Kette durch eine unabhängige
Instruktion zu unterbrechen. Auf dem 68080 zerstört genau das die
Fusing-Paarung, wenn die unabhängige Instruktion zwischen die beiden Paar-
Partner gerät.

### Anti-Pattern (wird gemessen)

```asm
    move.l  d2,d3
    move.l  d0,d1       ; "nützliche" unabhängige Arbeit dazwischen
    add.l   d4,d3
```

### Best Practice

```asm
    move.l  d2,d3
    add.l   d4,d3
    move.l  d0,d1       ; unabhängige Arbeit davor oder danach
```

### Warum das Anti-Pattern schadet

Die Fusion ist eine Erkennung benachbarter Instruktionen, kein dynamischer
Scheduler über beliebige Distanzen. Die unabhängige Instruktion mag auf einem
Zwei-Pipe-Core nützlich sein, aber sie verhindert hier die Zusammenfassung
des Paares. Der 68080 kann das Paar ohnehin in einer Pipe ausführen; die
„Entkopplung“ ist kontraproduktiv.

### Messung

- Sequenz: [`seq/ANTI-002.i`](seq/ANTI-002.i)
- Log: [v5 V4-Datei](measurements/v5-files/ANTI-002.run01.log) · [v1-Archiv](measurements/ANTI-002.log)
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

- **Vergleich:** [`FUS-001`](01-fusing.md#fus-001-movel--addl-zur-3-operanden-fusion).

**Gemessene Sequenz (verbatim aus `seq/ANTI-002.i`, gebaut als `bin-v5/ANTI-002`):**

```asm
; seq/ANTI-002.i - ANTI-PATTERN: 68060-style independent instruction breaks fusion
; Uses: d0, d1, d2, d3, d4
; MUST NOT touch: d5, d6, d7, a5, a6, a7
; NOTE: this sequence measures the anti-pattern on purpose.
        move.l  d2,d3
        move.l  d0,d1
        add.l   d4,d3
```

## ANTI-003: Word-Updates statt Long-Updates

**Kategorie:** Antipattern / Registerbreite  
**Status:** VERIFIZIERT (gemessen – Vampire V4, Log vorhanden)  
**Beleg:** VERIFIZIERT (Literatur): PRM S. 90  
**Betroffene CPUs:** 68080 (V4); nicht 68060

### Problem

`move.w` + `add.w` aktualisiert nur die unteren 16 Bit und muss die oberen
Bits des Ziels erhalten. Das PRM empfiehlt Long/Quad-Updates, wenn die
Semantik es erlaubt.

### Anti-Pattern (wird gemessen)

```asm
    move.w  d2,d3
    add.w   d4,d3
```

### Best Practice

```asm
    move.l  d2,d3
    add.l   d4,d3
```

### Warum das Anti-Pattern schadet

Ein Word-/Byte-Update ist ein Merge: Der Core liest Quelle und Ziel, um die
oberen Bits zu erhalten. Ein Long-Update schreibt das ganze Register und
vermeidet diese partielle Abhängigkeit. Beide können in einem Takt laufen;
der Unterschied liegt in der einfacheren Datenkette, nicht in einer
garantierten Taktverkürzung.

**Semantikwarnung:** Wenn die oberen 16 Bit erhalten bleiben **sollen**, ist
`move.w` richtig. Das Anti-Pattern gilt nur für den Fall, dass ohnehin das
ganze Register neu gesetzt werden soll.

### Messung

- Sequenz: [`seq/ANTI-003.i`](seq/ANTI-003.i)
- Log: [v5 V4-Datei](measurements/v5-files/ANTI-003.run01.log) · [v1-Archiv](measurements/ANTI-003.log)
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

- **Vergleich:** [`REG-003`](04-register.md#reg-003-longquad-updates-statt-byteword-merges).

**Gemessene Sequenz (verbatim aus `seq/ANTI-003.i`, gebaut als `bin-v5/ANTI-003`):**

```asm
; seq/ANTI-003.i - ANTI-PATTERN: word updates instead of long updates
; Uses: d2, d3, d4
; MUST NOT touch: d5, d6, d7, a5, a6, a7
; NOTE: this sequence measures the anti-pattern on purpose.
        move.w  d2,d3
        add.w   d4,d3
```

## ANTI-004: Zwei mehrzyklische Instruktionen auf Pipe 1 erwarten

**Kategorie:** Antipattern / Pipe-Zuteilung  
**Status:** VERIFIZIERT (gemessen – Vampire V4, Log vorhanden)  
**Beleg:** VERIFIZIERT (Literatur): PRM S. 83–84, 88; Wiki „Superscalar“  
**Betroffene CPUs:** 68080 (V4); nicht 68060

### Problem

`MOVEM` und `MUL` sind mehrzyklisch. Die Pipe-2-Einheiten (Bruno/Cesar)
nehmen laut PRM keine mehrzyklischen Instruktionen an. Zwei solche Befehle
hintereinander können deshalb nicht parallel laufen; sie serialisieren auf
Pipe 1.

### Anti-Pattern (wird gemessen)

```asm
    movem.l d0-d3,(a0)  ; ca. 3 Takte, Pipe 1
    mulu.w  d0,d1       ; 2-3 Takte, ebenfalls Pipe 1
```

### Best Practice

```asm
    movem.l d0-d3,(a0)
    addq.l  #1,d4       ; unabhängige 1-Takt-ALU auf Pipe 2
```

### Warum das Anti-Pattern schadet

Das PRM nennt `MOVEM=int(1+n/2)` und `MUL=2 oder 3`; beide sind
mehrzyklisch. Die Wiki-Seite sagt explizit: „Pipe2 cannot do MOVEM, CMPM,
MOVE (mem),(mem), MUL, DIV“. Zwei mehrzyklische Befehle können daher nicht
auf zwei Pipes verteilt werden. Der zweite Befehl wartet.

**Grenze:** Die genaue Überlappung mit umgebenden Instruktionen ist von
Datenabhängigkeiten und der Core-Revision abhängig. Die Aussage „beide auf
Pipe 1“ ist dokumentiert; die konkrete Taktzahl ist es nicht.

### Messung

- Sequenz: [`seq/ANTI-004.i`](seq/ANTI-004.i)
- Log: [v5 V4-Datei](measurements/v5-files/ANTI-004.run01.log) · [v1-Archiv](measurements/ANTI-004.log)
- **Messergebnis (Vampire V4, Harness v5, 10 × 100 Läufe):**
  - `BASELINE = 5` (Median der 10 Läufe)
  - `MIN = 8` Takte (in allen 10 Läufen identisch)
  - `MAX = 61.5` Takte (Median der 10 Läufe)
  - `AVG = 10` Takte (Median der 10 Läufe)
  - **Pipe-Slots (v5):** `IEP1 = 20.98`, `IEP2 = 0`, `IEP3 = 0` pro Lauf (Median); Summe = 20.98.
  - 10/10 Läufe mit 100/100 `RES`-Zeilen, `check_log.py` OK.
- **Warmup:** Die ersten 10 Läufe liegen bei MIN; der v1-Kaltstart ist weg.
- **Wiederholbarkeit:** MIN 10/10 identisch; MAX/AVG sind Mediane der 10 Läufe.
- **Baseline-Warnung:** `BASELINE = 5`; `NET = RES − BASELINE` ist nicht subtrahierbar.

- **Vergleich:** [`BND-004`](02-bonding.md#bnd-004-movem-belegt-pipe-1--unabhängige-alu-daneben-legen).

**Gemessene Sequenz (verbatim aus `seq/ANTI-004.i`, gebaut als `bin-v5/ANTI-004`):**

```asm
; seq/ANTI-004.i - ANTI-PATTERN: two multicycle instructions on pipe 1
; Uses: d0-d3, a0
; MUST NOT touch: d5, d6, d7, a5, a6, a7
; NOTE: this sequence measures the anti-pattern on purpose.
        movem.l d0-d3,(a0)
        mulu.w  d0,d1
```

## ANTI-005: Unausgerichtete Long-Writes

**Kategorie:** Antipattern / Speicher  
**Status:** VERIFIZIERT (gemessen – Vampire V4, Log vorhanden)  
**Beleg:** VERIFIZIERT (Literatur): PRM S. 90  
**Betroffene CPUs:** 68080 (V4); nicht 68060

### Problem

Zwei Long-Writes bei `a0+1` und `a0+5` überschreiten eine Quad-Grenze. Das
PRM empfiehlt ausdrücklich, Schreibzugriffe quad-aligned zu platzieren.

### Anti-Pattern (wird gemessen)

```asm
    move.l  d2,1(a0)
    move.l  d3,5(a0)
```

### Best Practice

```asm
    move.l  d2,(a0)
    move.l  d3,4(a0)
```

### Warum das Anti-Pattern schadet

Ein 8-Byte-Schreibfenster, das eine 8-Byte-Grenze überschreitet, passt nicht
in einen 64-Bit-Zugriff des Speicherpfads. Das PRM sagt: „Align writes for
fastest result, so quad write to quad bound address.“ Unausgerichtete
**Lese**zugriffe sind bei Cache-Treffer laut PRM kostenlos; die
Ausrichtungsregel gilt vor allem für Schreibzugriffe.

### Messung

- Sequenz: [`seq/ANTI-005.i`](seq/ANTI-005.i)
- Log: [v5 V4-Datei](measurements/v5-files/ANTI-005.run01.log) · [v1-Archiv](measurements/ANTI-005.log)
- **Messergebnis (Vampire V4, Harness v5, 10 × 100 Läufe):**
  - `BASELINE = 5` (Median der 10 Läufe)
  - `MIN = 3` Takte (in allen 10 Läufen identisch)
  - `MAX = 32` Takte (Median der 10 Läufe)
  - `AVG = 3` Takte (Median der 10 Läufe)
  - **Pipe-Slots (v5):** `IEP1 = 0`, `IEP2 = 0`, `IEP3 = 0` pro Lauf (Median); Summe = 0.
  - 10/10 Läufe mit 100/100 `RES`-Zeilen, `check_log.py` OK.
- **Warmup:** Die ersten 10 Läufe liegen bei MIN; der v1-Kaltstart ist weg.
- **Wiederholbarkeit:** MIN 10/10 identisch; MAX/AVG sind Mediane der 10 Läufe.
- **Baseline-Warnung:** `BASELINE = 5`; `NET = RES − BASELINE` wird negativ und ist kein gültiger Sequenzwert.

- **Vergleich:** [`MEM-001`](06-memory.md#mem-001-zwei-long-writes-an-einer-quad-grenze-ausrichten).

**Gemessene Sequenz (verbatim aus `seq/ANTI-005.i`, gebaut als `bin-v5/ANTI-005`):**

```asm
; seq/ANTI-005.i - ANTI-PATTERN: unaligned long writes
; Uses: d2, d3, a0
; MUST NOT touch: d5, d6, d7, a5, a6, a7
; NOTE: this sequence measures the anti-pattern on purpose.
        move.l  d2,1(a0)
        move.l  d3,5(a0)
```

## ANTI-006: TOUCH unmittelbar vor dem Load

**Kategorie:** Antipattern / Prefetch  
**Status:** VERIFIZIERT (gemessen – Vampire V4, Log vorhanden)  
**Beleg:** VERIFIZIERT (Literatur): PRM S. 40 („Use it 12 to 15 cycles
before needed“)  
**Betroffene CPUs:** 68080 (V4); nicht 68060

### Problem

`TOUCH` soll eine Cache-Zeile vorladen. Wenn der Load unmittelbar folgt, ist
der Prefetch noch nicht fertig; die Instruktion kostet nur Zeit.

### Anti-Pattern (wird gemessen)

```asm
    touch   (a0,d0.w)
    move.l  (a0),d1
```

### Best Practice

```asm
    touch   (a0,d0.w)
    ; ... 12-15 Takte unabhängige Arbeit ...
    move.l  (a0),d1
```

### Warum das Anti-Pattern schadet

Das PRM gibt den Vorlauf explizit mit 12–15 Takten an. Ein sofortiger Load
kann den Prefetch nicht mehr nutzen. In einer Schleife ist der richtige Ort
für `TOUCH` daher mehrere Iterationen vor dem tatsächlichen Zugriff – oder man
verlässt sich bei sequentiellen Strömen auf den automatischen Prefetcher
(siehe [`MEM-003`](06-memory.md#mem-003-sequentielle-loads-lösen-den-auto-prefetch-aus)).

### Messung

- Sequenz: [`seq/ANTI-006.i`](seq/ANTI-006.i)
- Log: [v5 V4-Datei](measurements/v5-files/ANTI-006.run01.log) · [v1-Archiv](measurements/ANTI-006.log)
- **Messergebnis (Vampire V4, Harness v5, 10 × 100 Läufe):**
  - `BASELINE = 5` (Median der 10 Läufe)
  - `MIN = 6` Takte (in allen 10 Läufen identisch)
  - `MAX = 6` Takte (Median der 10 Läufe)
  - `AVG = 6` Takte (Median der 10 Läufe)
  - **Pipe-Slots (v5):** `IEP1 = 7`, `IEP2 = 0`, `IEP3 = 0` pro Lauf (Median); Summe = 7.
  - 10/10 Läufe mit 100/100 `RES`-Zeilen, `check_log.py` OK.
- **Warmup:** Die ersten 10 Läufe liegen bei MIN; der v1-Kaltstart ist weg.
- **Wiederholbarkeit:** MIN 10/10 identisch; MAX/AVG sind Mediane der 10 Läufe.
- **Baseline-Warnung:** `BASELINE = 5`; `NET = RES − BASELINE` ist nicht subtrahierbar.

- **Vergleich:** [`MEM-002`](06-memory.md#mem-002-touch-mit-1215-takten-vorlauf).

**Gemessene Sequenz (verbatim aus `seq/ANTI-006.i`, gebaut als `bin-v5/ANTI-006`):**

```asm
; seq/ANTI-006.i - ANTI-PATTERN: TOUCH immediately before the load
; Uses: d0, d1, a0
; MUST NOT touch: d5, d6, d7, a5, a6, a7
; NOTE: this sequence measures the anti-pattern on purpose.
        moveq   #0,d0
        touch   (a0,d0.w)
        move.l  (a0),d1
```

## ANTI-007: Blindes Loop-Unrolling zerstört Decoder-Learning (ANTON/BRUNO/CESAR)

**Kategorie:** Loop Scheduling / Superscalar-Lernmodell  
**Status:** ARCHITEKTUR-PRINZIP & VERIFIZIERT (Architektur-Design von Gunnar von Boehn / Apollo Team; gemessen im Harness v5 Warmup vs Cold Run)  
**Beleg:** Gunnar von Boehn (BigGun), Discord 27.10.2026; Apollo 68080 PRM (Juni 2026) S. 83–84, 92  
**Betroffene CPUs:** 68080 (V4 / V2); im Gegensatz zu x86 oder alten RISC-CPUs

### Problem

Auf älteren Architekturen (wie klassischem 68000/68020 mit teuren DBRA-Branches
oder frühen x86/RISC-CPUs) war Loop-Unrolling eine Standardoptimierung, um den
Branch-Overhead zu eliminieren. Entwickler und KI-Modelle rollen daher Schleifen
oft blind vollständig oder 32-/64-fach aus („Loop Flattening“).

Auf dem Apollo 68080 ist extremes oder vollständiges Loop-Unrolling jedoch
**kontraproduktiv und verlangsamt die Ausführung massiv**:
1. **Zerstörung des Decoder-Learnings:** Der 68080 lernt Instruktionsgrenzen,
   Pairing und Fusing dynamisch im I-Cache über Schleifen-Iterationen hinweg.
   Im ersten Durchlauf decodiert nur **ANTON** (Single-Instruction-Modus).
   Erst ab dem 2. Durchlauf schaltet sich **BRUNO** (2-Way) und ab dem
   3. Durchlauf **CESAR** (3-Way Superscalar) zu.
   Wird eine Schleife plattgewalzt, läuft der gesamte Code permanent im
   Kalt-Zustand (1 Befehl pro Takt auf ANTON)!
2. **I-Cache Trashing & Speicherbandbreite:** Ein aufgeblähter linearer
   Befehlsstrom sprengt den L1-Instruction-Cache (16 KB / 32 KB). Der Core
   muss permanent aus dem langsameren DDR-RAM nachladen, statt mit vollem
   Durchsatz im Cache zu rotieren.
3. **Zerstörung der Branch-Prediction:** Die Branch Caches können keine
   Branch-Historie aufbauen, wenn keine Schleifensprünge existieren.

### Falscher 68k / KI-Ansatz (Unrolled / Flattened)

```asm
; FALSCH: 64-fach unrolled.
; Bläht den I-Cache auf, verhindert BRUNO- und CESAR-Superscalar-Zuschaltung!
; Läuft im Single-Instruction-Modus auf ANTON.
    move.l  (a0)+,(a1)+
    move.l  (a0)+,(a1)+
    move.l  (a0)+,(a1)+
    ; ... [60 weitere Wiederholungen] ...
    move.l  (a0)+,(a1)+
```

### Richtiger 68080-Ansatz (Kompakte Schleife im I-Cache)

```asm
; RICHTIG: Kompakte Schleife, bleibt vollständig im I-Cache.
; Durchlauf 1: ANTON decodiert und taggt Instruktionsgrenzen im I-Cache.
; Durchlauf 2: BRUNO schaltet sich zu (2-Way Superscalar).
; Durchlauf 3: CESAR schaltet sich zu (3-Way Superscalar).
; Maximale Bandbreite durch AMMX LOADQ/STOREQ oder VPERM.
    lsr.l   #5,d7           ; Anzahl 32-Byte-Blöcke
    subq.l  #1,d7           ; DBF-Korrektur
.loop:
    loadq   (a0)+,d0
    loadq   (a0)+,d1
    storeq  d0,(a1)+
    storeq  d1,(a1)+
    dbf     d7,.loop
```

> **Wichtiger Hinweis für Entwickler und KI-Assistenten:**  
> *„Unrolling code would be stupid = if you flatten the code that there is no
> loops, then you can not run super scalar. You want a loop! In real world
> time critical code is always running in loops: a memcopy moving 10 MB is a loop,
> compressing/uncompressing data, decoding a video, draw a game screen — all
> this what really needs CPU power runs in loops. This is maybe something we
> should TELL AI — I have seen AI created code several times now and AI was not
> aware of this.“* — **Gunnar von Boehn (BigGun)**


## Anti-Pattern-Checkliste

| Muster | Warum es auf dem 68080 schadet | Besser |
|---|---|---|
| `nop` zwischen abhängigen Partnern | zerstört Fusing-Nachbarschaft | Paar benachbart lassen |
| 060-Scheduling mit Füll-Instruktion | zerstört Fusing/Bonding | Paare gruppieren, unabhängige Arbeit außen |
| `move.w`/`add.w` für ganze Register | Partial-Register-Merge | `move.l`/`add.l`, wenn semantisch erlaubt |
| `MOVEM`/`MUL`/`DIV` paaren wollen | Pipe 2 nimmt keine Multicycle | Pipe 1, unabhängige ALU daneben |
| unausgerichtete Writes | zusätzliche Transaktionen | quad-aligned schreiben |
| `TOUCH` ohne Vorlauf | Prefetch kommt zu spät | 12–15 Takte Abstand oder Auto-Prefetch |
| `ANTI-007`: Blindes Loop-Unrolling | Verhindert BRUNO/CESAR-Lernen, bleibt in Single-Issue (ANTON), I-Cache Thrashing | Kurze, kompakte Schleifen im I-Cache halten |
