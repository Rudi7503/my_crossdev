> **Ab 2026-10-30:** Kanonisch ist ausschließlich **Harness v5**. Die alten v1–v4-Daten und Nicht-v5-Artefakte wurden aus dem Arbeitsbestand entfernt. Die historischen Abschnitte unten dokumentieren nur noch die Entwicklung, nicht den aktuellen Messbestand.

# 09 – Mess-Harness: Aufbau, Build und Fallstricke

**Dateien:** [`harness.asm`](harness.asm) (v2-Template),
[`harness-v1.asm`](harness-v1.asm) (archivierte v1-Vorlage),
[`seq/`](seq/) (Sequenzen), [`RUN-ALL`](RUN-ALL),
[`RUN-ALL-ONCE`](RUN-ALL-ONCE), [`tools/build_all.sh`](tools/build_all.sh),
[`tools/check_sequences.py`](tools/check_sequences.py),
[`tools/serial_capture.py`](tools/serial_capture.py),
[`tools/download_logs.sh`](tools/download_logs.sh),
[`tools/repeat_report.py`](tools/repeat_report.py).

**Ziel:** Jeder Eintrag wird mit **demselben** Harness, **derselben**
Registerkonvention und **derselben** Ausgabe gemessen. Die Sequenz wird
inline eingebettet; es gibt keinen `bsr`/`rts`-Overhead und keine
unterschiedliche Startroutine. Damit sind die Einträge methodisch direkt
vergleichbar.

> **42 Läufe vorhanden (Harness v1):** Alle Einträge wurden in einem Batchlauf
> auf einer Vampire V4 gemessen; die Logs liegen unter
> `measurements/<ID>.log`. `FUS-001.run1.log` ist ein separater
> Wiederholungslauf. Der Serialpfad läuft über die direkte UART
> (`SERDAT`/`SERPER`), nicht über `serial.device`.
>
> **Harness v4 (gemessen):** Warmup und Messung rufen dieselbe
> `measure_100`-Subroutine auf (gleiche Codeadresse), inklusive `a5`-Zeiger
> und Store. Die 100 Warmup-Werte werden überschrieben. **Kein stdout**
> mehr; nur Dateilog + UART. v4 wurde am 23.10. 10× gemessen: 740 Serial-
> und 740 Dateilogs, inhaltlich identisch, MIN 10/10 stabil, **Warmup
> wirkt** (erste 10 Läufe = MIN; v2: 100 % Kaltstart-Anteil). Der Dateilog
> geht absolut nach `Ram Disk:LOGS/<ID>.log`; `FILESTAT;1;0` bestätigt ihn.
> Befunde: `measurements/v4/REPORT.md`.
>
> **Harness v5 (Quelle, nicht gemessen):** v4 plus separater Zählerdurchgang
> (`IEP1/2/3`, `BPC/BPW`, `DCH/DCM`) **nach** der Taktmessung; siehe
> `harness-v5.asm` und §12.1. **v5 ist kanonisch**; 740/740 Dateilogs, `PERF`, MIN = v4. Bericht:
> `measurements/V5-REPORT.md`.
> `harness-v1.asm` ist die archivierte v1-Vorlage.

## 1. Ablauf

```text
_start
  ├─ Register retten (movem.l d0-d7/a0-a6)
  ├─ exec_base = $4
  ├─ Apollo-Bit in SR setzen (E-Register als Task-Zustand)
  ├─ Scratch-Register/-Zeiger deterministisch setzen
  ├─ Forbid()
  ├─ Warmup: bsr measure_100 -> cycle_buffer+4 (100 Läufe)
  │    └─ dieselbe Codeadresse wie die spätere Messung
  ├─ init_scratch: Scratch-Register/-Zeiger zurücksetzen
  ├─ Baseline: MOVEC CCC,d5; MOVEC CCC,d6; sub.l d5,d6 -> cycle_buffer[0]
  ├─ bsr measure_100 (dieselbe Subroutine, 100 Messläufe)
  │    └─ 10× unrolled: MOVEC, Sequenz, MOVEC, Sub, Store
  ├─ Permit()
  ├─ calc_stats über cycle_buffer[1..100] (MIN, MAX, SUM, AVG)
  ├─ open_output (dos.library-Dateilog + direkte UART, kein stdout)
  ├─ print_all (Datei + seriell, identische Zeilen)
  ├─ close_output
  └─ Register zurück, rts
```

### Warum die Reihenfolge so ist

- **Apollo-Bit vor dem Warmup:** `ori #$800,sr` markiert die E-Register als
  Task-Zustand. Das PRM erlaubt dieses Bit im User-Mode; ApolloOS/gepatchte
  Systeme retten die E-Register dann bei Task-Wechseln. Ohne das Bit können
  AMMX-Sequenzen nach einem Task-Wechsel andere E-Werte sehen.
