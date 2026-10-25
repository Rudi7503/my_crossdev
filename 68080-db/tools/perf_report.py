#!/usr/bin/env python3
"""Summarize PERF lines from Harness-v5 logs.

PERF format (sums over 100 runs, counter-read baseline subtracted):
  PERF;100;CCC;v;IEP1;v;IEP2;v;IEP3;v;BPC;v;BPW;v;DCH;v;DCM;v

The table prints per-run values (sum / 100), pipe shares, branch-prediction
rate and data-cache hit rate.  Missing/negative values are flagged.
"""
from __future__ import annotations

import argparse
import pathlib
import statistics

LABELS = ("CCC", "IEP1", "IEP2", "IEP3", "BPC", "BPW", "DCH", "DCM")


def parse_perf(path: pathlib.Path) -> dict[str, int] | None:
    for line in path.read_text().splitlines():
        if not line.startswith("PERF;"):
            continue
        parts = line.strip().split(";")
        if len(parts) != 18 or parts[1] != "100":
            return None
        vals: dict[str, int] = {}
        for n, label in enumerate(LABELS):
            if parts[2 + 2 * n] != label:
                return None
            vals[label] = int(parts[3 + 2 * n])
        return vals
    return None


def median(values: list[int]) -> float:
    return statistics.median(values) if values else float("nan")


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("directory", nargs="?", default="measurements/v5")
    args = ap.parse_args()
    root = pathlib.Path(args.directory)
    files = sorted(root.glob("*.run??.log"))
    if not files:
        print(f"keine PERF-Logs in {root}")
        return 1

    by_id: dict[str, list[dict[str, int]]] = {}
    for f in files:
        ident = f.name.split(".")[0]
        perf = parse_perf(f)
        if perf:
            by_id.setdefault(ident, []).append(perf)
    if not by_id:
        print("keine PERF-Zeilen gefunden")
        return 1

    print("| ID | CCC | IEP1 | IEP2 | IEP3 | IEP1% | IEP2% | IEP3% | BPC | BPW | BPW% | DCH | DCM | Hit% |")
    print("|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|")
    for ident in sorted(by_id):
        runs = by_id[ident]
        # per-run values (sums are over 100 runs)
        vals = {k: [r[k] / 100.0 for r in runs] for k in LABELS}
        m = {k: median(vals[k]) for k in LABELS}
        total = m["IEP1"] + m["IEP2"] + m["IEP3"]
        shares = [(m[k] / total * 100.0) if total > 0 else float("nan") for k in ("IEP1", "IEP2", "IEP3")]
        bp = m["BPC"] + m["BPW"]
        bpr = (m["BPW"] / bp * 100.0) if bp > 0 else float("nan")
        dc = m["DCH"] + m["DCM"]
        dcr = (m["DCH"] / dc * 100.0) if dc > 0 else float("nan")
        print(f"| {ident} | {m['CCC']:.1f} | {m['IEP1']:.1f} | {m['IEP2']:.1f} | {m['IEP3']:.1f} | "
              f"{shares[0]:.1f} | {shares[1]:.1f} | {shares[2]:.1f} | {m['BPC']:.1f} | {m['BPW']:.1f} | "
              f"{bpr:.1f} | {m['DCH']:.1f} | {m['DCM']:.1f} | {dcr:.1f} |")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
