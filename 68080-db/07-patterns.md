# 07 – Übergreifende Idiome

**Quellen:** Apollo 68080 PRM (Juni 2026), S. 85 (Fusing), S. 87
(Pipeline/EA), S. 90 (Optimizing), S. 92 (Cache/Bus); Apollo-Wiki „Instruction
Fusing“; `blitapollo.asm` für AMMX-Praxis.

**Kernaussage:** Die besten 68080-Routinen kombinieren mehrere kleine Regeln:
benachbarte Fusing-Paare, 64-Bit-Speicherzugriffe, Adressarithmetik in der
EA-Einheit und – wo sinnvoll – AMMX statt skalarer Byte-Schleifen. Die
folgenden Idiome sind bewusst als *Muster* dokumentiert; die Sequenzen messen
nur den jeweiligen Kern, nicht die komplette Schleife.

> **Messstatus:** Alle Einträge wurden mit **Harness v5** auf einer Vampire V4
> gemessen (10 × 100 Läufe). Die v5-Dateilogs enthalten zusätzlich `PERF`
> (Pipe-Slots). Referenz ist MIN, MAX/AVG sind Mediane der 10 Läufe; die
> v1-Logs bleiben als Archiv unter `measurements/<ID>.log`.

## PAT-001: Kopierschleife mit 16 Byte pro Iteration

**Kategorie:** Pattern / Kopieren  
**Status:** VERIFIZIERT (gemessen – Vampire V4, Log vorhanden)  
**Beleg:** VERIFIZIERT (Literatur): PRM S. 85 (zwei `move.l (An)+,(Am)+`),
S. 92 (64-Bit-Bus)  
**Betroffene CPUs:** 68080 (V4); nicht 68060

### Problem

Eine Kopierschleife mit einem Long pro Iteration hat viel Schleifen-Overhead
und nutzt den 64-Bit-Bus nicht. Zwei Long-Moves pro Iteration fusionieren zu
einem 64-Bit-Move; vier Long-Moves pro Iteration füllen ein 16-Byte-Fenster.

### Naiver 68k-Ansatz

```asm
.loop:
    move.l  (a0)+,(a1)+
    subq.l  #1,d0
    bne.s   .loop
```

### 68080-Ansatz

```asm
.loop:
    move.l  (a0)+,(a1)+
    move.l  (a0)+,(a1)+     ; ein 64-Bit-Move pro Paar
    move.l  (a0)+,(a1)+
    move.l  (a0)+,(a1)+     ; zweites Paar: 16 Byte gesamt
    subq.l  #1,d0
    bne.s   .loop
```

Die Sequenz misst die vier Moves (zwei Paare), damit der Schleifen-Overhead
nicht mit dem Fusing-Effekt vermischt wird. In der echten Schleife muss die
Anzahl der 16-Byte-Blöcke stimmen; ein Rest von 8 Byte braucht einen Nachlauf.

### Warum das funktioniert

Das PRM listet das Paar als Quad-Move. Zwei Paare pro Iteration ergeben
16 Byte; der Prefetcher sieht einen sequentiellen Strom und lädt das nächste
32-Byte-Fenster. Die Schleifensteuerung (`subq`/`bne`) bleibt außerhalb des
gemessenen Fragments.

