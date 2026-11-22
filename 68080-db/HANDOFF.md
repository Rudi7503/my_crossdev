# HANDOFF – 68080 Best-Practice-Datenbank

**Stand:** 2026-11-22  
**Einstieg für Menschen und fremde KIs:** [`CONTRIBUTING.md`](CONTRIBUTING.md)

## Aktueller kanonischer Zustand

- **Kanonischer Harness:** **Harness v6** (`harness-v6.asm`, `BASE_10 = 30`, 10×10 Messläufe mit Warmup + separatem `PERF`-Pass unter `Disable()`/`Enable()` — ersetzt den älteren Harness v5; historische v5-Baselines bleiben in `cores/core-11350/logs/` archiviert).
- **Core:** Apollo 68080 **11350**, PCR **1088/1**, ~**85–92 MHz** (reale V4-Taktfrequenz je Core-Build)
- **Sequenzen:** `seq/<ID>.i` (inkl. `HUR-001`–`HUR-074` für Hurrican-V4-Optimierungen)
- **v6-Logs:** `log-v6/<ID>.log` (Hurrican & neue Benchmarks), `build-v6/`, `bin-v6/`
- **Historische v5-Logs:** `cores/core-11350/logs/<ID>.runNN.log` (77 × 10 Baselines)
- **Summaries:** `cores/core-11350/CYCLE-SUMMARY.md`, `PERF-SUMMARY.md`, `34-complete-harness-v6-reference.md`
- **Golden-/Suite-Logs:** `cores/core-11350/golden/`, `cores/core-11350/suites/`
- **Validierung:** `tools/check_log.py`, `tools/build_hur_v6.py`

## Kanonische Befehle (Harness v6)

```bash
cd 68080-db
python3 tools/check_sequences.py seq/
python3 tools/build_hur_v6.py HUR-067 HUR-068
```

## Wichtige Regeln

1. **Harness v6** (`harness-v6.asm`) ist für alle neuen Messungen kanonisch (v5-Logs sind nur noch historisches Archiv).
2. Messwert ist **MIN** (bei deterministischen L1-Kernels) bzw. **MIN / AVG / MAX** bei DDR3-/FastRAM-Messungen; Baseline (`BASE_10 = 30`) wird für Netto-Zyklen abgezogen.
3. Jede Zahl im Dokument muss aus dem verlinkten Rohlog stammen.
4. Jede Code-Optimierung wird vor Übernahme auf echter V4-Hardware **100 % bit-gleich** (`0 Mismatches`) verifiziert.
5. Kanonische Mikrobenchmarks und separate Suite-/Golden-/Cold-FastRAM-Tests klar trennen.
6. Keine erfundenen Werte, keine handeditieren Summaries.

## Aktuelle Schwerpunkte

- `27-hurrican-blitting.md`: Hurrican V4-Optimierungen `HUR-001`–`HUR-074` + Cold-FastRAM DDR3-Burst-Analyse (64 B Copy, 32 B Key, 2-Quad Add)
- `34-complete-harness-v6-reference.md`: Vollständige Harness-v6-Referenz
- `35-saga-display-and-bandwidth.md`: SAGA-Bildausgabe (VBL, `$DFF1EC`, `VPOSR`, Double- vs. Triple-Buffering) & Speicherbandbreite unter SAGA-Display-DMA (`640×480×16` vs. `960×540×16`)
- `24-pmula-unroll-256.md` / `25-pmula-fullhd.md`: AMMX `pmula`-Alpha-Blending
- `22-sprite-alpha-pmula-golden.md` / `23-rgb565-alpha.md`: Bit-exakte Alpha-Verifikation (`diff=0`)

## Offene Punkte (Roadmap)

1. **GCC-68080 C-Muster (OpenSWOS):** Schlechte GCC-Patterns (u. a. 64-Bit-Mul/Div-Libcalls `__muldi3`/`__divdi3`), FPU vs. Integer auf 68080, Abgleich mit `ApolloTeam-dev/ApolloKnowledgeBase`.
2. **AmigaOS-Jitter / Hintergrund-Tasks:** Messung der 20–30 ms Ausreißer durch laufende Workbench-Tools/Uhren vs. Task-Priorität / `Forbid()`.
3. Weitere Core-/Boot-Reproduktion & 68060-Vergleichsmessungen.
