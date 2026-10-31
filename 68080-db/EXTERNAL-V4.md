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

On your V4 run [`tools/CAPTURE-CORE`](tools/CAPTURE-CORE), or execute:

```text
C:ApolloControl CORE
C:ApolloControl HZ
C:Version
C:Type ENV:VCoreRev
```

Save:

```text
cores/core-<ApolloCore>/CORE-IDENTITY.txt
cores/core-<ApolloCore>/CORE-INFO.md
```

Use [`templates/CORE-INFO.md`](templates/CORE-INFO.md).
For ACP/ASH paths and `AMIGAHOST` see [`V4-ACCESS.md`](V4-ACCESS.md).

> The `CORE;...` line in a harness log is the **PCR** identity, not the
> ApolloControl core build number. Both must be recorded.

## 3. Measure

Upload `bin-v5/*` and `RUN-ALL`, then on the V4:

```text
Execute "Ram Disk:RUN-ALL"
```

Download `Ram Disk:LOGS` to:

```text
cores/core-<ApolloCore>/logs/
```

Foreign-core logs stay **only** there. Do **not** copy them into
`measurements/v5-files/` and do **not** add them to `entries.yaml`; that
manifest is the canonical 11350 corpus. The static CI validates
`cores/core-11350/` and therefore does not validate foreign-core logs;
validate them locally with `check_log.py`.

`RUN-ALL` is a tracked file in this repository; `build_all.sh` does not
generate it. Upload both the binaries and the tracked batch:

```bash
ACP="/path/to/acp"
"$ACP" bin-v5/* "$AMIGAHOST:RAM/"
"$ACP" RUN-ALL "$AMIGAHOST:RAM/"
```

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
