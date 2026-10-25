# ADPCM Decoder – Agent Guide

Produktivstand: **Fresh3 (Stereo geloopt, MS/Mono unrolled)** in `decoder_4bit.s`.

## Pflicht vor jeder Änderung

```bash
python3 tests/auto_opt.py
```

Das baut, lädt per ACP hoch, führt per ASH auf der V4 aus, lädt die
Ergebnisse und prüft Golden + 100×-Repeat.

## Regeln

- `decoder_4bit.s` nur nach **Golden PASS + Repeat PASS** ändern.
- 100×-MIN/Median ist die Referenz; AVG/Max sind durch Interrupts verrauscht.
- Letzter Vergleich: Fresh3 1001/1047/816 vs Fresh-Asm 997/1237/963 (MIN).
- Kandidaten unter `experiments/` ablegen, nie direkt experimentieren.
- Ergebnisse unter `tests/auto-results/` sind generiert (gitignored).
- Doku: `tests/OPTIMIZATION.md`, `tests/AUTOMATION.md`.

## Umgebung

- V4: 192.168.178.50
- ACP/ASH: `C:\Program Files\ApolloExplorer`
- Toolchain: `Compilers/GCC-6.50-Latest`
- Server startet automatisch aus `S:Startup-Sequence` (1.4.0).

## Crash-Erkennung

```bash
python3 tests/crash_check.py --binary tests/golden_test_opt5 --name opt5
python3 tests/crash_check.py --command C:Reboot --name reboot
```

Zustände: `NO_CRASH`, `REBOOT`, `CRASH_REQUESTER`, `CRASH_NO_SCREENSHOT`,
`V4_DOWN`. Die RAM-Canary unterscheidet Reboot von Requester.
Details: `tests/AUTOMATION.md`.

## Live-Watchdog

`tests/live_runner.py` liest COM9 über `tests/serial_log.py` und resettet
den Timeout bei jedem `ALIVE`/`DONE`. `auto_opt.py` klassifiziert Crashs über
RAM-Canary + Screenshot. 30 ALIVE-Zeilen im Repeat-Lauf verifiziert.

## GitHub-Pflicht

Nach jeder Änderung:

1. `python3 tests/auto_opt.py` bzw. passenden Test ausführen
2. Ergebnis/Doku aktualisieren
3. `git add` der Quellen/Tests/Doku
4. **committen und pushen** – nichts Unpushed liegen lassen
5. Generierte Binaries/Logs bleiben gitignored

## Mikrobenchmark-Gate

Jede Änderung braucht einen Mikrobenchmark (100×, MIN/Median, Baseline)
**und** End-to-End Golden/Repeat. Bei Regression oder Verbesserung den Grund
dokumentieren. Ergebnisse in `tests/OPTIMIZATION.md` und `68080-db/`.
