# 04 – Registermodell und Allokation

**Quellen:** Apollo 68080 PRM (Juni 2026), S. 3–4 (User Model, Integer Unit),
S. 7 (B-Register), S. 13 (BANK), S. 87 (Pipeline/EA), S. 90 („Prefer long &
quad“); Apollo-Wiki „Superscalar“.

**Kernaussage:** Der 68080 erweitert das klassische 68k-Registermodell, aber
nicht als flachen „32 Allzweckregister“-Satz:

- **D0–D7:** 8 Datumregister, im Integerpfad 32 Bit sichtbar.
- **A0–A7:** 8 Adressregister.
- **B0–B7:** zusätzliche Adressregister, aber mit eingeschränkten Legacy-
  Operationen; für AMMX voll verwendbar.
- **E0–E23:** 24 zusätzliche 64-Bit-Register, die sich der Integer- und der
  AMMX-Pfad teilen. Der Zugriff läuft über den BANK-Prefix und ist **nicht
  für alle Legacy-Instruktionen korrekt implementiert** (PRM S. 4).

> **Messstatus:** Alle Einträge wurden mit **Harness v5** auf einer Vampire V4
> gemessen (10 × 100 Läufe). Die v5-Dateilogs enthalten zusätzlich `PERF`
> (Pipe-Slots). Referenz ist MIN, MAX/AVG sind Mediane der 10 Läufe; die
> v1-Logs bleiben als Archiv unter `measurements/<ID>.log`.

## REG-001: E-Register als 64-Bit-Speicher – aber nicht als Allzweck-ALU

**Kategorie:** Register  
**Status:** VERIFIZIERT (gemessen – Vampire V4, Log vorhanden)  
**Beleg:** VERIFIZIERT (Literatur): PRM S. 3–4, S. 13; vasm-Syntaxtest  
**Betroffene CPUs:** 68080 (V4); nicht 68060

### Problem

Der 68080 hat 24 zusätzliche 64-Bit-Register. Es liegt nahe, sie wie einen
flachen 32-Register-Satz zu behandeln und beliebige Integer-Operationen
darauf auszuführen. Das PRM warnt aber ausdrücklich: Die BANK-Behandlung ist
für eine Reihe von Instruktionen „not correct yet“, darunter `add`, `sub`,
`and`, `or`, `cmp`, `mul`, `div`.

### Naiver Ansatz

```asm
    move.l  d0,e0
    add.l   d1,e0       ; BANK + add: laut PRM nicht korrekt implementiert
    move.l  e0,d2
```

### 68080-Ansatz

```asm
    move.l  d0,e0       ; E-Register als 64-Bit-Ablage
    move.l  e0,d2       ; zurück in den Integerpfad
```

### Warum das funktioniert

`move.l` gehört zu den Instruktionen, die laut PRM mit E-Registern umgehen
können. Der BANK-Prefix erweitert die Registerfelder; `E0`–`E23` werden über
die Bank-Bits adressiert. Für reine Transport- und AMMX-Nutzung ist das
sinnvoll. Für Integer-Arithmetik bleibt man besser im D-Pfad und nutzt E nur
als spillfreien Zwischenspeicher.

**Wichtig:** Die 32 „Datenregister“ des 68080 sind **nicht** 32 gleichartige
Integer-Register. Es sind 8 D-Register plus 24 E-Register mit partieller
BANK-Unterstützung. AMMX sieht alle 32 als 64-Bit-Vektorregister; der
Integerpfad sieht D0–D7 und – eingeschränkt – E0–E23.

### Messung

- Sequenz: [`seq/REG-001.i`](seq/REG-001.i)
- Log: [v5 V4-Datei](measurements/v5-files/REG-001.run01.log) · [v1-Archiv](measurements/REG-001.log)
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

- **Register:** `d0`, `e0`, `d2`.

**Gemessene Sequenz (verbatim aus `seq/REG-001.i`, gebaut als `bin-v5/REG-001`):**

```asm
; seq/REG-001.i - E register as 64-bit storage, integer path stays in D
; Uses: d0, d2, e0
; MUST NOT touch: d5, d6, d7, a5, a6, a7
        move.l  d0,e0
        move.l  e0,d2
```

## REG-002: B-Register nur mit den erlaubten Legacy-Formen

**Kategorie:** Register / Adressierung  
**Status:** VERIFIZIERT (gemessen – Vampire V4, Log vorhanden)  
**Beleg:** VERIFIZIERT (Literatur): PRM S. 7  
**Betroffene CPUs:** 68080 (V4); nicht 68060

### Problem

Die B-Register sind keine vollwertigen Ersatz-Adressregister. Das PRM listet
genau, was der Integerpfad mit ihnen darf:

```asm
    movea.l <ea>,Bn
    move.l  Bn,<ea>
    lea     <ea>,Bn
    lea     (Bn),An          ; = move.l Bn,An
    addq.l  #n,Bn
    subq.l  #n,Bn
    cmp.l   Bn,Dn
```

