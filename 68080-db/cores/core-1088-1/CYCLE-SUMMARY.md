# Messübersicht – 42 Einträge (Harness v5)

**Lauf:** Vampire V4, 10 Wiederholungen je Eintrag (420 Dateilogs).  
**Harness:** **v5** – v4-Taktmessung + separater PERF-Zählerdurchgang.  
**Referenz:** MIN. MAX/AVG sind **Mediane der 10 Läufe**.  
**Pipe-Slots:** IEP1/IEP2/IEP3 pro Lauf (Median der 10 PERF-Werte).  
**Baseline:** nach dem Warmup gemessen; **nicht subtrahierbar**.  
**Daten:** `v5-files/<ID>.runNN.log`; `PERF` in jedem Log.

## Streuung und Pipe-Slots

- **MIN** – kleinster Wert über alle 10 × 100 `RES`-Läufe; in allen 42 Einträgen in jeder Wiederholung identisch.
- **Median / MAX / AVG / Mittelwert / Stdabw. / p95** – jeweils der **Median der 10 Läufe**.
- **IEP1/2/3** – Pipe-Slots pro Lauf, Median der 10 `PERF`-Summen/100.
- **IEP3** ist in allen 42 Einträgen 0; `IEPΣ` ist die Summe der drei Slots.
- `BPC`, `BPW`, `DCH`, `DCM` sind in allen 42 Einträgen 0 und werden nicht als Cache-/Branch-Auswertung interpretiert.
- **Warmup:** Bei 41/42 Einträgen sind die ersten 10 Läufe exakt MIN; Ausnahme mit einzelnen Ausreißern: `TIM-004`.
- **Baseline nicht abziehen:** `NET = RES − BASELINE` ist kein gültiger Sequenzwert.

## Alphabetisch

