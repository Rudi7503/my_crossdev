# Harness v5 – Zählerlauf (2026-10-24)

**Status:** v5 ist kanonisch; 420 V4-Dateilogs, kein Crash. Einträge und
`SUMMARY.md` sind auf v5 umgestellt (inkl. IEP-Spalten); v4 ist Archiv.

## Ergebnisse

- **420/420** V4-Dateilogs gültig (`check_log.py`), alle `FILESTAT;1;0`.
- `PERF` bei allen 420 Läufen vorhanden.
- **MIN identisch mit v4** (42/42); der Taktcode ist unverändert.
- Kein Serial-Mitschnitt in diesem Lauf; die V4-Dateilogs sind vollständig.
- Der frühere Crash lag am Stack-Code: `read_perf_to_stack` pushte 7 Werte
  und machte dann `rts` – dadurch wurde DCM als Rücksprungadresse benutzt.
  Jetzt werden die Startwerte **inline** gepusht; `FUS-001`, `AMX-001` und
  `AMX-005` (C2P) laufen durch.

## Pipe-Zähler

Die Tabelle steht in [`PERF-v5.md`](PERF-v5.md). Kurzbefund:

- `IEP1`/`IEP2` liefern plausible Werte; `IEP3` ist in allen 42 Einträgen 0.
- `BPC`, `BPW`, `DCH`, `DCM` sind in allen 42 Einträgen 0. Für dieses Korpus
  (keine Branches in den Sequenzen) ist BPW=0 plausibel; DCH/DCM=0 ist
  auffällig und wird **nicht** als Cache-Auswertung interpretiert.
- Der Median der IEP-Summe liegt bei 4,5 Pipe-Slots pro Lauf.
- Beispiele: FUS-001 `IEP1 4 / IEP2 0`, AMX-001 `4 / 0`,
  AMX-002 `6 / 0`, MEM-004 `10 / 0`, PAT-005 `10 / 0`,
  TIM-005 `3 / 0`, ANTI-002 `4 / 1`.

Die IEP-Zahlen sind **Pipe-Slots**, keine 1:1-Zählung der 68k-Quellbefehle
(FUS-001 hat zwei Befehle, liefert aber 4 Slots).

## Übernahme

Die 42 Einträge und `measurements/SUMMARY.md` stehen auf v5 und enthalten
die IEP-Pipe-Slots. v1–v4 bleiben als Archiv erhalten.
