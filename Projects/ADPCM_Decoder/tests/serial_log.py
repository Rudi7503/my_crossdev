#!/usr/bin/env python3
"""Raw COM9 logger for the live watchdog (runs under Windows Python)."""
import serial, sys, time
port, out = sys.argv[1], sys.argv[2]
s = serial.Serial(port, 115200, timeout=1)
with open(out, 'w', encoding='utf-8', buffering=1) as f:
    while True:
        line = s.readline()
        if line:
            f.write(line.decode('utf-8', errors='replace'))
            f.flush()