- **`Forbid()` vor dem Warmup:** Der Warmup läuft unter denselben Bedingungen
  wie die Messung (kein Task-Wechsel, Interrupts bleiben aktiv).
- **Warmup = dieselbe Subroutine:** 100 Läufe mit `a5`-Zeiger, `MOVEC`,
  Sequenz, Delta und Store in `measure_100`. Weil Warmup und Messung
  dieselbe Codeadresse aufrufen, ist auch der Messcode selbst warm; die
  Warmup-Werte werden nicht ausgewertet.
- **`init_scratch` nach dem Warmup:** Setzt `d0`–`d4` und `a0`–`a4` auf den
  dokumentierten Startzustand zurück. Die 100 Messläufe beginnen dadurch an
  denselben (jetzt warmen) Adressen und laufen nicht in einen Pufferüberlauf.
- **`Forbid()` vor der Baseline:** Kein Task-Wechsel während der Messung.
  Interrupts bleiben aktiv (siehe Fallstrick 2).
- **`Permit()` vor der Ausgabe:** DOS-Ausgabe und UART werden erst nach
  `Permit()` bedient. Die direkte UART wäre auch unter `Forbid()` schreibbar,
  aber die Ausgabe soll grundsätzlich nicht in den Messblock fallen.
- **Statistik nach `Permit()`:** `calc_stats` und die Division sind nicht Teil
  der Messung.

## 2. Der Zykluszähler: CCC, nicht `$809`, nicht `CYCLE_CNT_REG`

Das Apollo-PRM listet auf **S. 29**:

```text
$809  CCC  Clock Cycle Counter
```

Die entscheidende Assembler-Details:

```asm
        movec.l CCC,d5      ; richtig (vasm 2.0d/1.8c)
```

**Nicht** funktionieren mit vasm 2.0d:

```asm
        movec.l $809,d5             ; error: illegal operand types
CYCLE_CNT_REG equ $809
        movec.l CYCLE_CNT_REG,d5    ; error: illegal operand types
```

Vasm erwartet den symbolischen Kontrollregisternamen `CCC`; ein EQU-Alias
wird an dieser Stelle nicht als Kontrollregister erkannt. Das ursprüngliche
Template mit `CYCLE_CNT_REG equ ???` wurde deshalb bewusst **nicht**
übernommen. Der Name `CCC` steht direkt im Code; die Zahl `$809` ist nur
Dokumentation.

**Lesen im User-Mode:** Das PRM sagt, dass das Lesen eines Kontrollregisters
auf dem 68080 im User-Mode erlaubt ist („getting info is allowed on 080 in
user mode“). Das Schreiben bleibt Supervisor-only. Das Harness liest CCC nur.

**Breite:** Das PRM dokumentiert die Bitbreite von CCC nicht. Das Harness
subtrahiert 32-Bit-Werte. Bei diesen kurzen Sequenzen ist ein Überlauf über
2³² Takte ausgeschlossen; formal belegt ist die Breite nicht.

## 3. Register-Konvention

In der Messschleife reserviert; die Sequenz **darf sie nicht berühren**:

| Register | Rolle |
|---|---|
| `d5` | MOVEC-Startwert |
| `d6` | MOVEC-Stoppwert und Delta |
| `d7` | äußerer Schleifenzähler (10) |
| `a5` | Schreibzeiger in `cycle_buffer` |
| `a6` | `exec_base` |
| `a7` | Stack |

Frei für die Sequenz:

| Register | Rolle |
|---|---|
| `d0`–`d4` | Datum-Scratch |
| `a0`–`a4` | Adress-Scratch; zeigen auf 4-KB-Puffer |
| `e0`–`e23` | AMMX-Scratch (nur AMMX/E-fähige Befehle); Apollo-Bit wird gesetzt |

Jede `seq/<ID>.i` beginnt mit:

```asm
; Uses: <Liste>
; MUST NOT touch: d5, d6, d7, a5, a6, a7
```

`tools/check_sequences.py` entfernt Kommentare und lehnt jede Sequenz ab, die
eines der reservierten Register im Code nennt. Damit ist ein
„Register-Konflikt“ ein Build-Fehler, kein stiller Messfehler.

### Warum keine Labels in Sequenzen?

