# 06 – 64-Bit-Speicherbus, Prefetch und Cache

**Quellen:** Apollo 68080 PRM (Juni 2026), S. 27 (`MOVE16`), S. 40 (`TOUCH`),
S. 46/67 (`LOAD`/`STORE`), S. 88 (Timing), S. 90 („Optimizing“), S. 92
(„Specs“).

**Kernaussage:** Der 68080 hat einen 64-Bit-Datenbus, 32 KB Icache, 128 KB
Dcache mit drei Ports, 32-Byte-Bursts und einen automatischen Prefetcher für
sequentielle Zugriffe. Die wichtigsten Regeln:

- **Schreibzugriffe an Quad-Grenzen ausrichten.**
- **Unausgerichtete Lese-Treffer** kosten laut PRM keinen Aufpreis.
- **Sequentielle Ströme** lösen den automatischen Prefetch aus; man muss nicht
  pro Iteration manuell prefetchen.
- **`TOUCH`** ist für gezielte, nicht-sequentielle Vorladung gedacht und
  braucht 12–15 Takte Vorlauf.

> **Messstatus:** Alle Einträge wurden mit **Harness v5** auf einer Vampire V4
> gemessen (10 × 100 Läufe). Die v5-Dateilogs enthalten zusätzlich `PERF`
> (Pipe-Slots). Referenz ist MIN, MAX/AVG sind Mediane der 10 Läufe; die
> v1-Logs bleiben als Archiv unter `measurements/<ID>.log`.

## MEM-001: Zwei Long-Writes an einer Quad-Grenze ausrichten

**Kategorie:** Speicher / Ausrichtung  
**Status:** VERIFIZIERT (gemessen – Vampire V4, Log vorhanden)  
**Beleg:** VERIFIZIERT (Literatur): PRM S. 90 („Align writes for fastest
result, so quad write to quad bound address“)  
**Betroffene CPUs:** 68080 (V4); nicht 68060

### Problem

Zwei `move.l` hintereinander schreiben 8 Byte. Liegen sie an einer
ungünstigen Adresse, muss der Speicherpfad sie über eine Quad-Grenze
verteilen. Das PRM empfiehlt ausdrücklich, Schreibzugriffe an Quad-Grenzen
auszurichten.

### Naiver 68k-Ansatz

```asm
    move.l  d2,1(a0)    ; 8 Byte Fenster beginnt bei a0+1
    move.l  d3,5(a0)
```

### 68080-Ansatz

```asm
    move.l  d2,(a0)     ; a0 ist 8-Byte-aligned (Harness: align 8)
    move.l  d3,4(a0)
```

### Warum das funktioniert

Der 68080 hat einen 64-Bit-Speicherbus und einen 3-Port-Dcache, der pro Takt
8 Byte lesen **und** 8 Byte schreiben kann. Ein 8-Byte-Schreibfenster, das
eine Quad-Grenze überschreitet, passt nicht in einen solchen Zugriff; das PRM
empfiehlt deshalb die Ausrichtung, weil sonst zusätzliche Transaktionen
drohen. Puffer und Records nach Möglichkeit 8-Byte-aligned anlegen.

**Wichtig:** Das PRM sagt, unausgerichtete **Lese**zugriffe kosten bei
Cache-Treffer keinen Aufpreis. Die Ausrichtungsempfehlung gilt vor allem für
**Schreib**zugriffe und für Zugriffe, die eine Burst-Grenze überschreiten.

### Messung

- Sequenz: [`seq/MEM-001.i`](seq/MEM-001.i)
- Log: [v5 V4-Datei](measurements/v5-files/MEM-001.run01.log) · [v1-Archiv](measurements/MEM-001.log)
- **Messergebnis (Vampire V4, Harness v5, 10 × 100 Läufe):**
  - `BASELINE = 5` (Median der 10 Läufe)
  - `MIN = 3` Takte (in allen 10 Läufen identisch)
  - `MAX = 8` Takte (Median der 10 Läufe)
  - `AVG = 3` Takte (Median der 10 Läufe)
  - **Pipe-Slots (v5):** `IEP1 = 20.96`, `IEP2 = 0`, `IEP3 = 0` pro Lauf (Median); Summe = 20.96.
  - 10/10 Läufe mit 100/100 `RES`-Zeilen, `check_log.py` OK.
