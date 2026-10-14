@echo off
rem ---------------------------------------------------------------------------
rem v4ser_fenster.cmd - V4-DebugUART live mitlesen, EIN Doppelklick genuegt.
rem
rem Aufruf: v4_logger.sh (findet die V4 ueber ihren STABILEN Namen
rem /dev/serial/by-id/usb-1a86_USB_Serial..., liest NUR, sendet nichts).
rem Die Zeilen erscheinen im Fenster UND in logs\v4_seriell.log.
rem
rem Fenster OFFEN LASSEN. Strg+C beendet.
rem ---------------------------------------------------------------------------
setlocal
title V4 - DebugUART (Strg+C beendet)

echo ============================================================
echo   V4-DebugUART live
echo.
echo   Die V4 schreibt hier jede Zeile ihres Verlaufs hin,
echo   auch wenn die Oberflaeche haengt. Nur LESEN, kein Senden.
echo   Mitschnitt: logs\v4_seriell.log
echo.
echo   Es erscheint nur etwas, wenn auf der V4 ein Programm
echo   laeuft - im Ruhezustand ist die Leitung still.
echo ============================================================
echo.

wsl.exe -e bash -lc "bash /home/papa/Vampire_V4/hilfen/v4_logger.sh"

echo.
echo === Ende - Fenster bleibt offen ===
pause
