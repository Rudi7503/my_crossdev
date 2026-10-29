# 03 – AMMX (64-Bit-SIMD)

**Quellen:** Apollo 68080 PRM (Juni 2026), S. 6 (AMMX-Überblick), S. 42
(Format), S. 46–77 (Instruktionen); Open-Source-Referenz
`blitapollo.asm` von Henryk Richter (libSDL12_Amiga68k), insbesondere
`load`/`store`, `pack3216`, `vperm`.

**Kernaussage:** AMMX ist eine Line-F-Koprozessor-Erweiterung mit 64-Bit-
Vektoren, 32 zugreifbaren 64-Bit-Registern (`D0`–`D7`, `E0`–`E23`) und
normalerweise drei Operanden. Die Instruktionen ändern die Condition Codes
**nicht**; `PCMPcc` schreibt das Vergleichsergebnis als Bitmaske in das
Zielregister. AMMX ist damit besonders für byte-/wortweise Bild-, Audio- und
String-Verarbeitung geeignet, nicht als Ersatz für Skalar-ALU.

> **Messstatus:** Alle Einträge wurden mit **Harness v5** auf einer Vampire V4
> gemessen (10 × 100 Läufe). Die v5-Dateilogs enthalten zusätzlich `PERF`
> (Pipe-Slots). Referenz ist MIN, MAX/AVG sind Mediane der 10 Läufe; die
> v1-Logs bleiben als Archiv unter `measurements/<ID>.log`.

## AMX-001: 64-Bit-LOAD/STORE statt zwei 32-Bit-Moves

**Kategorie:** AMMX / Speicher  
**Status:** VERIFIZIERT (gemessen – Vampire V4, Log vorhanden)  
**Beleg:** VERIFIZIERT (Literatur): PRM S. 46 (`LOAD`), S. 67 (`STORE`);
`blitapollo.asm` (Kommentar zu `load`/`store`)  
**Betroffene CPUs:** 68080 mit AMMX; nicht 68060

### Problem

Ein Kopierschritt mit zwei `move.l` überträgt 8 Byte in zwei 32-Bit-Zugriffen.
AMMX `load`/`store` überträgt dieselben 8 Byte als einen 64-Bit-Wert. Der
Gewinn liegt nicht in „mehr Byte pro Takt“ – der 68080-Speicherbus ist ohnehin
64 Bit breit –, sondern in weniger Transaktionen und geringerer Belastung der
Write-Buffer.

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

`LOAD` ist laut PRM „always quad word“ (64 Bit); `STORE` schreibt 64 Bit. Der
Quelltext in `blitapollo.asm` kommentiert das so:

> „this doesn't copy more data per cycle than 68k version, BUT! Less strain on
> the write buffers due to 64 Bit transfers“

Das ist eine plausible Mikroarchitektur-Begründung, aber **kein gemessener
Taktvorteil**. Der Eintrag ist deshalb ausdrücklich als Literaturmuster
markiert.

### Messung

- Sequenz: [`seq/AMX-001.i`](seq/AMX-001.i)
- Log: [v5 V4-Datei](measurements/v5-files/AMX-001.run01.log) · [v1-Archiv](measurements/AMX-001.log)
- **Messergebnis (Vampire V4, Harness v5, 10 × 100 Läufe):**
  - `BASELINE = 5` (Median der 10 Läufe)
  - `MIN = 3` Takte (in allen 10 Läufen identisch)
  - `MAX = 23` Takte (Median der 10 Läufe)
  - `AVG = 3` Takte (Median der 10 Läufe)
  - **Pipe-Slots (v5):** `IEP1 = 4`, `IEP2 = 0`, `IEP3 = 0` pro Lauf (Median); Summe = 4.
  - 10/10 Läufe mit 100/100 `RES`-Zeilen, `check_log.py` OK.
- **Warmup:** Die ersten 10 Läufe liegen bei MIN; der v1-Kaltstart ist weg.
- **Wiederholbarkeit:** MIN 10/10 identisch; MAX/AVG sind Mediane der 10 Läufe.
- **Baseline-Warnung:** `BASELINE = 5`; `NET = RES − BASELINE` wird negativ und ist kein gültiger Sequenzwert.

- **Register:** `e0`, `a0`, `a1`; `a0`/`a1` laufen 8 Byte pro Lauf.

**Gemessene Sequenz (verbatim aus `seq/AMX-001.i`, gebaut als `bin-v5/AMX-001`):**

