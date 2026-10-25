# amiga_tool.py – lokale amiagent-Brücke

## Stand

- **amiagent 0.13.2** (aktuellstes Release, 04.10.2026)
- Agent auf der V4: `C:amiagent.020`, Port **7846**
- Der 68080-Build liegt als `C:amiagent` daneben, wird aber nicht verwendet;
  der offizielle 020-Build lauscht zuverlässig.
- Token: lokal in `~/.config/apollo-agent/env` (mode 600), **nicht im Repo**.
- Kein Cline, kein DeepSeek-API-Key, kein MCP nötig – der Harness ruft das
  CLI über Bash auf.

## Aufrufe

```bash
./Tools/amiga_tool.py probe
./Tools/amiga_tool.py info
./Tools/amiga_tool.py shell "dir C:"
./Tools/amiga_tool.py list C:
./Tools/amiga_tool.py read C:Version
./Tools/amiga_tool.py screenshot /tmp/v4.png
./Tools/amiga_tool.py pointer
./Tools/amiga_tool.py move 200 200
./Tools/amiga_tool.py click 100 100 --button left
./Tools/amiga_tool.py type "hello"
./Tools/amiga_tool.py key return
./Tools/amiga_tool.py ui-tree
./Tools/amiga_tool.py ui-click OK --window "Requester"
./Tools/amiga_tool.py menus
./Tools/amiga_tool.py menu-select "About..."
./Tools/amiga_tool.py arexx "ADDRESS AMIGA..."
```

## Verifiziert

- `probe`: amiagent 0.13.2, CPU **68080**, FPU, RAM, Volumes
- `shell C:Version`: Kickstart 51.51 / Workbench 40.0
- `list C:`: Dateien/Protection
- `screenshot`: 1280×720 PNG, korrekte Farben
- `move` + `pointer`: Pointer bei 200/200
- `ui-tree`: Workbench-Screen/Gadget-Baum lesbar

## Sicherheit

- TCP **plain**, Token im Klartext; nur vertrautes LAN, **kein Portforwarding**.
- Token und DeepSeek-API-Key sind getrennt; Token niemals in Chat/Git.
- Agent läuft **manuell**; Autostart ist noch nicht aktiv und braucht Freigabe.
- `amiga_tool.py` führt nur explizite Aufrufe aus.