Die Sequenz steht **zehnmal** in der gemeinsamen `measure_100`-Subroutine.
Warmup und Messung rufen **dieselbe Codeadresse** auf; sie wird **200×**
ausgeführt (2 × 100). Ein Label würde beim zweiten Aufruf nicht neu
definiert, wäre aber im gemessenen Fragment trotzdem tabu. Der Guard
verbietet deshalb Labels, `include`, `section`, `rts`, `bra`, `bsr`, `jsr`,
`jmp` und `dbra` in Sequenzen. Schleifen gehören in den umgebenden
Pattern-Code, nicht in das gemessene Fragment.

## 4. Datenabhängigkeit und Startzustand

Der Harness setzt `d0`–`d4` und `a0`–`a4` **einmal vor dem Warmup**:

```asm
d0=1, d1=2, d2=3, d3=4, d4=5
a0=scratch_a, a1=scratch_b, a2=scratch_c, a3=scratch_d, a4=scratch_e
```

Danach werden die Register **nicht** pro Lauf zurückgesetzt. Das ist Absicht:
Eine Sequenz mit Datenabhängigkeit (Division, Multiplikation, Cache-Misses,
Zeigerläufe) soll ihre Abhängigkeit im Messblock dokumentieren, nicht durch
eine versteckte Initialisierung pro Lauf kaschieren.

**v2-Warmup und Reset:** Der Warmup führt 100 volle Messläufe aus und verändert
dabei `d0`–`d4` und `a0`–`a4`. Direkt danach ruft der Harness `init_scratch`
erneut auf und setzt Register und Zeiger auf den dokumentierten Startzustand
zurück. Die 100 Messläufe beginnen dadurch an denselben Adressen – jetzt warm,
aber ohne Pufferüberlauf. Die **Speicherinhalte** der Scratch-Puffer bleiben
vom Warmup beeinflusst; das ist eine dokumentierte Datenabhängigkeit, keine
Nullinitialisierung.

Konsequenzen:

- Reine Register-Operationen laufen nach dem ersten Lauf mit anderen Werten
  weiter. Bei Addition/Multiplikation ist die Laufzeit davon meist unabhängig;
  bei Division durch wechselnde Werte nicht.
- Zeigerläufe (`(a0)+`) verbrauchen die 4-KB-Puffer. 100 Warmup-Läufe mit
  maximal 32 Byte/Lauf sind 3200 Byte; danach werden die Zeiger
  zurückgesetzt, die 100 Messläufe nutzen erneut maximal 3200 Byte. Das passt
  in 4 KB. Bei größeren Schritten muss der Puffer größer sein oder die Sequenz
  muss den Zeiger selbst zurücksetzen (was mitgemessen würde).
- Eine notwendige Init-Sequenz gehört in die Sequenz und wird mitgemessen.
  Sie muss im Eintrag ausdrücklich genannt werden.

## 5. Baseline und Netto-Wert

`cycle_buffer[0]` enthält:

```asm
        movec.l CCC,d5
        movec.l CCC,d6
        sub.l   d5,d6
        move.l  d6,cycle_buffer+0
```

Das ist der Overhead von zwei MOVEC-Reads, der Subtraktion und dem Store,
gemessen **einmal**. In den **v1-Logs** wurde sie nach dem 3×-Sequenz-Warmup,
aber vor der Messschleife gemessen und war daher kalt; in **v2** wird sie nach
dem 100-Lauf-Warmup gemessen und ist warm, aber weiterhin einmalig. Die 100
Messläufe enthalten dieselben zwei MOVEC-Reads plus Sequenz. Ein
**Netto-Wert** wäre naiv:

```text
NET ≈ RES - BASELINE
```

**Beim ersten v1-Lauf (FUS-001) ist dieser Abzug ungültig:** `BASELINE = 5`,
`MIN = 3`, also `NET = -2`. Die kalte Baseline ist nicht mit dem stationären
Messloop vergleichbar; außerdem überlappen die MOVEC-Reads mit umgebenden
Instruktionen. Die Baseline ist daher nur eine Orientierung und **kein**
blinder Abzug.

Das Harness gibt `BASELINE`, `RES` und `SUMMARY` aus; die Differenz kann im
Log nachgerechnet werden. v2 misst die Baseline nach dem Warmup; sie bleibt
aber eine Einzelmessung. Eine Mehrfachmessung mit Median ist weiterhin offen.

Die Baseline geht **nicht** in MIN/MAX/AVG ein. `calc_stats` liest ab
`cycle_buffer+4`.

## 6. MIN, MAX, AVG

`calc_stats` läuft über die 100 Werte `cycle_buffer[1..100]`:

Die aktuellen 42 Logs sind **v4** (10 × 100 Läufe). Die Beispiele beziehen
sich auf einen Lauf; in `SUMMARY.md` stehen die Mediane der 10 Läufe.

