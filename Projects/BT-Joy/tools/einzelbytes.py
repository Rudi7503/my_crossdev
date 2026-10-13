#!/usr/bin/env python3
# Sendet wiederholt kurze, klar unterscheidbare Nachrichten und zeigt an, welche
# Bytewerte dabei auf die Leitung gehen. So ist der Vergleich mit dem moeglich,
# was die V4 anzeigt.
import serial, time, sys

log = open("/tmp/einzelbytes.log", "a", buffering=1)
def aus(t):
    print(t, flush=True); log.write(t + "\n")

try:
    s = serial.Serial("/dev/ttyUSB1", 115200, timeout=0.2)
except Exception as e:
    aus("Port nicht zu oeffnen: %s" % e); raise SystemExit

t0 = time.time()
runde = 0
while time.time() - t0 < 120:
    runde += 1
    for text in ("A", "B", "C"):
        nachricht = "SINGLE %s\n" % text
        s.write(nachricht.encode())
        aus("%s  gesendet: %-12s Bytes: %s" %
            (time.strftime("%H:%M:%S"), nachricht.strip(),
             " ".join("%02X" % b for b in nachricht.encode())))
        time.sleep(2.5)
    aus("%s  --- Runde %d ---" % (time.strftime("%H:%M:%S"), runde))
    time.sleep(2)
s.close()
aus("=== Ende ===")
