#!/usr/bin/env python3
# Sendet nacheinander mit verschiedenen Baudraten eine klar benannte Zeile.
# So sieht man auf der V4, welche Rate lesbar ankommt.
import serial, time, sys

RATEN = [115200, 9600, 19200, 38400, 57600]

log = open("/tmp/ratensuche.log", "a", buffering=1)
def aus(t):
    print(t, flush=True); log.write(t + "\n")

for baud in RATEN:
    aus("=== %d Baud ===" % baud)
    try:
        s = serial.Serial("/dev/ttyUSB1", baud, timeout=0.2)
    except Exception as e:
        aus("  nicht zu oeffnen: %s" % e); continue
    for n in range(1, 7):
        text = "RATE %d NR %d\n" % (baud, n)
        s.write(text.encode())
        aus("  senden @ %d: %s" % (baud, text.strip()))
        time.sleep(1.0)
    s.close()
aus("=== Ende ===")
