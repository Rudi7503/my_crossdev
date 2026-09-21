# PROTOCOL_V4_SYNC.md — Synchronisation V4-Master ⇄ ESP32-Slave

**Fassung:** 3.0 (`proto_ver = 3`, 128-Byte-READ-Frame)
**Gilt für:** Vampire V4 (I²C-Master, AmigaOS) ⇄ ESP32 (I²C-Slave)
**Status:** auf echter Hardware durchgelaufen — PING, Status, Scan, Verbinden,
SD-Karte, Verzeichnis, Datei und **Wiedergabe über Bluetooth**; Mitschnitte in
`tests/field-log-v4-probe.txt` und `tests/field-log-v4-console-play.txt`.

Dieses Dokument ist die **verbindliche Fassung** der Schnittstelle. Das frühere
`PROTOCOL_V4_MASTER.md` ist nicht mehr vorhanden; wo unten eine Regel von der
damaligen Spezifikation abweicht, steht das ausdrücklich dabei (Abschnitt 10).
Alle Offsets, Codes und Zeitwerte sind aus der Referenzimplementierung
(`v4_proto.h`, `v4_proto.c`, `v4_master.c`) übernommen und im Feld überprüft.

---

## 1. Rollen, Transport, Adresse

| Punkt | Festlegung |
|---|---|
| Master | Vampire V4, AmigaOS, `i2c.library` V39+ (`i2c.library 40.0 (09 Dec 21) for Apollo Core boards`) |
| Slave | ESP32, 7-Bit-Adresse **0x50** — auf der Leitung also `0xA0` (Schreiben) bzw. `0xA1` (Lesen) |
| Bus | 3,3 V, ein Master; der Slave **darf nicht clock-stretchen** |
| Taktrate | Spezifikation nennt 100 kHz als robust, 400 kHz als möglich; die Library wird nicht umkonfiguriert |
| Byte-Reihenfolge | **Mehrbyte-Felder auf der Leitung immer Little Endian** (§0). Ein Byte-Order-Fehler bleibt im Linux-Harness unsichtbar — deshalb der Selbsttest |
| Laufzeitverhalten | `SendI2C`/`ReceiveI2C` blockieren bis zum Ende der Transaktion. **Die Library kennt keine Zeitüberwachung**: hängt der Slave, blockiert der Master unbegrenzt. Es gibt also keinen Timeout, auf den man sich verlassen könnte — die Verlässlichkeit liegt beim Slave |

**Byte-Zugriff (verbindlich).** Frame-Bytes werden ausschließlich byteweise
berührt, über `v4p_put_*_le` / `v4p_get_*_le`. Verboten sind
`*(uint16_t *)p`, `*(uint32_t *)p`, `memcpy` auf Structs, `packed` und
Bitfelder. `make lint` (Portabilitätsregeln) und `make asm` (m68k-Objektcode
greift nur mit `move.b` auf Puffer zu) erzwingen das beim Build.

---

## 2. Rahmen

Es gibt drei Rahmenarten. **Die Längen sind fest** — auch wenn weniger Nutzlast
nötig wäre. Genau daran erkennt der Master einen verschobenen Strom.

### 2.1 WRITE — Master → Slave, fest 32 Byte

| Offset | Länge | Feld | Inhalt |
|---|---|---|---|
| 0 | 1 | `magic` | **0xA5** |
| 1 | 1 | `cmd` | Befehlscode (Abschnitt 4) |
| 2 | 1 | `seq` | Sequenznummer (Abschnitt 3.2) |
| 3 | 1 | `len` | Länge der Nutzlast, 0…27 |
| 4 | 27 | `payload` | Nutzlast, Rest mit 0x00 aufgefüllt |
| 31 | 1 | `crc8` | CRC-8 über Byte 0…30 |

### 2.2 READ — Slave → Master, fest 128 Byte

| Offset | Länge | Feld | Inhalt |
|---|---|---|---|
| 0 | 1 | `magic` | **0x5A** |
| 1 | 1 | `cmd` | **Echo** des empfangenen `cmd` |
| 2 | 1 | `seq` | **Echo** der empfangenen `seq` |
| 3 | 1 | `status` | Statuscode (Abschnitt 5) |
| 4 | 1 | `len` | Länge der Nutzlast, 0…**121** |
| 5 | 121 | `payload` | Nutzlast, Rest mit 0x00 aufgefüllt |
| 126 | 1 | `flags` | `V4P_FLAG_*` (Bit 0 = `NAME_TRUNCATED`) |
| 127 | 1 | `crc8` | CRC-8 über Byte 0…126 |

