#!/usr/bin/env python3
"""Local CLI bridge from the DeepSeek harness to amiagent 0.13.2.

Reads ~/.config/apollo-agent/env (AMIGA_HOST, AMIGA_PORT, AMIGA_TOKEN).
No MCP/Cline/API key required; every call is an explicit command.
"""
from __future__ import annotations
import argparse, os, sys, base64
from pathlib import Path

ROOT = Path(__file__).resolve().parent
sys.path.insert(0, str(ROOT / 'amimcp' / 'server'))
from amiga import Amiga, AmigaError, AmigaUnreachable  # noqa: E402
import png  # noqa: E402

ENV = Path('/home/papa/.config/apollo-agent/env')

def load_env():
    if ENV.exists():
        for line in ENV.read_text().splitlines():
            line = line.strip()
            if line and not line.startswith('#') and '=' in line:
                k, v = line.split('=', 1)
                os.environ.setdefault(k.strip(), v.strip())

def client() -> Amiga:
    load_env()
    host = os.environ.get('AMIGA_HOST', '192.168.178.50')
    port = int(os.environ.get('AMIGA_PORT', '7846'))
    token = os.environ.get('AMIGA_TOKEN', '')
    return Amiga(host, port, token, timeout=120.0)

def main() -> int:
    ap = argparse.ArgumentParser(description='amiagent CLI bridge')
    sub = ap.add_subparsers(dest='cmd', required=True)
    sub.add_parser('probe')
    sub.add_parser('info')
    p = sub.add_parser('shell'); p.add_argument('command')
    p = sub.add_parser('read'); p.add_argument('path')
    p = sub.add_parser('write'); p.add_argument('path'); p.add_argument('file')
    p = sub.add_parser('list'); p.add_argument('path')
    p = sub.add_parser('screenshot'); p.add_argument('out'); p.add_argument('--x', type=int, default=0); p.add_argument('--y', type=int, default=0); p.add_argument('--w', type=int, default=0); p.add_argument('--h', type=int, default=0); p.add_argument('--screen', type=int, default=0)
    sub.add_parser('pointer')
    sub.add_parser('screens')
    p = sub.add_parser('type'); p.add_argument('text')
    p = sub.add_parser('key'); p.add_argument('key'); p.add_argument('--qual', default='')
    p = sub.add_parser('click'); p.add_argument('x', type=int); p.add_argument('y', type=int); p.add_argument('--button', default='left'); p.add_argument('--count', type=int, default=1)
    p = sub.add_parser('move'); p.add_argument('x', type=int); p.add_argument('y', type=int)
    sub.add_parser('ui-tree')
    p = sub.add_parser('ui-click'); p.add_argument('gadget'); p.add_argument('--window', default=''); p.add_argument('--double', action='store_true')
    p = sub.add_parser('menus'); p.add_argument('--window', default='')
    p = sub.add_parser('menu-select'); p.add_argument('item'); p.add_argument('--window', default='')
    p = sub.add_parser('arexx'); p.add_argument('source')
    sub.add_parser('break')
    a = ap.parse_args()
    c = client()
    try:
        if a.cmd == 'break':
            print(c.break_command())
        elif a.cmd == 'probe':
            print(c.ping())
            for k, v in c.system_info().items(): print(f'{k}: {v}')
        elif a.cmd == 'info':
            for k, v in c.system_info().items(): print(f'{k}: {v}')
        elif a.cmd == 'shell':
            rc, out = c.exec_command(a.command)
            print(out, end='' if out.endswith('\n') else '\n')
            print(f'[returncode {rc}]'); return 0 if rc == 0 else 1
        elif a.cmd == 'read':
            data = c.read_file(a.path)
            sys.stdout.buffer.write(data)
        elif a.cmd == 'write':
            c.write_file(a.path, Path(a.file).read_bytes()); print('written')
        elif a.cmd == 'list':
            for e in c.list_dir(a.path): print(f'{"D" if e.is_dir else "F"} {e.size:8d} {e.protection} {e.date} {e.name}')
        elif a.cmd == 'screenshot':
            s = c.screenshot(x=a.x, y=a.y, w=a.w, h=a.h, screen=a.screen)
            Path(a.out).write_bytes(png.screenshot_png(s)); print(f'{a.out} {s.width}x{s.height}')
        elif a.cmd == 'pointer':
            print(c.pointer())
        elif a.cmd == 'screens':
            for s in c.screens(): print(s)
        elif a.cmd == 'type':
            c.input_text(a.text); print('typed')
        elif a.cmd == 'key':
            qual = int(a.qual) if a.qual.isdigit() else 0
            c.input_key(a.key, qualifiers=qual); print('key')
        elif a.cmd == 'click':
            c.input_click(a.x, a.y, button=a.button, count=a.count); print('clicked')
        elif a.cmd == 'move':
            c.input_move(a.x, a.y); print('moved')
        elif a.cmd == 'ui-tree':
            print(c.ui_tree())
        elif a.cmd == 'ui-click':
            print(c.ui_click(a.gadget, window=a.window, double=a.double))
        elif a.cmd == 'menus':
            print(c.menus(window=a.window))
        elif a.cmd == 'menu-select':
            print(c.menu_select(a.item, window=a.window))
        elif a.cmd == 'arexx':
            rc, out = c.arexx(a.source); print(out); print(f'[returncode {rc}]'); return 0 if rc == 0 else 1
        return 0
    except (AmigaError, AmigaUnreachable, OSError) as e:
        print(f'ERROR: {e}', file=sys.stderr); return 2

if __name__ == '__main__':
    raise SystemExit(main())