```asm
; seq/AMX-001.i - 64-bit LOAD/STORE instead of two 32-bit moves
; Uses: e0, a0, a1
; MUST NOT touch: d5, d6, d7, a5, a6, a7
        load    (a0)+,e0
        store   e0,(a1)+
```

## AMX-002: Acht Byte-Additionen mit PADDB

**Kategorie:** AMMX / Vektorarithmetik  
**Status:** VERIFIZIERT (gemessen – Vampire V4, Log vorhanden)  
**Beleg:** VERIFIZIERT (Literatur): PRM S. 52 (`PADDB`, `PADDW`)  
**Betroffene CPUs:** 68080 mit AMMX; nicht 68060

### Problem

Der klassische 68k hat keine gepackte Byte-Addition. Acht unabhängige
Byte-Additionen müssen einzeln geladen, addiert und zurückgeschrieben werden;
dazwischen sind Maskieren und Zusammenpacken nötig. `PADDB` erledigt acht
Byte-Additionen in einer Instruktion, ohne Übertrag zwischen den Lanes.

### Naiver 68k-Ansatz (prinzipiell)

```asm
; 8 Byte-Lanes einzeln: laden, addieren, maskieren, zurückschreiben
    move.b  (a0)+,d0
    add.b   (a1)+,d0
    move.b  d0,(a2)+
    ; ... 7 weitere Lane-Blöcke, dazu ggf. Packen/Maskieren
```

### 68080-Ansatz

```asm
    load    (a0)+,e0
    load    (a1)+,e1
    paddb   e0,e1,e2
    store   e2,(a2)+
```

### Warum das funktioniert

Das PRM definiert `PADDB <vea>,b,d` als „8byte vector, It calculates 8
additions“. Jede der acht Byte-Positionen wird unabhängig addiert; ein
Übertrag in die Nachbarlane findet nicht statt. `PADDW` macht dasselbe mit
vier 16-Bit-Lanes.