- `MIN` – **Referenzwert** für den Eintrag; in v4 10/10 identisch.
- `MAX` – Ausreißer durch Interrupts, Busarbitrierung, Cache-Effekte;
  in `SUMMARY.md` der Median der 10 Läufe.
- `SUM` – 32-Bit-Summe.
- `AVG` – `SUM / 100` über `DIVU.W #100`, also **ganzzahlig abgeschnitten**;
  in `SUMMARY.md` der Median der 10 Läufe.
  Beim v1-Wiederholungslauf `FUS-001.run1` ist `AVG = 3`, der arithmetische
  Mittelwert aber 3,8.

**Warum MIN?** `Forbid()` verhindert Task-Wechsel, aber **nicht** Interrupts.
Ein Interrupt während eines Laufs verlängert genau diesen Lauf; MIN ist der
Lauf ohne solchen Eingriff. AVG ist nur informativ.

**AVG-Grenze:** `DIVU.W #100` berechnet einen 16-Bit-Quotienten. Das ist sicher,
solange der Durchschnitt unter 65536 Takten liegt. Die Korpus-Sequenzen sind
weit darunter. Für beliebige Langläufe müsste man eine 32-Bit-Division
implementieren.

**Streuung innerhalb eines Laufs:** `measurements/SUMMARY.md` ergänzt aus den
100 `RES`-Werten der **v1-Logs** den **Median** (50. Wert), die **Stdabw.**
(Stichprobe, n−1) und **p95** (Nearest-Rank). Im kanonischen v5-Lauf ist MIN bei **74/74**
Einträgen über alle 10 Läufe identisch; die erste-10-Kaltstartphase ist
weg. Die Streuung enthält noch Interrupt-Ausreißer, ist also kein CPU-Jitter
und sagt nichts über die Streuung zwischen Boots aus.

## 7. Ausgabeformat

Jede Zeile wird zuerst in `line_buf` gebaut und dann identisch an die Datei und die
Datei `LOGS/<ID>.log` und die direkte UART geschickt:

```text
BEGIN;<ID>
FILESTAT;<1/0>;<IoErr>             ← v3/v4; 1 = Dateilog offen
CORE;<PCR-ID>;<PCR-REV>;<VCoreRev>   ← optional bei v1-Logs
BASELINE;<Takte>
RES;<Lauf>;<Takte>          ← 100 Zeilen, Lauf 1..100
SUMMARY;<MIN>;<MAX>;<AVG>
PERF;100;CCC;<v>;IEP1;<v>;IEP2;<v>;IEP3;<v>;BPC;<v>;BPW;<v>;DCH;<v>;DCM;<v>  ← v5, optional
END;<ID>
```

Beispiel (Werte sind **Platzhalter**, keine Messung):

```text
BEGIN;FUS-001
FILESTAT;1;0
CORE;1088;0;7389
BASELINE;0
RES;1;1
...
RES;100;1
SUMMARY;1;1;1
END;FUS-001
```

Die `CORE`-Zeile kommt aus dem Harness selbst:

- `PCR-ID` ist dezimal `1088` (`$0440` = AC68080), gelesen mit `movec.l PCR`.
- `PCR-REV` ist die Revision aus PCR (laut PRM `0`=V2, `1`=V4).
- `VCoreRev` ist die exakte Flash-Core-Revision aus `ENV:VCoreRev`, wenn
  `VControl SETENV` gelaufen ist (z. B. `7389`); sonst bleibt das Feld leer.

`check_log.py` akzeptiert die `CORE`-Zeile optional, damit die v1-Logs
weiterhin gültig bleiben. Die tatsächlichen Werte entstehen erst auf der
Vampire. Ein Log ohne `BEGIN`/`END` oder mit weniger als 100 `RES`-Zeilen ist
ungültig; `tools/serial_capture.py` legt es deshalb als `.log.part` ab und
benennt es erst nach `END` in `.runNN.log` um.

### Dateilog auf der V4

`open_output` öffnet nach `Permit()` zusätzlich
`Ram Disk:LOGS/<ID>.log` mit `MODE_NEWFILE` (absoluter Pfad, unabhängig
vom aktuellen Verzeichnis). Wichtig: `OpenLibrary` nimmt den **Name in
`a1`** und die **Version in `d0`** – nicht `d0`/`d1`; das war die Ursache
für `dos_base=0` in v3. `FILESTAT;1;0` bestätigt den offenen Dateilog;
bei `FILESTAT;0;<IoErr>` ist `Open` fehlgeschlagen. Datei, UART und die
Messung laufen in beiden Fällen weiter. Die Datei wird nach der Messung
geschrieben und beeinflusst die Taktwerte nicht.

