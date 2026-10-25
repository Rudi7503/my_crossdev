#!/usr/bin/env python3
"""Run a binary under ASH and classify crash mode.

Modes:
  NO_CRASH          ASH returned normally
  REBOOT            V4 became unreachable and came back
  CRASH_REQUESTER   V4 alive, screenshot captured
  V4_DOWN           V4 did not come back
"""
from __future__ import annotations
import argparse, json, subprocess, sys, time
from datetime import datetime
from pathlib import Path
from PIL import Image
import struct, io

sys.path.insert(0, str(Path(__file__).parent))
from auto_opt import (BASE, PROJ, TESTS, ACP, ASH, IP, acp_upload, acp_download, ash_run, win_path)

def alive(timeout: int = 8) -> bool:
    try:
        r = subprocess.run([str(ASH), IP, 'version'], text=True, capture_output=True, timeout=timeout)
        return r.returncode == 0 and 'Kickstart' in (r.stdout + r.stderr)
    except Exception:
        return False

def wait_alive(seconds: int = 90) -> bool:
    end = time.time() + seconds
    while time.time() < end:
        if alive(4):
            return True
        time.sleep(2)
    return False

def bmp_to_png(src: Path, dst: Path) -> None:
    raw = bytearray(src.read_bytes())
    w, h = struct.unpack_from('<ii', raw, 18)
    bpp = struct.unpack_from('<H', raw, 28)[0]
    if bpp == 24:
        struct.pack_into('<I', raw, 34, w * h * 3)
    img = Image.open(io.BytesIO(raw)).convert('RGB')
    r,g,b = img.split()
    img = Image.merge('RGB',(b,g,r))
    img.save(dst)

def screenshot(outdir: Path) -> Path | None:
    print('== crash screenshot ==', flush=True)
    try:
        ash_run('C:grabrtgwb savefile=RAM:crash.bmp frontscreen bmp quiet', timeout=25)
    except subprocess.TimeoutExpired:
        print('screenshot command timed out', file=sys.stderr)
    # crash.bmp lives on RAM:, not Ram Disk:
    try:
        target = outdir / 'crash.bmp'
        if target.exists():
            target.unlink()
        from auto_opt import sh
        sh([ACP, f'{IP}:RAM/crash.bmp', win_path(outdir)])
        if not target.exists():
            return None
        png = outdir / 'crash.png'
        bmp_to_png(target, png)
        return png
    except Exception as e:
        print(f'screenshot download/convert failed: {e}', file=sys.stderr)
        return None

def canary_exists(canary: str) -> bool:
    try:
        r = ash_run(f'C:List {canary}', timeout=10)
        out = r.stdout + r.stderr
        return 'object not found' not in out and 'Directory' in out
    except Exception:
        return False


def run_case(binary: Path, name: str) -> int:
    stamp = datetime.now().strftime('%Y%m%d-%H%M%S')
    outdir = TESTS / 'crash-results' / f'{name}-{stamp}'
    outdir.mkdir(parents=True)
    canary = f'RAM:canary_{name}_{stamp.replace("-", "_")}'
    ash_run(f'C:MakeDir {canary}')
    acp_upload(binary)
    print(f'== run {binary.name} ==', flush=True)
    timed_out = False
    ash_out = ''
    try:
        r = ash_run(f'RAM:{binary.name}', timeout=25)
        ash_out = r.stdout + r.stderr
    except subprocess.TimeoutExpired:
        timed_out = True
    time.sleep(3)
    state = ''
    if alive():
        if not canary_exists(canary):
            state = 'REBOOT'
        elif timed_out or not ash_out.strip():
            shot = screenshot(outdir)
            state = 'CRASH_REQUESTER' if shot else 'CRASH_NO_SCREENSHOT'
        else:
            state = 'NO_CRASH'
    else:
        print('V4 not reachable; waiting for reboot...', flush=True)
        state = 'REBOOT' if wait_alive(90) else 'V4_DOWN'
    result = {'binary': str(binary), 'name': name, 'timed_out': timed_out,
              'ash_output': ash_out, 'state': state, 'canary': canary, 'canary_exists': canary_exists(canary)}
    (outdir / 'result.json').write_text(json.dumps(result, indent=2))
    print(f'CRASH_RESULT;{name};{state};{outdir}', flush=True)
    return {'NO_CRASH': 0, 'REBOOT': 10, 'CRASH_REQUESTER': 11,
            'CRASH_NO_SCREENSHOT': 12, 'V4_DOWN': 13}.get(state, 1)

def run_command(command: str, name: str) -> int:
    stamp = datetime.now().strftime('%Y%m%d-%H%M%S')
    outdir = TESTS / 'crash-results' / f'{name}-{stamp}'
    outdir.mkdir(parents=True)
    canary = f'RAM:canary_{name}_{stamp.replace("-", "_")}'
    ash_run(f'C:MakeDir {canary}')
    print(f'== run command {command} ==', flush=True)
    timed_out = False
    ash_out = ''
    try:
        r = ash_run(command, timeout=25)
        ash_out = r.stdout + r.stderr
    except subprocess.TimeoutExpired:
        timed_out = True
    time.sleep(3)
    if alive():
        if not canary_exists(canary):
            state = 'REBOOT'
        elif timed_out or not ash_out.strip():
            shot = screenshot(outdir)
            state = 'CRASH_REQUESTER' if shot else 'CRASH_NO_SCREENSHOT'
        else:
            state = 'NO_CRASH'
    else:
        print('V4 not reachable; waiting for reboot...', flush=True)
        state = 'REBOOT' if wait_alive(90) else 'V4_DOWN'
    result = {'command': command, 'name': name, 'timed_out': timed_out,
              'ash_output': ash_out, 'state': state, 'canary': canary, 'canary_exists': canary_exists(canary)}
    (outdir / 'result.json').write_text(json.dumps(result, indent=2))
    print(f'CRASH_RESULT;{name};{state};{outdir}', flush=True)
    return {'NO_CRASH': 0, 'REBOOT': 10, 'CRASH_REQUESTER': 11,
            'CRASH_NO_SCREENSHOT': 12, 'V4_DOWN': 13}.get(state, 1)


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument('--binary')
    ap.add_argument('--command')
    ap.add_argument('--name', default='case')
    a = ap.parse_args()
    if a.command:
        return run_command(a.command, a.name)
    if not a.binary:
        ap.error('--binary or --command required')
    return run_case(Path(a.binary), a.name)

if __name__ == '__main__':
    raise SystemExit(main())
