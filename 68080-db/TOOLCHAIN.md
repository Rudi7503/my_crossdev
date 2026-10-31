# Toolchain for 68080-db

The database build and its static checks do not need the Amiga NDK. Only
`vasm` (68080/AMMX capable) and Python 3 are required for building/checking.

## 1. Python

```bash
python3 --version        # 3.10+ recommended
python3 -m pip install -r tools/requirements.txt
```

`pyserial` is only needed for the optional serial capture; it is not needed
for `check_sequences.py`, `check_docs.py` or `check_log.py`.

## 2. vasm

Use **vasm 2.0d or newer** with the M68k/Mot syntax backend, including 68080
and AMMX support. From the ApolloCrossDev root, the normal installation is:

```bash
./GCC-6.50-Dev-Linux.sh       # Linux
# or the matching macOS script
```

Then point the DB at the binary:

```bash
export VASM="$PWD/Compilers/GCC-6.50-Dev/bin/vasmm68k_mot"
```

If you already have vasm:

```bash
export VASM=/path/to/vasmm68k_mot
```

The build script does not link the sequence; it assembles the self-contained
harness plus the inlined sequence into `bin-v5/<ID>`.

### Minimum verification

```bash
cd 68080-db
VASM="$VASM" HARNESS=harness-v5.asm BUILD_DIR=build-v5 BIN_DIR=bin-v5 ./tools/build_all.sh
python3 tools/check_sequences.py seq/
python3 tools/check_docs.py
```

If vasm rejects `-m68080`, `movec.l CCC,d0` or AMMX mnemonics, it is too old or
the wrong backend.

## 3. V4 access (only for real measurements)

Building and static checks work without a Vampire. A canonical measurement
needs:

- A Vampire V4 with ApolloOS
- ACP for upload
- ASH for execution
- the V4 reachable on the LAN

See [`V4-ACCESS.md`](V4-ACCESS.md). The repository's tested path is Windows
ACP/ASH; a Linux ACP from WSL may not reach the V4 LAN because of WSL NAT.

Minimal flow:

```bash
# upload bin-v5/* and RUN-ALL, then on the V4:
Execute "Ram Disk:RUN-ALL"
# download Ram Disk:LOGS to measurements/v5-files, then:
python3 tools/check_log.py measurements/v5-files/<ID>.run01.log
```

## 4. What CI does and does not do

The GitHub workflow `.github/workflows/db-check.yml` performs **static checks
only**:

- sequence guard
- documentation/count/link consistency
- validation of all tracked `cores/core-11350/logs` files

It cannot run V4 hardware. Hardware runs remain manual or use a self-hosted
V4 runner.

## 5. Common failures

| Symptom | Cause |
|---|---|
| `vasmm68k_mot not found` | set `VASM=/path/to/vasmm68k_mot` |
| `unknown mnemonic <loadq>` | `loadq` is not available in vasm 2.0d; use `load` |
| `-m68080` rejected | vasm too old or wrong CPU backend |
| `check_docs` fails on logs | local `measurements/v5-files` incomplete; core logs are the fallback in CI |
| `check_log` fails | missing `BEGIN`, `FILESTAT`, `CORE`, `BASELINE`, 100×`RES`, `SUMMARY`, `PERF` or `END` |

## 6. No NDK

`harness-v5.asm` is self-contained. The C reference tests in `Projects/` may
need the cross compiler, but they are outside the 77-entry measurement corpus.
