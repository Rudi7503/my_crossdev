# 68080 Best-Practice-Datenbank

Kuratierte, quellenbelegte Optimierungsmuster für den Apollo-Core **68080**
(Vampire V4) – mit einem reproduzierbaren Mess-Harness, das jeden Eintrag mit
vasm baut, unter `Forbid()` 100-mal misst (nach 100 Warmup-Läufen) und die
Rohdaten über stdout und die direkte UART (`SERDAT`/`SERPER`, 115200 8N1)
ausgibt.

> **Status vorab:** **Alle 42 Einträge wurden mit Harness v1 auf einer Vampire
> V4 gemessen** (ein Batchlauf, COM9/115200 8N1, 2026-10-22). Die Logs liegen
> unter `measurements/<ID>.log`; `FUS-001.run1.log` ist ein separater
> Wiederholungslauf. Referenz ist **MIN**; MAX/AVG sind Mediane der 10
> v5-Läufe. Die Baseline ist 5–41 Takte und **kein gültiger Abzug**.
> Die Mechanismen sind aus PRM und Apollo-Wiki belegt.
>
> **Harness v5 (gemessen, kanonisch):** v4-Taktmessung plus separater
> PERF-Zählerdurchgang **nach** der Taktmessung. Die Taktmessung bleibt
> unverändert; jedes Log enthält `PERF;100;CCC;…;IEP1;…;DCM`. v5 wurde
> am 24.10. 10× gemessen: **420 Dateilogs**, alle `FILESTAT;1;0`, MIN
> 10/10 identisch mit v4, Warmup wirkt. Zusätzlich sind die
> **IEP-Pipe-Slots** in Einträgen und `SUMMARY.md` dokumentiert.
> `BPC/BPW/DCH/DCM` sind im gesamten Korpus 0 und werden nicht als
> Cache-/Branch-Auswertung interpretiert. **v5 ist kanonisch**;
> v1–v4 bleiben Archiv. Bericht: `measurements/V5-REPORT.md`, Tabelle:
> `measurements/PERF-v5.md`.

## Inhalt

| Datei | Kategorie | Einträge |
|---|---|---:|
| [`01-fusing.md`](01-fusing.md) | Instruction Fusing | 5 |
| [`02-bonding.md`](02-bonding.md) | Instruction Bonding / Superscalar | 5 |
| [`03-ammx.md`](03-ammx.md) | AMMX (64-Bit-SIMD) | 6 |
| [`04-register.md`](04-register.md) | Registermodell & Allokation | 5 |
| [`05-timing.md`](05-timing.md) | Instruktions-Timing & Fallstricke | 5 |
| [`06-memory.md`](06-memory.md) | 64-Bit-Speicherbus, Prefetch, Cache | 5 |
| [`07-patterns.md`](07-patterns.md) | Übergreifende Idiome | 5 |
| [`08-antipatterns.md`](08-antipatterns.md) | Was den 68080 bremst | 6 |
| [`09-harness.md`](09-harness.md) | Mess-Harness: Aufbau, Build, Fallstricke | – |
| [`12-matrix.md`](12-matrix.md) | Matrix-Multiplikation (FPU Float & AMMX Fixed) | 6 |
| [`13-fft.md`](13-fft.md) | Fast Fourier Transform (FPU Float & AMMX Fixed) | 3 |

**Summe: 42 Einträge**, jeder mit Vorher/Nachher-Code, Hardware-Mechanismus,
Registerliste und Messblock. Jeder Eintrag endet mit der **verbatim
gemessenen Sequenz** aus `seq/<ID>.i` (gebaut als `bin/<ID>`). Der
alphabetische Index steht in
[`index.md`](index.md), die Messübersicht (MIN, Median, MAX, AVG, Stdabw.,
p95) in [`measurements/SUMMARY.md`](measurements/SUMMARY.md). Der Übergabestand
für eine neue Session steht in [`HANDOFF.md`](HANDOFF.md).

## Quellenhierarchie

