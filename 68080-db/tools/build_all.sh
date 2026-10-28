#!/bin/bash
# Build every seq/<ID>.i into <BIN_DIR>/<ID> with an Apollo/AMMX-capable vasm.
#
# Usage:
#   ./tools/build_all.sh            # build all entries (build/ + bin/)
#   BUILD_DIR=build-v2 BIN_DIR=bin-v2 ./tools/build_all.sh
#   VASM=/path/to/vasmm68k_mot ./tools/build_all.sh
#
# Requirements:
#   - vasmm68k_mot with 68080/AMMX support (tested: vasm 2.0d, 1.8c)
#   - python3 (sequence guard only)
#
# The generated <BUILD_DIR>/<ID>.asm is kept for inspection.  It is the
# unchanged harness plus the inlined sequence; no linker and no startup
# library needed.
set -euo pipefail

ROOT=$(cd -- "$(dirname -- "$0")/.." && pwd)
cd "$ROOT"

BUILD_DIR=${BUILD_DIR:-build}
BIN_DIR=${BIN_DIR:-bin}
HARNESS=${HARNESS:-harness.asm}

if [[ -n "${VASM:-}" ]]; then
    :
elif command -v vasmm68k_mot >/dev/null 2>&1; then
    VASM=$(command -v vasmm68k_mot)
else
    # Common local installs; override with VASM=... if yours differs.
    for cand in \
        "$HOME/Vampire_V4/ApolloCrossDev/Compilers/GCC-6.50-Latest/bin/vasmm68k_mot" \
        "$HOME/Vampire_V4/ApolloCrossDev/Compilers/GCC-6.50-Dev/bin/vasmm68k_mot" \
        "/opt/m68k-amigaos/bin/vasmm68k_mot" \
        "/usr/local/bin/vasmm68k_mot"
    do
        if [[ -x "$cand" ]]; then
            VASM=$cand
            break
        fi
    done
fi

if [[ -z "${VASM:-}" || ! -x "$VASM" ]]; then
    echo "error: vasmm68k_mot not found. Set VASM=/path/to/vasmm68k_mot." >&2
    exit 1
fi

mkdir -p "$BUILD_DIR" "$BIN_DIR"
python3 tools/check_sequences.py seq/

# Preflight: make sure this vasm accepts -m68080 and the CCC mnemonic.
tmpdir=$(mktemp -d)
trap 'rm -rf "$tmpdir"' EXIT
cat >"$tmpdir/probe.asm" <<'EOF'
        section code,code
_start:
        movec.l CCC,d0
        rts
EOF
if ! "$VASM" -quiet -Fhunkexe -m68080 -o "$tmpdir/probe" "$tmpdir/probe.asm" \
        >"$tmpdir/probe.out" 2>&1; then
    echo "error: $VASM does not build 68080 code with MOVEC CCC." >&2
    cat "$tmpdir/probe.out" >&2
    exit 1
fi

count=0
: >"$BUILD_DIR/manifest.txt"
for seq in seq/*.i; do
    [[ -e "$seq" ]] || continue
    id=$(basename "$seq" .i)
    out="$BUILD_DIR/${id}.asm"
    sed -e "s|; @@INCLUDE_SEQ@@|        include \"$seq\"|" \
        -e "s|@@ID@@|${id}|g" \
        "$HARNESS" >"$out"
    "$VASM" -quiet -Fhunkexe -m68080 -o "$BIN_DIR/${id}" "$out"
    printf '%s\n' "$id" >>"$BUILD_DIR/manifest.txt"
    count=$((count + 1))
done

echo "built $count entries with $VASM (build=$BUILD_DIR bin=$BIN_DIR)"
"$VASM" </dev/null 2>&1 | head -n 4 || true
