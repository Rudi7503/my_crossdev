# HANDOFF – 68080-Datenbank

Stand: **2026-10-23 13:10 (Europe/Berlin)**

## Aktueller Zustand

- Projekt: `/home/papa/Vampire_V4/ApolloCrossDev/68080-db`
- **75 Einträge**, alle mit `VERIFIZIERT (gemessen – Vampire V4, Log vorhanden)`;
  die Messwerte sind **Harness v1**.
- **42 v1-Logs** unter `measurements/<ID>.log`, alle mit `check_log.py` geprüft
- zusätzlich `measurements/FUS-001.run1.log` als separater Wiederholungslauf
- **42 Sequenzen** unter `seq/<ID>.i`
- **42 Binaries** unter `bin/<ID>` plus `bin/SERIAL-PROBE` (v1, vor dem Umbau)
- Batchdatei `RUN-ALL`: **75 Einträge × 10 Wiederholungen** (750 Läufe),
  Dateilogs in `Ram Disk:LOGS/<ID>.runNN.log`; `RUN-ALL-ONCE` = Einzellauf
- Messübersicht: `measurements/SUMMARY.md` (v4); **Median, Stdabw. und p95**
  sind Mediane der 10 Läufe; **MIN 75/75 identisch**, Warmup erste 10 = MIN.
- Harness: `harness-v5.asm` = **v5** (v4-Taktmessung + PERF-Zählerdurchgang,
  Stack-Startwerte, `PERF`-Zeile, kein stdout); `harness.asm` = v4,
  `harness-v1.asm` = v1-Vorlage; Doku: `09-harness.md`
- `build/` und `bin/` sind **v1** (gemessene Artefakte).
  `build-v2/`/`bin-v2/` = v2 (gemessen; Dateilogs fehlen).
  `build-v3/`/`bin-v3/` = v3 (gemessen; Warmup wirkt, Dateilogs fehlen).
  `build-v4/`/`bin-v4/` = v4 (gemessen; 740 Serial + 740 Dateilogs).
  `build-v5/`/`bin-v5/` = **v5 (gemessen; 740 Dateilogs, `PERF`, MIN = v4)**.
  Befunde: `measurements/V5-REPORT.md`, `measurements/PERF-v5.md`; v1–v4
  sind Archiv.
- v2-Warmup (historisch): 100 volle Messläufe in `cycle_buffer+4` (MOVEC, Sequenz, MOVEC,
  Sub, Store), danach `init_scratch`-Reset. Die Warmup-Werte werden von den
  100 Messwerten überschrieben; `calc_stats` und `check_log.py` bleiben
  kompatibel (CORE-Zeile optional).
- v2-Dateilog (historisch): nach `Permit()` wird `LOGS/<ID>.log` geschrieben; `RUN-ALL`
  benennt nach jedem Lauf in `LOGS/<ID>.runNN.log` um. Der PC-Mitschnitt
  schreibt ebenfalls `<ID>.runNN.log`; `tools/repeat_report.py` vergleicht
  beide Kanäle und wertet die 10 Wiederholungen aus.
- **v2-Testlauf 2026-10-23:** 750/750 Serial-Logs OK, 10/10 je Eintrag,
  MIN über alle Wiederholungen identisch (75/75). **V4-Dateilogs fehlen**
  (`Ram Disk:LOGS/` leer); der Warmup wirkt nicht auf die erste echte
  Iteration (Makro wird zweimal expandiert → zwei Codeadressen).
  v1–v4 bleiben Archiv; **v5 ist kanonisch**.
