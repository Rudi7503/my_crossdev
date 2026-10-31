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
SRC=${AMIGA_LOGDIR:-"Ram Disk:LOGS"}
ACP=${ACP:-"$ROOT/../Projects/ApolloExplorer/acp/acp"}

if [[ ! -x "$ACP" ]]; then
    echo "error: ACP not found or not executable: $ACP" >&2
    echo "set ACP=/path/to/acp if it lives elsewhere" >&2
    exit 1
fi

mkdir -p "$DEST"
echo "downloading $HOST:$SRC -> $DEST"
"$ACP" -r "$HOST:$SRC" "$DEST"

# ACP may copy the directory as DEST/LOGS; flatten that one level.
if [[ -d "$DEST/LOGS" ]]; then
    find "$DEST/LOGS" -maxdepth 1 -type f -exec mv -t "$DEST" {} +
    rmdir "$DEST/LOGS" 2>/dev/null || true
fi

count=$(find "$DEST" -maxdepth 1 -type f -name '*.log' | wc -l)
echo "downloaded $count log file(s) to $DEST"
