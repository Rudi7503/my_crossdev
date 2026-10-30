# ADPCM Decoder – Agent Guide

Produktivstand (Core **11350**, PCR 1088/1, VCoreRev 0):
- **4-Bit (`decoder_4bit.s`):** **Fresh15** (Signed-32-Bit-Delta-Tabelle, Stereo voll unrolled, 64-Byte-Mono-Alignment, MS/Mono unrolled) → **784 / 920 / 729** (Stereo/MS/Mono, 100× MIN).
- **2-/3-/5-/6-Bit & Hybrid-M/S (`decoder_2bit.s`, `decoder_3bit.s`, `decoder_5bit.s`, `decoder_6bit.s`, `decoder_3_2ms.s`, `decoder_4_2ms.s`, `decoder_5_3ms.s`):** **Signed-32-Bit-Delta-Tabelle + Inline-Makros (`d1`-Scratch)** → **38–60 % schneller** als Baseline, alle 15 Modi Golden (512 Blöcke) + 100× Repeat **PASS**.

## Pflicht vor jeder Änderung

```bash
python3 tests/auto_opt.py            # 4-Bit End-to-End (Golden + 100x Repeat)
bash tests/build_golden_all.sh       # 2/3-Bit Golden + 100x Repeat (echte _orig.s)
bash tests/run_5_6.sh                # 5/6-Bit Golden + 100x Repeat (echte _orig.s)
bash tests/run_hybrid.sh             # Alle 15 Modi Golden + Hybrid 100x Repeat (echte _orig.s, lokaler Generator)
```

Das baut, lädt per ACP hoch, führt per ASH auf der V4 aus und prüft Golden + 100×-Repeat.

## Regeln & Compiler-Fallstricke (WICHTIG!)

- Decoder-Dateien (`decoder_*.s`) nur nach **Golden PASS + 100× Repeat PASS** ändern.
- **NIEMALS `-m68080 -O2` bei `m68k-amigaos-gcc` (GCC 6.5.0) für C-Tests/Referenzen verwenden!**
  - Mit `-m68080 -O2` emittiert GCC fehlerhafte Kurz-Immediates (`moviw.l #-32768` → Opcode `0x303d 0x8000` schreibt `-65536` statt `-32768`) sowie ungerade `dbf`-Sprungziele (`51ca ffe7` → sofortiger Address-Error-Crash auf der V4).
  - **Pflicht für C-Test-Harnesse:** Immer `-m68020 -O2` (oder `-O0`) für C-Dateien nutzen und gegen die mit `vasmm68k_mot -m68080` assemblierten `.o`-Dateien linken.
- **Amiga-GCC `int` ist 16 Bit** (ohne explizite Breite); für PCM-Akkumulatoren und Zähler > 32767 immer `int32_t` / `uint32_t` verwenden.
- **`decoder_5_3ms.s` ABI-Besonderheit:** Erwartet als 5. Parameter (`block_cnt`) die **Anzahl der M/S-Paare (Bytes)** (`15` pro 15-Byte-Block), während alle anderen Decoder die Anzahl der 15-Byte-Blöcke (`1`) erwarten.
- 100×-MIN/Median ist die Referenz; AVG/Max sind durch Interrupts verrauscht.
- Kandidaten unter `experiments/` ablegen, nie direkt experimentieren.
- Ergebnisse unter `tests/auto-results/` sind generiert (gitignored).
- Doku: `tests/OPTIMIZATION.md`, `tests/AUTOMATION.md`, `tests/AUDIO_COMPARE.md`.

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

Vergleichsbasis: `tests/ORIGINAL_VS_SIGNED.md` (echte `experiments/*_orig.s`, nicht die signierte Produktion).