Prüfreihenfolge des Masters (jede Stufe bricht ab und wird wiederholt):
`Länge == 128` → `magic == 0x5A` → `crc8` → **`cmd`/`seq` zurückgespiegelt** →
`len <= 121`. Erst danach wird die Nutzlast angefasst.

### 2.3 BULK — Slave → Master, fest 12 + `chunk` Byte

Nur für `FILE_READ` (und als Fehlerantwort darauf). Es ist der einzige Rahmen
mit variabler Länge, und die Länge ist **konstant für die Dauer eines
`chunk`** — auch der letzte, kurze Block wird auf `chunk` aufgefüllt.

| Offset | Länge | Feld | Inhalt |
|---|---|---|---|
| 0 | 1 | `magic` | **0x5B** |
| 1 | 1 | `cmd` | Echo (`0x51`) |
| 2 | 1 | `status` | Statuscode |
| 3 | 1 | `handle` | Echo des Dateihandles |
| 4 | 2 | `block` | Echo des Blockindex (LE) |
| 6 | 2 | `len` | tatsächliche Nutzlast im Datenfeld (LE) |
| 8 | `chunk` | `data` | Daten, Rest mit 0x00 aufgefüllt |
| 8+`chunk` | 4 | `crc32` | CRC-32 über Byte 0…7+`chunk` (LE) |

`chunk` ist 16…1024 (Vorgabe 128) und wird mit `SET_CHUNK` vereinbart; der
Master prüft `handle`, `block` und `cmd` gegen seine Erwartung
(`V4P_CHECK_MISMATCH`).

### 2.4 CRC-Parameter (verbindlich, identisch auf beiden Seiten)

| Rahmen | Verfahren | Parameter |
|---|---|---|
| WRITE, READ | **CRC-8** | Polynom `0x07`, Startwert `0x00`, MSB-first, **keine** Reflexion, **kein** XOR am Ende — Bit-für-Bit: `crc ^= byte; 8×: crc = (crc & 0x80) ? (crc << 1) ^ 0x07 : (crc << 1)` |
| BULK | **CRC-32** | Standard-`zlib`/IEEE 802.3: Polynom `0xEDB88320` (reflektiert), Startwert `0xFFFFFFFF`, XOR am Ende `0xFFFFFFFF`. Gegenprobe: `python3 -c "import zlib;print(hex(zlib.crc32(b'123456789')))"` → `0xcbf43926` |

Der CRC-32 deckt **die Auffüllbytes mit ab** — der Slave muss also vor dem
Rechnen auf `chunk` auffüllen.

---

## 3. Das Synchronisationsmodell

Die drei Regeln, auf denen alles beruht:

* **R1 — genau eine Antwort pro Befehl.** Auf jeden WRITE-Rahmen antwortet der
  Slave genau einmal: mit einem READ-Rahmen (128 Byte) oder — bei `FILE_READ` —
  mit einem BULK-Rahmen (12+`chunk`). Nie gar nicht, nie zweimal.
* **R2 — konstante Antwortlänge.** Die Länge hängt nur von der Rahmenart und
  `chunk` ab, **nie** von der Nutzlast. Ein Slave, der „so viele Bytes wie
  nötig" sendet, ist nicht protokollkonform.
* **R3 — vor jedem Lesen warten.** Der Slave kann nicht stretchen; ein zu früh
  gelesener Rahmen ist Müll. Der Master wartet `t_wait` (Abschnitt 6) und liest
  erst dann.

### 3.1 SEQ: wann sie steigt und wann nicht

| Ereignis | SEQ |
|---|---|
| Befehl erfolgreich beantwortet (`status == 0x00`) | **+1** für den nächsten Befehl |
| Framing-Fehler (Magic/CRC/len/Größe), Lesefehler | **unverändert** |
| `status == BUSY (0x01)` | **unverändert** (der Befehl gilt als nicht angenommen) |
| `status == BAD_CRC (0x02)` | **unverändert** |
| Antwort gehört zu `cmd`/`seq` nicht (Echo falsch) | **unverändert**, Antwort verworfen |
| `status` ist ein anderer Fehler (z. B. `NOT_FOUND`) | **+1** — der Befehl wurde verstanden und ausgeführt |