`RUN-ALL` benennt die Datei nach jedem Lauf in `<ID>.runNN.log` um. Der
PC-Mitschnitt verwendet dieselbe Namensform, sodass beide Kanäle direkt
verglichen werden können. Die v1-Logs bleiben unter `measurements/<ID>.log`.

### stdout (deaktiviert)

Seit v4 wird `dos.library/Output()` **nicht mehr** aufgerufen; `stdout_fh`
bleibt 0. Die Shell-Ausgabe war beim 10×-Lauf der langsamste Teil. Die
Zeilen gehen nur noch in den Dateilog und über UART; `print_all` baut sie
unverändert in `line_buf`.

### Direkte UART (SERDAT/SERPER)

Der auf der getesteten V4 angeschlossene USB-Serial-Adapter hängt am
**Amiga-UART**, nicht an einem funktionierenden `serial.device`-Port. Der
funktionierende Pfad ist derselbe wie in
`Projects/ApolloLib/ApolloCrossDev_Debug.c`:

```text
SERDAT   = $dff030
SERDATR  = $dff018
SERPER   = $dff032
INTREQ   = $dff09c
CIAB_PRA = $bfd000
CIAB_DDRA= $bfd200
```

Initialisierung:

```asm
        move.b  #$c0,CIAB_DDRA
        clr.b   CIAB_PRA
        move.w  #30,SERPER          ; SERPER_BAUD(3546895,115200)
```

Zeichenausgabe:

```asm
        move.w  SERDATR,d1
        btst    #13,d1              ; TBE
        beq.s   warten
        move.w  #1,INTREQ
        move.w  #$0100|char,SERDAT  ; STP8 | Datenbyte
```

- 115200 8N1, `SERPER = 30` (PAL-Basis 3546895 Hz).
- LF wird als CR/LF ausgegeben.
- Vor dem Verlassen wird auf `TSRE` (Bit 12) gewartet.
- Es wird **kein** `serial.device`, kein IOExtSer und kein `DoIO` benötigt.

**Empirischer Beleg:** Der Diagnose-Lauf `SERIAL-PROBE` lieferte auf COM9
exakt `SERIALPROBE;BEGIN`, `SERIALPROBE;HELLO-FROM-V4`, `SERIALPROBE;END`.
Der vorherige `serial.device`-Pfad lieferte keine Ausgabe.

## 8. Bauen

```bash
cd 68080-db

# v5 getrennt bauen; v1–v4 bleiben unangetastet
HARNESS=harness-v5.asm BUILD_DIR=build-v5 BIN_DIR=bin-v5 ./tools/build_all.sh
```

Ohne `BUILD_DIR`/`BIN_DIR` schreibt das Skript nach `build/` und `bin/`
und überschreibt damit die v1-Artefakte. Für die v1-Referenz daher
entweder die Env-Variablen verwenden oder vorher sichern:

```bash
cp -a build build-v1
cp -a bin bin-v1
```

Das Skript:

1. findet `vasmm68k_mot` (ENV `VASM`, `PATH` oder bekannte lokale Pfade),
2. prüft mit einem Mini-Programm, ob `-m68080` und `MOVEC CCC` akzeptiert
   werden,
3. validiert alle Sequenzen mit `tools/check_sequences.py`,
4. ersetzt alle `; @@INCLUDE_SEQ@@` (zehnmal in `measure_100` plus einmal
   in `perf_one_run`) durch `include "seq/<ID>.i"` und `@@ID@@` durch die ID,
5. baut `<BIN_DIR>/<ID>` (Default `bin/<ID>`) mit `-Fhunkexe -m68080`.

Getestet mit:

- vasm 2.0d (M68k-Backend 2.8, Motorola-Syntax 3.19d),
- vasm 2.0f (M68k-Backend 2.8c) für v2, und
- vasm 1.8c (M68k-Backend 2.3b, Motorola-Syntax 3.11c) für v1.

Beide erzeugen ein Hunk-Executable. Der Befehl im Header von `harness.asm`
ist der manuelle Einzelbuild:

```bash
vasmm68k_mot -Fhunkexe -m68080 -o bin-v5/FUS-001 build-v5/FUS-001.asm
```

## 9. Batchlauf auf der V4

Für den kompletten Korpus liegt ein AmigaDOS-Script bereit:

```text
Ram Disk:RUN-ALL
```

Es wechselt nach `Ram Disk:`, legt `LOGS/` an, löscht alte `LOGS/*.log` und
ruft **jedes der 71 Programme 10×** auf (740 Läufe). Nach jedem Lauf wird
`LOGS/<ID>.log` in `LOGS/<ID>.runNN.log` umbenannt. Vorher wird – falls
vorhanden – `C:VControl SETENV` ausgeführt, damit die `CORE`-Zeile die exakte
Flash-Core-Revision enthält; `VControl CORE` schreibt zusätzlich die
vollständige Core-Zeichenkette nach `LOGS/CORE.txt`.

