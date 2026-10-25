#!/usr/bin/env python3
"""Summarize repeated harness logs and optionally compare two capture dirs.

Typical use after RUN-ALL (10 repeats per entry):

    python3 tools/repeat_report.py measurements/v2
    python3 tools/repeat_report.py measurements/v2 \
        --compare measurements/v2-serial

File names are <ID>.runNN.log (or the legacy <ID>.log).  For each ID the
per-run MIN/MAX/AVG values are printed; a MIN spread across runs is flagged.
With --compare, files with the same name in both directories are compared
line by line (V4 file log vs. PC UART capture).
"""
from __future__ import annotations

import argparse
import re
import statistics
import sys
from collections import defaultdict
from pathlib import Path

from check_log import parse

NAME = re.compile(r"^([A-Z]+-\d{3})(?:\.run(\d+))?\.log$")


def collect(directory: Path) -> dict[str, list[tuple[int, Path]]]:
    found: dict[str, list[tuple[int, Path]]] = defaultdict(list)
    for path in sorted(directory.glob("*.log")):
        m = NAME.match(path.name)
        if not m:
            continue
        run = int(m.group(2)) if m.group(2) else 0
        found[m.group(1)].append((run, path))
    return found


def compare(a: Path, b: Path) -> int:
    files_a = {p.name: p for p in a.glob("*.log")}
    files_b = {p.name: p for p in b.glob("*.log")}
    same = diff = missing = 0
    for name in sorted(set(files_a) | set(files_b)):
        pa, pb = files_a.get(name), files_b.get(name)
        if pa is None or pb is None:
            print(f"MISSING {name}: {'A' if pa is None else 'B'}")
            missing += 1
            continue
        la = pa.read_text(encoding="utf-8", errors="replace").splitlines()
        lb = pb.read_text(encoding="utf-8", errors="replace").splitlines()
        if la == lb:
            same += 1
            continue
        diff += 1
        print(f"DIFF    {name}")
        for n, (x, y) in enumerate(zip(la, lb), 1):
            if x != y:
                print(f"  line {n}: A={x!r} B={y!r}")
                break
        if len(la) != len(lb):
            print(f"  line count: A={len(la)} B={len(lb)}")
    print(f"compare: identical={same} different={diff} missing={missing}")
    return 1 if diff or missing else 0


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("directory", type=Path, help="directory with <ID>.runNN.log")
    ap.add_argument("--compare", type=Path, metavar="DIR", help="second capture directory")
    args = ap.parse_args()

    if not args.directory.is_dir():
        print(f"error: not a directory: {args.directory}", file=sys.stderr)
        return 2

    found = collect(args.directory)
    if not found:
        print(f"error: no <ID>.runNN.log files in {args.directory}", file=sys.stderr)
        return 2

    print(f"dir: {args.directory}")
    print(f"{'ID':9} {'runs':>4}  {'CORE(PCR/REV/VCoreRev)':>24}  {'MIN range':>9}  {'MIN med':>7}  {'MAX med':>7}  {'AVG med':>7}  note")
    bad = 0
    for ident in sorted(found):
        runs = sorted(found[ident])
        mins: list[int] = []
        maxs: list[int] = []
        avgs: list[int] = []
        cores: list[tuple[str, str, str] | None] = []
        ok = True
        for _run, path in runs:
            try:
                _ident, _baseline, _res, summary, core = parse(path)
            except Exception as exc:
                print(f"{ident:9} {len(runs):4d}  INVALID {path.name}: {exc}")
                ok = False
                bad += 1
                break
            mins.append(summary[0])
            maxs.append(summary[1])
            avgs.append(summary[2])
            cores.append(core)
        if not ok:
            continue
        spread = max(mins) - min(mins)
        c0 = cores[0] if cores else None
        core_txt = "-" if c0 is None else f"{c0[0]}/{c0[1]}/{c0[2] or '-'}"
        note = ""
        if any(c != c0 for c in cores):
            core_txt += "*"
            note = "CORE differs"
            bad += 1
        if spread:
            note = (note + "; " if note else "") + "MIN spread: " + " ".join(map(str, mins))
            bad += 1
        print(
            f"{ident:9} {len(runs):4d}  {core_txt:>24}"
            f"  {min(mins):3d}..{max(mins):<4d}"
            f"  {statistics.median(mins):7.1f}  {statistics.median(maxs):7.1f}"
            f"  {statistics.median(avgs):7.1f}  {note}"
        )

    if args.compare is not None:
        if not args.compare.is_dir():
            print(f"error: not a directory: {args.compare}", file=sys.stderr)
            return 2
        return compare(args.directory, args.compare)

    return 1 if bad else 0


if __name__ == "__main__":
    raise SystemExit(main())