| ID | MIN | Median | MAX | AVG | Mittelwert | Stdabw. | p95 | BASELINE | IEP1 | IEP2 | IEP3 | IEPΣ | Log |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---|
| [`AMX-001`](v5-files/AMX-001.run01.log) | 3 | 3 | 23 | 3 | 3.21 | 2.15 | 3 | 5 | 4 | 0 | 0 | 4 | [Log](v5-files/AMX-001.run01.log) |
| [`AMX-002`](v5-files/AMX-002.run01.log) | 5 | 5 | 7 | 5 | 5.03 | 0.21 | 5 | 5 | 6 | 0 | 0 | 6 | [Log](v5-files/AMX-002.run01.log) |
| [`AMX-003`](v5-files/AMX-003.run01.log) | 5 | 5 | 7 | 5 | 5.02 | 0.20 | 5 | 5 | 6 | 0 | 0 | 6 | [Log](v5-files/AMX-003.run01.log) |
| [`AMX-004`](v5-files/AMX-004.run01.log) | 7 | 7 | 7 | 7 | 7.00 | 0.00 | 7 | 5 | 8 | 0 | 0 | 8 | [Log](v5-files/AMX-004.run01.log) |
| [`AMX-005`](v5-files/AMX-005.run01.log) | 4 | 4 | 14.5 | 4 | 4.17 | 1.26 | 4 | 5 | 5 | 0 | 0 | 5 | [Log](v5-files/AMX-005.run01.log) |
| [`AMX-006`](v5-files/AMX-006.run01.log) | 5 | 5 | 5 | 5 | 5.00 | 0.00 | 5 | 5 | 6 | 0 | 0 | 6 | [Log](v5-files/AMX-006.run01.log) |
| [`ANTI-001`](v5-files/ANTI-001.run01.log) | 9 | 9 | 9 | 9 | 9.00 | 0.00 | 9 | 5 | 5 | 0 | 0 | 5 | [Log](v5-files/ANTI-001.run01.log) |
| [`ANTI-002`](v5-files/ANTI-002.run01.log) | 3 | 3 | 3 | 3 | 3.00 | 0.00 | 3 | 5 | 4 | 1 | 0 | 5 | [Log](v5-files/ANTI-002.run01.log) |
| [`ANTI-003`](v5-files/ANTI-003.run01.log) | 3 | 3 | 3 | 3 | 3.00 | 0.00 | 3 | 5 | 4 | 0 | 0 | 4 | [Log](v5-files/ANTI-003.run01.log) |
| [`ANTI-004`](v5-files/ANTI-004.run01.log) | 8 | 8 | 61.5 | 10 | 10.30 | 8.37 | 29.5 | 5 | 21 | 0 | 0 | 21 | [Log](v5-files/ANTI-004.run01.log) |
| [`ANTI-005`](v5-files/ANTI-005.run01.log) | 3 | 3 | 32 | 3 | 3.29 | 2.90 | 3 | 5 | 0 | 0 | 0 | 0 | [Log](v5-files/ANTI-005.run01.log) |
| [`ANTI-006`](v5-files/ANTI-006.run01.log) | 6 | 6 | 6 | 6 | 6.00 | 0.00 | 6 | 5 | 7 | 0 | 0 | 7 | [Log](v5-files/ANTI-006.run01.log) |
| [`BND-001`](v5-files/BND-001.run01.log) | 2 | 2 | 2 | 2 | 2.00 | 0.00 | 2 | 5 | 3 | 1 | 0 | 4 | [Log](v5-files/BND-001.run01.log) |
| [`BND-002`](v5-files/BND-002.run01.log) | 3 | 3 | 3 | 3 | 3.00 | 0.00 | 3 | 5 | 4 | 0 | 0 | 4 | [Log](v5-files/BND-002.run01.log) |
| [`BND-003`](v5-files/BND-003.run01.log) | 2 | 2 | 2 | 2 | 2.00 | 0.00 | 2 | 5 | 3 | 1 | 0 | 4 | [Log](v5-files/BND-003.run01.log) |
| [`BND-004`](v5-files/BND-004.run01.log) | 7 | 7 | 37 | 10 | 10.21 | 8.63 | 33.5 | 5 | 21 | 0 | 0 | 21 | [Log](v5-files/BND-004.run01.log) |
| [`BND-005`](v5-files/BND-005.run01.log) | 3 | 3 | 3 | 3 | 3.00 | 0.00 | 3 | 5 | 4 | 1 | 0 | 5 | [Log](v5-files/BND-005.run01.log) |
| [`FUS-001`](v5-files/FUS-001.run01.log) | 3 | 3 | 3 | 3 | 3.00 | 0.00 | 3 | 5 | 4 | 0 | 0 | 4 | [Log](v5-files/FUS-001.run01.log) |
| [`FUS-002`](v5-files/FUS-002.run01.log) | 3 | 3 | 3 | 3 | 3.00 | 0.00 | 3 | 5 | 4 | 0 | 0 | 4 | [Log](v5-files/FUS-002.run01.log) |
| [`FUS-003`](v5-files/FUS-003.run01.log) | 3 | 3 | 3 | 3 | 3.00 | 0.00 | 3 | 26 | 4 | 0 | 0 | 4 | [Log](v5-files/FUS-003.run01.log) |
| [`FUS-004`](v5-files/FUS-004.run01.log) | 3 | 3 | 13.5 | 3 | 3.10 | 1.05 | 3 | 5 | 4 | 0 | 0 | 4 | [Log](v5-files/FUS-004.run01.log) |
| [`FUS-005`](v5-files/FUS-005.run01.log) | 2 | 2 | 13 | 2 | 2.15 | 1.17 | 2 | 26 | 21 | 0 | 0 | 21 | [Log](v5-files/FUS-005.run01.log) |
| [`MEM-001`](v5-files/MEM-001.run01.log) | 3 | 3 | 8 | 3 | 3.06 | 0.54 | 3 | 5 | 21 | 0 | 0 | 21 | [Log](v5-files/MEM-001.run01.log) |
| [`MEM-002`](v5-files/MEM-002.run01.log) | 5 | 5 | 5 | 5 | 5.00 | 0.00 | 5 | 5 | 6 | 0 | 0 | 6 | [Log](v5-files/MEM-002.run01.log) |
| [`MEM-003`](v5-files/MEM-003.run01.log) | 3 | 3 | 3 | 3 | 3.00 | 0.00 | 3 | 5 | 6 | 0 | 0 | 6 | [Log](v5-files/MEM-003.run01.log) |
| [`MEM-004`](v5-files/MEM-004.run01.log) | 5 | 5 | 5 | 5 | 5.00 | 0.00 | 5 | 5 | 10 | 0 | 0 | 10 | [Log](v5-files/MEM-004.run01.log) |
| [`MEM-005`](v5-files/MEM-005.run01.log) | 3 | 3 | 4 | 3 | 3.01 | 0.10 | 3 | 5 | 4 | 0 | 0 | 4 | [Log](v5-files/MEM-005.run01.log) |
| [`PAT-001`](v5-files/PAT-001.run01.log) | 5 | 5 | 31.5 | 5 | 5.89 | 4.40 | 5 | 5 | 6 | 0 | 0 | 6 | [Log](v5-files/PAT-001.run01.log) |
| [`PAT-002`](v5-files/PAT-002.run01.log) | 2 | 2 | 29 | 2 | 2.38 | 3.13 | 2 | 5 | 21 | 0 | 0 | 21 | [Log](v5-files/PAT-002.run01.log) |
| [`PAT-003`](v5-files/PAT-003.run01.log) | 2 | 2 | 14 | 2 | 2.12 | 1.21 | 2 | 5 | 3 | 1 | 0 | 4 | [Log](v5-files/PAT-003.run01.log) |
| [`PAT-004`](v5-files/PAT-004.run01.log) | 7 | 7 | 7 | 7 | 7.00 | 0.00 | 7 | 5 | 8 | 0 | 0 | 8 | [Log](v5-files/PAT-004.run01.log) |
| [`PAT-005`](v5-files/PAT-005.run01.log) | 9 | 9 | 50 | 10 | 10.69 | 7.06 | 24 | 5 | 10 | 0 | 0 | 10 | [Log](v5-files/PAT-005.run01.log) |
| [`REG-001`](v5-files/REG-001.run01.log) | 3 | 3 | 3 | 3 | 3.00 | 0.00 | 3 | 5 | 4 | 0 | 0 | 4 | [Log](v5-files/REG-001.run01.log) |
| [`REG-002`](v5-files/REG-002.run01.log) | 3 | 3 | 3 | 3 | 3.00 | 0.00 | 3 | 5 | 4 | 0 | 0 | 4 | [Log](v5-files/REG-002.run01.log) |
| [`REG-003`](v5-files/REG-003.run01.log) | 3 | 3 | 3 | 3 | 3.00 | 0.00 | 3 | 5 | 4 | 0 | 0 | 4 | [Log](v5-files/REG-003.run01.log) |
| [`REG-004`](v5-files/REG-004.run01.log) | 2 | 2 | 2 | 2 | 2.00 | 0.00 | 2 | 5 | 3 | 0 | 0 | 3 | [Log](v5-files/REG-004.run01.log) |
| [`REG-005`](v5-files/REG-005.run01.log) | 3 | 3 | 3 | 3 | 3.00 | 0.00 | 3 | 5 | 4 | 0 | 0 | 4 | [Log](v5-files/REG-005.run01.log) |
| [`TIM-001`](v5-files/TIM-001.run01.log) | 3 | 3 | 3 | 3 | 3.00 | 0.00 | 3 | 5 | 4 | 0 | 0 | 4 | [Log](v5-files/TIM-001.run01.log) |
| [`TIM-002`](v5-files/TIM-002.run01.log) | 2 | 2 | 2 | 2 | 2.00 | 0.00 | 2 | 5 | 3 | 0 | 0 | 3 | [Log](v5-files/TIM-002.run01.log) |
| [`TIM-003`](v5-files/TIM-003.run01.log) | 3 | 3 | 3 | 3 | 3.00 | 0.00 | 3 | 5 | 4 | 0 | 0 | 4 | [Log](v5-files/TIM-003.run01.log) |
| [`TIM-004`](v5-files/TIM-004.run01.log) | 7 | 7 | 63.5 | 9.5 | 10.09 | 10.29 | 29.5 | 5 | 20 | 0 | 0 | 20 | [Log](v5-files/TIM-004.run01.log) |
| [`TIM-005`](v5-files/TIM-005.run01.log) | 5 | 5 | 7 | 5 | 5.02 | 0.20 | 5 | 5 | 3 | 0 | 0 | 3 | [Log](v5-files/TIM-005.run01.log) |

## Archiv

v1-Logs: `measurements/<ID>.log`; v1-Tabelle: `SUMMARY-v1.md`.  
v4-Tabelle: `SUMMARY-v4.md`; v4-Logs: `measurements/v4/` und `v4-files/`.