1. **Apollo 68080 Programmer's Reference Manual (PRM)**, Stand Juni 2026,
   insbesondere S. 29 (MOVEC/CCC), S. 42–77 (AMMX), S. 83–90
   (Superscalar/Fusing/Timing/Optimizing), S. 92 (Spezifikationen).
   <http://www.apollo-core.com/documentation/AC68080PRM.pdf>
2. **Offizielle Apollo-Wiki-Seiten** (Original-URLs sind heute nicht mehr
   erreichbar; die zitierten Fassungen liegen im Internet Archive):
   - Instruction Fusing:
     <https://web.archive.org/web/20200925054412/https://wiki.apollo-accelerators.com/doku.php/apollo_core:fusing>
   - Instruction Bonding:
     <https://web.archive.org/web/20200925062239/https://wiki.apollo-accelerators.com/doku.php/apollo_core:bonding>
   - Superscalar:
     <https://web.archive.org/web/20200925071951/https://wiki.apollo-accelerators.com/doku.php/apollo_core:superscalar>
3. **Verifizierter Open-Source-68080-Code**:
   Henryk Richters `blitapollo.asm` (libSDL12_Amiga68k), u. a. mit
   `load`/`store`/`vperm`/`pack3216` und der Begründung „less strain on the
   write buffers due to 64 Bit transfers“:
   <https://github.com/HenrykRichter/libSDL12_Amiga68k/blob/master/video/apollo/blitapollo.asm>
4. **Werkzeug- und Registerbelege**:
   - vasm M68k-Backend (getestet: vasm 2.0d, 1.8c), 68080/AMMX-Support.
   - MOVEC-Registerliste (CCC = `$809`):
     <http://adevnoo.wikidot.com/movec-080>
   - Apollo-Instruktionsübersicht:
     <http://www.apollo-core.com/instructions.html>

## Status-Legende

| Status | Bedeutung |
|---|---|
| `VERIFIZIERT (gemessen)` | Ein vollständiges Log `measurements/<ID>.log` mit `BEGIN`, 100×`RES`, `SUMMARY`, `END` liegt vor und wurde mit diesem Harness erzeugt. |
| `VERIFIZIERT (Literatur)` | Der Mechanismus ist im PRM bzw. in einer offiziellen Apollo-Quelle belegt; die Sequenz baut mit vasm. **Kein** Hardware-Messwert. |
| `UNVERIFIZIERT (nicht gemessen)` | Wie oben, aber ohne Hardwarelauf. Für künftige neue Einträge; der aktuelle Korpus ist vollständig gemessen. |
| `UNVERIFIZIERT (Register-Konflikt)` | Sequenz berührt `d5`, `d6`, `d7`, `a5`, `a6` oder `a7`; der Guard `tools/check_sequences.py` lehnt sie ab. |

Jeder Eintrag trennt deshalb sauber:

- **Status** – Messstatus,
- **Beleg** – Literaturquelle,
- **Messergebnis** – Rohdaten aus `measurements/<ID>.log` (42/42 vorhanden),
- **Literaturwert** – was PRM/Wiki aussagen (kein Messwert).

## Schnellstart

Der Zugriff auf die V4 (Windows-ACP, IP, Pfad-Syntax, Troubleshooting) steht
in [`V4-ACCESS.md`](V4-ACCESS.md). Das Linux-ACP aus WSL erreicht die V4
wegen des WSL-NAT nicht.

```bash
cd 68080-db

# 1. v5 bauen, ohne v1–v4 zu überschreiben
HARNESS=harness-v5.asm BUILD_DIR=build-v5 BIN_DIR=bin-v5 ./tools/build_all.sh

# 2. Auf der Vampire starten, z. B. bin-v5/FUS-001, und auf dem PC mitschneiden
#    (v3-Logs getrennt von v1/v2 ablegen)
#    Linux:
python3 -m pip install -r tools/requirements.txt
python3 tools/serial_capture.py --port /dev/ttyUSB0 --baud 115200 --out measurements/v5
#    Windows (getesteter Fall, CH340 an COM9):
#    python tools/serial_capture.py --port COM9 --baud 115200 --out measurements/v4

# 3. Log validieren (100 RES-Zeilen, SUMMARY konsistent)
python3 tools/check_log.py measurements/v5-files/FUS-001.run01.log
```