Der Slave **muss `cmd` und `seq` unverändert zurückspiegeln** — auch bei einem
Fehler. Das ist das einzige Mittel des Masters, einen verschobenen Strom zu
erkennen (ein um ein Byte versetzter Rahmen hat eine andere Magic oder CRC, aber
mit Echo-Prüfung fällt auch der Fall auf, dass zufällig ein *alter* Rahmen
gelesen wird).

### 3.2 Wiederholungen

| Fall | Verhalten des Masters | Obergrenze |
|---|---|---|
| Schreibfehler (Library-Fehler) | gleicher Rahmen, gleiche SEQ | 4 Versuche (`V4_RETRIES`, mit `-r` erhöhbar) |
| Lesefehler (Library-Fehler) | gleicher Rahmen, gleiche SEQ, **als unsicher gezählt** | dieselben Versuche |
| Framing-Fehler nach gültigem Lesen | gleicher Rahmen, gleiche SEQ, **unsicher**, und **verlängerte Wartezeit** (Abschnitt 6) | dieselben Versuche |
| `BUSY` | ganzer Befehl neu, gleiche SEQ, `t_busy` dazwischen | 40 Runden ≈ 80 ms (`V4_BUSY_TRIES`), bei `PLAY_FILE`/`STOP_PLAY` 500 Runden ≈ 1 s |
| `BAD_CRC` | ganzer Befehl neu, gleiche SEQ | 4 Runden (`V4_BADCRC_TRIES`) |