- **V4-Zugriff:** Windows-ACP
  `C:\Program Files\ApolloExplorer\acp.exe` (aus WSL:
  `/mnt/c/Program Files/ApolloExplorer/acp.exe`), Host `ApolloOS`
  `192.168.178.50`. Das **Linux-ACP aus WSL funktioniert nicht** (WSL-NAT
  `172.23.x`, keine Route ins `192.168.178.0/24`). Details, Pfad-Syntax und
  Troubleshooting: [`V4-ACCESS.md`](V4-ACCESS.md). Am 2026-10-23 22:15 war
  die V4 stabil (0 % Verlust); **44 Dateien** (42 v2-Binaries + `RUN-ALL` +
  `RUN-ALL-ONCE`) liegen in `Ram Disk:` und wurden per Rückdownload
  **44/44 byte-identisch** verifiziert. `LOGS/` entsteht beim ersten Lauf.
- `MEM-003`/`MEM-004`: Die Sequenzinhalte waren gegenüber den Eintragstiteln
  vertauscht. Die Einträge wurden an die tatsächlich gebauten und gemessenen
  Sequenzen angepasst; Logs, IDs und Messwerte blieben unverändert.
- Alle 75 Einträge enthalten am Ende den **verbatim gemessenen Code** aus
  `seq/<ID>.i` (gebaut als `bin/<ID>`).

### Integritätsprüfung (zuletzt ausgeführt)

```text
entries 42
seq 42
bins 43   (75 Einträge + SERIAL-PROBE)
logs 43   (75 Einträge + FUS-001.run1)
parts 0
measured 42
minfields 42
broken_links 0
odd_fences 0
```

## Technische Kernfakten

1. **Serialpfad:** Der getestete CH340 an COM9 hängt an der direkten
   Amiga-UART. Der Harness schreibt über `SERDAT`/`SERPER`/`SERDATR`
   (115200 8N1), **nicht** über `serial.device`. Der frühere
   `serial.device`-Pfad lieferte keine Ausgabe.
2. **CCC:** vasm akzeptiert nur `movec.l CCC,d5`; `$809` und ein EQU-Alias
   scheitern mit „illegal operand types“.
3. **Apollo-Bit:** Der Harness setzt `ori #$800,sr`, damit E0–E23 als
   Task-Zustand behandelt werden.
4. **Baseline:** In v1 einmalig/kalt gemessen, Werte **5–41 Takte**; bei
   vielen Einträgen größer als MIN und **nicht subtrahierbar**.
   `NET = RES − BASELINE` ist ungültig (Beispiel FUS-001: 3 − 5 = −2). In v2
   wird sie nach dem Warmup gemessen (warm), bleibt aber eine Einzelmessung.
5. **Referenz:** ausschließlich **MIN**. MAX/AVG enthalten Interrupt- und
   (in v1) Kaltstart-Ausreißer. AVG wird durch `DIVU.W #100` ganzzahlig
   abgeschnitten.
6. **Erste Iteration (v1):** Bei FUS-001 sind die ersten 10 Läufe 11/34/…, ab
   Lauf 11 konstant 3. Ursache ist der kalte Messcode, nicht die Sequenz.
   v2 wärmt 100 volle Messblöcke vor; dieser Effekt sollte dort entfallen.
7. **Ein v1-Batchlauf pro Eintrag**, noch keine Wiederholungsserie über
   mehrere Boots/Core-Revisionen. Core-Revision/Firmware/Takt wurden nicht
   erfasst.
8. **v2-Warmup:** `MEASURE10` ist der gemeinsame 10×10-Block für Warmup und
   Messung. Die Sequenz steht 20× im Code (10 pro Expansion) und wird 200×
   ausgeführt (100 Warmup + 100 Messung). Nach dem Warmup setzt
   `init_scratch` die Zeiger/Register zurück; die Messung startet an denselben,
   jetzt warmen Adressen.
9. **v2-Dateilog und CORE:** Jedes Programm schreibt nach `Permit()`
   `LOGS/<ID>.log` (dos.library `Open`/`Write`/`Close`) und loggt
   `CORE;<PCR-ID>;<PCR-REV>;<VCoreRev>`. `RUN-ALL` wiederholt jeden Eintrag
   10×, löscht am Start alte `LOGS/*.log` und benennt in
   `LOGS/<ID>.runNN.log` um. Ohne `VControl SETENV` bleibt `VCoreRev` leer;
   PCR (V2/V4) wird immer gelesen.