- **Warmup:** Die ersten 10 Läufe liegen bei MIN; der v1-Kaltstart ist weg.
- **Wiederholbarkeit:** MIN 10/10 identisch; MAX/AVG sind Mediane der 10 Läufe.
- **Baseline-Warnung:** `BASELINE = 5`; `NET = RES − BASELINE` wird negativ und ist kein gültiger Sequenzwert.

- **Register:** `d2`, `d3`, `a0`.

**Gemessene Sequenz (verbatim aus `seq/MEM-001.i`, gebaut als `bin-v5/MEM-001`):**

```asm
; seq/MEM-001.i - align two long writes to one quad boundary
; Uses: d2, d3, a0
; MUST NOT touch: d5, d6, d7, a5, a6, a7
        move.l  d2,(a0)
        move.l  d3,4(a0)
```

## MEM-002: TOUCH mit 12–15 Takten Vorlauf

**Kategorie:** Speicher / Prefetch  
**Status:** VERIFIZIERT (gemessen – Vampire V4, Log vorhanden)  
**Beleg:** VERIFIZIERT (Literatur): PRM S. 40  
**Betroffene CPUs:** 68080 (V4); nicht 68060

### Problem

Der automatische Prefetcher erkennt sequentielle Ströme. Für einen gezielten
Zugriff auf Daten, die nicht in einem sequentiellen Muster liegen, gibt es
`TOUCH`: Es lädt die entsprechende Cache-Zeile vor. Wer `TOUCH` unmittelbar
vor den Ladebefehl setzt, hat keinen Nutzen; das PRM empfiehlt **12–15 Takte
Vorlauf**.

### Naiver 68k-Ansatz

```asm
    touch   (a0,d0.w)   ; zu spät: der Load folgt sofort
    move.l  (a0),d1
```

### 68080-Ansatz

```asm
    touch   (a0,d0.w)   ; 12-15 Takte vor dem Gebrauch
    ; ... unabhängige Arbeit (der eigentliche Schleifenkörper) ...
    move.l  (a0),d1
```

### Warum das funktioniert

Das PRM beschreibt `TOUCH <ea>` als „Preload data cache. Use it 12 to 15
cycles before needed.“ Unterstützt werden nur Adressmodi mit Index bzw.
indirekter Zugriff („address index & indirect“). Der Befehl ist ein
Line-F-Koprozessor-Befehl mit id=3 (wie `MOVE16`) und wird laut PRM auch von
Pipe 2 akzeptiert.

**vasm-Fallstrick:** vasm 2.0d lehnt `touch (a0)` ab („illegal operand
types“), akzeptiert aber die indexierte Form `touch (a0,d0.w)`. Die Sequenz
setzt deshalb `d0 = 0` und verwendet `(a0,d0.w)`. Das ist eine
Assembler-Eigenheit, keine Aussage über die Hardware.

### Messung

- Sequenz: [`seq/MEM-002.i`](seq/MEM-002.i)
- Log: [v5 V4-Datei](measurements/v5-files/MEM-002.run01.log) · [v1-Archiv](measurements/MEM-002.log)
- **Messergebnis (Vampire V4, Harness v5, 10 × 100 Läufe):**
  - `BASELINE = 5` (Median der 10 Läufe)
  - `MIN = 5` Takte (in allen 10 Läufen identisch)
  - `MAX = 5` Takte (Median der 10 Läufe)
  - `AVG = 5` Takte (Median der 10 Läufe)
  - **Pipe-Slots (v5):** `IEP1 = 6`, `IEP2 = 0`, `IEP3 = 0` pro Lauf (Median); Summe = 6.
  - 10/10 Läufe mit 100/100 `RES`-Zeilen, `check_log.py` OK.
- **Warmup:** Die ersten 10 Läufe liegen bei MIN; der v1-Kaltstart ist weg.
- **Wiederholbarkeit:** MIN 10/10 identisch; MAX/AVG sind Mediane der 10 Läufe.
- **Baseline-Warnung:** `BASELINE = 5`; `NET = RES − BASELINE` ist nicht subtrahierbar.