**Grenze:** `subq.l #1,d0` + `bne.s` ist selbst ein Fusing-Paar (PRM S. 85:
„subq.l #1,dn / bne.s/s+/w almost dbra“). Ein `dbra` ist trotzdem oft die
kompaktere Form.

### Messung

- Sequenz: [`seq/PAT-001.i`](seq/PAT-001.i)
- Log: [v5 V4-Datei](measurements/v5-files/PAT-001.run01.log) · [v1-Archiv](measurements/PAT-001.log)
- **Messergebnis (Vampire V4, Harness v5, 10 × 100 Läufe):**
  - `BASELINE = 5` (Median der 10 Läufe)
  - `MIN = 5` Takte (in allen 10 Läufen identisch)
  - `MAX = 31.5` Takte (Median der 10 Läufe)
  - `AVG = 5` Takte (Median der 10 Läufe)
  - **Pipe-Slots (v5):** `IEP1 = 6`, `IEP2 = 0`, `IEP3 = 0` pro Lauf (Median); Summe = 6.
  - 10/10 Läufe mit 100/100 `RES`-Zeilen, `check_log.py` OK.
- **Warmup:** Die ersten 10 Läufe liegen bei MIN; der v1-Kaltstart ist weg.
- **Wiederholbarkeit:** MIN 10/10 identisch; MAX/AVG sind Mediane der 10 Läufe.
- **Baseline-Warnung:** `BASELINE = 5`; `NET = RES − BASELINE` ist nicht subtrahierbar.

- **Register:** `a0`, `a1`.

**Gemessene Sequenz (verbatim aus `seq/PAT-001.i`, gebaut als `bin-v5/PAT-001`):**

```asm
; seq/PAT-001.i - copy pattern: 16 bytes per source iteration
; Uses: a0, a1
; MUST NOT touch: d5, d6, d7, a5, a6, a7
        move.l  (a0)+,(a1)+
        move.l  (a0)+,(a1)+
        move.l  (a0)+,(a1)+
        move.l  (a0)+,(a1)+
```

## PAT-002: Clear-Schleife mit 8 Byte pro Iteration

**Kategorie:** Pattern / Clear  
**Status:** VERIFIZIERT (gemessen – Vampire V4, Log vorhanden)  
**Beleg:** VERIFIZIERT (Literatur): PRM S. 85 (`clr.l (An)+`-Paar)  
**Betroffene CPUs:** 68080 (V4); nicht 68060

### Problem

Ein Clear-Loop mit einem Long pro Iteration halbiert die mögliche
Speicherbreite. Das benachbarte Paar `clr.l (a0)+` wird als Quad-Clear
ausgeführt.

### Naiver 68k-Ansatz

```asm
.loop:
    clr.l   (a0)+
    subq.l  #1,d0
    bne.s   .loop
```

### 68080-Ansatz

```asm
.loop:
    clr.l   (a0)+
    clr.l   (a0)+       ; Quad-Clear
    subq.l  #1,d0
    bne.s   .loop
```

### Warum das funktioniert

Das PRM führt das Paar als „quad clr“. Der Core fasst die beiden
Nullschreibzugriffe zu einem 64-Bit-Schreibvorgang zusammen. Zusammen mit
ausgerichteten Puffern (siehe [`MEM-001`](06-memory.md#mem-001-zwei-long-writes-an-einer-quad-grenze-ausrichten))
ist das der optimale Clear-Pfad.

**Nicht `clr.q` verwenden:** Diese LineA-Form ist laut PRM nur auf bestimmten
Scores verfügbar und ausdrücklich nicht zur allgemeinen Nutzung gedacht.

### Messung

- Sequenz: [`seq/PAT-002.i`](seq/PAT-002.i)
- Log: [v5 V4-Datei](measurements/v5-files/PAT-002.run01.log) · [v1-Archiv](measurements/PAT-002.log)
- **Messergebnis (Vampire V4, Harness v5, 10 × 100 Läufe):**
  - `BASELINE = 5` (Median der 10 Läufe)
  - `MIN = 2` Takte (in allen 10 Läufen identisch)
  - `MAX = 29` Takte (Median der 10 Läufe)
  - `AVG = 2` Takte (Median der 10 Läufe)
  - **Pipe-Slots (v5):** `IEP1 = 21`, `IEP2 = 0`, `IEP3 = 0` pro Lauf (Median); Summe = 21.
  - 10/10 Läufe mit 100/100 `RES`-Zeilen, `check_log.py` OK.
- **Warmup:** Die ersten 10 Läufe liegen bei MIN; der v1-Kaltstart ist weg.
- **Wiederholbarkeit:** MIN 10/10 identisch; MAX/AVG sind Mediane der 10 Läufe.
- **Baseline-Warnung:** `BASELINE = 5`; `NET = RES − BASELINE` wird negativ und ist kein gültiger Sequenzwert.

- **Register:** `a0`.

**Gemessene Sequenz (verbatim aus `seq/PAT-002.i`, gebaut als `bin-v5/PAT-002`):**

```asm
; seq/PAT-002.i - clear pattern: 8 bytes per source iteration
; Uses: a0
; MUST NOT touch: d5, d6, d7, a5, a6, a7
        clr.l   (a0)+
        clr.l   (a0)+
```

## PAT-003: Zeiger in der EA-Einheit fortschreiben

**Kategorie:** Pattern / Adressierung  
**Status:** VERIFIZIERT (gemessen – Vampire V4, Log vorhanden)  
**Beleg:** VERIFIZIERT (Literatur): PRM S. 87  
**Betroffene CPUs:** 68080 (V4); nicht 68060

### Problem

In einer Kopierschleife werden oft beide Zeiger über D-Register berechnet und
dann nach A kopiert. Das erzeugt ALU->EA-Blasen. `lea`-Operationen bleiben im
EA-Pfad.

### Naiver 68k-Ansatz

```asm
    move.l  a0,d0
    addq.l  #16,d0
    move.l  d0,a0
    move.l  a1,d1
    addq.l  #16,d1
    move.l  d1,a1
```

### 68080-Ansatz

```asm
    lea     16(a0),a0
    lea     16(a1),a1
```

### Warum das funktioniert

Das PRM sagt „EA result to EA input = no LATENCY“. Beide `lea`-Operationen
sind unabhängig und bleiben im Adresspfad; es gibt keine 2-Takt-Blase wie bei
einem ALU-Ergebnis, das als EA-Eingang dient. In einer unrolled Kopierschleife
kann man die Zeiger entweder pro Iteration mit `lea` fortschreiben oder die
Postincrement-Adressierung der Moves nutzen; beides vermeidet den D-Pfad.

**Grenze:** `lea 16(a0),a0` ist eine EA-Operation, aber kein Load/Store. Die
eigentlichen Speicherzugriffe bleiben von Cache/Bus abhängig.

### Messung

- Sequenz: [`seq/PAT-003.i`](seq/PAT-003.i)
- Log: [v5 V4-Datei](measurements/v5-files/PAT-003.run01.log) · [v1-Archiv](measurements/PAT-003.log)
- **Messergebnis (Vampire V4, Harness v5, 10 × 100 Läufe):**
  - `BASELINE = 5` (Median der 10 Läufe)
  - `MIN = 2` Takte (in allen 10 Läufen identisch)
  - `MAX = 14` Takte (Median der 10 Läufe)
  - `AVG = 2` Takte (Median der 10 Läufe)
  - **Pipe-Slots (v5):** `IEP1 = 3`, `IEP2 = 1`, `IEP3 = 0` pro Lauf (Median); Summe = 4.
  - 10/10 Läufe mit 100/100 `RES`-Zeilen, `check_log.py` OK.
- **Warmup:** Die ersten 10 Läufe liegen bei MIN; der v1-Kaltstart ist weg.
- **Wiederholbarkeit:** MIN 10/10 identisch; MAX/AVG sind Mediane der 10 Läufe.
- **Baseline-Warnung:** `BASELINE = 5`; `NET = RES − BASELINE` wird negativ und ist kein gültiger Sequenzwert.

- **Register:** `a0`, `a1`.

**Gemessene Sequenz (verbatim aus `seq/PAT-003.i`, gebaut als `bin-v5/PAT-003`):**

```asm
; seq/PAT-003.i - advance pointers in the EA unit
; Uses: a0, a1
; MUST NOT touch: d5, d6, d7, a5, a6, a7
        lea     16(a0),a0
        lea     16(a1),a1
```

## PAT-004: Branchless Byte-Blend mit AMMX

**Kategorie:** Pattern / branchless  
**Status:** VERIFIZIERT (gemessen – Vampire V4, Log vorhanden)  
**Beleg:** VERIFIZIERT (Literatur): PRM S. 44 (`BSEL`), S. 56 (`PCMPGTB`);
`blitapollo.asm` (AMMX-Blitting)  
**Betroffene CPUs:** 68080 mit AMMX; nicht 68060

### Problem

Ein byteweises „nimm den größeren Wert“ oder ein Alpha-Blend wird skalar
schnell zu einem Branch pro Byte. Auf dem 68080 kann man die
Vergleichsmaske in einem Vektorregister erzeugen und mit `bsel` bitweise
auswählen.

### Naiver 68k-Ansatz

```asm
    move.b  (a0)+,d0
    move.b  (a1)+,d1
    cmp.b   d0,d1
    bls.s   .keep
    move.b  d0,d1
.keep:
    move.b  d1,(a2)+
    ; ... pro Byte
```

### 68080-Ansatz

```asm
    load    (a0)+,e0
    load    (a1)+,e1
    pand    e0,e0,e3        ; "else"-Quelle
    pcmpgtb e0,e1,e2        ; Maske: e1 > e0 ?
    bsel    e1,e2,e3
    store   e3,(a2)+
```

### Warum das funktioniert

`pcmpgtb` erzeugt pro Byte eine Maske aus `$FF`/`$00`; `bsel` wählt bitweise
zwischen zwei Quellen. Das Ergebnis ist branchless und damit frei von
Fehlvorhersagen. `blitapollo.asm` verwendet dieselbe Idee für Pixel-
Konvertierungen und Blends.

**Wichtig:** AMMX setzt keine CCR. Ein `bne` nach `pcmpgtb` sieht nicht das
Vergleichsergebnis; die Maske muss explizit weiterverarbeitet werden.

### Messung

- Sequenz: [`seq/PAT-004.i`](seq/PAT-004.i)
- Log: [v5 V4-Datei](measurements/v5-files/PAT-004.run01.log) · [v1-Archiv](measurements/PAT-004.log)
- **Messergebnis (Vampire V4, Harness v5, 10 × 100 Läufe):**
  - `BASELINE = 5` (Median der 10 Läufe)
  - `MIN = 7` Takte (in allen 10 Läufen identisch)
  - `MAX = 7` Takte (Median der 10 Läufe)
  - `AVG = 7` Takte (Median der 10 Läufe)
  - **Pipe-Slots (v5):** `IEP1 = 8`, `IEP2 = 0`, `IEP3 = 0` pro Lauf (Median); Summe = 8.
  - 10/10 Läufe mit 100/100 `RES`-Zeilen, `check_log.py` OK.
- **Warmup:** Die ersten 10 Läufe liegen bei MIN; der v1-Kaltstart ist weg.
- **Wiederholbarkeit:** MIN 10/10 identisch; MAX/AVG sind Mediane der 10 Läufe.
- **Baseline-Warnung:** `BASELINE = 5`; `NET = RES − BASELINE` ist nicht subtrahierbar.

- **Register:** `e0`–`e3`, `a0`, `a1`, `a2`.

**Gemessene Sequenz (verbatim aus `seq/PAT-004.i`, gebaut als `bin-v5/PAT-004`):**

```asm
; seq/PAT-004.i - branchless byte blend with AMMX compare/select
; Uses: e0-e3, a0, a1, a2
; MUST NOT touch: d5, d6, d7, a5, a6, a7
        load    (a0)+,e0
        load    (a1)+,e1
        pand    e0,e0,e3
        pcmpgtb e0,e1,e2
        bsel    e1,e2,e3
        store   e3,(a2)+
```

## PAT-005: 32-Byte-Kopierburst pro Iteration

**Kategorie:** Pattern / Kopieren  
**Status:** VERIFIZIERT (gemessen – Vampire V4, Log vorhanden)  
**Beleg:** VERIFIZIERT (Literatur): PRM S. 85 (Fusing), S. 92 (32-Byte-Burst);
`blitapollo.asm` (64-Bit-`load`/`store`-Kopierschleife)  
**Betroffene CPUs:** 68080 (V4); nicht 68060

### Problem

Bei größeren Kopien ist ein 16-Byte-Schritt gut, aber ein 32-Byte-Schritt
passt genau auf das Burst-Fenster des Speichers. Acht Long-Moves pro
Iteration ergeben vier 64-Bit-Moves und 32 Byte pro Iteration.

### Naiver 68k-Ansatz

```asm
.loop:
    move.l  (a0)+,(a1)+
    subq.l  #1,d0
    bne.s   .loop
```

### 68080-Ansatz

```asm
.loop:
    move.l  (a0)+,(a1)+
    move.l  (a0)+,(a1)+
    move.l  (a0)+,(a1)+
    move.l  (a0)+,(a1)+
    move.l  (a0)+,(a1)+
    move.l  (a0)+,(a1)+
    move.l  (a0)+,(a1)+
    move.l  (a0)+,(a1)+     ; vier 64-Bit-Moves = 32 Byte
    subq.l  #1,d0
    bne.s   .loop
```

### Warum das funktioniert

Vier benachbarte Long-Move-Paare fusionieren zu vier 64-Bit-Moves. 32 Byte
entsprechen dem PRM-Burst `4x8`. Der sequentielle Strom löst den
automatischen Prefetch aus. `blitapollo.asm` verwendet für den schnellen
Kopierpfad `load (a0)+,E0` / `store E0,(a1)+` und begründet das mit weniger
Write-Buffer-Druck; die 68k-Variante mit acht Moves ist die portablere Form.

**Grenze:** Bei 32-Byte-Schritten muss die Restbehandlung stimmen (0–31 Byte).
Ein falsch dimensionierter Nachlauf ist ein klassischer Kopierfehler.

### Messung

- Sequenz: [`seq/PAT-005.i`](seq/PAT-005.i)
- Log: [v5 V4-Datei](measurements/v5-files/PAT-005.run01.log) · [v1-Archiv](measurements/PAT-005.log)
- **Messergebnis (Vampire V4, Harness v5, 10 × 100 Läufe):**
  - `BASELINE = 5` (Median der 10 Läufe)
  - `MIN = 9` Takte (in allen 10 Läufen identisch)
  - `MAX = 50` Takte (Median der 10 Läufe)
  - `AVG = 10` Takte (Median der 10 Läufe)
  - **Pipe-Slots (v5):** `IEP1 = 10`, `IEP2 = 0`, `IEP3 = 0` pro Lauf (Median); Summe = 10.
  - 10/10 Läufe mit 100/100 `RES`-Zeilen, `check_log.py` OK.
- **Warmup:** Die ersten 10 Läufe liegen bei MIN; der v1-Kaltstart ist weg.
- **Wiederholbarkeit:** MIN 10/10 identisch; MAX/AVG sind Mediane der 10 Läufe.
- **Baseline-Warnung:** `BASELINE = 5`; `NET = RES − BASELINE` ist nicht subtrahierbar.

- **Register:** `a0`, `a1`.

**Gemessene Sequenz (verbatim aus `seq/PAT-005.i`, gebaut als `bin-v5/PAT-005`):**

```asm
; seq/PAT-005.i - 32-byte copy burst per iteration
; Uses: a0, a1
; MUST NOT touch: d5, d6, d7, a5, a6, a7
        move.l  (a0)+,(a1)+
        move.l  (a0)+,(a1)+
        move.l  (a0)+,(a1)+
        move.l  (a0)+,(a1)+
        move.l  (a0)+,(a1)+
        move.l  (a0)+,(a1)+
        move.l  (a0)+,(a1)+
        move.l  (a0)+,(a1)+
```

