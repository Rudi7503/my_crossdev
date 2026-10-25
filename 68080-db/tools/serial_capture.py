#!/usr/bin/env python3
"""Receive benchmark lines from the Vampire and store one log per entry.

Protocol produced by harness.asm:

    BEGIN;<ID>
    BASELINE;<cycles>
    RES;<run>;<cycles>          (100 lines)
    SUMMARY;<min>;<max>;<avg>
    END;<ID>

A log is written as measurements/v2/<ID>.runNN.log.part while the entry is
running and renamed to <ID>.runNN.log after END;<ID>.  The run number is
chosen per ID from the first free slot, so repeated runs of the same entry
(RUN-ALL executes each entry 10 times) do not overwrite each other.  An
interrupted capture leaves the .part file in place, so an incomplete run is
visible instead of being silently treated as a valid measurement.
"""
from __future__ import annotations

import argparse
import os
import re
import sys
from pathlib import Path

try:
    import serial
except ImportError:  # pragma: no cover - depends on the host installation
    serial = None

SAFE_ID = re.compile(r"^[A-Za-z0-9_.-]+$")


def capture_paths(out: Path, ident: str, counters: dict[str, int]) -> tuple[Path, Path]:
    """Return (part, final) for the next free <ID>.runNN.log slot."""
    n = counters.get(ident, 1)
    while True:
        part = out / f"{ident}.run{n:02d}.log.part"
        final = out / f"{ident}.run{n:02d}.log"
        if not part.exists() and not final.exists():
            counters[ident] = n + 1
            return part, final
        n += 1


def parse_args() -> argparse.Namespace:
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--port", default="/dev/ttyUSB0", help="serial port (default: /dev/ttyUSB0)")
    p.add_argument("--baud", type=int, default=115200, help="baud rate (default: 115200)")
    p.add_argument("--out", default="measurements", help="output directory (default: measurements)")
    p.add_argument("--timeout", type=float, default=1.0, help="read timeout in seconds")
    return p.parse_args()


def main() -> int:
    args = parse_args()
    if serial is None:
        print("error: pyserial is missing; install with: python3 -m pip install pyserial", file=sys.stderr)
        return 2

    out = Path(args.out)
    out.mkdir(parents=True, exist_ok=True)

    try:
        port = serial.Serial(args.port, args.baud, timeout=args.timeout)
    except Exception as exc:  # serial.SerialException and OS errors
        print(f"error: cannot open {args.port}: {exc}", file=sys.stderr)
        return 1

    current: str | None = None
    fh = None
    part: Path | None = None
    final: Path | None = None
    counters: dict[str, int] = {}

    def close_current(ok: bool) -> None:
        nonlocal current, fh, part, final
        if fh is not None:
            fh.flush()
            os.fsync(fh.fileno())
            fh.close()
        if ok and part is not None and final is not None:
            os.replace(part, final)
            print(f"[saved {final}]", file=sys.stderr)
        elif part is not None:
            print(f"[incomplete {part}]", file=sys.stderr)
        current, fh, part, final = None, None, None, None

    print(f"listening on {args.port} @ {args.baud} 8N1; Ctrl-C to stop", file=sys.stderr)
    try:
        with port:
            while True:
                raw = port.readline()
                if not raw:
                    continue
                line = raw.decode("utf-8", errors="replace").strip()
                if not line:
                    continue
                print(line, flush=True)

                if line.startswith("BEGIN;"):
                    close_current(ok=False)
                    ident = line.split(";", 1)[1].strip()
                    if not SAFE_ID.match(ident):
                        print(f"[ignored unsafe ID {ident!r}]", file=sys.stderr)
                        continue
                    current = ident
                    part, final = capture_paths(out, ident, counters)
                    fh = open(part, "w", encoding="utf-8", newline="\n")
                    fh.write(line + "\n")
                    continue

                if fh is not None:
                    fh.write(line + "\n")
                    fh.flush()
                    if line.startswith("END;") or line.startswith("DONE;"):
                        close_current(ok=True)
    except KeyboardInterrupt:
        close_current(ok=False)
        print("\nstopped", file=sys.stderr)
        return 130
    finally:
        close_current(ok=False)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