- **Register:** `d0`, `a0`.

**Gemessene Sequenz (verbatim aus `seq/MEM-002.i`, gebaut als `bin-v5/MEM-002`):**

```asm
; seq/MEM-002.i - isolated TOUCH cost (scheduling distance is documented)
; Uses: d0, a0
; MUST NOT touch: d5, d6, d7, a5, a6, a7
        moveq   #0,d0
        touch   (a0,d0.w)
```

## MEM-003: Sequentielle Loads lösen den Auto-Prefetch aus

**Kategorie:** Speicher / Prefetch  
**Status:** VERIFIZIERT (gemessen – Vampire V4, Log vorhanden)  
**Beleg:** VERIFIZIERT (Literatur): PRM S. 92 („The CPU itself detect
continuous memory access and will automatically prefetch“)  
**Betroffene CPUs:** 68080 (V4); nicht 68060

### Problem

Manche 68k-Programme setzen pro Iteration ein manuelles `TOUCH` vor jeden
sequentiellen Load. Das ist auf dem 68080 unnötig und kann sogar schaden,
weil es zusätzliche Line-F-Operationen erzeugt. Der Core erkennt den
fortlaufenden Strom selbst.

### Naiver 68k-Ansatz

```asm
    touch   (a0,d0.w)
    move.l  (a0)+,d1
    touch   (a0,d0.w)
    move.l  (a0)+,d2
    ; ... TOUCH pro Iteration
```

### 68080-Ansatz

```asm
    move.l  (a0)+,d0
    move.l  (a0)+,d1
    move.l  (a0)+,d2
    move.l  (a0)+,d3    ; sequentiell: Prefetch läuft automatisch
```

### Warum das funktioniert