Nicht erlaubt sind unter anderem `move.l Bn,Bm`, `exg` mit Beteiligung eines
B-Registers und – im Legacy-Pfad – indirekte Adressierung über `(Bn)`.

### Naiver Ansatz

```asm
    move.l  (b0),d0     ; Legacy-EA mit Bn: nicht erlaubt
    move.l  b0,b1       ; B -> B: nicht erlaubt
```

### 68080-Ansatz

```asm
    move.l  a0,b0       ; A -> B ist erlaubt
    move.l  b0,d0       ; B -> D ist erlaubt
```

### Warum das funktioniert

Das PRM beschreibt B-Register als „handicapped versus its brother A“: Sie
können lange Daten oder AMMX-Daten manipulieren, aber nicht als allgemeine
Adressbasis im Legacy-Befehlssatz dienen. Für AMMX sind sie dagegen laut PRM
„fully interchangeable“ und in den Vektor-Adressmodi `(Bn)`, `(Bn)+`,
`-(Bn)`, `d16(Bn)`, `d8(Bn,Xn.w*Size)` verfügbar.

**Praxis:** B-Register eignen sich als zusätzliche Zeigerablage für AMMX-
Schleifen oder als langfristige Adressreserve; für klassische 68k-Code-
Sequenzen bleibt A0–A7 die sichere Wahl.

### Messung

- Sequenz: [`seq/REG-002.i`](seq/REG-002.i)
- Log: [v5 V4-Datei](measurements/v5-files/REG-002.run01.log) · [v1-Archiv](measurements/REG-002.log)
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

- **Register:** `a0`, `b0`, `d0`.

**Gemessene Sequenz (verbatim aus `seq/REG-002.i`, gebaut als `bin-v5/REG-002`):**

```asm
; seq/REG-002.i - B address register: valid move forms only
; Uses: a0, b0, d0
; MUST NOT touch: d5, d6, d7, a5, a6, a7
        move.l  a0,b0
        move.l  b0,d0
```

## REG-003: Long/Quad-Updates statt Byte/Word-Merges

**Kategorie:** Register / Datenbreite  
**Status:** VERIFIZIERT (gemessen – Vampire V4, Log vorhanden)  
**Beleg:** VERIFIZIERT (Literatur): PRM S. 90 („Prefer long & quad to update
registers“)  
**Betroffene CPUs:** 68080 (V4); nicht 68060

### Problem

Ein Byte- oder Word-Update eines Registers ist keine reine Überschreibung:
Der Core muss die übrigen Bits des Ziels erhalten und dafür Quelle und Ziel
zusammenführen. Das PRM empfiehlt deshalb, Register möglichst mit Long- oder
Quad-Operationen zu aktualisieren.

### Naiver Ansatz

```asm
    move.w  d2,d3
    add.w   d4,d3
```

### 68080-Ansatz

```asm
    move.l  d2,d3
    add.l   d4,d3
```

### Warum das funktioniert

Das PRM formuliert den Mechanismus so: Register werden intern als Long oder
Quad aktualisiert; ein Word-/Byte-Update liest deshalb Quelle **und** Ziel,
um die oberen Bits zu erhalten. Beide Formen können in einem Takt laufen –
der Vorteil liegt nicht in einer kürzeren Latenz, sondern in der einfacheren
Abhängigkeitskette und der Vermeidung von Partial-Register-Merges.

**Semantikwarnung:** `move.w` und `move.l` sind nur dann austauschbar, wenn
die oberen 16 Bit des Ziels ohnehin nicht gebraucht werden. Wenn sie erhalten
bleiben sollen, ist `move.w` korrekt und darf nicht durch `move.l` ersetzt
werden.

### Messung

- Sequenz: [`seq/REG-003.i`](seq/REG-003.i)
- Log: [v5 V4-Datei](measurements/v5-files/REG-003.run01.log) · [v1-Archiv](measurements/REG-003.log)
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

**Gemessene Sequenz (verbatim aus `seq/REG-003.i`, gebaut als `bin-v5/REG-003`):**

```asm
; seq/REG-003.i - prefer long/quad register updates over byte/word merges
; Uses: d2, d3, d4
; MUST NOT touch: d5, d6, d7, a5, a6, a7
        move.l  d2,d3
        add.l   d4,d3
```

## REG-004: Adressarithmetik in der EA-Einheit mit LEA

**Kategorie:** Register / Pipeline  
**Status:** VERIFIZIERT (gemessen – Vampire V4, Log vorhanden)  
**Beleg:** VERIFIZIERT (Literatur): PRM S. 87 (Pipeline, EA/ALU-Blasen)  
**Betroffene CPUs:** 68080 (V4); nicht 68060

### Problem

Wenn ein ALU-Ergebnis als Adresse oder Index der nächsten EA-Operation dient,
entsteht laut PRM eine **2-Takt-Blase** („ALU result to EA input = 2 cycle
bubble“). Wer eine Adresse über D-Register berechnet und dann in ein
A-Register kopiert, zahlt diese Blase.

