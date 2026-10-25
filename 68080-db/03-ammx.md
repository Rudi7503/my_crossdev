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