```text
Execute "Ram Disk:RUN-ALL"
```

Auf dem PC wird parallel mitgeschnitten; `serial_capture.py` erzeugt pro
`BEGIN`/`END` eine Datei `measurements/v2/<ID>.runNN.log`. Die V4-Dateilogs
werden anschließend mit `tools/download_logs.sh` heruntergeladen und mit dem
Serial-Mitschnitt verglichen:

```bash
AMIGAHOST=apollo ./tools/download_logs.sh measurements/v2-v4
python3 tools/repeat_report.py measurements/v2-v4 --compare measurements/v2
```

Nach `RUN-ALL DONE` (Bildschirm) alle Logs mit `tools/check_log.py` prüfen.
Der Lauf dauert etwa ein bis zwei Minuten; 740 Logs belegen rund 0,5 MB auf
der Ram Disk. **Vor einem erneuten Lauf die Logs herunterladen**, weil
`RUN-ALL` alte `LOGS/*.log` löscht. Für einen schnellen Einzellauf gibt es
`RUN-ALL-ONCE` (42 Läufe, ohne Wiederholungen).

## 10. Auf der Vampire ausführen

1. `bin-v2/<ID>` per ACP auf die Vampire kopieren (v2; `bin/<ID>` ist v1):
   ```bash
   cd /home/papa/Vampire_V4/ApolloCrossDev/68080-db
   ../Projects/ApolloExplorer/acp/acp bin-v2/FUS-001 "$AMIGAHOST:Ram Disk/"
   ```
2. Auf der V4 von einer Shell starten: `Ram Disk:FUS-001`.
3. Die Ausgabe erscheint im Shell-Fenster, in `LOGS/<ID>.log` und parallel auf
   der direkten UART (`SERDAT`/`SERPER`).
4. Auf dem PC mitschneiden. Linux:
   ```bash
   python3 -m pip install -r tools/requirements.txt
   python3 tools/serial_capture.py --port /dev/ttyUSB0 --baud 115200 --out measurements/v2
   ```
   Windows (getesteter Fall, CH340 an COM9):
   ```powershell
   python -m pip install -r tools/requirements.txt
   python tools/serial_capture.py --port COM9 --baud 115200 --out measurements/v2
   ```
5. Log prüfen: `BEGIN`, optional `CORE`, 100×`RES`, `SUMMARY`, `END`. Danach
   Status im Eintrag auf `VERIFIZIERT (gemessen)` setzen und die
   `SUMMARY`-Werte eintragen.
6. V4-Dateilogs herunterladen und mit dem Serial-Mitschnitt vergleichen:
   ```bash
   AMIGAHOST=apollo ./tools/download_logs.sh measurements/v2-v4
   python3 tools/repeat_report.py measurements/v2-v4 --compare measurements/v2
   ```

**v1/v2:** Die vorhandenen Logs sind **v1**. Für v2 neue Dateien verwenden
(`measurements/v2/<ID>.runNN.log`) und die v1-Logs nicht überschreiben. Der
100-Lauf-Warmup erscheint nicht im Log – seine Werte werden von den
Messwerten überschrieben; `BEGIN`/`CORE`/`RES`/`SUMMARY`/`END` bleiben die
einzigen Zeilen.

**CORE-Zeile:** PCR-ID/Revision kommen direkt aus dem 68080; die exakte
Flash-Revision steht in `ENV:VCoreRev`, wenn `VControl SETENV` gelaufen ist.
Ohne VControl bleibt das letzte Feld leer, die Messung läuft weiter.

**Hinweis:** Alle 42 v1-Logs wurden auf COM9 mitgeschnitten. Der direkte
UART-Pfad funktioniert; `serial.device` war auf dieser V4 nicht die richtige
Gegenstelle.

## 11. Fallstricke

### 11.1 Falscher Zähler / falsche Syntax

Ohne korrektes `CCC` ist jede Messung wertlos. `$809` und ein EQU-Alias
scheitern in vasm. Ersatz-Zeitgeber, falls CCC auf einer Core-Version nicht
verfügbar ist: CIA-Timer (z. B. CIA-B Timer) oder ein anderes dokumentiertes
Zählregister. Solche Läufe sind **nicht** mit CCC-Läufen vergleichbar und
müssen separat gekennzeichnet werden.

### 11.2 `Forbid()` schaltet keine Interrupts ab