## Wichtigste Messergebnisse

| Vergleich | Ergebnis |
|---|---|
| `FUS-001` MOVE+ADD fusioniert | MIN = 3 |
| `ANTI-001` mit NOP dazwischen | MIN = 9 → **3× langsamer** |
| `BND-001` Bonding | MIN = 2 (niedrigster Wert) |
| `TIM-005` MOVE16 | MIN = 5; `PAT-001` vier Moves ebenfalls 5 |
| `AMX-001` LOAD/STORE | MIN = 3; `FUS-004` ebenfalls 3 |
| `ANTI-002`/`ANTI-003`/`ANTI-005` | MIN = 3, **kein** messbarer Nachteil |
| `ANTI-004` MOVEM+MUL | MIN = 8 gegen `BND-004` MIN = 7 |
| `MEM-004` 32-Byte-Burst | MIN = 5, Median 6,5; nur 48/100 Läufe auf MIN |

Die Antipattern-Einträge sagen ausdrücklich, wenn der erwartete Nachteil im
Kurzztest **nicht** messbar war. Das ist keine Aufforderung, die
PRM-Mechanismen zu verwerfen, sondern ein ehrlicher Messbefund.

## Dateien und Rollen

| Pfad | Inhalt |
|---|---|
| `README.md` | Übersicht, Quellen, Status, offene Punkte |
| `index.md` | alphabetischer Index mit Statusspalte |
| `V4-ACCESS.md` | **V4-Zugriff:** Windows-ACP, IP, Pfad-Syntax, Troubleshooting |
| `NEXT-TEST.md` | **nächster Testlauf:** Pipeline-/Cache-Zähler (IEP1–3, BPC/BPW, DCH/DCM) |
| `01-fusing.md` … `08-antipatterns.md` | 75 Einträge |
| `09-harness.md` | Harness, UART, Batch, Fallstricke |
| `harness.asm` | **v2**-Template: Warmup = voller 10×10-Block |
| `harness-v1.asm` | archivierte v1-Vorlage (Quelle der 42 v1-Logs) |
| `build-v2/`, `bin-v2/` | v2-Buildquellen und 42 v2-Binaries (gemessen, Dateilogs fehlen) |
| `build-v3/`, `bin-v3/` | v3-Buildquellen und 42 v3-Binaries (gemessen; Warmup wirkt) |
| `build-v4/`, `bin-v4/` | v4-Buildquellen und 42 v4-Binaries (gemessen; ohne stdout, Dateilog OK) |
| `harness-v5.asm`, `build-v5/`, `bin-v5/` | v5: v4 + PERF-Zählerdurchgang (gemessen, 750/750 Dateilogs) |
| `tools/perf_report.py` | PERF-Auswertung: Pipe-Anteile, BPW-Quote, D-Cache-Hitrate |
| `harness-v3-diag.asm` | Diagnose-Fassung mit erweitertem FILESTAT/Fallback (archiviert) |
| `seq/<ID>.i` | 42 Messsequenzen |
| `measurements/<ID>.log` | 42 echte Rohlogs |
| `measurements/SUMMARY.md` | v4-Tabelle aller 42 (MIN, Median, MAX, AVG, Stdabw., p95) |
| `measurements/SUMMARY-v1.md` | v1-Archivtabelle |
| `measurements/v2/` | 740 v2-Serial-Logs (10× je Eintrag, nicht kanonisch) |
| `measurements/v2/REPORT.md` | v2-Auswertung: MIN stabil, Warmup-/Dateilog-Befund |
| `measurements/v3/` | 740 v3-Serial-Logs (10× je Eintrag, nicht kanonisch) |
| `measurements/v3/REPORT.md` | v3-Auswertung: Warmup wirkt, Dateilog fehlt |
| `measurements/v4/` | 740 v4-Serial-Logs (10× je Eintrag, kein stdout) |
| `measurements/v4-files/` | 740 v4-V4-Dateilogs (identisch mit Serial) |
| `measurements/v4/REPORT.md` | v4-Auswertung: Dateilog funktioniert, MIN stabil |
| `measurements/FUS-001.run1.log` | Wiederholungslauf |
| `RUN-ALL` | AmigaDOS-Batch: 75 Einträge × 10 Läufe, Dateilogs in `LOGS/` |
| `RUN-ALL-ONCE` | AmigaDOS-Batch: 75 Einträge einmal (schneller Test) |
| `tools/download_logs.sh` | V4-Dateilogs per ACP herunterladen |
| `tools/repeat_report.py` | 10 Wiederholungen auswerten, V4-/Serial-Logs vergleichen |
| `tools/` | Build, Guard, Serial-Capture, Log-Prüfer |