Das PRM sagt, die CPU erkenne kontinuierliche Speicherzugriffe und prefetche
automatisch. Ein sequentieller Load-Strom mit konstanter Schrittweite ist
genau das Muster, für das der Prefetcher gedacht ist. `TOUCH` ist dagegen für
gezielte, nicht-sequentielle Zugriffe vorgesehen (siehe
[`MEM-002`](06-memory.md#mem-002-touch-mit-1215-takten-vorlauf)).

**Grenze:** Der Prefetcher erkennt „continuous“ Zugriffe; ein Sprung, eine
indirekte Adressierung oder ein Wechsel der Zeigerbasis kann das Muster
unterbrechen. Dann ist `TOUCH` wieder sinnvoll.

### Messung

- Sequenz: [`seq/MEM-003.i`](seq/MEM-003.i)
- Log: [v5 V4-Datei](measurements/v5-files/MEM-003.run01.log) · [v1-Archiv](measurements/MEM-003.log)
- **Messergebnis (Vampire V4, Harness v5, 10 × 100 Läufe):**
  - `BASELINE = 5` (Median der 10 Läufe)
  - `MIN = 3` Takte (in allen 10 Läufen identisch)
  - `MAX = 3` Takte (Median der 10 Läufe)
  - `AVG = 3` Takte (Median der 10 Läufe)
  - **Pipe-Slots (v5):** `IEP1 = 6`, `IEP2 = 0`, `IEP3 = 0` pro Lauf (Median); Summe = 6.
  - 10/10 Läufe mit 100/100 `RES`-Zeilen, `check_log.py` OK.
- **Warmup:** Die ersten 10 Läufe liegen bei MIN; der v1-Kaltstart ist weg.
- **Wiederholbarkeit:** MIN 10/10 identisch; MAX/AVG sind Mediane der 10 Läufe.
- **Baseline-Warnung:** `BASELINE = 5`; `NET = RES − BASELINE` wird negativ und ist kein gültiger Sequenzwert.

- **Register:** `d0`–`d3`, `a0` (4 Loads, 16 Byte pro Lauf).

**Gemessene Sequenz (verbatim aus `seq/MEM-003.i`, gebaut als `bin-v5/MEM-003`):**

```asm
; seq/MEM-003.i - sequential loads trigger the automatic prefetcher
; Uses: d0, d1, d2, d3, a0
; MUST NOT touch: d5, d6, d7, a5, a6, a7
        move.l  (a0)+,d0
        move.l  (a0)+,d1
        move.l  (a0)+,d2
        move.l  (a0)+,d3
```

## MEM-004: 32-Byte-Burst mit acht Long-Loads füllen

**Kategorie:** Speicher / Burst  
**Status:** VERIFIZIERT (gemessen – Vampire V4, Log vorhanden)  
**Beleg:** VERIFIZIERT (Literatur): PRM S. 92 („mem burst=32byte (4x8),
latency around 12 CPU cycle“)  
**Betroffene CPUs:** 68080 (V4); nicht 68060

### Problem

Der Speicherbus arbeitet mit 32-Byte-Bursts (4×8 Byte). Ein einzelner
Long-Load löst zwar einen Zugriff aus, nutzt aber den Burst nicht aus. Acht
aufeinanderfolgende Long-Loads decken genau 32 Byte ab und passen in das
Burst-Fenster.

### Naiver 68k-Ansatz

```asm
    move.l  (a0),d0     ; ein 4-Byte-Zugriff aus einem 32-Byte-Fenster
```

### 68080-Ansatz

```asm
    move.l  (a0)+,d0
    move.l  (a0)+,d1
    move.l  (a0)+,d2
    move.l  (a0)+,d3
    move.l  (a0)+,d0
    move.l  (a0)+,d1
    move.l  (a0)+,d2
    move.l  (a0)+,d3    ; 32 Byte, ein Burst-Fenster
```

### Warum das funktioniert

Das PRM spezifiziert „mem burst=32byte (4x8)“ und eine Latenz von ungefähr
12 CPU-Takten. Der automatische Prefetcher erkennt kontinuierliche Zugriffe
und lädt das nächste Fenster. Wer die acht Longs zusammen aus einem
32-Byte-aligned Puffer liest, gibt dem Prefetcher ein klares Muster.

**Grenze:** Die acht Loads sind keine Garantie für einen einzigen Burst; sie
beschreiben die optimale Fenstergröße. Die tatsächliche Zahl der Transaktionen
hängt von Cache-Zustand, Alignment und Busarbitrierung ab.

### Messung

- Sequenz: [`seq/MEM-004.i`](seq/MEM-004.i)
- Log: [v5 V4-Datei](measurements/v5-files/MEM-004.run01.log) · [v1-Archiv](measurements/MEM-004.log)
- **Messergebnis (Vampire V4, Harness v5, 10 × 100 Läufe):**
  - `BASELINE = 5` (Median der 10 Läufe)
  - `MIN = 5` Takte (in allen 10 Läufen identisch)
  - `MAX = 5` Takte (Median der 10 Läufe)
  - `AVG = 5` Takte (Median der 10 Läufe)
  - **Pipe-Slots (v5):** `IEP1 = 10`, `IEP2 = 0`, `IEP3 = 0` pro Lauf (Median); Summe = 10.
  - 10/10 Läufe mit 100/100 `RES`-Zeilen, `check_log.py` OK.
- **Warmup:** Die ersten 10 Läufe liegen bei MIN; der v1-Kaltstart ist weg.
- **Wiederholbarkeit:** MIN 10/10 identisch; MAX/AVG sind Mediane der 10 Läufe.
- **Baseline-Warnung:** `BASELINE = 5`; `NET = RES − BASELINE` ist nicht subtrahierbar.

- **Register:** `d0`–`d3`, `a0` (8 Loads, 32 Byte pro Lauf).

**Gemessene Sequenz (verbatim aus `seq/MEM-004.i`, gebaut als `bin-v5/MEM-004`):**

```asm
; seq/MEM-004.i - fill a 32-byte burst window with eight long loads
; Uses: d0, d1, d2, d3, a0
; MUST NOT touch: d5, d6, d7, a5, a6, a7
        move.l  (a0)+,d0
        move.l  (a0)+,d1
        move.l  (a0)+,d2
        move.l  (a0)+,d3
        move.l  (a0)+,d0
        move.l  (a0)+,d1
        move.l  (a0)+,d2
        move.l  (a0)+,d3
```

## MEM-005: 64-Bit-Load/Store statt zweier 32-Bit-Zugriffe

**Kategorie:** Speicher / Busbreite  
**Status:** VERIFIZIERT (gemessen – Vampire V4, Log vorhanden)  
**Beleg:** VERIFIZIERT (Literatur): PRM S. 46/67; `blitapollo.asm`
(Kommentar zu `load`/`store`); PRM S. 92 (64-Bit-Datenbus)  
**Betroffene CPUs:** 68080 mit AMMX; nicht 68060

### Problem

Zwei 32-Bit-Moves für 8 Byte erzeugen zwei Transaktionen. `load`/`store`
übertragen 8 Byte als einen 64-Bit-Wert. Der 68080-Speicherbus ist 64 Bit
breit; die AMMX-Variante passt besser zur Busbreite.

### Naiver 68k-Ansatz

```asm
    move.l  (a0)+,d0
    move.l  d0,(a1)+
    move.l  (a0)+,d1
    move.l  d1,(a1)+
```

### 68080-Ansatz

```asm
    load    (a0)+,e0
    store   e0,(a1)+
```

### Warum das funktioniert

`LOAD` liest immer ein Quadword (64 Bit), `STORE` schreibt 64 Bit. Der
Kommentar in `blitapollo.asm` begründet den Einsatz nicht mit mehr Byte pro
Takt, sondern mit geringerer Belastung der Write-Buffer durch 64-Bit-
Transfers. Das passt zur PRM-Spezifikation eines 64-Bit-Datenbusses und eines
Dcache, der pro Takt 8 Byte lesen und 8 Byte schreiben kann.

**Grenze:** Der Vorteil ist mikroarchitektonisch und nicht in jedem Kontext
messbar. Bei einem kopierten 8-Byte-Block ist der Unterschied klein; bei
vielen Schreibzugriffen hintereinander kann er sichtbar werden.

### Messung

- Sequenz: [`seq/MEM-005.i`](seq/MEM-005.i)
- Log: [v5 V4-Datei](measurements/v5-files/MEM-005.run01.log) · [v1-Archiv](measurements/MEM-005.log)
- **Messergebnis (Vampire V4, Harness v5, 10 × 100 Läufe):**
  - `BASELINE = 5` (Median der 10 Läufe)
  - `MIN = 3` Takte (in allen 10 Läufen identisch)
  - `MAX = 4` Takte (Median der 10 Läufe)
  - `AVG = 3` Takte (Median der 10 Läufe)
  - **Pipe-Slots (v5):** `IEP1 = 4`, `IEP2 = 0`, `IEP3 = 0` pro Lauf (Median); Summe = 4.
  - 10/10 Läufe mit 100/100 `RES`-Zeilen, `check_log.py` OK.
- **Warmup:** Die ersten 10 Läufe liegen bei MIN; der v1-Kaltstart ist weg.
- **Wiederholbarkeit:** MIN 10/10 identisch; MAX/AVG sind Mediane der 10 Läufe.
- **Baseline-Warnung:** `BASELINE = 5`; `NET = RES − BASELINE` wird negativ und ist kein gültiger Sequenzwert.

- **Register:** `e0`, `a0`, `a1`.

**Gemessene Sequenz (verbatim aus `seq/MEM-005.i`, gebaut als `bin-v5/MEM-005`):**

```asm
; seq/MEM-005.i - 64-bit load/store keeps the write buffer narrower
; Uses: e0, a0, a1
; MUST NOT touch: d5, d6, d7, a5, a6, a7
        load    (a0)+,e0
        store   e0,(a1)+
```

## Cache- und Bus-Spezifikationen (PRM S. 92)

| Ressource | Wert |
|---|---|
| Datenbus | 64 Bit |
| Icache | 32 KB, 16 Byte pro Takt zum CPU-Kern |
| Dcache | 128 KB, 3-ported: 8 Byte lesen **und** 8 Byte schreiben **und** mit dem Speicher sprechen |
| Branch Target Cache | 1024 Einträge |
| Speicher-Burst | 32 Byte (4×8) |
| Speicherlatenz | ca. 12 CPU-Takte |
| Prefetch | automatisch bei kontinuierlichem Zugriffsmuster |

Diese Werte sind Spezifikationen, keine Garantien für jede Core-Revision.
Für Messungen immer Core-Version und Firmware mitloggen.
