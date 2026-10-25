# ADPCM Decoder – Agent Guide

Produktivstand: **Fresh14 (Signed-32-Bit-Delta-Tabelle, Stereo-Byte-Unroll, 64-Byte-Mono-Alignment, MS/Mono unrolled)** in `decoder_4bit.s`.

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

## Core-Version ist Pflicht

Jedes Ergebnis muss die Core-Version enthalten:

- `core-before.json` / `core-after.json`
- `core_before` / `core_after` in `summary.json`
- Harness-Logs: `CORE;<PCR-ID>;<PCR-REV>;<VCoreRev>`

Vergleiche nur Ergebnisse derselben Core-Version. Aktuell: **Core 11350,
PCR 1088/1, VCoreRev 0**.

## Amiga-Fernsteuerung

`Tools/amiga_tool.py` (amiagent 0.13.2, Port 7846) ist der lokale CLI-Bridge; **Autostart aktiv** aus `S:Startup-Sequence`. Token nur lokal, nicht im Git. Kein Cline/API-Key nötig. Details: `Tools/AMIGA-TOOL.md`.
