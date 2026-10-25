#!/usr/bin/env python3
"""Validate and summarize a harness log.

Accepts the optional CORE;<PCR-ID>;<PCR-REV>;<VCoreRev> line from harness v2
and the legacy v1 logs without it.
"""
from __future__ import annotations

import argparse
import statistics
import sys
from pathlib import Path


def parse(path: Path) -> tuple[str, int, list[int], tuple[int, int, int], tuple[str, str, str] | None]:
    lines = [ln.strip() for ln in path.read_text(encoding="utf-8").splitlines() if ln.strip()]
    if len(lines) < 4:
        raise ValueError("log is too short")
    begin = lines[0].split(";")
    if begin[0] != "BEGIN" or len(begin) != 2:
        raise ValueError("first line is not BEGIN;<ID>")
    ident = begin[1]
    if lines[-1] != f"END;{ident}":
        raise ValueError("last line is not END;<same ID>")

    baseline = None
    summary = None
    core = None
    results: list[int] = []
    for ln in lines[1:-1]:
        parts = ln.split(";")
        if parts[0] == "PERF":
            if len(parts) != 18:
                raise ValueError("PERF expects 18 fields")
            if parts[1] != "100":
                raise ValueError("PERF run count must be 100")
            labels = ["CCC", "IEP1", "IEP2", "IEP3", "BPC", "BPW", "DCH", "DCM"]
            for n, label in enumerate(labels):
                if parts[2 + 2*n] != label:
                    raise ValueError(f"PERF label {parts[2+2*n]} != {label}")
                int(parts[3 + 2*n])
        elif parts[0] == "FILESTAT" and len(parts) in (3, 6):
            for value in parts[1:]:
                int(value)
        elif parts[0] == "CORE" and len(parts) == 4:
            int(parts[1])  # PCR id
            int(parts[2])  # PCR revision
            core = (parts[1], parts[2], parts[3])
        elif parts[0] == "BASELINE" and len(parts) == 2:
            baseline = int(parts[1])
        elif parts[0] == "RES" and len(parts) == 3:
            run, cycles = int(parts[1]), int(parts[2])
            if run != len(results) + 1:
                raise ValueError(f"RES runs are not consecutive at {run}")
            if cycles < 0:
                raise ValueError(f"negative cycle count at run {run}")
            results.append(cycles)
        elif parts[0] == "SUMMARY" and len(parts) == 4:
            summary = (int(parts[1]), int(parts[2]), int(parts[3]))
        else:
            raise ValueError(f"unexpected line: {ln}")

    if baseline is None:
        raise ValueError("missing BASELINE line")
    if summary is None:
        raise ValueError("missing SUMMARY line")
    if len(results) != 100:
        raise ValueError(f"expected 100 RES lines, found {len(results)}")
    calc = (min(results), max(results), statistics.fmean(results))
    if summary[0] != calc[0] or summary[1] != calc[1]:
        raise ValueError(f"SUMMARY MIN/MAX {summary[:2]} != computed {calc[:2]}")
    if abs(summary[2] - calc[2]) > 1:
        raise ValueError(f"SUMMARY AVG {summary[2]} != computed {calc[2]:.1f}")
    return ident, baseline, results, summary, core


def main() -> int:
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("logs", nargs="+", type=Path)
    args = p.parse_args()
    failed = False
    for path in args.logs:
        try:
            ident, baseline, results, summary, core = parse(path)
        except Exception as exc:
            print(f"{path}: INVALID: {exc}", file=sys.stderr)
            failed = True
            continue
        net = [r - baseline for r in results]
        core_txt = ""
        if core is not None:
            core_txt = f"  core=PCR:{core[0]}/{core[1]} VCoreRev={core[2] or '-'}"
        print(
            f"{path}: {ident} OK  baseline={baseline}  "
            f"MIN={summary[0]} MAX={summary[1]} AVG={summary[2]}  "
            f"NET_MIN={min(net)} NET_AVG={statistics.fmean(net):.2f}{core_txt}"
        )
        if min(net) < 0:
            print("  warning: negative NET; the baseline is not subtractable")
    return 1 if failed else 0


if __name__ == "__main__":
    raise SystemExit(main())
