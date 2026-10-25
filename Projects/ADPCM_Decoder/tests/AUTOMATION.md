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

- Produktiv: **Opt9** (exakte Delta-Tabelle, E2–E5 L/R/MS-Pair, Matrix inline)
- Golden + 100×: PASS
- Letzter automatisierter Lauf: `tests/auto-results/opt9-20261025-110250`
- Optimierungsverlauf: `tests/OPTIMIZATION.md`