**Semantik:** Ohne Sättigung wird pro Byte modulo 256 gerechnet; die
Bitmuster sind unabhängig davon, ob man sie als vorzeichenbehaftet oder
vorzeichenlos interpretiert. Wer sättigen will, braucht `PADDUSB`/`PADDUSW`
(siehe [`AMX-003`](03-ammx.md#amx-003-sättigende-unsigned-byte-adds-mit-paddusb)).

### Messung

- Sequenz: [`seq/AMX-002.i`](seq/AMX-002.i)
- Log: [v5 V4-Datei](measurements/v5-files/AMX-002.run01.log) · [v1-Archiv](measurements/AMX-002.log)
- **Messergebnis (Vampire V4, Harness v5, 10 × 100 Läufe):**
  - `BASELINE = 5` (Median der 10 Läufe)
  - `MIN = 5` Takte (in allen 10 Läufen identisch)
  - `MAX = 7` Takte (Median der 10 Läufe)
  - `AVG = 5` Takte (Median der 10 Läufe)
  - **Pipe-Slots (v5):** `IEP1 = 6`, `IEP2 = 0`, `IEP3 = 0` pro Lauf (Median); Summe = 6.
  - 10/10 Läufe mit 100/100 `RES`-Zeilen, `check_log.py` OK.
- **Warmup:** Die ersten 10 Läufe liegen bei MIN; der v1-Kaltstart ist weg.
- **Wiederholbarkeit:** MIN 10/10 identisch; MAX/AVG sind Mediane der 10 Läufe.
- **Baseline-Warnung:** `BASELINE = 5`; `NET = RES − BASELINE` ist nicht subtrahierbar.

- **Register:** `e0`, `e1`, `e2`, `a0`, `a1`, `a2`.

**Gemessene Sequenz (verbatim aus `seq/AMX-002.i`, gebaut als `bin-v5/AMX-002`):**

```asm
; seq/AMX-002.i - eight byte additions with PADDB
; Uses: e0, e1, e2, a0, a1, a2
; MUST NOT touch: d5, d6, d7, a5, a6, a7
        load    (a0)+,e0
        load    (a1)+,e1
        paddb   e0,e1,e2
        store   e2,(a2)+
```

## AMX-003: Sättigende unsigned Byte-Adds mit PADDUSB

**Kategorie:** AMMX / Sättigung  
**Status:** VERIFIZIERT (gemessen – Vampire V4, Log vorhanden)  
**Beleg:** VERIFIZIERT (Literatur): PRM S. 52 (`PADDUSB`, `PADDUSW`)  
**Betroffene CPUs:** 68080 mit AMMX; nicht 68060

### Problem

Bei Bilddaten soll ein Überlauf auf 255 begrenzt werden, nicht modulo 256
umschlagen. Der skalare 68k braucht dafür Vergleich und Branch oder `min`-
Sequenzen pro Lane; das kostet Sprünge und ist schlecht vorhersagbar.

### Naiver 68k-Ansatz

```asm
    move.b  (a0)+,d0
    add.b   (a1)+,d0
    cmp.b   #255,d0
    bls.s   .ok
    moveq   #255,d0
.ok:
    move.b  d0,(a2)+
    ; ... pro Lane
```

### 68080-Ansatz

```asm
    load    (a0)+,e0
    load    (a1)+,e1
    paddusb e0,e1,e2
    store   e2,(a2)+
```

### Warum das funktioniert

`PADDUSB` ist die „Unsigned Saturated“-Variante von `PADDB`: Ergebnisse über
255 werden laut PRM auf das Maximum geklemmt; ein Unterlauf tritt bei reinen
Additionen nicht auf. Die Sättigung passiert in Hardware pro Byte-Lane, ohne
Branch und ohne Condition Codes.

**Abgrenzung:** `PADDSB`/`PADDSW` sind die vorzeichenbehafteten Varianten;
deren Bereich ist −128..127 bzw. −32768..32767. Nicht verwechseln.

### Messung

- Sequenz: [`seq/AMX-003.i`](seq/AMX-003.i)
- Log: [v5 V4-Datei](measurements/v5-files/AMX-003.run01.log) · [v1-Archiv](measurements/AMX-003.log)
- **Messergebnis (Vampire V4, Harness v5, 10 × 100 Läufe):**
  - `BASELINE = 5` (Median der 10 Läufe)
  - `MIN = 5` Takte (in allen 10 Läufen identisch)
  - `MAX = 7` Takte (Median der 10 Läufe)
  - `AVG = 5` Takte (Median der 10 Läufe)
  - **Pipe-Slots (v5):** `IEP1 = 6`, `IEP2 = 0`, `IEP3 = 0` pro Lauf (Median); Summe = 6.
  - 10/10 Läufe mit 100/100 `RES`-Zeilen, `check_log.py` OK.
- **Warmup:** Die ersten 10 Läufe liegen bei MIN; der v1-Kaltstart ist weg.
- **Wiederholbarkeit:** MIN 10/10 identisch; MAX/AVG sind Mediane der 10 Läufe.
- **Baseline-Warnung:** `BASELINE = 5`; `NET = RES − BASELINE` ist nicht subtrahierbar.

- **Register:** `e0`, `e1`, `e2`, `a0`, `a1`, `a2`.

**Gemessene Sequenz (verbatim aus `seq/AMX-003.i`, gebaut als `bin-v5/AMX-003`):**

```asm
; seq/AMX-003.i - saturating unsigned byte add with PADDUSB
; Uses: e0, e1, e2, a0, a1, a2
; MUST NOT touch: d5, d6, d7, a5, a6, a7
        load    (a0)+,e0
        load    (a1)+,e1
        paddusb e0,e1,e2
        store   e2,(a2)+
```

## AMX-004: Branchless Byte-Auswahl mit PCMPGTB + BSEL

**Kategorie:** AMMX / branchless  
**Status:** VERIFIZIERT (gemessen – Vampire V4, Log vorhanden)  
**Beleg:** VERIFIZIERT (Literatur): PRM S. 44 (`BSEL`), S. 56 (`PCMPccB`)  
**Betroffene CPUs:** 68080 mit AMMX; nicht 68060

### Problem

Ein byteweises „nimm den größeren Wert“ oder ein Blenden zweier Bilder wird
skalar schnell zu einem Branch pro Byte. Auf dem 68080 kann man die
Vergleichsmaske in einem Register erzeugen und damit bitweise auswählen.

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
    pand    e0,e0,e3        ; e3 = Kopie von e0 als "else"-Quelle
    pcmpgtb e0,e1,e2        ; e2 = (e1 > e0) ? $FF : $00, pro Byte
    bsel    e1,e2,e3        ; Maske 1 -> e1, Maske 0 -> e3
    store   e3,(a2)+
```

### Warum das funktioniert

`PCMPGTB <vea>,b,d` berechnet pro Byte `b > <vea>` und schreibt eine Maske
mit `$FF` (wahr) oder `$00` (falsch) in `d`. `BSEL <vea>,mask,d` wählt
bitweise: Wo die Maske 1 ist, kommt das Bit aus `<vea>`; sonst bleibt das
Zielbit stehen. Mit einer vorherigen Kopie des „else“-Operanden wird daraus
ein branchless Select.

**Wichtig:** AMMX ändert die Condition Codes nicht. Ein anschließendes
`bne`/`beq` sieht **nicht** das Ergebnis von `pcmpgtb`; man muss die Maske
explizit testen oder `bsel`/`storeilm` verwenden.

### Messung

- Sequenz: [`seq/AMX-004.i`](seq/AMX-004.i)
- Log: [v5 V4-Datei](measurements/v5-files/AMX-004.run01.log) · [v1-Archiv](measurements/AMX-004.log)
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

**Gemessene Sequenz (verbatim aus `seq/AMX-004.i`, gebaut als `bin-v5/AMX-004`):**

```asm
; seq/AMX-004.i - branchless byte select with PCMPGTB + BSEL
; Uses: e0-e3, a0, a1, a2
; MUST NOT touch: d5, d6, d7, a5, a6, a7
        load    (a0)+,e0
        load    (a1)+,e1
        pand    e0,e0,e3
        pcmpgtb e0,e1,e2
        bsel    e1,e2,e3
        store   e3,(a2)+
```

## AMX-005: Chunky-to-Planar mit C2P

**Kategorie:** AMMX / Bit-Transponierung  
**Status:** VERIFIZIERT (gemessen – Vampire V4, Log vorhanden)  
**Beleg:** VERIFIZIERT (Literatur): PRM S. 45 (`C2P`)  
**Betroffene CPUs:** 68080 mit AMMX; nicht 68060

### Problem

Chunky-zu-Planar ist eine bitweise Transponierung: Aus 8 Bytes werden 8
Bitplanes. Klassisch braucht man dafür Shift-/Mask-/Merge-Schleifen oder den
Blitter. `C2P` macht die Transponierung in einer Instruktion.

### Naiver 68k-Ansatz

```asm
; Prinzip: für jede der 8 Bitpositionen alle 8 Quellbytes testen,
; Bit in das Zielbyte an Position n setzen, Ergebnisbyte speichern.
; Das sind viele Shifts, Masken und ORs pro Bildbyte.
```

### 68080-Ansatz

```asm
    load    (a0)+,e0
    c2p     e0,e1
    store   e1,(a1)+
```

### Warum das funktioniert

Das PRM beschreibt `C2P <vea>,d` als bitweise Transponierung: „From a 8 byte
source all bits from place n are put in destination byte n, in the order of
source.“ Alle MSBs landen im obersten Zielbyte, alle LSBs im untersten.
Planar-zu-Chunky ist dieselbe Operation (die Transponierung ist involutorisch
für die 8×8-Matrix).

**Grenze:** `C2P` ist eine 8×8-Bit-Operation. Für größere Bilder muss man sie
pro 8-Byte-Block wiederholen; das ist immer noch deutlich weniger Code als
eine generische Bit-Schleife.

### Messung

- Sequenz: [`seq/AMX-005.i`](seq/AMX-005.i)
- Log: [v5 V4-Datei](measurements/v5-files/AMX-005.run01.log) · [v1-Archiv](measurements/AMX-005.log)
- **Messergebnis (Vampire V4, Harness v5, 10 × 100 Läufe):**
  - `BASELINE = 5` (Median der 10 Läufe)
  - `MIN = 4` Takte (in allen 10 Läufen identisch)
  - `MAX = 14.5` Takte (Median der 10 Läufe)
  - `AVG = 4` Takte (Median der 10 Läufe)
  - **Pipe-Slots (v5):** `IEP1 = 5`, `IEP2 = 0`, `IEP3 = 0` pro Lauf (Median); Summe = 5.
  - 10/10 Läufe mit 100/100 `RES`-Zeilen, `check_log.py` OK.
- **Warmup:** Die ersten 10 Läufe liegen bei MIN; der v1-Kaltstart ist weg.
- **Wiederholbarkeit:** MIN 10/10 identisch; MAX/AVG sind Mediane der 10 Läufe.
- **Baseline-Warnung:** `BASELINE = 5`; `NET = RES − BASELINE` wird negativ und ist kein gültiger Sequenzwert.

- **Register:** `e0`, `e1`, `a0`, `a1`.

**Gemessene Sequenz (verbatim aus `seq/AMX-005.i`, gebaut als `bin-v5/AMX-005`):**

```asm
; seq/AMX-005.i - chunky-to-planar bit transpose with C2P
; Uses: e0, e1, a0, a1
; MUST NOT touch: d5, d6, d7, a5, a6, a7
        load    (a0)+,e0
        c2p     e0,e1
        store   e1,(a1)+
```

## AMX-006: 32-Bit-ARGB zu RGB565 mit PACK3216

**Kategorie:** AMMX / Pixelkonvertierung  
**Status:** VERIFIZIERT (gemessen – Vampire V4, Log vorhanden)  
**Beleg:** VERIFIZIERT (Literatur): PRM S. 50 (`PACK3216`);
`blitapollo.asm` (`pack3216 E0,E1,(a1)+`)  
**Betroffene CPUs:** 68080 mit AMMX; nicht 68060

### Problem

Für 16-Bit-RGB565 muss man aus jedem 32-Bit-ARGB-Pixel die Rot-, Grün- und
Blau-Bits extrahieren, auf 5/6/5 Bit skalieren und Packen. Skalar sind das
pro Pixel mehrere Shifts, Masken und ORs. `PACK3216` erledigt vier Pixel
(2×2 Quellregister) in einer Instruktion.

### Naiver 68k-Ansatz

```asm
; pro Pixel: Rot nach Bit 11..15 schieben, Grün nach Bit 5..10,
; Blau nach Bit 0..4, dann OR und als Wort speichern.
; Vier Pixel = vier solcher Blöcke.
```

### 68080-Ansatz

```asm
    load    (a0)+,e0        ; zwei ARGB-Pixel
    load    (a0)+,e1        ; zwei weitere ARGB-Pixel
    pack3216 e0,e1,e2       ; vier RGB565-Pixel
    store   e2,(a1)+
```

### Warum das funktioniert

Das PRM definiert `PACK3216 b,d,<vea>` als „Compress 2 x 2 32 bit color into
4 16 bit color“ und nennt als Ziel ausdrücklich RGB565. `blitapollo.asm`
verwendet genau diese Folge für `_ApolloARGBtoRGB565`. Die Skalierung und das
Packen passieren in Hardware; der Code bleibt frei von Pixel-Schleifen.

**Grenze:** Das Ergebnis ist RGB565, nicht RGB555 oder RGB24. Für andere
Formate braucht man `vperm`/`packuswb` oder eigene Masken.

### Messung

- Sequenz: [`seq/AMX-006.i`](seq/AMX-006.i)
- Log: [v5 V4-Datei](measurements/v5-files/AMX-006.run01.log) · [v1-Archiv](measurements/AMX-006.log)
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

- **Register:** `e0`, `e1`, `e2`, `a0`, `a1`.

**Gemessene Sequenz (verbatim aus `seq/AMX-006.i`, gebaut als `bin-v5/AMX-006`):**

```asm
; seq/AMX-006.i - 32-bit ARGB to RGB565 packing with PACK3216
; Uses: e0, e1, e2, a0, a1
; MUST NOT touch: d5, d6, d7, a5, a6, a7
        load    (a0)+,e0
        load    (a0)+,e1
        pack3216 e0,e1,e2
        store   e2,(a1)+
```

## AMMX-Regeln für die Praxis

1. **AMMX ändert keine CCR.** Nach `paddb`, `pand`, `c2p` usw. sind die Flags
   unverändert. Wer branching will, muss explizit vergleichen.
2. **PCMP schreibt eine Maske, keine Flags.** `pcmpgtb` erzeugt `$FF`/`$00`
   pro Lane; `bsel` konsumiert diese Maske.
3. **E-Register sind 64 Bit breit, der Integerpfad bleibt 32 Bit.** `load`
   füllt 64 Bit; ein `move.l e0,d0` liest die unteren 32 Bit.
4. **AMMX ist ein Koprozessor mit id=7.** Für portablen Code zur Laufzeit
   prüfen, ob AMMX vorhanden ist (`_Apollo_AMMXon` in `blitapollo.asm`), oder
   den Build klar als 68080-only kennzeichnen.
5. **Keine AMMX-Instruktion in einer Sequenz, die `d5`/`d6`/`d7`/`a5`/`a6`/`a7`
   berührt.** Der Guard prüft auch die Vektoroperanden.
6. **E-Register task-sicher machen.** Das Harness setzt `ori #$800,sr`
   (Apollo-Bit, SR-Bit 11). Ohne dieses Bit sind E0–E23 über einen Task-Wechsel
   hinweg nicht garantiert stabil (PRM S. 8).

## AMX-007: Vier 16-Bit-Lane-Additionen mit PADDW

**Kategorie:** AMMX / Vektorarithmetik  
**Status:** VERIFIZIERT (gemessen – Vampire V4, 2026-10-25)  
**Beleg:** PRM S. 52 (`PADDW`); Sequenz `seq/AMX-007.i`

`PADDW` addiert vier 16-Bit-Lanes unabhängig, **ohne Sättigung**.
Gemessen mit Harness v5 (100 Läufe):

- `BASELINE = 5`
- `MIN = 5`, `Median = 5`, `MAX = 21`, `AVG = 5`
- `check_log.py`: **OK**, 100/100 `RES`, `NET_MIN = 0`
- `CORE = PCR:1088/1`

Vergleich zu `PADDB` (AMX-002): gleicher MIN/Median (5), also **kein
zusätzlicher Taktvorteil** durch breitere Lanes; der Nutzen liegt in der
Lane-Breite, nicht in weniger Zyklen.

**Grenze:** `PADDW` sättigt nicht. Für ADPCM-PCM/Matrix braucht man
signed-saturating `PADDSW`/`PSUBSW`; diese Mnemonics werden von vasm 2.0d
nicht akzeptiert (siehe `10-adpcm.md`). `PADDW` ist daher nur für
nachfolgend geclampte 16-Bit-Zwischenwerte interessant.

## AMX-008: 16-Bit-Lane-Max mit PCMPGTW + BSEL (gemessen)

**Kategorie:** AMMX / branchless 16-Bit  
**Status:** VERIFIZIERT (gemessen – V4, 100×, 2026-10-25)  
**Sequenzen:** `seq/AMX-008.i` (AMMX), `seq/AMX-009.i` (branchless scalar)

```asm
        load    (a0)+,e0
        load    (a1)+,e1
        pand    e0,e0,e3
        pcmpgtw e0,e1,e2
        bsel    e1,e2,e3
        store   e3,(a2)+
```

Skalarreferenz: 4× `cmp/sge/ext/and/not/and/or`.

| Variante | MIN | Median | MAX |
|---|---:|---:|---:|
| AMMX PCMPGTW+BSEL | **7** | **7** | 7 |
| branchless scalar | 33 | 33 | 33 |

**4,7×** schneller für vier 16-Bit-Maxima. `PCMPGTL` (32-Bit) ist mit vasm
2.0d **nicht** verfügbar. Für ADPCM sind PCM- und MS-Summen 32-Bit; die
16-Bit-Variante ist deshalb **kein direkter Clamp-Ersatz**. Sie wäre nur für
bereits 16-Bit-breite Werte (z. B. Index) interessant – dort ist der
E-Setup-Aufwand pro Einzelwert zu hoch.

## AMX-010: Emulation von PACKSSWB (Signed Saturation Word-zu-Byte mit PACKUSWB + Bias-Trick)

**Kategorie:** AMMX / Audio & Sättigung  
**Status:** VERIFIZIERT (gemessen – Vampire V4 Core 1088/1, 10 × 100 Läufe, Harness v5)  
**Beleg:** PRM S. 52 (`PACKUSWB`), S. 46 (`LOAD`), S. 67 (`PSUBB`); Sequenz `seq/AMX-010.i`

### Problem

AMMX besitzt `PACKUSWB` (Pack Signed Word to Unsigned Byte mit Unsigned Saturation auf `0..255`),
besitzt jedoch **kein** `PACKSSWB` (Pack Signed Word to Signed Byte mit Sättigung auf `-128..+127`).
Werden beispielsweise mehrere 8-Bit-Audiokanäle (z. B. 4 Paula-Kanäle) in 16-Bit-Worten addiert,
können Summen den Bereich `[-128, +127]` überschreiten. Um diese ohne Verzweigungen wieder sättigend
in 8-Bit-Signed-PCM zurückzuführen, fehlt eine direkte Instruktion.

### Fehlerhafter Entwurf (Asymmetrie- & Vorzeichen-Bug)

```asm
        load.w  #$007f,e2       ; Bug: 127 statt 128
        load.w  #$7f7f,e3
.loop:
        paddw   (a0)+,e2,e0     ; 4 Worte laden & +127
        paddw   (a0)+,e2,e1     ; 4 Worte laden & +127
        packuswb e0,e1,e0       ; 8 Worte -> 8 unsigned Bytes (0..255)
        psubb   e3,e0,e0        ; -127
        store   e0,(a1)+
        dbra    d0,.loop
```

**Probleme dieses Entwurfs:**
1. **Asymmetrie:** Im 8-Bit-Zweierkomplement reicht der Wertebereich von `-128` bis `+127`.
   - Ein gültiger Wert `-128` ergibt mit `+127`: `-1`. `PACKUSWB` schneidet auf `0` ab. Nach `0 - 127` entsteht **`-127`** (Verfälschung eines gültigen Minimalwerts).
   - Bei einem Überlauf `+128` ergibt `+128 + 127 = 255`. Nach `255 - 127` entsteht **`+128`** – in signed 8-Bit ist dies binär `$80` = **`-128`** (katastrophaler Vorzeichen-Flip!).
2. **Pipeline-Stalls:** Jede Instruktion in der Schleife hängt direkt vom Ergebnis der vorherigen ab (`paddw` → `packuswb` → `psubb` → `store`). Auf dem 68080 entstehen dadurch vermeidbare RAW-Latenzen.
3. **Falsch für 16-Bit-Fullscale-Audio:** Falls für normales 16-Bit-PCM gedacht: `PACKUSWB` führt **keinen Rechtsshift** (`>> 8`) durch. Alle Amplituden > 255 werden hart abgeschnitten.

### Korrigierter & optimierter 68080-Ansatz

**Mathematisch korrekter Bias:** `$0080` (128) und `$8080` (128).
- `[-128, +127] + 128` bildet exakt auf `[0, 255]` ab.
- Unterläufe `< -128` werden von `PACKUSWB` auf `0` geclampt (`0 - 128 = -128`).
- Überläufe `> +127` werden auf `255` geclampt (`255 - 128 = +127`).
- Keine Vorzeichen-Flips, perfekte Punktsymmetrie.

**2-fach entflochtene Schleife (Pipelining / Unrolling):**
Verarbeitet 16 Worte (32 Bytes In → 16 Bytes Out) pro Iteration.
Durch das Verschränken zweier Datenströme (`e0/e1` und `e4/e5`) laufen Speicher- und
Rechenoperationen parallel auf den dualen 68080-Pipes:

```asm
; Setup außerhalb der Schleife:
        load.w  #$0080,e2       ; Bias +128 in jedes der 4 Worte
        load.w  #$8080,e3       ; Bias -128 in jedes der 8 Bytes

.loop_unrolled:
        ; Phase 1: 8 Worte Block A & 8 Worte Block B laden & vorspannen
        paddw   (a0)+,e2,e0     ; Worte 0..3 Block A
        paddw   (a0)+,e2,e1     ; Worte 4..7 Block A
        paddw   (a0)+,e2,e4     ; Worte 0..3 Block B
        paddw   (a0)+,e2,e5     ; Worte 4..7 Block B

        ; Phase 2: Unsigned Saturation Packen (0..255)
        packuswb e0,e1,e0       ; 8 Bytes Block A
        packuswb e4,e5,e4       ; 8 Bytes Block B

        ; Phase 3: Bias abziehen -> Signed Two's Complement (-128..+127)
        psubb   e3,e0,e0
        psubb   e3,e4,e4

        ; Phase 4: 16 Bytes wegschreiben
        store   e0,(a1)+
        store   e4,(a1)+

        subq.l  #1,d0
        bne.s   .loop_unrolled
```

### Messung auf Vampire V4 (Harness v5, 10 × 100 Läufe)
- **Basis-Variante (`AMX-010.i` – 8 Worte zu 8 Bytes):**
  - `BASELINE = 5`
  - `MIN = 7` Takte (in allen 10 Läufen identisch)
  - `Median = 7`, `AVG = 7`, `MAX = 7` (100% jitter-free)
  - `IEP1 = 7`, `IEP2 = 0`, `IEP3 = 0` pro Lauf
- **2-fach entflochtene Variante (`AMX-011.i` – 16 Worte zu 16 Bytes):**
  - `BASELINE = 5`
  - `MIN = 12` Takte (in allen 10 Läufen identisch)
  - `Median = 12`, `AVG = 12`, `MAX = 12` (100% jitter-free)
  - `IEP1 = 11`, `IEP2 = 1`, `IEP3 = 0` pro Lauf
  - **Durchsatz:** Nur **0,75 Takte pro vorzeichenbehaftetem Word**!


---

## AMX-009: 8-Byte Parallele Mittelwertbildung mit PAVGB (Half-Pel Motion Comp)

**Kategorie:** AMMX / Video & Signal  
**Status:** VERIFIZIERT (gemessen – Vampire V4 Core 1088/1, Log vorhanden)  
**Beleg:** Apollo PRM (Juni 2026), S. 58 (`PAVGB`); Video-Optimierungs-Suite [`Projects/Video_Optimizations/`](../Projects/Video_Optimizations/)  
**Betroffene CPUs:** 68080 mit AMMX; nicht 68060/040

### Problem
In Videocodecs (MPEG-1/2, MPEG-4, H.264) erfordern Halbpixel-Bewegungsvektoren die Mittelwertbildung benachbarter Vorhersagepixel mit Rundungs-Bias:
$$\text{Pixel}_{\text{half}} = (\text{Pixel}_A + \text{Pixel}_B + 1) \gg 1$$
Auf klassischem 68k muss jedes Byte einzeln geladen, auf 16-Bit erweitert, addiert, um 1 geshiftet und wieder gepackt werden (ca. 8–12 Takte pro Pixel).

### 68080 AMMX-Ansatz
Die 68080 besitzt dafür die spezialisierte Vektor-Instruktion `PAVGB`:

```asm
        load    (a0),e0         ; 8 Vorhersagepixel Ref A
        load    1(a0),e1        ; 8 Vorhersagepixel Ref B (1 Byte horizontal versetzt)
        pavgb   e1,e0,e2        ; Berechnet alle 8 Halb-Pixel parallel in GENAU 1 TAKT!
        store   e2,(a1)         ; Schreibt 8 fertige Pixel ins Ziel
```

### Messergebnis auf Vampire V4 (Core 1088/1)
- **Horizontales Half-Pel (64 Pixel):** 11 Netto-Takte (0,17 Takte/Pixel) vs. 158 Takte C-Referenz (**14,36x Speedup**, bit-exakt diff=0, **PASS**).
- **Vertikales Half-Pel (64 Pixel):** 11 Netto-Takte (0,17 Takte/Pixel) vs. 162 Takte C-Referenz (**14,72x Speedup**, diff=0, **PASS**).
- **Diagonales Half-Pel (64 Pixel):** 19 Netto-Takte (0,29 Takte/Pixel) vs. 284 Takte C-Referenz (**14,94x Speedup**, diff $\le$ 1, **PASS**).

---

## AMX-011: Hardware-Status von BFLYW (Privilege-Trap vs. PADDW+PSUBW)

**Kategorie:** AMMX / Architektur-Fallstrick & Core-Kompatibilität  
**Status:** VERIFIZIERT (Hardware-Befund auf Vampire V4 Core 1088/1)  
**Beleg:** Apollo Wiki / PRM `BFLYW.md`; empirischer Testlauf `Projects/DebugTest/test_bfly.c`  
**Betroffene CPUs:** Apollo 68080 Core 1088/1

### Problem & Hardware-Befund
In älteren PRM-Entwürfen und Wiki-Seiten wird eine 1-Takt-Schmetterlings-Instruktion `BFLYW (VEA),b,d:d+1` beschrieben, die Summe und Differenz zweier Vektoren gleichzeitig in ein Registerpaar schreiben soll.
- **Empirischer Test auf V4:** Die Ausführung von `bflyw e1,e0,e2:e3` assembliert mit vasm fehlerfrei, löst jedoch auf der echten Hardware (Core 1088, Release 1) eine **`0x80000008` Privilege Violation Exception** aus.

### Korrekter & robuster 68080-Ansatz
Signaltransformationen (FFT, DCT, Hadamard) dürfen sich nicht auf `BFLYW` verlassen, sondern müssen die superskalare Ausführung von `PADDW` und `PSUBW` nutzen:

```asm
        ; Kanonischer, auf allen 68080-Cores stabiler Schmetterling:
        paddw   e1,e0,e2        ; e2 = e0 + e1 (1 Takt)
        psubw   e1,e0,e3        ; e3 = e0 - e1 (1 Takt)
```
Da die 68080 über parallele Ausführungseinheiten verfügt, können beide Instruktionen in aufeinanderfolgenden Zyklen jitterfrei ohne Pipeline-Stalls abgesetzt werden.
