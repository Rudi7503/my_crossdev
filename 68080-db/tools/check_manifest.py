#!/usr/bin/env python3
"""Validate entries.yaml against seq/, index.md and the core log mirror."""
from __future__ import annotations
import re
import sys
from pathlib import Path
import yaml

ROOT = Path(__file__).resolve().parent.parent
EXPECTED = 77

def main() -> int:
    errors = []
    data = yaml.safe_load((ROOT / "entries.yaml").read_text())
    entries = data.get("entries", [])
    if len(entries) != EXPECTED:
        errors.append(f"manifest has {len(entries)} entries, expected {EXPECTED}")
    seq = {p.stem for p in (ROOT / "seq").glob("*.i")}
    ids = [e.get("id") for e in entries]
    if len(set(ids)) != len(ids):
        errors.append("manifest has duplicate IDs")
    if set(ids) != seq:
        errors.append(f"manifest/seq mismatch missing={sorted(seq-set(ids))} extra={sorted(set(ids)-seq)}")
    index_ids = set(re.findall(r"^\| \[`([A-Z0-9-]+)`\]", (ROOT / "index.md").read_text(), re.M))
    if set(ids) != index_ids:
        errors.append(f"manifest/index mismatch missing={sorted(index_ids-set(ids))} extra={sorted(set(ids)-index_ids)}")
    for e in entries:
        ident = e["id"]
        if not (ROOT / e["doc"]).exists():
            errors.append(f"{ident}: missing doc {e['doc']}")
        if not (ROOT / e["seq"]).exists():
            errors.append(f"{ident}: missing seq {e['seq']}")
        n = len(list((ROOT / "cores/core-11350/logs").glob(f"{ident}.run??.log")))
        if n != 10:
            errors.append(f"{ident}: {n} core logs, expected 10")
    if errors:
        print("check_manifest: FAIL")
        for e in errors: print(" -", e)
        return 1
    print(f"check_manifest: OK ({len(entries)} entries)")
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
