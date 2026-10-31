# External V4 measurement hand-off

Use this if you run the corpus on **your own Vampire V4**, not on the
maintainer's machine. There is deliberately **no self-hosted CI** in this
repository.

## 1. Build

```bash
cd 68080-db
VASM=/path/to/vasmm68k_mot \
HARNESS=harness-v5.asm BUILD_DIR=build-v5 BIN_DIR=bin-v5 \
./tools/build_all.sh
```

## 2. Capture the core identity (mandatory)

Set your own host and tools first (see [`V4-ACCESS.md`](V4-ACCESS.md)):

```bash
export AMIGAHOST=192.168.x.x
ACP="/mnt/c/Program Files/ApolloExplorer/acp.exe"
ASH="/mnt/c/Program Files/ApolloExplorer/ash.exe"
```

Upload and run the capture script:

```bash
"$ACP" tools/CAPTURE-CORE "$AMIGAHOST:RAM/"
"$ASH" "$AMIGAHOST" 'C:Protect RAM:CAPTURE-CORE +e'
"$ASH" "$AMIGAHOST" 'C:Execute RAM:CAPTURE-CORE'
"$ACP" "$AMIGAHOST:RAM/CORE-IDENTITY.txt" "cores/core-<ApolloCore>/CORE-IDENTITY.txt"
```

The script captures:

```text
C:ApolloControl CORE
C:ApolloControl HZ
C:Version
C:Type ENV:VCoreRev
```

Fill [`templates/CORE-INFO.md`](templates/CORE-INFO.md) as
`cores/core-<ApolloCore>/CORE-INFO.md`.

> The `CORE;...` line in a harness log is the **PCR** identity, not the
> ApolloControl core build number. Both must be recorded.

## 3. Measure: all 77 programs

There is exactly one canonical batch: **`RUN-ALL`**. It contains all 77
entries from `entries.yaml` (AMX, BND, BLT, DCT, …), with **10 runs per
entry**. Do **not** run individual binaries and do **not** modify the batch.

Upload all binaries and the tracked batch:

```bash
"$ACP" bin-v5/* "$AMIGAHOST:RAM/"
"$ACP" RUN-ALL "$AMIGAHOST:RAM/"
"$ASH" "$AMIGAHOST" 'C:Protect RAM:#? +e'
"$ASH" "$AMIGAHOST" 'C:Execute RAM:RUN-ALL'
```

`RAM:#?` protects every uploaded binary and the batch. The AmigaDOS side is
`Ram Disk:`; ACP addresses it as `RAM:`.

Download the logs:

```bash
AMIGAHOST="$AMIGAHOST" ACP="$ACP" ./tools/download_logs.sh cores/core-<ApolloCore>/logs
```

The downloader flattens ACP's optional `LOGS/` subdirectory and reports the
number of `.log` files.

Foreign-core logs stay **only** there. Do **not** copy them into
`measurements/v5-files/` and do **not** add them to `entries.yaml`; that
manifest is the canonical 11350 corpus. The static CI validates
`cores/core-11350/` and therefore does not validate foreign-core logs;
validate them locally with `check_log.py`.

## 4. Validate

```bash
python3 tools/check_log.py cores/core-<ApolloCore>/logs/*.log
```

All 10 logs per entry must pass. Do not merge foreign-core logs into
`cores/core-11350/` if the core or hardware differs.

## 5. Submit

Open a PR containing:

- `cores/core-<ApolloCore>/CORE-INFO.md`
- `cores/core-<ApolloCore>/CORE-IDENTITY.txt`
- `cores/core-<ApolloCore>/logs/*.log`
- the comparison/notes for the entries

Attribution/licensing: see [`NOTICE`](NOTICE).


## 6. Known constraints (read before starting)

### Exact volume name

The harness and `RUN-ALL` hardcode the AmigaDOS volume name:

```text
Ram Disk:
```

A differently named RAM disk makes the harness fail to open
`Ram Disk:LOGS/<ID>.log` (`FILESTAT;0`) and produces no file log. Use a
volume named exactly **`Ram Disk:`**, or adapt the harness/batch deliberately
(not part of the canonical workflow).

### Wait for completion

`RUN-ALL` is a long-running AmigaDOS batch. `C:Execute RAM:RUN-ALL` normally
returns after the batch finishes, but do not assume it. Verify completion by
counting the resulting files:

```bash
"$ACP" -d "$AMIGAHOST:RAM/LOGS" | grep -c '\.run[0-9][0-9]\.log$'
```

Expected: **770** logs. Only then download and validate.

### Transport and IP

- The tested path is Windows ACP/ASH; ACP from WSL may not reach the V4 LAN.
- Set `AMIGAHOST` to your own V4 IP; `acp -l` is not reliable on this setup.
- `CAPTURE-CORE` needs `C:ApolloControl`; `ENV:VCoreRev` may be absent on
  older systems. Record what the machine actually returns; do not guess.

### Hardware assumptions

The harness uses IEP1/IEP2/IEP3 and AMMX instructions unconditionally. On an
older 2-pipe or non-AMMX core it can trap or produce invalid counters. Such a
core needs a separate harness variant; do not present its output as a valid
11350 measurement.

### Correctness vs. measurement

This corpus is a **measurement** corpus. The harness does not compare every
sequence against a C reference; only dedicated golden tests (PMULA/RGB565)
do that. A foreign-core log proves that the sequence ran, not that its result
is bit-exact.

### Foreign logs are not canonical

`check_docs.py` and `entries.yaml` describe the canonical **11350 / 770-log**
corpus. Foreign-core logs live only under `cores/core-<ApolloCore>/logs/` and
are not merged into `measurements/v5-files/` or `entries.yaml`.