### 10×-Lauf (v5)

```bash
# Alle Binaries + RUN-ALL auf die V4 kopieren
for f in bin-v5/*; do ../Projects/ApolloExplorer/acp/acp "$f" "$AMIGAHOST:Ram Disk/"; done
../Projects/ApolloExplorer/acp/acp RUN-ALL "$AMIGAHOST:Ram Disk/"

# Auf der V4:  Execute "Ram Disk:RUN-ALL"
#   -> 42 Eintraege x 10 Laeufe, Dateilogs in Ram Disk:LOGS/<ID>.runNN.log

# Seriell mitschneiden (zweiter, unabhaengiger Kanal)
python3 tools/serial_capture.py --port /dev/ttyUSB0 --baud 115200 --out measurements/v4

# V4-Dateilogs herunterladen und mit dem Serial-Mitschnitt vergleichen
AMIGAHOST=apollo ./tools/download_logs.sh measurements/v5-files
python3 tools/repeat_report.py measurements/v5-files
```

Der Build braucht **keine NDK-Includes**: `harness.asm` ist self-contained und
enthält die wenigen benötigten exec/dos/serial-Offsets mit Quellenangabe im
Kommentar. Dadurch ist der Vergleich aller Einträge methodisch identisch.
Das Harness setzt zu Beginn das Apollo-Bit (`ori #$800,sr`), damit die
E-Register als Task-Zustand behandelt werden (PRM S. 8).

## Messprotokoll (Kurzfassung)

1. Init: `exec_base`, DOS-Output.
2. `Forbid()`.
3. **Warmup: 10 × 10 = 100 volle Messblöcke** in `cycle_buffer+4`; identischer
   Code wie die Messung (`MOVEC`, Sequenz, `MOVEC`, Sub, Store). Die Werte
   werden nicht ausgewertet.
4. Scratch-Register und Zeiger zurücksetzen (`init_scratch`).
5. Baseline: zwei `MOVEC CCC`-Reads, Delta → `cycle_buffer[0]`.
6. **10 × 10** Messläufe; pro Lauf `MOVEC d5`, Sequenz inline, `MOVEC d6`,
   `sub.l d5,d6`, `move.l d6,(a5)+`.
7. `Permit()`.
8. `calc_stats` über `cycle_buffer[1..100]`: MIN, MAX, SUM, AVG.
9. Ausgabe an stdout, `LOGS/<ID>.log` und die direkte UART
   (`SERDAT`/`SERPER`, 115200 8N1), **nach** `Permit()`. Nach `BEGIN` folgt
   die `CORE;…`-Zeile mit PCR-ID/Revision und optional `ENV:VCoreRev`.

**Referenzwert ist MIN**, nicht AVG: `Forbid()` verhindert Task-Wechsel, aber
keine Interrupts. AVG ist nur informativ. Die Baseline wird nach dem Warmup
gemessen, ist aber weiterhin einmalig; sie geht nicht in die Statistik ein.
In den **v1-Logs** war sie kalt (5–41 Takte); bei v2 ist sie warm.

## Register-Konvention

| Register | Rolle im Messblock | Sequenz darf es benutzen? |
|---|---|---|
| `d5` | MOVEC Startwert | **nein** |
| `d6` | MOVEC Stoppwert / Delta | **nein** |
| `d7` | äußerer Zähler (10) | **nein** |
| `a5` | Schreibzeiger `cycle_buffer` | **nein** |
| `a6` | `exec_base` | **nein** |
| `a7` | Stack | **nein** |
| `d0`–`d4` | Scratch | ja |
| `a0`–`a4` | Scratch | ja |
| `e0`–`e23` | AMMX-Scratch | ja (nur AMMX/E-fähige Befehle) |

Der Guard prüft die Sequenzen statisch. Ein Verstoß ist ein Build-Fehler, kein
„UNVERIFIZIERT“-Eintrag.

## Werkzeuge