## Regeln für die nächste Session

- **Nichts erfinden.** Neue Werte nur aus echten `measurements/<ID>.log`.
- **MIN ist Referenz.** Baseline nicht abziehen; MAX/AVG nur informativ.
- **Logs nicht überschreiben.** Die 42 v1-Logs bleiben Archiv. v2-Logs
  separat ablegen (z. B. `measurements/v2/<ID>.log` oder `<ID>.v2.log`).
- **v1-Artefakte sichern.** Vor `./tools/build_all.sh`:
  `cp -a build build-v1 && cp -a bin bin-v1`. `harness-v1.asm` ist die
  archivierte v1-Vorlage.
- **Direkte UART verwenden.** `serial.device` ist auf dieser V4 nicht der
  richtige Pfad.
- **Nach jedem Lauf:** `tools/check_log.py`, Status im Eintrag,
  `measurements/SUMMARY.md` und ggf. `index.md` aktualisieren.
- **Keine Labels/Kontrollfluss in `seq/<ID>.i`.** Der Guard
  `tools/check_sequences.py` muss grün sein.
- **Build:** `./tools/build_all.sh`; getestet mit vasm 2.0d und 1.8c.
  Der aktuelle Stand baut **v2**.

## Sinnvolle nächste Aufgaben

1. **v5 ist übernommen:** 75 Einträge und `SUMMARY.md` stehen auf v5
   (MIN/Median/MAX/AVG + IEP-Spalten); `SUMMARY-v4.md` ist Archiv.
   Bericht: `measurements/V5-REPORT.md`; Tabelle `measurements/PERF-v5.md`.
2. Core-Revision/Firmware/Takt: PCR-ID/Revision und `ENV:VCoreRev` sind jetzt
   im `CORE`-Feld; Firmware/Takt und die exakte Flash-Revision ohne VControl
   noch dokumentieren.
3. Wiederholungsläufe über mehrere Boots; Streuung **zwischen** Boots
   dokumentieren (für v4 in `SUMMARY.md`).
4. Baseline-Methodik verbessern (mehrfach nach Warmup, Median).
5. 68060-Vergleichsmessungen mit identischen Sequenzen.
6. AMMX-Laufzeitdetektion im Harness.
7. **Nächster Testlauf (erst nach dem aktuellen v2-Lauf):**
   Pipeline-/Cache-Zähler `IEP1`/`IEP2`/`IEP3`, `BPC`/`BPW`, `DCH`/`DCM`
   erfassen. Details, Registeradressen und Referenzcode stehen in
   [`NEXT-TEST.md`](NEXT-TEST.md). Zähler nicht innerhalb des gemessenen
   `MOVEC CCC`-Blocks lesen.

## Prompt für die neue Session

Diesen Text in die neue Session mit Workspace
`/home/papa/Vampire_V4/ApolloCrossDev` einfügen:

```text
Arbeitsverzeichnis: /home/papa/Vampire_V4/ApolloCrossDev
Projekt: ./68080-db (Apollo-68080-Best-Practice-Datenbank)

Lies zuerst vollständig:
1. 68080-db/HANDOFF.md
2. 68080-db/README.md
3. 68080-db/measurements/SUMMARY.md
4. 68080-db/09-harness.md

Status: Die 75 Einträge sind mit **Harness v1** auf einer Vampire V4 gemessen.
Die v1-Logs liegen unter 68080-db/measurements/<ID>.log, FUS-001.run1.log ist
ein separater Wiederholungslauf. Referenz ist MIN; die v1-Baseline (5–41 Takte)
ist NICHT subtrahierbar. AVG ist ganzzahlig abgeschnitten. Der Serialpfad ist
die direkte UART (SERDAT/SERPER), nicht serial.device. SUMMARY.md enthält
zusätzlich Median, Stdabw. und p95 aus den 100 RES-Werten (41/42: Median = MIN;
Ausnahme MEM-004, 32-Byte-Burst). `harness.asm` ist inzwischen **v2**: Der
Warmup führt 100 volle Messblöcke inkl. a5-Zeiger/Store aus, danach werden die
Zeiger zurückgesetzt; die Warmup-Werte werden überschrieben. Zusätzlich
schreibt jedes Programm absolut `Ram Disk:LOGS/<ID>.log` und loggt
`FILESTAT;<1/0>;<IoErr>` sowie `CORE;<PCR-ID>;<PCR-REV>;<VCoreRev>`.
`RUN-ALL` wiederholt jeden Eintrag 10× und benennt in
`LOGS/<ID>.runNN.log` um.
`build/` und `bin/` sind v1. `bin-v2/` (Makro-Warmup) und `bin-v3/`
(gemeinsame Subroutine) wurden je 10× gemessen; v3 behebt den Warmup,
aber der Dateilog scheiterte. **v4** behebt beides: kein stdout,
`OpenLibrary` mit `a1`/`d0`, 740 Serial- **und** 740 Dateilogs, alle
identisch, `FILESTAT;1;0`, MIN 10/10 stabil. **v5 ist kanonisch**;
v1–v4 sind Archiv. Befunde: measurements/V5-REPORT.md, measurements/PERF-v5.md.
`harness-v1.asm` ist die archivierte v1-Vorlage.
V4-Zugriff: Windows-ACP C:\Program Files\ApolloExplorer\acp.exe (aus WSL
/mnt/c/Program Files/ApolloExplorer/acp.exe), Host ApolloOS 192.168.178.50.
Das Linux-ACP aus WSL funktioniert nicht (NAT). Details: 68080-db/V4-ACCESS.md.

Prüfe vor der ersten Änderung:
- 75 Einträge, 42 Sequenzen, 42 Logs, 42 Binaries
- keine measurements/*.part
- tools/check_log.py für alle 42 Logs
- tools/check_sequences.py
- 42 Statuszeilen "VERIFIZIERT (gemessen)"
- harness.asm (v3) baut mit ./tools/build_all.sh
- RUN-ALL enthält 42 × 10 Läufe und 42 × 10 Renames

Behandle die vorhandenen Logs als echte Messdaten und überschreibe sie nicht
ohne Backup. Erfinde keine Taktwerte. v5 ist kanonisch; v1–v4 bleiben
Archiv. Neue Läufe separat ablegen (z. B. measurements/v6/<ID>.runNN.log),
Artefakte vor dem Rebuild sichern und danach Eintrag, SUMMARY.md und index.md
aktualisieren. V4-Dateilogs mit tools/download_logs.sh holen und mit
tools/repeat_report.py gegen den Serial-Mitschnitt prüfen.

Aufgabe: [hier die konkrete nächste Aufgabe eintragen]
```