Sind die Versuche erschöpft, meldet der Master `V4_ERR_LINK`
(„keine gültige Antwort") und nennt im Log den letzten Prüfgrund.

### 3.3 Idempotenz — die gefährliche Stelle

Wird ein Befehl **nach einem erfolgreichen Schreiben** wiederholt (Lesefehler,
Framing-Fehler), dann kann der Slave ihn zweimal ausgeführt haben. Für
idempotente Befehle (`GET_STATUS`, `DEV_GET`, `DIR_OPEN` …) ist das harmlos.
**Nicht idempotent sind:**

* `CONNECT`, `DISCONNECT`, `FORGET` — ein zweiter Aufruf wechselt den Zustand
* `FILE_OPEN`, `DIR_OPEN` — ein zweiter Aufruf kann ein **zweites Handle**
  belegen (der Slave hat nur `V4P_MAX_DIR_HANDLES = 2` bzw.
  `V4P_MAX_FILE_HANDLES = 4`!). Der Master zählt solche Fälle als
  `unsafe_retries` und meldet `possible_handle_leaks` im Log
* `PLAY_FILE`, `STOP_PLAY`, `PATH_APPEND` — der zweite Aufruf ist zwar
  inhaltlich gleich (derselbe Pfad ist idempotent), kostet aber eine
  Audio-Umschaltung

**Regel für beide Seiten:** nach einem unsicheren Wiederholungsfall ist der
Zustand des Slaves zu *erfragen*, nicht anzunehmen. Der Master tut das (z. B.
`GET_STATUS` nach `CONNECT`) — der Slave muss seinen Zustand dabei ehrlich
melden.

### 3.4 Handle-Disziplin

* Öffnen und Schließen müssen sich **paaren**: `DIR_OPEN`/`DIR_CLOSE`,
  `FILE_OPEN`/`FILE_CLOSE`. Der Slave gibt Handles **nur** bei `*_CLOSE` frei.
* Der Master schließt Handles auch im Fehlerpfad (`DIR_CLOSE`/`FILE_CLOSE`
  immer) und liest eine Datei bis `END` oder bis zum Fehler.
* Ist ein Handle nicht offen, antwortet der Slave `NO_HANDLE (0x0B)` — **nicht**
  mit einem Absturz und nicht mit einem anderen Status.
* Feldbefund: `NO_HANDLE` auf `DIR_OPEN` hieß in der Praxis fast immer, dass die
  Karte **nicht eingerichtet** war (der vorangegangene `SD_MOUNT` war am Framing
  gescheitert). Reihenfolge deshalb: `SD_MOUNT` bestätigen, dann `DIR_OPEN`.

### 3.5 Zustandsmaschine

| Zustand | Wert | erlaubte Befehle (Auszug) |
|---|---|---|
| `IDLE` | 0 | alles außer `DISCONNECT`; `SCAN_START` beginnt die Suche |
| `SCANNING` | 1 | `DEV_COUNT`, `DEV_GET`, `CONNECT`, `SCAN_STOP` |
| `CONNECTING` | 2 | nur `GET_STATUS` (der Aufbau läuft) |
| `CONNECTED` | 3 | `DISCONNECT`, `SD_*`, `DIR_*`, `FILE_*`, `PLAY_FILE`, `STOP_PLAY` |
| `SUSPENDED` | 4 | reserviert |

Ein Befehl im falschen Zustand ergibt `BAD_STATE (0x05)`. **Wichtig:** `CONNECT`
ist nur der Startschuss — der Aufbau dauert Sekunden (im Feld ~22 s bis
`CONNECTED`), deshalb wird `GET_STATUS` gepollt und **`conn_index` gegen den
gewünschten Index geprüft**. Meldet der Slave `CONNECTED` mit einem anderen
Index oder mit `0xFF` (unbekannt), ist das ein Fehler und zu protokollieren.

---

## 4. Befehle

Nutzlastlängen beziehen sich auf den WRITE-Rahmen; Antwortlängen auf die
READ-Nutzlast (bzw. BULK bei `FILE_READ`). „unsicher" markiert Befehle, die nach
einem Wiederholungsfall nicht gefahrlos ein zweites Mal laufen (Abschnitt 3.3).

| Code | Befehl | Nutzlast | Antwort | Besonderheiten |
|---|---|---|---|---|
| `0x01` | `PING` | 0 | 12 (`v4p_info_t`) | Handshake; `proto_ver`, `write_frame_len`, `read_frame_len` müssen 3 / 32 / 128 sein |
| `0x02` | `GET_STATUS` | 0 | 17 (`v4p_status_t`) | nie wiederholen bei `status != 0` (reines Lesen) |
| `0x03` | `GET_INFO` | 0 | 12 | wie `PING` |
| `0x10` | `SCAN_START` | Muster/Dauer/Flag | 0 | asynchron! Erfolg heißt nur „Suche läuft" |
| `0x11` | `SCAN_STOP` | 0 | 0 | |
| `0x12` | `DEV_COUNT` | 0 | ≥1 | erst sinnvoll, wenn der Scan Geräte gefunden hat |
| `0x13` | `DEV_GET` | Index (1) | 8 + Name | Name max 113 Zeichen |
| `0x20` | `CONNECT` | Index (1) | 0 | **unsicher**; Ergebnis über `GET_STATUS` prüfen |
| `0x21` | `CONNECT_BDA` | BDA (6) | 0 | **unsicher** |
| `0x22` | `DISCONNECT` | 0 | 0 | **unsicher**; nur bei `CONNECTED` senden |
| `0x23` | `FORGET` | Index (1) | 0 | **unsicher** |
| `0x30` | `SD_MOUNT` | Force-Flag (1) | 0 | kann `BUSY` liefern → Runden abwarten |
| `0x31` | `SD_INFO` | 0 | 11 (`v4p_sdinfo_t`) | |
| `0x32` | `SET_CHUNK` | `chunk` (LE16) | 0 | 16…1024 |
| `0x38` | `PATH_CLEAR` | 0 | 0 | setzt den Pfadspiegel auf die Wurzel |
| `0x39` | `PATH_APPEND` | Fragment | 0 | ergänzt den Spiegel; max. 27 Byte je Aufruf |
| `0x40` | `DIR_OPEN` | Pfad (0 = Spiegel) | 1 (Handle) | **unsicher** |
| `0x41` | `DIR_NEXT` | Handle (1) | 8 + Name | `END` am Ende; Name auf 113 gekürzt (`NAME_TRUNCATED`) |
| `0x42` | `DIR_CLOSE` | Handle (1) | 0 | |
| `0x50` | `FILE_OPEN` | Pfad (0 = Spiegel) | 6 | **unsicher** |
| `0x51` | `FILE_READ` | Handle + Block (LE16) | **BULK** | letzter Block kürzer, aber auf `chunk` aufgefüllt |
| `0x52` | `FILE_CLOSE` | Handle (1) | 0 | |
| `0x60` | `PLAY_FILE` | Pfad | 0 | **unsicher**; Umbau der Audio-Pipeline → langes BUSY-Budget (500 Runden) |
| `0x61` | `STOP_PLAY` | 0 | 0 | wie `PLAY_FILE` |
| `0x7E` | `RESET` | 0 | 0 | Slave in den Grundzustand |

**Pfadregeln.** `len == 0` im WRITE-Rahmen bedeutet „benutze den PATH-Spiegel".
Der leere String und `"/"` meinen die **Wurzel** — als Nutzlast wird dann ein
einzelnes `/` (Länge 1) gesendet, nicht Länge 0 (das würde den Spiegel öffnen).
Ein Pfad bis 27 Byte passt in einen Rahmen; längere werden über
`PATH_CLEAR`/`PATH_APPEND` in 27-Byte-Stücken aufgebaut.

---

## 5. Status- und Prüfcodes

| Status | Wert | Bedeutung | Master wiederholt? |
|---|---|---|---|
| `OK` | 0x00 | erledigt, Nutzlast gültig | — |
| `BUSY` | 0x01 | nicht fertig | ja, gleiche SEQ, `t_busy` |
| `BAD_CRC` | 0x02 | Request-Framing verworfen | ja, gleiche SEQ |
| `BAD_CMD` | 0x03 | unbekannt | **nein** |
| `BAD_ARG` | 0x04 | Argument unzulässig | nein |
| `BAD_STATE` | 0x05 | im Zustand nicht erlaubt | nein |
| `NO_SD` | 0x06 | keine Karte / Mount fehlgeschlagen | nein |
| `NOT_FOUND` | 0x07 | Gerät/Pfad/Eintrag fehlt | nein |
| `IO_ERR` | 0x08 | Dateisystem-/Treiberfehler | nein |
| `END` | 0x09 | Liste/Datei zu Ende | nein |
| `TOO_LONG` | 0x0A | Pfad/Name zu lang | nein |
| `NO_HANDLE` | 0x0B | Handle nicht offen | nein |
| `BT_ERR` | 0x0C | Bluetooth-Stack hat abgelehnt | nein |

Fehler des Masters (nicht auf der Leitung): `V4_ERR_ARG 0xE0`,
`V4_ERR_NOMEM 0xE1`, … `V4_ERR_LINK 0xE4` („keine gültige Antwort"),
`V4_ERR_PROTO 0xE7`. Prüfgründe im Log: `SIZE`, `MAGIC`, `CRC`, `ECHO`, `LEN`,
`MISMATCH` (`v4_check_str()`).

---

## 6. Zeitverhalten — die wichtigste Feldlehre

| Größe | Vorgabe | Bedeutung |
|---|---|---|
| `t_wait` | **2000 µs** | Wartezeit **vor jedem** Lesen. Die Spezifikation nennt eine **Untergrenze** — länger warten ist erlaubt und im Feld sogar nötig |
| `t_busy` | 2000 µs | Wartezeit vor dem erneuten Senden nach `BUSY` |
| BUSY-Runden | 40 (≈80 ms) | je Befehl; bei `PLAY_FILE`/`STOP_PLAY` 500 (≈1 s) |
| CONNECT-Dauer | Sekunden | im Feld ~22 s bis `CONNECTED`; nicht als Fehler deuten |
| Verzeichnisliste | ~1 Transaktion je Eintrag | 72 Einträge = ~74 Transaktionen |

**Gemessen auf der V4 mit dem ESP32:** mit `t_wait = 2000 µs` lieferte der Slave
bei rund jedem vierten Lesen einen unbrauchbaren Rahmen (`falsche Magic`),
während der Busfehler `0x000000FF` = **OK** meldete — die Übertragung lief also,
die Antwort war nur noch nicht fertig. Mit `t_wait = 10000 µs` lief eine
72-Einträge-Liste auf Anhieb durch.

Deshalb (Master-Seite, alles implementiert):

1. **Erster Versuch** immer mit `t_wait` — die Regel R3 bleibt eingehalten.
2. **Bei einem Framing-Fehler verdoppelt** der Master die Wartezeit für den
   nächsten Versuch (bis 100 ms, `V4_WAIT_BACKOFF`).
3. **Die verdoppelte Zeit bleibt als neue Grundwartezeit** erhalten
   (Selbstkalibrierung). Im Feld stand danach `t_wait=32000 us` im Log, und die
   Sitzung lief ohne weiteren Fehler durch.
4. Schalter der Referenzkonsole: `-w <us>` pinnt `t_wait`, `-r <n>` setzt die
   Versuche je Transaktion, `-c <s>` die Wartezeit auf `CONNECTED`.

**Für die Slave-Seite heißt das:** die Antwort muss **innerhalb von `t_wait`**
fertig sein, sonst ist das Protokoll nicht einhaltbar. Wird die Antwort erst in
einem späteren Task fertig, muss der Slave den Rahmen bis dahin mit
**`status = BUSY`** beantworten (das ist der vorgesehene Weg!) statt einen
halbfertigen Rahmen zu senden.

---

## 7. Was der Slave garantieren muss (Checkliste)

1. **Adresse 0x50** (7 Bit) bestätigen; auf jeden WRITE-Rahmen **genau eine**
   Antwort mit **konstanter** Länge.
2. **`cmd` und `seq` unverändert zurückspiegeln** — auch im Fehlerfall.
3. **`len` korrekt** setzen (≤ 121) und die restliche Nutzlast mit 0x00 füllen.
4. **CRC-8 über Byte 0…126** (READ) bzw. **0…30** (WRITE prüfen) mit den
   Parametern aus Abschnitt 2.4; bei falschem CRC `BAD_CRC` antworten (nicht
   schweigen — der Master wartet sonst bis zum Library-Fehler).
5. **Nicht stretchen** und die Antwort **innerhalb von `t_wait`** liefern; sonst
   `BUSY`.
6. **Zustand ehrlich melden**: `state`, `conn_index` (0xFF = unbekannt),
   `dev_count`, `sd_mounted`, `sd_card_present`, `scan_gen`, `audio_flags`,
   `chunk`, `proto_ver`, `fw_ver` — der Master trifft Entscheidungen daraus.
7. **Handles begrenzt und paarig**: max. 2 Verzeichnis-, 4 Dateihandles; ohne
   offenes Handle `NO_HANDLE`.
8. **`SD_MOUNT` bestätigen**, bevor Verzeichnisse geöffnet werden.
9. **`CONNECT` asynchron**: `CONNECTING` melden, dann `CONNECTED` (oder `IDLE`
   bei Fehlschlag) — mit korrektem `conn_index`.
10. **`PLAY_FILE`** idempotent für denselben Pfad; `audio_flags` auf `0x03`
    (A2DP + SD-Wiedergabe), nach `STOP_PLAY` zurück auf `0x01` (nur A2DP).

## 8. Was der Master garantiert (Checkliste)

1. **Byteweise** Frame-Zugriffe, Little Endian — erzwungen durch `make lint` und
   `make asm`.
2. **Wartet `t_wait`** vor jedem Lesen, `t_busy` vor jeder BUSY-Wiederholung.
3. **Wiederholt nur wie in Abschnitt 3.2**, mit **gleicher SEQ**; `SEQ` steigt
   ausschließlich nach einer gültigen Antwort.
4. **Prüft jede Antwort** (Größe, Magic, CRC, Echo, `len`) und zählt unsichere
   Wiederholungen.
5. **Fragt den Zustand**, statt ihn anzunehmen (nach `CONNECT`, bei `NO_HANDLE`).
6. **Schließt Handles** auch im Fehlerpfad.
7. **Protokolliert den Prüfgrund** jedes Fehlers (`print_failure`) samt
   `t_wait`, Versuchszahl und Rahmenbilanz.

---

## 9. Diagnose-Konventionen

* **`i2c.library`-Fehlercodes:** `CC != 0` heißt **OK** (`0x000000FF`), `CC == 0`
  heißt Fehler. Das ist umgekehrt zu „0 = OK" und war die teuerste Verwechslung
  dieses Projekts (`v4_i2c_err_is_ok()`).
* **Logdatei:** das Konsolenprogramm schreibt jede Zeile zusätzlich nach
  `Programs:test/v4_console.log` (`make log` holt sie per `acp`). Jede Zeile
  wird geschlossen, damit ein Absturz sie nicht verschluckt; mit `-t 50` wartet
  das Programm zusätzlich 1 s je Zeile.
* **`-a`** schaltet den Trace ein (jede Transaktion, `cmd=0x.. (KLARTEXT)`),
  dazu die Diagnoseschritte.
* **Framingfehler richtig lesen:** bleibt `retries` im Bericht konstant, war das
  Framing in Ordnung — dann hat der Slave geantwortet und das Problem liegt bei
  ihm (`NO_HANDLE`, `BAD_STATE` …). Steigt `retries`, kommt der Rahmen nicht
  sauber an → `t_wait`/`-w`, `-r` oder Verkabelung.
* **Takt/Leitung:** `SDA always LO/HI`, `SDA trashed` deuten auf Pegel oder
  fehlende Pull-ups, nicht auf das Protokoll.

---

## 10. Offene Punkte und Abweichungen von der alten Spezifikation

1. **§8.2 gegen §14.3 (BUSY hinter dem Dateiende).** Umgesetzt ist §8.2: eine
   BUSY-Runde, dann `END`. Der Master versteckt BUSY hinter 40 internen Runden,
   ein Client sieht es nie. **Mit der ESP32-Seite abzugleichen.**
2. **`BAD_CRC`: Anzahl der Wiederholungen** war nicht festgelegt; eingeführt ist
   `V4_BADCRC_TRIES = 4` in der Größenordnung der Framing-Versuche.
3. **`SD_MOUNT` mit `BUSY`** (§8.1 gegen §6/§14.1): der Aufrufer fährt die Runden
   selbst (`v4_sd_mount_wait`).
4. **Zu kurze Nutzlast bei gültigem Rahmen** wird als Fehler behandelt, nicht
   als Erfolg mit fehlenden Feldern.
5. **Pfadregeln:** leerer String und `"/"` = Wurzel (siehe Abschnitt 4).
6. **`NAME_TRUNCATED`** steht im READ-Frame (`+126`), nicht in der Nutzlast; der
   Aufrufer erfährt es über `v4p_dev_t.flags`/`v4p_dirent_t.flags`.
7. **Idempotenz gegen Wiederholung** — die schärfste Lücke: siehe Abschnitt 3.3.
8. **Flags-Offset** `+126` (die alte Überschrift „+62" stammt aus der
   64-Byte-Fassung).
9. **`GET_STATUS`-Tabelle:** maßgeblich sind die `V4P_ST_OFF_*`-Offsets, `L = 17`,
   `sd_card_present` als neues Byte bei `+16`.
10. **USB-Adapter (§9) widerspricht §1.5** — redaktionell, offen. Die
    Spezifikation rät zu einem USB-Adapter, die Zielplattform ist aber der
    Direktanschluss an der V4.

**Geräte in der Scan-Liste:** der Slave liefert je Eintrag nur Name und
Bluetooth-Adresse — **kein** Feld „erreichbar/verbunden". Ein ausgeschaltetes
Headset kann also in der Liste stehen. Der Master kann das nicht unterscheiden;
er verbindet und prüft den Zustand (`IDLE`/Zeitablauf = nicht erreichbar). Wenn
die Liste aufgeräumt werden soll, muss das der Slave tun — `scan_gen` zeigt an,
ob die Liste überhaupt neu aufgebaut wurde.

---

## 11. Referenzimplementierung

| Datei | Inhalt |
|---|---|
| `v4_proto.h` / `v4_proto.c` | Codes, Rahmenaufbau, Codecs, CRCs, Selbsttest der Byte-Ordnung |
| `v4_master.h` / `v4_master.c` | Transaktionskern (`v4_transact_n`), Wiederholungen, BUSY/BAD_CRC, Befehle, Zeitverhalten |
| `v4_console.c` | Konsolenprogramm: PING, Scan, Verbinden, Dateibrowser, Wiedergabe, Logdatei |
| `tools/v4_probe.c` | Stufenprobe für die Hardware (Schritt für Schritt mit Logdatei) |
| `tests/` | 157 Testfälle, 12 Smoke-Läufe, Feldmitschnitte |
| `README.md` | Verifikationsstand, Klärungen, Bedienung, offene Punkte |

Prüfen lässt sich alles ohne Hardware: `make verify` (Lint, Tests, Sanitizer,
Smoke, m68k-Bytezugriff, `moviw`-Gate, Amiga-Build, Stufenprobe).
