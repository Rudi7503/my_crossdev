#!/usr/bin/env python3
"""End-to-end ADPCM decoder test/optimization runner.

Build (vasm+gcc) -> ACP upload -> ASH execute -> serial START/DONE ->
ACP download -> parse PASS/cycles.

Usage:
  python3 tests/auto_opt.py                  # production decoder, version opt9
  python3 tests/auto_opt.py --source experiments/decoder_4bit_opt8.s --version opt8
"""
from __future__ import annotations
import argparse, io, json, os, re, struct, subprocess, sys, time
from pathlib import Path
from datetime import datetime
from PIL import Image
from live_runner import run_with_watchdog

BASE = Path('/home/papa/Vampire_V4/ApolloCrossDev')
PROJ = BASE / 'Projects/ADPCM_Decoder'
TESTS = PROJ / 'tests'
PREFIX = BASE / 'Compilers/GCC-6.50-Latest'
VASM = PREFIX / 'bin/vasmm68k_mot'
CC = PREFIX / 'bin/m68k-amigaos-gcc'
ACP = Path('/mnt/c/Program Files/ApolloExplorer/acp.exe')
ASH = Path('/mnt/c/Program Files/ApolloExplorer/ash.exe')
SERIAL = BASE / '68080-db/tools/serial_capture.py'
IP = '192.168.178.50'


def win_path(p) -> str:
    return '\\\\wsl.localhost\\Ubuntu' + str(p).replace('/', '\\')
INC = [
    f'-I{PREFIX}/m68k-amigaos/ndk32-include/Include_H',
    f'-I{PREFIX}/m68k-amigaos/sys-include',
]
NDK = PREFIX / 'm68k-amigaos/DevPac/Include'


def sh(cmd: list[str], timeout: int = 180, check: bool = True) -> subprocess.CompletedProcess:
    print('+', ' '.join(str(c) for c in cmd), flush=True)
    r = subprocess.run([str(c) for c in cmd], text=True, capture_output=True, timeout=timeout)
    if r.stdout.strip():
        print(r.stdout.strip())
    if r.stderr.strip():
        print(r.stderr.strip(), file=sys.stderr)
    if check and r.returncode != 0:
        raise SystemExit(f'command failed ({r.returncode}): {cmd[0]}')
    return r


def build(source: Path, version: str) -> tuple[Path, Path, Path]:
    bdir = TESTS / 'auto-build'
    bdir.mkdir(exist_ok=True)
    dec = bdir / 'decoder_auto.o'
    uart = bdir / 'uart_asm.o'
    sh([VASM, '-quiet', '-Fhunk', '-kick1hunks', '-m68080', '-m68881',
        '-opt-speed', f'-I{NDK}', '-o', dec, source])
    sh([VASM, '-quiet', '-Fhunk', '-kick1hunks', '-m68080', '-o', uart,
        TESTS / 'uart_asm.s'])
    go = bdir / 'golden_test_auto.o'
    ro = bdir / 'repeat_test_auto.o'
    sh([CC, '-noixemul', '-O0', '-m68080', '-m68881', '-fno-inline', *INC,
        f'-DTEST_VERSION="{version}"', '-c', TESTS / 'golden_test.c', '-o', go])
    sh([CC, '-noixemul', '-O0', '-m68080', '-m68881', '-fno-inline', *INC,
        f'-DTEST_VERSION="{version}"', '-c', TESTS / 'repeat_test.c', '-o', ro])
    golden = TESTS / 'golden_test_auto'
    repeat = TESTS / 'repeat_test_auto'
    sh([CC, '-noixemul', '-O0', '-m68080', '-m68881', '-fno-inline', *INC,
        go, dec, uart, '-o', golden, '-lm'])
    sh([CC, '-noixemul', '-O0', '-m68080', '-m68881', '-fno-inline', *INC,
        ro, dec, uart, '-o', repeat, '-lm'])
    return golden, repeat, bdir


def acp_upload(*files: Path) -> None:
    for f in files:
        sh([ACP, f, f'{IP}:RAM/'])


def acp_download(name: str, outdir: Path) -> Path:
    target = outdir / name
    if target.exists():
        target.unlink()
    sh([ACP, f'{IP}:Ram Disk/{name}', win_path(outdir)])
    if not target.exists():
        raise SystemExit(f'download missing: {name}')
    return target


