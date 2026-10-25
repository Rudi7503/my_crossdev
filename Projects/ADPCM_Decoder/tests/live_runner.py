#!/usr/bin/env python3
"""ASH runner with serial heartbeat watchdog."""
from __future__ import annotations
import shutil, subprocess, time
from pathlib import Path

BASE = Path('/home/papa/Vampire_V4/ApolloCrossDev')
SERIAL = Path(__file__).parent / 'serial_log.py'
ASH = Path('/mnt/c/Program Files/ApolloExplorer/ash.exe')
PY = shutil.which('python.exe') or 'python.exe'


def win_path(p) -> str:
    return '\\\\wsl.localhost\\Ubuntu' + str(p).replace('/', '\\')


class SerialCapture:
    def __init__(self, outdir: Path):
        self.outdir = outdir
        self.proc = None
        self.log_win = f'C:\\Windows\\Temp\\adpcm_serial_{outdir.name}.log'
        self.log = Path(self.log_win.replace('C:\\', '/mnt/c/').replace('\\', '/'))
    def start(self):
        self.outdir.mkdir(parents=True, exist_ok=True)
        self.proc = subprocess.Popen(
            [PY, win_path(SERIAL), 'COM9', self.log_win],
            stdout=subprocess.DEVNULL, stderr=subprocess.PIPE, text=True)
        time.sleep(1)
        if self.proc.poll() is not None:
            err = self.proc.stderr.read() if self.proc.stderr else ''
            raise RuntimeError(f'serial logger failed: {err}')
    def stop(self):
        if self.proc and self.proc.poll() is None:
            self.proc.terminate()
            try:
                self.proc.wait(timeout=5)
            except subprocess.TimeoutExpired:
                self.proc.kill()
    def text(self) -> str:
        try:
            return self.log.read_text(errors='replace')
        except OSError:
            return ''


def run_with_watchdog(cmd: str, outdir: Path, idle: float = 20.0, max_total: float = 240.0) -> tuple[int | None, str, bool, str]:
    cap = SerialCapture(outdir)
    cap.start()
    p = subprocess.Popen([str(ASH), '192.168.178.50', cmd],
                         stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
    start = last = time.time()
    timed_out = False
    while True:
        if p.poll() is not None:
            break
        # Any new serial output (ALIVE/DONE/output) resets the idle timer.
        files = list(outdir.glob('*.part')) + list(outdir.glob('*.log'))
        if cap.log.exists():
            files.append(cap.log)
        latest = max((f.stat().st_mtime for f in files), default=0)
        if latest > last:
            last = latest
        if 'DONE;' in cap.text():
            p.wait(timeout=10)
            break
        if time.time() - last > idle:
            timed_out = True
            p.kill()
            break
        if time.time() - start > max_total:
            timed_out = True
            p.kill()
            break
        time.sleep(0.2)
    try:
        out, _ = p.communicate(timeout=5)
    except Exception:
        out = ''
    serial = cap.text()
    cap.stop()
    return p.returncode, out, timed_out, serial
