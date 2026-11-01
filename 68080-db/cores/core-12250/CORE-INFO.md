# Core 12250 on Toby's Apollo A6000 Unicorn

All 77 programs completed ten repetitions each. The unchanged `check_log.py` accepts all 770 logs; the unchanged `repeat_report.py` also returns zero. These are harness measurements, not exhaustive instruction correctness tests.

## Identity

- Owner: Toby; prepared and executed with Toby's coding agent.
- Board: Apollo 6000 Unicorn, A6000.
- `C:ApolloControl CORE`: `12250`.
- `C:ApolloControl HZ`: `AC68080 @ 99 MHz (x14)`.
- Every harness log: `CORE;1088;1;12250` (PCR ID / PCR revision / VCoreRev).
- OS: ApolloOS. `C:Version` reports `Kickstart 51.51, Workbench 40.0`; see the unmodified `CORE-IDENTITY.txt`.
- Date: 2026-11-01, 10:43:06 to 10:45:08 Europe/Berlin.
- Firmware, ROM and network drivers were not changed for this test.

## Reproduction

- Source: `Rudi7503/my_crossdev`, commit `a6a8216f96147808e51f0ed4ca21b2072ab2d682`.
- Assembler: vasm 2.0f, m68k backend 2.8c, native Windows executable.
- Build flags: `-quiet -Fhunkexe -m68080`.
- Build used the same include and ID substitutions as `tools/build_all.sh`, implemented on Windows. The sequence guard accepted all 77 sequences.
- `harness-v5.asm`, all sequences and `RUN-ALL` are unchanged from that commit. This includes Rudi's corrected Dn/Bn version of REG-002.
- ApolloExplorer package: 1.4.0. The Amiga server identifies itself as 1.4.
- All 77 executables and the batch were uploaded into RAM and verified byte-for-byte through the authenticated existing amiagent connection.
- Measurement command: Windows ASH 1.4.0, `C:Execute RAM:RUN-ALL`.
- One complete batch; ASH returned zero after 121.39 seconds. No network polling, screenshots or transfers during measurement. No serial capture.

## Validation

- Exactly 770 expected filenames, ten runs for every one of 77 IDs; none missing or unexpected.
- `check_log.py`: 770 accepted, zero invalid, return code zero.
- All 770 logs contain `FILESTAT;1;0`, the same core identity, 100 RES records and one PERF record.
- `repeat_report.py`: return code zero; MIN is stable across the ten repetitions of each ID.
- `perf_report.py`: return code zero; output in `PERF-SUMMARY.md`.
- `VALIDATION.json`, `CHECK-RESULT.txt` and `LOG-SHA256.json` provide the machine-readable result, original checker output and individual file hashes.

## Interpretation

Compared with the pinned repository's core-11350 reference logs, median MIN cycles are identical for 59 IDs, lower for four and higher for 14. See `COMPARISON.csv`. This is a comparison of separate boards/cores and captures, not a controlled same-board core-switch experiment.

Forty logs contain negative PERF fields after the harness baseline subtraction. The affected filenames, fields and negative values are identical to the pinned core-11350 reference. `COMPARISON.json` preserves those details. Parser acceptance does not establish the physical interpretation of these counter values; they should be reviewed before drawing IPC or pipeline-utilization conclusions.

The previously intermittent network problem did not interrupt this particular batch, the following local backup or the paced archive download. This successful session does not establish that the recurring network fault is fixed.

## Data location and attribution

Only this `cores/core-12250/` directory contains the new measurement data. The canonical core-11350 logs, `measurements/v5-files` and `entries.yaml` were not modified.

The Apollo also holds a copy at `Personal:A6000-Test-12250-20261101/`, including all logs, the raw core identity and a compressed archive.

Harness and corpus: 68080 Best-Practice Database, Rudi7503 and contributors, https://github.com/Rudi7503/my_crossdev. Documentation and data: CC BY 4.0; see the repository NOTICE and LICENSE-DOCS. Test capture by Toby with his coding agent.
