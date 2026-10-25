# ADPCM Test-Automation (für Menschen und KI-Agenten)

## Ein Befehl

```bash
cd /home/papa/Vampire_V4/ApolloCrossDev/Projects/ADPCM_Decoder
python3 tests/auto_opt.py
```

Ablauf:

1. `decoder_4bit.s` mit vasm bauen
2. `golden_test_auto` / `repeat_test_auto` mit m68k-gcc bauen
3. beide per ACP nach `192.168.178.50:RAM/` hochladen
4. per ASH ausführen:
   ```text
   ash 192.168.178.50 RAM:golden_test_auto
   ash 192.168.178.50 RAM:repeat_test_auto
   ```
5. `ADPCM_GOLDEN.txt` und `ADPCM_REPEAT.txt` per ACP herunterladen
6. PASS und MIN/Median auswerten

Ergebnisverzeichnis:

```text
tests/auto-results/<version>-<YYYYmmdd-HHMMSS>/
  ADPCM_GOLDEN.txt
  ADPCM_REPEAT.txt
  golden.ash.txt
  repeat.ash.txt
  summary.json
```

## Kandidaten testen

```bash
python3 tests/auto_opt.py \
  --source experiments/decoder_4bit_opt8.s \
  --version opt8
```

Nur Golden überspringen:

```bash
python3 tests/auto_opt.py --skip-golden
```

## Erfolgskriterien

- Golden: `RESULT;PASS`
- Repeat: alle drei Zeilen `STEREO/MS/MONO` mit `PASS`
- Produktiv wird ein Kandidat nur, wenn **Golden und 100×-Repeat** passen
  und der Median nicht schlechter ist.

## Umgebung

- V4: `192.168.178.50`
- ApolloExplorerSrv 1.4.0 läuft automatisch aus `S:Startup-Sequence`
  (Eintrag vor `EndCLI`)
- ACP/ASH: `C:\Program Files\ApolloExplorer\{acp,ash}.exe`
- Toolchain: `Compilers/GCC-6.50-Latest`
- Testausgabe: `RAM:`; Ergebnisdateien: `Ram Disk:`

## Lifecycle-Marker

Die Tests senden zusätzlich über UART:

```text
START;<programm>;<version>
DONE;<programm>;<version>
```

Damit kann ein serieller Mitschnitt (`68080-db/tools/serial_capture.py`)
den Lauf verfolgen. Die Pipeline nutzt ASH als Completion-Signal und die
Ergebnisdateien; der serielle Mitschnitt ist optional.

## Stolpersteine

- `acp -l` muss `V4-SA (StandAlone)` finden; sonst Server/Netz prüfen.
- `ash 192.168.178.50 version` muss Kickstart/Workbench ausgeben.
- `RAM:` muss beschreibbar sein; Test binaries werden dort abgelegt.
- Alte `ADPCM_GOLDEN.txt`/`ADPCM_REPEAT.txt` werden pro Lauf überschrieben.
- Der Test schreibt bewusst nach `Ram Disk:`, nicht nach `RAM:`.

## Aktueller Stand

- Produktiv: **Fresh3** (Stereo geloopt, MS/Mono unrolled; d0/d1-Nibbles, d6/d7-Temps)
- Golden + 100×: PASS
- Letzter automatisierter Lauf: `tests/auto-results/opt9-20261025-110250`
- Optimierungsverlauf: `tests/OPTIMIZATION.md`

## Crash-Erkennung

`tests/crash_check.py` führt ein Binary oder einen ASH-Befehl aus und
klassifiziert den Ausgang:

| Zustand | Bedeutung | Exit |
|---|---|---:|
| `NO_CRASH` | ASH kam normal zurück | 0 |
| `REBOOT` | V4 war weg und kam zurück | 10 |
| `CRASH_REQUESTER` | V4 lebt, Software-Failure-Screenshot | 11 |
| `CRASH_NO_SCREENSHOT` | V4 lebt, Screenshot fehlgeschlagen | 12 |
| `V4_DOWN` | V4 kam nicht zurück | 13 |

### RAM-Canary

Vor dem Lauf wird `RAM:canary_<name>_<zeit>` per `C:MakeDir` angelegt.
Nach dem Lauf:

- Canary **weg** → V4 wurde rebootet (`REBOOT`)
- Canary **da** + Screenshot → Crash-Requester
- Canary **da** + ASH normal → `NO_CRASH`

### Beispiele

```bash
python3 tests/crash_check.py --binary tests/golden_test_opt5 --name opt5
python3 tests/crash_check.py --command C:Reboot --name reboot
python3 tests/crash_check.py --command version --name normal
```

Crash-Screenshot:

```text
tests/crash-results/<name>-<zeit>/crash.png
```

Bei erkanntem Requester wird `C:grabrtgwb savefile=RAM:crash.bmp frontscreen bmp quiet`
ausgeführt und die BMP per ACP geholt.

### Verifizierte Fälle

- `golden_test_opt5` → `CRASH_REQUESTER`
  - Software Failure, `8000000A - Line 1010 (A) Emulator error`
- `C:Reboot` → `REBOOT`
- `version` → `NO_CRASH`

Der Crash-Requester blockiert die Workbench; der ASH-Server läuft weiter,
deshalb funktioniert der Screenshot trotzdem.

## Live-Heartbeats und Watchdog

Die Tests senden während des Laufs:

```text
ALIVE;<programm>;<zaehler>
```

- `repeat_test` alle 10 Wiederholungen
- `golden_test` vor jedem Dekodierlauf
- zusätzlich auf UART und stdout

`tests/serial_log.py` ist ein Roh-Logger (Windows Python), der COM9
zeilenweise nach `C:\Windows\Temp\adpcm_serial_<lauf>.log` schreibt.
`tests/live_runner.py` startet ihn, führt ASH aus und setzt den Idle-Timeout
bei **jeder neuen seriellen Zeile** zurück (Default 20 s, Gesamtlimit 300 s).
Bei Abbruch:

- RAM-Canary weg → `REBOOT`
- Canary da + Screenshot → `CRASH_REQUESTER`
- sonst → `CRASH_NO_SCREENSHOT`

Verifiziert: 30 `ALIVE`-Zeilen im Repeat-Lauf, plus `START`/`DONE`.
`auto_opt.py` nutzt diesen Watchdog und liefert bei Crash Exit 4/5.

## Core-Version in Ergebnissen

Jeder `auto_opt.py`-Lauf schreibt:

```text
core-before.json
core-after.json
summary.json  (core_before/core_after)
```

Quelle: `C:ApolloControl CORE` und `ENV:VCoreRev`.
Aktuell: **Core 11350, PCR 1088/1, VCoreRev 0**.
Ergebnisse ohne Core-Datei sind für Vergleiche nicht zulässig.