def v4_alive(timeout: int = 8) -> bool:
    try:
        r = subprocess.run([str(ASH), IP, 'version'], text=True, capture_output=True, timeout=timeout)
        return r.returncode == 0 and 'Kickstart' in (r.stdout + r.stderr)
    except Exception:
        return False


def read_core() -> dict:
    """Read the V4 core identity; always store it with results."""
    core = {'apollo_core': None, 'vcore_rev': None, 'raw': ''}
    try:
        r = ash_run('C:ApolloControl CORE', timeout=10)
        core['raw'] = (r.stdout + r.stderr).strip()
        nums = re.findall(r'\b(\d+)\b', core['raw'])
        if nums:
            core['apollo_core'] = int(nums[-1])
    except Exception as e:
        core['raw'] = f'CORE error: {e}'
    try:
        ash_run('C:ApolloControl SETENV', timeout=10)
        r = ash_run('C:Type ENV:VCoreRev', timeout=10)
        nums = re.findall(r'\b(\d+)\b', r.stdout + r.stderr)
        if nums:
            core['vcore_rev'] = int(nums[-1])
    except Exception:
        pass
    return core


def wait_alive(seconds: int = 90) -> bool:
    end = time.time() + seconds
    while time.time() < end:
        if v4_alive(4):
            return True
        time.sleep(2)
    return False


def canary_exists(canary: str) -> bool:
    try:
        r = ash_run(f'C:List {canary}', timeout=10)
        out = r.stdout + r.stderr
        return 'object not found' not in out and 'Directory' in out
    except Exception:
        return False


def screenshot_crash(outdir: Path) -> Path | None:
    try:
        ash_run('C:grabrtgwb savefile=RAM:crash.bmp frontscreen bmp quiet', timeout=25)
        target = outdir / 'crash.bmp'
        target.unlink(missing_ok=True)
        sh([ACP, f'{IP}:RAM/crash.bmp', win_path(outdir)])
        if not target.exists():
            return None
        raw = bytearray(target.read_bytes())
        w, h = struct.unpack_from('<ii', raw, 18)
        bpp = struct.unpack_from('<H', raw, 28)[0]
        if bpp == 24:
            struct.pack_into('<I', raw, 34, w * h * 3)
        img = Image.open(io.BytesIO(raw)).convert('RGB')
        r, g, b = img.split()
        img = Image.merge('RGB', (b, g, r))
        png = outdir / 'crash.png'
        img.save(png)
        return png
    except Exception as e:
        print(f'crash screenshot failed: {e}', file=sys.stderr)
        return None


def ash_run(cmd: str, timeout: int = 180) -> subprocess.CompletedProcess:
    return sh([ASH, IP, cmd], timeout=timeout, check=False)


def ps(command: str, timeout: int = 30) -> subprocess.CompletedProcess:
    return sh(['powershell.exe', '-NoProfile', '-Command', command], timeout=timeout, check=False)


def start_capture(outdir: Path) -> int:
    win_script = win_path(SERIAL)
    win_out = win_path(outdir)
    cap_out_win = f'C:\\Windows\\Temp\\adpcm_{outdir.name}.out'
    cap_err_win = f'C:\\Windows\\Temp\\adpcm_{outdir.name}.err'
    for tmp in (cap_out_win, cap_err_win):
        Path(tmp.replace('C:\\', '/mnt/c/')).unlink(missing_ok=True)
    cmd = (f'$p = Start-Process -FilePath python -ArgumentList @("{win_script}","--port","COM9",'
           f'"--baud","115200","--out","{win_out}") -PassThru -WindowStyle Hidden '
           f'-RedirectStandardOutput "{cap_out_win}" -RedirectStandardError "{cap_err_win}"; $p.Id')
    r = ps(cmd)
    pid = 0
    for tok in r.stdout.split():
        if tok.strip().isdigit():
            pid = int(tok.strip())
    time.sleep(2)
    cap_err = Path(cap_err_win.replace('C:\\', '/mnt/c/'))
    err = cap_err.read_text(errors='replace') if cap_err.exists() else ''
    if pid <= 0 or 'listening' not in err:
        if pid:
            ps(f'Stop-Process -Id {pid} -ErrorAction SilentlyContinue')
        raise SystemExit(f'serial capture failed: {err}')
    print(f'serial capture PID {pid}, log {outdir}', flush=True)
    return pid


