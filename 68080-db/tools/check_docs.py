#!/usr/bin/env python3
"""Static consistency check for the 68080 database documentation."""
from __future__ import annotations
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
EXPECTED_ENTRIES = 77
EXPECTED_LOGS = 770

def fail(msg: str, errors: list[str]) -> None:
    errors.append(msg)

def main() -> int:
    errors: list[str] = []
    seq = sorted(p.stem for p in (ROOT / "seq").glob("*.i"))
    local_log_dir = ROOT / "measurements/v5-files"
    core_log_dir = ROOT / "cores/core-11350/logs"
    log_files = sorted(local_log_dir.glob("*.run??.log"))
    core_logs = sorted(core_log_dir.glob("*.run??.log"))
    log_dir = local_log_dir if local_log_dir.exists() else core_log_dir
    index_text = (ROOT / "index.md").read_text()
    index_ids = re.findall(r"^\| \[`([A-Z0-9-]+)`\]", index_text, re.M)
    readme = (ROOT / "README.md").read_text()
    contributing = (ROOT / "CONTRIBUTING.md").read_text()

    if len(seq) != EXPECTED_ENTRIES:
        fail(f"seq count {len(seq)} != {EXPECTED_ENTRIES}", errors)
    if len(log_files) != EXPECTED_LOGS and local_log_dir.exists():
        fail(f"v5 log count {len(log_files)} != {EXPECTED_LOGS}", errors)
    if len(core_logs) != EXPECTED_LOGS:
        fail(f"core log count {len(core_logs)} != {EXPECTED_LOGS}", errors)
    if len(index_ids) != EXPECTED_ENTRIES:
        fail(f"index rows {len(index_ids)} != {EXPECTED_ENTRIES}", errors)
    if set(index_ids) != set(seq):
        fail(f"index/seq ID mismatch: missing={sorted(set(seq)-set(index_ids))} extra={sorted(set(index_ids)-set(seq))}", errors)
    for ident in seq:
        n = len(list(log_dir.glob(f"{ident}.run??.log")))
        if n != 10:
            fail(f"{ident}: {n} logs in {log_dir.name}, expected 10", errors)
    if f"{EXPECTED_ENTRIES} Einträge" not in readme:
        fail("README missing current entry count", errors)
    if f"{EXPECTED_LOGS}/" not in readme:
        fail("README missing current log count", errors)
    if f"{EXPECTED_ENTRIES} entries" not in contributing:
        fail("CONTRIBUTING missing current entry count", errors)

    # README content table: numeric entry counts must sum to EXPECTED_ENTRIES.
    table_sum = 0
    for line in readme.splitlines():
        m = re.match(r"\| \[`(0[1-9]|1[0-9]|2[0-5])-[^`]+`\]\([^)]*\) \| [^|]+ \| (\d+)", line)
        if m:
            table_sum += int(m.group(2))
    if table_sum != EXPECTED_ENTRIES:
        fail(f"README content table sums to {table_sum}, expected {EXPECTED_ENTRIES}", errors)

    # Documentation links to tracked raw logs must resolve.
    for md in ROOT.glob("*.md"):
        text = md.read_text()
        for target in re.findall(r"\]\((cores/core-11350/logs/[^)]+)\)", text):
            if not (ROOT / target).exists():
                fail(f"{md.name}: broken link {target}", errors)

    if errors:
        print("check_docs: FAIL")
        for e in errors:
            print(" -", e)
        return 1
    print(f"check_docs: OK ({EXPECTED_ENTRIES} entries, {EXPECTED_LOGS} logs)")
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