### Naiver Ansatz

```asm
    move.l  a0,d0
    addq.l  #8,d0
    move.l  d0,a1       ; ALU-Ergebnis -> EA-Eingang
```

### 68080-Ansatz

```asm
    lea     8(a0),a1    ; reine EA-Operation, kein ALU->EA-Bubble
```

### Warum das funktioniert

Das PRM ordnet `LEA`, `ADDQ/SUBQ` auf An, `ADDA`/`SUBA`, `MOVEA` und
`MOVE.L #im,An` der EA-Einheit zu. „EA result to EA input = no LATENCY“;
die Adressberechnung bleibt also im Adresspfad. Ein ALU-Ergebnis, das erst
in einen EA-Eingang geschoben wird, kostet dagegen zwei Takte Blase.

**Grenze:** `lea d8(An,Dn*Scale),Am` ist ein EA-Befehl; der Index `Dn` muss
aber rechtzeitig bereitstehen. Die Blase entsteht nur, wenn `Dn` selbst gerade
aus der ALU kommt.

### Messung

- Sequenz: [`seq/REG-004.i`](seq/REG-004.i)
- Log: [v5 V4-Datei](measurements/v5-files/REG-004.run01.log) · [v1-Archiv](measurements/REG-004.log)
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

**Gemessene Sequenz (verbatim aus `seq/REG-004.i`, gebaut als `bin-v5/REG-004`):**

```asm
; seq/REG-004.i - address arithmetic in the EA unit with LEA
; Uses: a0, a1
; MUST NOT touch: d5, d6, d7, a5, a6, a7
        lea     8(a0),a1
```

## REG-005: Keine 3-Operanden-Syntax erzwingen

**Kategorie:** Register / Fusing  
**Status:** VERIFIZIERT (gemessen – Vampire V4, Log vorhanden)  
**Beleg:** VERIFIZIERT (Literatur): PRM S. 4, S. 13, S. 85; vasm-Test
(„illegal operand types“)  
**Betroffene CPUs:** 68080 (V4); nicht 68060

### Problem

Das PRM beschreibt die Integer-ALU als intern 3-operandig. Daraus folgt
**nicht**, dass der Assembler eine allgemeine 3-Operanden-Form akzeptiert.
Vasm 2.0d lehnt `add.l d0,d1,d2` mit „illegal operand types“ ab. Die
3-Operanden-Form entsteht nur intern durch Fusing eines 2-Operanden-Paares.

### Naiver Ansatz (baut nicht)

```asm
    add.l   d0,d1,d2    ; vasm: illegal operand types
```

### 68080-Ansatz

```asm
    move.l  d0,d2
    add.l   d1,d2       ; 2-Operanden-Paar, intern 3-operandig gefused
```

### Warum das funktioniert

Die Fusion erkennt das Paar `move.l d0,d2` + `add.l d1,d2` und macht daraus
intern `add.l d0,d1,d2`. Der BANK-Prefix kann zwar Registerfelder erweitern,
aber laut PRM ist die BANK-Behandlung für viele Instruktionen noch unvollständig
(„handling of banking not correct yet“). Ein generelles 3-Operanden-
Quelltextformat ist daher weder dokumentiert noch in vasm implementiert.

**AMMX ist die Ausnahme:** AMMX-Instruktionen wie `paddb e0,e1,e2` sind echte
3-Operanden-Instruktionen und werden vom Assembler akzeptiert. Das gilt aber
nur für den AMMX-Befehlssatz, nicht für `add`/`sub`/`and`/`or` des Integerpfads.

### Messung

- Sequenz: [`seq/REG-005.i`](seq/REG-005.i)
- Log: [v5 V4-Datei](measurements/v5-files/REG-005.run01.log) · [v1-Archiv](measurements/REG-005.log)
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

**Gemessene Sequenz (verbatim aus `seq/REG-005.i`, gebaut als `bin-v5/REG-005`):**

```asm
; seq/REG-005.i - do not write 3-operand source syntax; let the core fuse
; Uses: d2, d3, d4
; MUST NOT touch: d5, d6, d7, a5, a6, a7
        move.l  d2,d3
        add.l   d4,d3
```

## Allokationsregeln (Kurzfassung)

| Ziel | Empfehlung | Grund |
|---|---|---|
| Häufige Skalarwerte | D0–D7 | voller Legacy-Befehlssatz, keine BANK-Sonderfälle |
| Zusätzliche Adressen | A0–A7 bevorzugen | B-Register sind im Legacy-EA eingeschränkt |
| 64-Bit-Vektordaten | E0–E23 | AMMX-Register, 3-Operanden-fähig |
| Spill-Vermeidung | E-Register nur mit `move`/AMMX | BANK-Arithmetik ist laut PRM unvollständig |
| Adressberechnung | `lea`/`addq` auf An | EA-Einheit, keine ALU->EA-Blase |
| Breite | Long/Quad, wenn semantisch erlaubt | vermeidet Partial-Register-Merges |
