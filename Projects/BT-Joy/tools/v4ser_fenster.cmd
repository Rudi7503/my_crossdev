@echo off
rem ---------------------------------------------------------------------------
rem v4ser_fenster.cmd - eigenes Fenster fuer die serielle Verbindung zur V4.
rem
rem Aufruf: Doppelklick, oder aus WSL heraus:
rem   cmd.exe /c start "" "\\wsl.localhost\Ubuntu\home\papa\Vampire_V4\ApolloCrossDev\Projects\BT-Joy\tools\v4ser_fenster.cmd"
rem
rem Es darf nur EIN v4ser laufen - das Programm fuehrt /tmp/v4ser.lock.
rem Befehle an die V4: eine Zeile in logs\v4ser_cmd.txt schreiben.
rem ---------------------------------------------------------------------------
setlocal
title V4 seriell - v4ser (Strg+C beendet)

echo ============================================================
echo   V4 seriell (SERPER, 115200)
echo.
echo   Strg+C beendet.
echo   Befehle an die V4: Zeilen in  logs\v4ser_cmd.txt
echo   Mitschnitt:                       logs\v4_seriell.log
echo ============================================================
echo.

wsl.exe -e bash -lc "cd /home/papa/Vampire_V4/ApolloCrossDev/Projects/BT-Joy && python3 tools/v4ser.py"

echo.
echo === beendet - Fenster bleibt offen, damit die Meldung lesbar ist ===
pause