| Werkzeug | Zweck |
|---|---|
| `tools/build_all.sh` | alle 42 Einträge mit vasm bauen |
| `tools/check_sequences.py` | Register-/Label-Guard für Sequenzen |
| `tools/serial_capture.py` | seriellen Strom mitschneiden; schreibt `<ID>.runNN.log` (kein Überschreiben bei Wiederholungen) |
| `tools/download_logs.sh` | V4-Dateilogs aus `Ram Disk:LOGS` per ACP herunterladen |
| `tools/repeat_report.py` | 10 Wiederholungen je Eintrag auswerten; optional V4- und Serial-Logs vergleichen |
| `tools/check_log.py` | Log-Format, `CORE`-Zeile und SUMMARY gegen RES prüfen |
| `RUN-ALL` | AmigaDOS-Batch: jedes der 42 Programme 10× (420 Läufe), Dateilog in `Ram Disk:LOGS` |
| `RUN-ALL-ONCE` | Einzellauf aller 42 Programme (für schnelle Tests) |

## Reproduzierbarkeit und Grenzen

- **Hardware:** Alle 42 Einträge wurden mit Harness v4 auf einer Vampire V4
  gemessen (10 × 100 Läufe, COM9, 115200 8N1). Die Streuung **über die 10
  Läufe** (Median, Stdabw., p95) steht in
  [`measurements/SUMMARY.md`](measurements/SUMMARY.md); sie sagt nichts über
  die Streuung zwischen Boots/Core-Revisionen.
- **Direkte UART statt `serial.device`:** Der getestete USB-Serial-Adapter
  hängt am SERDAT/SERPER-Pfad. `serial.device` lieferte keine Ausgabe; die
  direkte Registeransteuerung funktioniert (siehe
  `Projects/ApolloLib/ApolloCrossDev_Debug.c`).
- **Core-Revision beachten:** Das PRM (Juni 2026) beschreibt ab Core ~11350
  drei Pipes; ältere V4-Cores haben zwei. Die Wiki-Seiten von 2020 beschreiben
  SILVER2 (zwei Pipes). Einträge, die davon abhängen, sind markiert.
- **CCC-Breite** ist im PRM nicht dokumentiert. Das Harness rechnet mit
  32-Bit-Deltas; ein Überlauf über 2³² Takte ist bei diesen Sequenzen
  ausgeschlossen, aber nicht formal belegt.
- **AMMX-Verfügbarkeit:** Das Harness setzt einen 68080 mit AMMX voraus.
  Portabler Code prüft zur Laufzeit `_Apollo_AMMXon` (siehe `blitapollo.asm`).
- **68060-Vergleich:** Es wurden keine 68060-Messungen durchgeführt. Aussagen
  wie „auf dem 68060 wäre das langsamer“ sind in den Einträgen als
  Literatur/Mechanismus begründet, nicht als gemessene Differenz.

## Offene Punkte

1. Wiederholungsläufe über mehrere Boots/Core-Revisionen durchführen und
   die Streuung **zwischen** Läufen dokumentieren (die Streuung **innerhalb**
   eines Laufs steht bereits in `measurements/SUMMARY.md`).
2. Core-Revision, Firmware und Taktrate pro Log dokumentieren.
3. CCC-Breite und Überlaufverhalten experimentell prüfen.
4. 68060-Vergleichsmessungen mit identischen Sequenzen (separater Harness)
   ergänzen.
5. Prüfen, ob `TOUCH (An)` in neueren vasm-Versionen akzeptiert wird; aktuell
   wird die indexierte Form `(An,Xn.w)` verwendet.
6. AMMX-Laufzeitdetektion in den Harness aufnehmen, damit derselbe Build auf
   Nicht-AMMX-68080-Cores sauber abbricht statt undefiniert zu laufen.

- [10-adpcm.md](10-adpcm.md) – verifizierte ADPCM-4-Bit-Case-Study (gut/schlecht, Messwerte)

- [11-adpcm-candidates.md](11-adpcm-candidates.md) – ADPCM-Instruktionskandidaten, gemessen vs. geschätzt
