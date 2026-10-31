# V4-Zugriff (ApolloOS / Vampire V4)

**Stand:** 2026-10-23  
**Host:** `ApolloOS`, `192.168.178.50` (DHCP; nach Neustart prüfen)  
**Windows-PC (LAN):** `192.168.178.54`  
**WSL:** `172.23.131.88/20` (NAT, **kein** Route ins `192.168.178.0/24`)

Wenn die IP wechselt: `acp -l` zeigt bei Erfolg `ApolloOS <IP>`; sonst im
Router/DHCP nachsehen und die IP hier sowie in `AMIGAHOST` aktualisieren.

## Wichtigster Punkt

Das **Linux-ACP aus WSL funktioniert nicht** für die V4:

```text
Projects/ApolloExplorer/acp/acp   # aus WSL: keine Route ins LAN
```

> **ApolloExplorer-Version:** aktuell halten (getestet: **1.4.0**). Alte
> ASH-Versionen können `C:Execute`/Batch-Aufrufe nicht ausführen.

Der funktionierende Weg ist das **Windows-ACP**:

```text
C:\Program Files\ApolloExplorer\acp.exe
```

Aus WSL heraus:

```bash
ACP="/mnt/c/Program Files/ApolloExplorer/acp.exe"
```

## Verbindung prüfen

```bash
ACP="/mnt/c/Program Files/ApolloExplorer/acp.exe"

# Host suchen (bei Erfolg: ApolloOS 192.168.178.50)
# Achtung: -l ist bei dieser V4 unzuverlässig; direkte IP verwenden.
"$ACP" -l

# Ram Disk auflisten
"$ACP" -d "192.168.178.50:Ram Disk"
```

Pfad-Syntax:

| Ziel | Befehl |
|---|---|
| Volume-Root | `-d "192.168.178.50:Ram Disk"` (ohne abschließenden Doppelpunkt) |
| Unterverzeichnis | `-d "192.168.178.50:Ram Disk/LOGS"` |
| Datei hochladen | `SRC=$(wslpath -w bin-v2/FUS-001); "$ACP" "$SRC" "192.168.178.50:Ram Disk/"` |
| Datei herunterladen | `"$ACP" "192.168.178.50:Ram Disk/LOGS/FUS-001.run01.log" "$(wslpath -w /mnt/c/Users/papa/V4LOGS)"` |
| Verzeichnis herunterladen | `"$ACP" -r "192.168.178.50:Ram Disk/LOGS" "$(wslpath -w /mnt/c/Users/papa/V4LOGS)"` |

`Ram Disk:` mit abschließendem Doppelpunkt ist mehrdeutig (`hostname:path`); deshalb
für den Volume-Root ohne letzten Doppelpunkt arbeiten.

## Windows-Pfade beachten

Das Windows-ACP erwartet **Windows-Pfade**. Wenn du es aus WSL aufrufst, lokale
Pfade mit `wslpath -w` umwandeln:

```bash
SRC=$(wslpath -w bin-v2/FUS-001)
"$ACP" "$SRC" "192.168.178.50:Ram Disk/"
```

Für Downloads am einfachsten in ein Windows-Verzeichnis laden, z. B.
`C:\Users\papa\V4LOGS`, und aus WSL über `/mnt/c/Users/papa/V4LOGS` darauf
zugreifen.

## Aktueller Zustand (2026-10-23, 22:15)

- V4 ist stabil erreichbar: Ping **0 % Verlust**, ARP OK,
  Windows-ACP findet `ApolloOS 192.168.178.50`.
- **44 Dateien** nach `Ram Disk:` hochgeladen: 42 v2-Binaries +
  `RUN-ALL` + `RUN-ALL-ONCE`.
- Alle 44 per Rückdownload **byte-identisch** verifiziert (44/44).
- `LOGS/` existiert noch nicht; das legt `RUN-ALL-ONCE` beim Start an.
- Nächster Schritt auf der V4: `Execute "Ram Disk:RUN-ALL-ONCE"`.
- v1-Artefakte auf dem PC bleiben unverändert (`bin/`, `build/`,
  `measurements/*.log`).

## Wenn die V4 nicht erreichbar ist

1. Auf der V4 prüfen, ob ApolloOS vollständig gebootet hat und das Netzwerk
   oben ist (IP in der Shell: `ShowNetStatus` / `ifconfig` je nach Installation).
2. IP prüfen: Router/DHCP oder auf der V4 `ping 192.168.178.54`.
3. Von Windows aus:
   ```powershell
   "C:\Program Files\ApolloExplorer\acp.exe" -l
   arp -a | findstr 192.168.178.50
   ```
   Kein ARP-Eintrag ⇒ V4 ist nicht im LAN (aus, Bootfehler, Kabel, andere IP).
4. Der ACP-Server auf der V4 muss laufen; ggf. ApolloExplorer auf der V4
   starten.
5. Port 1234 ist **kein** guter Lebendigkeitstest; ACP nutzt einen eigenen
   Dienst. `-l`/`-d` verwenden.
6. Ping mit Paketverlust/verschwindendem ARP ⇒ Netzwerkproblem (Kabel/WLAN,
   Last, IP-Konflikt), nicht ACP. Erst das Netz stabilisieren.
7. ACP-Aufrufe immer mit `timeout` fahren; eine hängende Verbindung lässt sich
   durch Neustart des ApolloExplorer-Servers auf der V4 lösen.
8. Upload-Ziel ist ein **Verzeichnis** (`...:Ram Disk/T/`), nicht der volle
   Ziel-Dateiname; ACP nimmt den Dateinamen vom Quellpfad.

## Dateien auf der V4

- `Ram Disk:` nur für temporäre Kopien; nach Stromausfall weg.
- Für dauerhafte Kopien ein persistentes Volume (`Work:`, `DH0:`, CF/SD)
  verwenden, sobald bekannt ist, welches Volume auf der V4 existiert.
- Die 42 v1-Messlogs liegen auf dem PC unter `measurements/`, nicht auf der V4.
