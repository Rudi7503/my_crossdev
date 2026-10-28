#!/usr/bin/env python3
"""Static guard for the 68080 measurement sequences.

The harness embeds the sequence ten times in the shared measure_100
subroutine and once in the v5 counter pass (perf_one_run). Warmup and
measurement call measure_100, so the assembled source contains 11 copies
and the sequence executes 2 x 100 (cycles) + 10 + 100 (counters) = 310
times. Therefore a sequence may not define labels, include files, or
change sections.
It also may not touch the registers reserved by the harness.

This is intentionally conservative: it strips comments and then rejects any
mention of d5/d6/d7/a5/a6/a7 in the remaining code.
"""
from __future__ import annotations

import re
import sys
from pathlib import Path

FORBIDDEN = re.compile(r"(?<![A-Za-z0-9_])(d[567]|a[567])(?![A-Za-z0-9_])", re.I)
LABEL = re.compile(r"^[A-Za-z_.][A-Za-z0-9_.]*:", re.M)
DIRECTIVE = re.compile(
    r"^\s*(include|section|incbin|org|equ\s|macro|endm|rts|rte|bra|bsr|jsr|jmp|dbra|dbf)\b",
    re.I | re.M,
)
ID_RE = re.compile(r"^(FUS|BND|AMX|REG|TIM|MEM|PAT|ANTI|MAT|FPU)-\d{3}$")


def code_only(text: str) -> str:
    """Remove ';' comments, keeping line count for useful errors."""
    return "\n".join(line.split(";", 1)[0] for line in text.splitlines())


def check(path: Path) -> list[str]:
    errs: list[str] = []
    ident = path.stem
    if not ID_RE.match(ident):
        errs.append(f"{path}: invalid entry id {ident!r}")
    text = path.read_text(encoding="utf-8")
    code = code_only(text)
    if "Uses:" not in text:
        errs.append(f"{path}: missing 'Uses:' header")
    if "MUST NOT touch: d5, d6, d7, a5, a6, a7" not in text:
        errs.append(f"{path}: missing register contract header")
    for m in FORBIDDEN.finditer(code):
        line = code[: m.start()].count("\n") + 1
        errs.append(f"{path}:{line}: forbidden harness register {m.group(0)}")
    for m in LABEL.finditer(code):
        line = code[: m.start()].count("\n") + 1
        errs.append(f"{path}:{line}: labels are not allowed (sequence is included 11x / executed 310x)")
    for m in DIRECTIVE.finditer(code):
        line = code[: m.start()].count("\n") + 1
        errs.append(f"{path}:{line}: directive/control flow not allowed: {m.group(0).strip()}")
    if not code.strip():
        errs.append(f"{path}: empty sequence")
    return errs


def main(argv: list[str]) -> int:
    roots = [Path(a) for a in argv[1:]] or [Path("seq")]
    files: list[Path] = []
    for root in roots:
        files.extend(sorted(root.glob("*.i")))
    if not files:
        print("check_sequences: no .i files found", file=sys.stderr)
        return 1
    errs: list[str] = []
    for path in files:
        errs.extend(check(path))
    if errs:
        print("\n".join(errs), file=sys.stderr)
        return 1
    print(f"check_sequences: {len(files)} sequences OK")
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv))
