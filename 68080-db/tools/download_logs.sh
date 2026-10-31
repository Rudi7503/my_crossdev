#!/bin/bash
# Download the V4-side file logs (Ram Disk:LOGS) to the PC.
#
# Usage:
#   export AMIGAHOST=apollo          # host name used by ACP
#   ./tools/download_logs.sh [DEST]  # default: measurements/v5-files
#
# The V4-side RUN-ALL writes <ID>.runNN.log into Ram Disk:LOGS and renames
# each file after the run.  Download before rerunning RUN-ALL, because the
# batch deletes old LOGS/*.log at start.
set -euo pipefail

ROOT=$(cd -- "$(dirname -- "$0")/.." && pwd)
DEST=${1:-"$ROOT/measurements/v5-files"}
HOST=${AMIGAHOST:?Set AMIGAHOST, e.g. export AMIGAHOST=apollo}
SRC=${AMIGA_LOGDIR:-"RAM:LOGS"}
ACP=${ACP:-"$ROOT/../Projects/ApolloExplorer/acp/acp"}

if [[ ! -x "$ACP" ]]; then
    echo "error: ACP not found or not executable: $ACP" >&2
    echo "set ACP=/path/to/acp if it lives elsewhere" >&2
    exit 1
fi

mkdir -p "$DEST"

# Refuse to start if the source is not the log directory.  ACP can otherwise
# fall back to the volume root and copy binaries instead of logs.
LIST=$("$ACP" -d "$HOST:$SRC" 2>&1 | tr -d '\r' || true)
if ! printf '%s
' "$LIST" | grep -qE '\.run[0-9][0-9]\.log'; then
    echo "error: no log files at $HOST:$SRC" >&2
    echo "       expected <ID>.runNN.log; check AMIGA_LOGDIR/RAM:LOGS" >&2
    exit 1
fi

# Windows ACP cannot create POSIX paths such as /tmp/foo.  When ACP is a
# Windows .exe under WSL, hand it the UNC form of the WSL destination.
ACP_DEST="$DEST"
if [[ "$ACP" == *.exe || "$ACP" == *.EXE ]] && command -v wslpath >/dev/null 2>&1; then
    ACP_DEST=$(wslpath -w "$DEST")
fi
echo "downloading $HOST:$SRC -> $DEST"
"$ACP" -r "$HOST:$SRC" "$ACP_DEST"

# ACP may copy the directory as DEST/LOGS; flatten that one level.
if [[ -d "$DEST/LOGS" ]]; then
    find "$DEST/LOGS" -maxdepth 1 -type f -exec mv -t "$DEST" {} +
    rmdir "$DEST/LOGS" 2>/dev/null || true
fi

count=$(find "$DEST" -maxdepth 1 -type f -name '*.log' | wc -l)
echo "downloaded $count log file(s) to $DEST"
if [[ "$count" -eq 0 ]]; then
    echo "error: no logs downloaded (ACP can return 0 on path errors)" >&2
    exit 1
fi
