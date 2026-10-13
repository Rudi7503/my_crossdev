#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
v4ser.py - serielle Verbindung zur Vampire V4, sauber.

WAS DAS PROGRAMM MACHT
    Es haelt die serielle Leitung zur V4 (Vorgabe /dev/ttyUSB1, 115200 Baud),
    zeigt Empfangenes LIVE mit Zeitstempel an und schreibt es zusaetzlich in
    eine Logdatei. Befehle, die an die V4 gehen sollen, werden aus einer
    Kommandodatei gelesen - damit braucht kein zweites Programm den Port.

WARUM SO UND NICHT ANDERS
    Der erste Versuch ging schief, weil zwei Programme gleichzeitig denselben
    Port geoeffnet hatten: zwei Leser auf einer Leitung ergeben Zeichenbrei
    oder Schweigen, und niemand weiss, wer schuld ist. Deshalb:
      * genau EIN Prozess haelt den Port (Sperrdatei, siehe SPERRDATEI)
      * gesendet wird ueber die Kommandodatei, nicht ueber einen zweiten Prozess
      * alles wird mitgeschrieben, damit man nachsehen kann

DIE V4-SEITE
    Die V4 sendet ueber den direkten Hardware-UART (SERPER, 115200). Das
    Pruefprogramm dazu ist Projects/BT-Joy/serprobe: es sendet, empfaengt und
    schickt jede empfangene Zeile als "V4-HAT-EMPFANGEN: ..." zurueck.

AUFRUF
    v4ser.py                     Vorgabe: /dev/ttyUSB1, 115200, Fensterbetrieb
    v4ser.py --port <pfad>
    v4ser.py --baud <zahl>
    v4ser.py --log <datei>       Vorgabe: logs/v4_seriell.log
    v4ser.py --sende "<text>"    einmal senden und beenden
    v4ser.py --dauer <sekunden>  nach dieser Zeit beenden (Vorgabe: endlos)
"""

import argparse
import os
import sys
import time

try:
    import serial
except ImportError:
    print("pyserial fehlt: python3 -m pip install pyserial", file=sys.stderr)
    raise SystemExit(1)

SPERRDATEI = "/tmp/v4ser.lock"
KOMMANDODATEI = "/home/papa/Vampire_V4/ApolloCrossDev/Projects/BT-Joy/logs/v4ser_cmd.txt"


def sperre_nehmen():
    """Sicherstellen, dass nur EIN Programm den Port haelt."""
    if os.path.exists(SPERRDATEI):
        try:
            with open(SPERRDATEI, "r") as f:
                pid = int(f.read().strip())
            os.kill(pid, 0)        # laeuft der noch?
            print("### Es laeuft bereits v4ser (PID %d). Erst beenden. ###" % pid,
                  file=sys.stderr)
            return False
        except (ValueError, ProcessLookupError, PermissionError, OSError):
            pass                    # Leiche - Sperre ist verwaist
    with open(SPERRDATEI, "w") as f:
        f.write(str(os.getpid()))
    return True


def sperre_freigeben():
    try:
        if os.path.exists(SPERRDATEI):
            with open(SPERRDATEI, "r") as f:
                if int(f.read().strip()) == os.getpid():
                    os.remove(SPERRDATEI)
    except (ValueError, OSError):
        pass


def main():
    p = argparse.ArgumentParser(description="Serielle Verbindung zur Vampire V4")
    p.add_argument("--port", default="/dev/ttyUSB1")
    p.add_argument("--baud", type=int, default=115200)
    p.add_argument("--log", default="/home/papa/Vampire_V4/ApolloCrossDev/Projects/BT-Joy/logs/v4_seriell.log")
    p.add_argument("--sende", default=None, help="einmal senden und beenden")
    p.add_argument("--dauer", type=float, default=0.0, help="0 = endlos")
    p.add_argument("--herzschlag", type=float, default=5.0,
                   help="alle N Sekunden eine Zeile an die V4 senden (0 = aus)")
    args = p.parse_args()

    if not sperre_nehmen():
        return 1

    try:
        os.makedirs(os.path.dirname(args.log), exist_ok=True)
    except OSError:
        pass

    try:
        ser = serial.Serial(args.port, args.baud, timeout=0.2)
    except Exception as e:
        print("### %s nicht zu oeffnen: %s ###" % (args.port, e), file=sys.stderr)
        print("### Angesteckt? Durchgereicht? (usbipd attach) ###", file=sys.stderr)
        sperre_freigeben()
        return 1

    log = open(args.log, "a", buffering=1, encoding="utf-8", errors="replace")

    def ausgabe(text):
        print(text, flush=True)
        log.write(text + "\n")

    ausgabe("=== v4ser auf %s @ %d Baud, %s ===" %
            (args.port, args.baud, time.strftime("%Y-%m-%d %H:%M:%S")))
    ausgabe("=== Befehle an die V4: eine Zeile in %s schreiben ===" % KOMMANDODATEI)
    ausgabe("=== Mitschnitt: %s ===" % args.log)

    if args.sende is not None:
        ser.write((args.sende + "\n").encode("utf-8"))
        ausgabe("SENDEN: %s" % args.sende)
        ser.close()
        log.close()
        sperre_freigeben()
        return 0

    # Kommandodatei ab ihrer aktuellen Laenge lesen - alte Zeilen nicht wiederholen.
    try:
        with open(KOMMANDODATEI, "r") as f:
            f.seek(0, os.SEEK_END)
            stand = f.tell()
    except OSError:
        open(KOMMANDODATEI, "a").close()
        stand = 0

    zeile = ""
    t0 = time.time()
    herz = 0
    herz_naechste = 0.0
    try:
        while True:
            if args.dauer > 0 and (time.time() - t0) >= args.dauer:
                ausgabe("=== Zeit abgelaufen (%g s) ===" % args.dauer)
                break

            # 1. Etwas zu senden?
            try:
                with open(KOMMANDODATEI, "r") as f:
                    f.seek(stand)
                    for neu in f.read().splitlines():
                        if neu.strip():
                            ser.write((neu + "\n").encode("utf-8"))
                            ausgabe("%s  SENDEN    : %s" %
                                    (time.strftime("%H:%M:%S"), neu))
                    stand = f.tell()
            except OSError:
                pass

            # 2. Herzschlag: damit die V4 sieht, dass der PC da ist - und mit
            #    dem Pruefprogramm auf der V4 den Rundlauf bestaetigt.
            if args.herzschlag > 0 and (time.time() - t0) >= herz_naechste:
                herz += 1
                text = "PC-HALLO %d" % herz
                ser.write((text + "\n").encode("utf-8"))
                ausgabe("%s  SENDEN    : %s" % (time.strftime("%H:%M:%S"), text))
                herz_naechste += args.herzschlag

            # 3. Etwas empfangen?
            daten = ser.read(4096)
            if daten:
                for ch in daten.decode("utf-8", "replace"):
                    if ch == "\r":
                        continue
                    if ch == "\n":
                        if zeile:
                            ausgabe("%s  EMPFANGEN : %s" %
                                    (time.strftime("%H:%M:%S"), zeile))
                        zeile = ""
                    else:
                        zeile += ch
    except KeyboardInterrupt:
        ausgabe("=== mit Strg+C beendet ===")
    finally:
        ser.close()
        log.close()
        sperre_freigeben()

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