`Forbid()` verhindert Task-Wechsel, nicht Interrupts. MIN ist der
Referenzwert; AVG und MAX können durch Interrupts verfälscht sein. Wer
Interrupts wirklich ausschalten will, braucht `Disable()` und muss die
Konsequenzen (verpasste Serial-/Disk-Interrupts) beherrschen. Das Harness
macht das bewusst nicht.

### 11.3 Ausgabe erst nach `Permit()`

Die direkte UART ist mit Registerzugriffen auch unter `Forbid()` schreibbar.
Trotzdem bleibt die Reihenfolge Messung → `Permit()` → `open_output` →
`print_all`: Ausgabe soll nicht in den Messblock fallen, und DOS-/Device-I/O
kann blockieren. Der frühere `serial.device/DoIO`-Pfad ist verworfen.

### 11.4 vasm muss 68080/AMMX können

Nicht jedes `vasmm68k_mot`-Binary kennt `-m68080`. Das Build-Skript prüft das
mit einem Probe-Build und bricht sonst ab. Ein 68k-Assembler, der `-m68080`
nur ignoriert, ist unbrauchbar.

### 11.5 Sequenz inline, kein `bsr`/`rts`

Die Sequenz wird per `include` mitten in den Messblock eingesetzt. Dadurch
gibt es keinen Call-Overhead und keine unterschiedliche Registerrettung.
Sequenzen dürfen deshalb keine Labels, Sections oder Kontrollfluss-
Instruktionen enthalten (siehe Guard).

### 11.6 Innere Schleife ist unrolled

Zehn Messblöcke pro äußerer Iteration, zehn äußere Iterationen = 100 Läufe.
Der Sinn ist, den `dbra`-Overhead nur einmal pro zehn Läufe zu zahlen. Die
Sequenz selbst wird **nicht** vom Compiler/Assembler optimiert; sie bleibt
byte-genau.

### 11.7 Baseline geht nicht in die Statistik ein

`cycle_buffer[0]` ist die Baseline; `calc_stats` startet bei
`cycle_buffer+4`. Wer die Baseline mitzählt, bekommt 101 Werte und einen
falschen MIN/AVG.

### 11.8 `TOUCH (An)` und vasm

Vasm 2.0d akzeptiert `touch (a0)` nicht, aber `touch (a0,d0.w)`. Die
indexierte Form wird verwendet; `d0=0` hält die effektive Adresse bei `a0`.
Das ist ein Assembler-Detail, keine Hardware-Einschränkung.

### 11.9 `MOVE16` ist nicht automatisch schneller

`MOVE16=4` Takte; zwei fused 64-Bit-Moves können rechnerisch zwei Takte
brauchen. `MOVE16` ist trotzdem nützlich: ein Befehl, 16 Byte, keine
Alignmentschranke auf dem 68080. Nicht pauschal als „schnellster Kopierer“
annehmen.

### 11.10 `clr.q`/`move.q`/3-Operanden sind keine portablen Quelltextformen

- `clr.q` ist LineA und laut PRM nur auf bestimmten Scores verfügbar.
- `move.q` wird von vasm 2.0d nicht als Opcode akzeptiert.
- `add.l d0,d1,d2` wird von vasm 2.0d abgelehnt.

Die internen Fusionen entstehen durch benachbarte 2-Operanden-Paare, nicht
durch neue Mnemonics.

### 11.11 E-Register brauchen das Apollo-Bit

Das Harness setzt zu Beginn `ori #$800,sr` (SR-Bit 11). Laut PRM S. 8 dürfen
Programme dieses Bit im User-Mode setzen; es sorgt dafür, dass die
E-Register beim Task-Wechsel als Teil des Task-Zustands behandelt werden.
Ohne das Bit sind AMMX-E-Register über einen Task-Wechsel hinweg nicht
garantiert stabil. Auf einem System, das das Bit nicht unterstützt, würde
die Instruktion eine Exception auslösen – dann ist der Harness auf diesem
OS/Core nicht lauffähig.

### 11.12 Cache-Warmup ist nur ein Warmup

v2 wärmt 100 volle Messläufe vor (Code, Store-Pfad, `cycle_buffer` und die
Scratch-Adressen bis maximal 3200 Byte). Das ersetzt keine systematische
Vorbereitung des gesamten 128-KB-Dcache und keine Kontrolle über Interrupts
oder Busarbitrierung. Für Memory-Einträge muss der Arbeitsbereich im Eintrag
dokumentiert sein.

### 11.13 Keine erfundenen Messwerte

Wenn kein Lauf vorliegt, bleibt der Status `UNVERIFIZIERT`. Ein plausibler
Taktwert ist kein Messwert. Die Datenbank trennt deshalb konsequent
`Messergebnis` (leer) und `Literaturwert` (PRM/Wiki).