def stop_capture(pid: int) -> None:
    ps(f'Stop-Process -Id {pid} -ErrorAction SilentlyContinue')


def serial_done(outdir: Path, program: str, version: str, timeout: float = 10.0) -> bool:
    needle = f'DONE;{program};{version}'
    end = time.time() + timeout
    while time.time() < end:
        for f in outdir.glob('*.log'):
            try:
                if needle in f.read_text(errors='replace'):
                    return True
            except OSError:
                pass
        time.sleep(0.25)
    return False


def parse_golden(path: Path) -> tuple[bool, str]:
    text = path.read_text(errors='replace')
    ok = 'RESULT;PASS' in text
    return ok, text


def parse_repeat(path: Path) -> dict[str, tuple[int, int]]:
    out = {}
    for line in path.read_text(errors='replace').splitlines():
        m = re.match(r'(STEREO|MS|MONO);runs=100;min=(\d+);median=(\d+);.*;PASS', line)
        if m:
            out[m.group(1)] = (int(m.group(2)), int(m.group(3)))
    return out


def main() -> int:
    global IP
    ap = argparse.ArgumentParser()
    ap.add_argument('--source', default=str(PROJ / 'decoder_4bit.s'))
    ap.add_argument('--version', default='fresh3')
    ap.add_argument('--ip', default=IP)
    ap.add_argument('--skip-golden', action='store_true')
    args = ap.parse_args()
    IP = args.ip

    stamp = datetime.now().strftime('%Y%m%d-%H%M%S')
    outdir = TESTS / 'auto-results' / f'{args.version}-{stamp}'
    outdir.mkdir(parents=True)
    core_before = read_core()
    (outdir / 'core-before.json').write_text(json.dumps(core_before, indent=2))
    print(f'CORE before: {core_before}', flush=True)
    print(f'== build {args.source} ({args.version}) ==', flush=True)
    golden, repeat, _ = build(Path(args.source), args.version)

    print('== upload ==', flush=True)
    acp_upload(golden, repeat)

    def run_one(label: str, cmd: str, phase: str, idle: float = 20.0) -> tuple[bool, str]:
        canary = f'RAM:canary_{args.version}_{phase}_{stamp}'
        ash_run(f'C:MakeDir {canary}')
        print(f'== run {label} ==', flush=True)
        rc, ash_out, timed_out, serial = run_with_watchdog(cmd, outdir, idle=idle, max_total=300)
        (outdir / f'{phase}.ash.txt').write_text(ash_out or '')
        (outdir / f'{phase}.serial.txt').write_text(serial or '')
        if not canary_exists(canary):
            if not v4_alive():
                wait_alive(90)
            print(f'{label}: REBOOT detected', file=sys.stderr)
            return False, 'REBOOT'
        if timed_out or rc not in (0, None):
            shot = screenshot_crash(outdir)
            state = 'CRASH_REQUESTER' if shot else 'CRASH_NO_SCREENSHOT'
            print(f'{label}: {state}', file=sys.stderr)
            return False, state
        return True, 'OK'

    if not args.skip_golden:
        ok, state = run_one('golden', 'RAM:golden_test_auto', 'golden')
        if not ok:
            return 4 if state == 'REBOOT' else 5
        gp = acp_download('ADPCM_GOLDEN.txt', outdir)
        ok, text = parse_golden(gp)
        print(text, flush=True)
        if not ok:
            print('GOLDEN FAILED', file=sys.stderr)
            return 2

    ok, state = run_one('repeat', 'RAM:repeat_test_auto', 'repeat')
    if not ok:
        return 4 if state == 'REBOOT' else 5
    rp = acp_download('ADPCM_REPEAT.txt', outdir)
    vals = parse_repeat(rp)
    text = rp.read_text(errors='replace')
    print(text, flush=True)
    if not vals:
        print('REPEAT FAILED', file=sys.stderr)
        return 3
    core_after = read_core()
    (outdir / 'core-after.json').write_text(json.dumps(core_after, indent=2))
    summary = {'version': args.version, 'source': args.source,
               'core_before': core_before, 'core_after': core_after,
               'repeat': {k: {'min': v[0], 'median': v[1]} for k, v in vals.items()}}
    (outdir / 'summary.json').write_text(json.dumps(summary, indent=2))
    print(f'== DONE -> {outdir} ==', flush=True)
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
