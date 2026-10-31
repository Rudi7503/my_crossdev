# Contributing to the 68080 Best-Practice Database

This file is the entry point for human developers **and AI agents** who want to
extend the database without inventing measurements or breaking the corpus.

## 1. What this repository is

`68080-db/` is a measured best-practice database for the Apollo 68080
(Vampire V4). It contains:

- `seq/<ID>.i` — canonical, self-contained measurement sequences
- `harness-v5.asm` — the canonical harness (100 warmup + 100 measured runs)
- `cores/core-11350/` — canonical logs and summaries for the current V4
- `measurements/v5-files/` — local canonical v5 log set
- `README.md` / `index.md` — rules and the complete entry index
- `RUN-ALL` / `RUN-ALL-ONCE` — 10× / 1× batch for all entries

Current canonical state: **77 entries, 770 v5 logs**, Apollo Core **11350**,
PCR 1088/1, VCoreRev 0, **92 MHz**. Only **Harness v5** is canonical; do not
add v1–v4 data.

## 2. Read before changing anything

1. `README.md` — rules, status legend, measurement protocol
2. `index.md` — all IDs and the current count
3. `09-harness.md` — harness internals (the historical sections are context,
   not a second data set)
4. `tools/check_sequences.py` — the hard register/label contract
5. One recent entry, e.g. `seq/BLT-005.i` and `24-pmula-unroll-256.md`
6. `cores/core-11350/CORE-INFO.md` — core identity and log layout

## 3. Adding a new entry

### 3.1 Choose an ID and category

Use an existing prefix (`FUS`, `BND`, `AMX`, `REG`, `TIM`, `MEM`, `PAT`,
`ANTI`, `MAT`, `FPU`, `FIX`, `FFT`, `DCT`, `BLT`) and the next free number.
The file must be `seq/<ID>.i`.

### 3.2 Write the sequence

A sequence is **included 11 times** into the harness: one copy per
`measure_100` block (warmup, measurement) and one in the PERF pass. The
harness resets the scratch pointers between those phases; within one phase
there are 100 consecutive executions. A sequence therefore runs at most
100 times per phase, 310 times total across the binary. It must be
self-contained and branch-free:

```asm
; seq/<ID>.i - short title
; Uses: e0, a0, a1
; MUST NOT touch: d5, d6, d7, a5, a6, a7
        load    (a0)+,e0
        store   e0,(a1)+
```

Rules:

- No labels, no `bra`/`bsr`/`dbf`, no `section`, no `include`, no `rts`.
- `d5`, `d6`, `d7`, `a5`, `a6`, `a7` are reserved by the harness.
- Only `d0–d4`, `a0–a4`, `e0–e23` are scratch.
- Inputs are the five harness scratch buffers `scratch_a` … `scratch_e`
  (4 KiB each, 8-byte aligned); `a0`–`a4` point to them.
- A sequence that advances `a0` by N bytes must satisfy `100 × N ≤ 4096`
  within one phase (for example 32 B × 100 = 3200 B). Document the buffer
  size and any data-dependent assumptions.
- `load`, `store`, `pmula` and the other AMMX mnemonics are vasm/AMMX
  instructions, not repository macros.
- The sequence is a **measurement kernel**, not a complete application.

### 3.3 Validate and build

```bash
cd 68080-db
python3 tools/check_sequences.py seq/
VASM=/path/to/vasmm68k_mot ./tools/build_all.sh
# or build all with the default build-v5 layout:
HARNESS=harness-v5.asm BUILD_DIR=build-v5 BIN_DIR=bin-v5 ./tools/build_all.sh
```

`build_all.sh` uses `VASM` if set, otherwise a local toolchain path. It never
links the sequence; the generated `<BIN_DIR>/<ID>` is directly runnable.

### 3.4 Measure on real hardware

The canonical method is 10 runs per entry, each with 100 warmup + 100 measured
iterations:

```bash
# upload bin-v5/* and RUN-ALL (see README for ACP paths)
# on the V4:
Execute "Ram Disk:RUN-ALL"
```

Result: `Ram Disk:LOGS/<ID>.run01.log` … `.run10.log`.

### 3.5 Install and validate the logs

```bash
python3 tools/check_log.py measurements/v5-files/<ID>.run01.log
python3 tools/repeat_report.py measurements/v5-files
python3 tools/perf_report.py measurements/v5-files
```

`measurements/v5-files/` is the local canonical set. `cores/core-11350/logs/`
is the tracked mirror used by the documentation links. Copy the new logs to
**both** and keep them byte-identical:

```text
measurements/v5-files/<ID>.runNN.log
cores/core-11350/logs/<ID>.runNN.log   # tracked, used by docs
```

All 10 logs must pass `check_log.py`. Never edit a log or fabricate a cycle
count. A measured entry needs `BEGIN`, `FILESTAT;1;0`, `CORE`, `BASELINE`,
100×`RES`, `SUMMARY`, `PERF` and `END`.

### 3.6 Document and index

- Add/update the category document (`01`…`25`).
- Show the canonical sequence verbatim.
- Link **all 10 raw logs** from the tracked `cores/core-11350/logs/` path.
- Separate canonical microbenchmark numbers from separate project/suite runs.
- Add a row to `index.md`.
- Update `README.md`, `CORE-INFO.md`, `REPORT.md`, `HANDOFF.md` and all counts
  (77 entries / 770 logs).
- State the exact core: Apollo 11350, PCR 1088/1, VCoreRev 0, 92 MHz.

### 3.7 Commit

```bash
git add 68080-db
git commit -m "68080 DB: add <ID> <short description>"
git push origin main
```

## 4. If you have no V4 hardware

A contributor without a Vampire can still help, but must not claim a
measurement:

- Add the sequence and static analysis.
- Add a **C reference** and a correctness test.
- Mark the entry `UNVERIFIZIERT (nicht gemessen)`.
- Do **not** add `measurements/` or `cores/.../logs/` files.
- Do **not** invent or copy cycle counts from another entry.
- A hardware owner must run the canonical 10× measurement and add the logs
  before the status becomes `gemessen`.

## 5. What not to change

- Do not add v1–v4 data or revive deleted harnesses.
- Do not change `harness-v5.asm` without a migration note and a full rerun.
- Do not mix project/suite measurements into the 77-entry canonical corpus.
- Do not rewrite historical sections of `09-harness.md`; add a note instead.
- Do not edit `cores/core-11350/PERF-SUMMARY.md` or `CYCLE-SUMMARY.md` by hand;
  regenerate them with the tools.

## 6. Review checklist for a foreign AI

- [ ] `check_sequences.py` is green.
- [ ] `check_docs.py` is green.
- [ ] `build_all.sh` builds the new entry.
- [ ] 10 logs exist and all pass `check_log.py`.
- [ ] The numbers in the doc match the logs (MIN, not AVG).
- [ ] All 10 raw-log links resolve under `cores/core-11350/logs/`.
- [ ] `index.md`, counts and core identity are consistent.
- [ ] No v1–v4 artifacts, no invented data, no untracked binaries committed.

## 7. Useful commands

```bash
cd 68080-db
python3 tools/check_sequences.py seq/
python3 tools/check_docs.py
python3 tools/check_log.py measurements/v5-files/<ID>.run01.log
python3 tools/repeat_report.py measurements/v5-files
python3 tools/perf_report.py measurements/v5-files
./tools/build_all.sh
```

For V4 access (ACP/ASH, IP, paths) see [`V4-ACCESS.md`](V4-ACCESS.md).
