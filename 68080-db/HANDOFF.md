# HANDOFF – 68080 Best-Practice-Datenbank

**Stand:** 2026-10-31  
**Einstieg für Menschen und fremde KIs:** [`CONTRIBUTING.md`](CONTRIBUTING.md)

## Aktueller kanonischer Zustand

- **77 Einträge**, **770 v5-Logs** (`77 × 10`), ausschließlich Harness v5
- **Core:** Apollo 68080 **11350**, PCR **1088/1**, VCoreRev **0**, **92 MHz**
- **Sequenzen:** `seq/<ID>.i`
- **Lokale Logs:** `measurements/v5-files/<ID>.runNN.log`
- **Getrackte Logs für Doku-Links:** `cores/core-11350/logs/<ID>.runNN.log`
- **Summaries:** `cores/core-11350/CYCLE-SUMMARY.md`, `PERF-SUMMARY.md`
- **Golden-/Suite-Logs:** `cores/core-11350/golden/`, `cores/core-11350/suites/`
- **Batch:** `RUN-ALL` (77 × 10) und `RUN-ALL-ONCE` (77 × 1)
- **Validierung:** `tools/check_log.py` für alle 770 Logs; `PERF` ohne `nan`

## Kanonische Befehle

```bash
cd 68080-db
python3 tools/check_sequences.py seq/
HARNESS=harness-v5.asm BUILD_DIR=build-v5 BIN_DIR=bin-v5 ./tools/build_all.sh
python3 tools/check_log.py measurements/v5-files/<ID>.run01.log
python3 tools/repeat_report.py measurements/v5-files
python3 tools/perf_report.py measurements/v5-files
```

## Wichtige Regeln

1. Nur **v5** ist kanonisch; keine v1–v4-Logs oder alten Harnesse hinzufügen.
2. Ein Eintrag braucht **10** valide v5-Logs und `CORE;1088;1;0` (PCR).
3. Messwert ist **MIN**, nicht AVG.
4. Jede Zahl im Dokument muss aus dem verlinkten Rohlog stammen.
5. Kanonische Mikrobenchmarks und separate Suite-/Golden-Tests klar trennen.
6. Keine erfundenen Werte, keine handeditieren Summaries.
7. Änderungen an `harness-v5.asm` erfordern einen vollständigen Neulauf aller
   Einträge.

## Aktuelle Schwerpunkte

- `24-pmula-unroll-256.md`: 4×-Register vs. Memory-VEA, 256×256
- `25-pmula-fullhd.md`: Full-HD-Alpha, 3,103 Takte/Pixel, 119 MB/s
- `22-sprite-alpha-pmula-golden.md`: 256 Alphas + 10k Zufall + 16/32/64, `diff=0`
- `23-rgb565-alpha.md`: RGB565-Alpha, skalar, `diff=0`
- `19`/`20`: kanonische BLT-001/002 plus separate 28/28-/56/56-Suite-Logs

## Offene Punkte

1. Weitere Core-/Boot-Reproduktion (Streuung zwischen Boots).
2. CCC-Breite/Überlauf experimentell prüfen.
3. 68060-Vergleichsmessungen.
4. AMMX-Laufzeitdetektion im Harness.
5. Neue Kandidaten nur mit vollständigem 10×-Lauf und Rohlogs aufnehmen.