## 12. Neuen Eintrag hinzufügen

1. ID nach Schema `KAT-###` wählen (`FUS`, `BND`, `AMX`, `REG`, `TIM`, `MEM`,
   `PAT`, `ANTI`).
2. `seq/<ID>.i` anlegen; Kopf mit `Uses:` und `MUST NOT touch:`.
3. Nur `d0`–`d4`, `a0`–`a4` und AMMX-E-Register verwenden; keine Labels,
   keine Kontrollfluss-Instruktionen.
4. Eintrag in der passenden Kategorie nach dem Schema schreiben:
   Problem, naiver Ansatz, 68080-Ansatz, Mechanismus, Messung, Quellen.
   Am Ende den Block **Gemessene Sequenz** mit dem verbatim Inhalt von
   `seq/<ID>.i` einfügen.
5. `./tools/build_all.sh` laufen lassen; der Guard und vasm müssen grün sein.
6. Auf Hardware messen, Log als **v2** ablegen (v1 nicht überschreiben),
   Status und `SUMMARY` für v2 nachtragen.
7. `index.md` ergänzen.

## 12.1 Performance-Zähler (v5)

`harness-v5.asm` ergänzt v4 um einen **separaten Zählerdurchgang nach der
Taktmessung**. Die Taktmessung selbst bleibt unverändert; kein Zähler wird
zwischen `MOVEC CCC,d5` und `MOVEC CCC,d6` gelesen.

Ablauf:

1. `measure_100` misst wie in v4 die 100 Taktwerte; sie liegen bereits im
   `cycle_buffer`.
2. Danach `Disable()` → `perf_measure` → `Enable()`.
3. `perf_measure` führt zuerst 100 **leere** Läufe (`perf_baseline`) und
   dann 100 Läufe mit **einer** Sequenzkopie (`perf_one_run`) aus.
   `perf_one_run` liest `MOVEC CCC,d5` **direkt vor** und `MOVEC CCC,d6`
   **direkt nach** der Sequenz; `d5`/`d6` sind reserviert. Die sieben
   übrigen Zähler (`read_perf_no_ccc`) werden davor/danach gelesen, also
   außerhalb des CCC-Fensters. Deren Differenz „Sequenz − leere Läufe“
   entfernt den Leseoverhead; `CCC` ist die reine Sequenz-Zyklenzahl.
4. `PERF;100;CCC;<…>;IEP1;<…>;…;DCM;<…>` enthält die Summen über 100 Läufe
   (noch nicht durch 100 geteilt). `tools/perf_report.py` rechnet auf
   Pro-Lauf-Werte, Pipe-Anteile, BPW-Quote und D-Cache-Hitrate um.

Wichtig: Die Zähler sind eine **Näherung** (Cache-Interaktion zwischen
Sequenz und Zähler-Lesern, begrenzte Baseline-Subtraktion). Die **Taktwerte
`RES`/`MIN` bleiben die Referenz**; `PERF` ist eine zusätzliche Auswertung.

## 13. Was noch fehlt

- **Harness v4 ist gemessen:** 740 Serial- und 740 Dateilogs, identisch;
  MIN 10/10 stabil, Warmup wirkt, kein stdout, `PERF` bei allen. v5 ist
  kanonisch; v1–v4 bleiben Archiv.
- Streuung **zwischen** Boots; `RUN-ALL` wiederholt innerhalb eines Boots 10×,
  aber es gibt noch keine Serie über mehrere Boots. Die Streuung **innerhalb**
  eines Laufs (Median, Stdabw., p95) steht für v1 in `SUMMARY.md`; für die
  10 Wiederholungen liefert `tools/repeat_report.py` die MIN-Spanne.
- Firmware und CPU-Takt pro Log; PCR-ID/Revision und `VCoreRev` stehen jetzt
  im `CORE`-Feld, Firmware/Takt noch nicht.
- Baseline-Methodik: v2 misst die Baseline nach dem Warmup, aber weiterhin
  einmalig. Eine Mehrfachmessung mit Median ist offen. Die v1-Baseline war
  kalt (5–41 Takte) und bei vielen Einträgen größer als MIN.
- Rest-Kaltstart nach `init_scratch` im ersten v2-Messlauf beobachten.
- CCC-Breite und Überlaufverhalten.
- 68060-Vergleichsmessungen.
- AMMX-Laufzeitdetektion im Harness.
- Ein Parser, der die Streuung über **Boots** vergleicht; `tools/repeat_report.py`
  wertet die 10 Läufe eines Boots aus, `check_log.py` prüft ein einzelnes Log.
