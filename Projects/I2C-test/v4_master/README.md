# v4_master — I2C-Master der Vampire V4

Umsetzung der **V4-Seite (I2C-Master)** aus [`../PROTOCOL_V4_MASTER.md`](../PROTOCOL_V4_MASTER.md)
(Version 2.0, Proto-Version 2) gegen den ESP32-Slave.

Enthalten sind der portable Protokollcode, zwei Plattformschichten (Linux-Harness
und Amiga), ein ESP32-Emulator für Tests ohne Hardware, die Testsuite aus §12/§13/§14
und ein interaktives Konsolenprogramm, das auf beiden Plattformen läuft.

```
v4_master/
├── v4_proto.h/.c        gemeinsamer Vertrag: LE-Helfer, CRC-8/32, Frame-Aufbau
│                        und -Prüfung, Nutzlast-Codec  (§2, §3, §4, §5, §11)
├── v4_master.h/.c       Transaktionskern (SEQ/Retry/BUSY) und alle Wrapper (§6–§11)
├── v4_linux_i2c.c       Plattformschicht Linux-Harness, /dev/i2c-N        (§1.5)
├── v4_amiga_i2c.c       Plattformschicht Zielsystem: i2c.library + timer.device
├── v4_mock.h/.c         ESP32-Emulator als zweite Plattformimplementierung (§14.1)
├── v4_console.c         interaktives Programm für Linux UND Amiga         (§13)
├── tests/               Testsuite (§0, §3.3, §11, §12, §14.1, §14.2.3, §14.3)
│                        und smoke_console.c (End-to-End-Lauf des Clients)
├── tools/check_portability.sh      Lint der Byte-Order-Regeln (§0/§2)
├── tools/check_m68k_byteaccess.sh  Nachweis im erzeugten m68k-Objektcode
└── Makefile
```

## Schnellstart

```sh
make            # Lint + native Testsuite (Little Endian)
make verify     # alles ohne Hardware: Lint, Tests, Sanitizer, Smoke, m68k-Nachweis, Amiga-Build
make test-san   # derselbe Testlauf unter ASan + UBSan
make test-m68k  # derselbe Testlauf unter qemu-m68k (Big Endian) — siehe unten
make asm        # Nachweis im erzeugten m68k-Objektcode
make smoke      # Konsolenprogramm einmal komplett gegen den Mock durchlaufen
make demo       # Konsolenprogramm für den Harness (braucht /dev/i2c-N)
make amiga      # AmigaOS-Executable für die V4
make probe      # Stufenprobe für die V4 (Start in 8 nummerierten Schritten)
```

Das Programm auf der V4:

```sh
Programs:test/v4_console          # Trace an, Logdatei an (Vorgabe)
Programs:test/v4_console -q       # still
Programs:test/v4_console -x       # zusaetzlich Hexdump der ersten 32 Antwortbytes
Programs:test/v4_console -n       # keine Logdatei, nur Konsole
Programs:test/v4_console -o ram:lauf.log   # anderer Logpfad

# nach einem Absturz: Logdatei holen und ansehen
make log
```

Voraussetzungen: `gcc`, `make` für den Harness; die Apollo-Toolchain unter
`../../../Compilers/GCC-6.50-Latest` (Pfad über `APOLLO_PREFIX` überschreibbar)
für das Zielsystem.

## Verifikationsstand

Ehrlich getrennt nach „geprüft" und „ungeprüft":

| Prüfung | Werkzeug | Ergebnis |
|---|---|---|
| Byte-Order-Selbsttest (§0) | `make test` | grün, auf dem Wirt Little Endian |
| CRC-Vektoren §3.3 (12 Werte) | `make test` | grün, unabhängig nachgerechnet |
| Golden Frames §12.1–§12.5 | `make test` | grün, byte-genau |
| Golden Nutzlasten §11 (6 Antworten) | `make test` | grün, von Hand aus den Tabellen abgeschrieben |
| Verhaltensmatrix §14.1 (10 Fälle) | `make test` | grün |
| Grenzfälle §14.3 (8 Fälle) | `make test` | grün |
| Sitzungsablauf §13 | `make test` | grün |
| Dauerlauf §14.2.3 (10 000× GET_STATUS mit Störungen) | `make test` | grün |
| Summe | | **156 Fälle, 383 559 Checks, 0 Fehler** |
| R3/§1.4: Wartezeiten pro Transaktion | `make test` | grün, Invariante: 1× `t_wait` je Schreibversuch, 1× `t_busy` je BUSY-/BAD_CRC-Runde |
| Zahlencode-Pinning (§5.1/§11) | `make test` | grün, alle Status-/Befehlscodes numerisch festgenagelt |
| Fuzzing der Parser (deterministisch, 160 000 Frames) | `make test` | grün, kein Zufallsframe akzeptiert |
| Derselbe Lauf unter AddressSanitizer + UndefinedBehaviorSanitizer | `make test-san` | grün, keine Befunde (inkl. Leck-Erkennung) |
| Statische Analyse (`gcc -fanalyzer`) über alle Quellen | manuell | keine Befunde |
| Smoketest des echten Konsolenprogramms gegen den Mock | `make smoke` | grün, 6 Durchläufe (inkl. PLAY_FILE/STOP_PLAY, Schalterfehler) |
| Ausgabe-Spiegelung Konsole → Logdatei | `make smoke` | grün, Logdatei per `cmp` **byte-gleich** zur Konsole (bis auf die Marke vor dem Öffnen) |
| Lint der Byte-Order-Regeln | `make lint` | grün, mit Selbsttest und Live-Positivkontrolle |
| m68k-Objektcode greift nur byteweise auf Puffer zu | `make asm` | grün, Gate nachweislich nicht vakuant |
| Amiga-Build (`-Wall -Wextra -Werror`, `m68080`) | `make amiga` | grün, erzeugt AmigaOS-Executable |
| **Testlauf auf echtem Big Endian (qemu-m68k)** | `make test-m68k` | **SKIP — Werkzeuge fehlen** |
| **Lauf auf echter V4-Hardware (§14.2)** | — | **nicht erfolgt** |

### Was NICHT geprüft ist — bitte vor dem Hardwareeinsatz lesen

1. **Kein Lauf auf einem Big-Endian-Ziel.** `gcc-m68k-linux-gnu` und
   `qemu-user-static` sind auf diesem Rechner nicht installiert, und `sudo` ist
   durch `no_new_privs` blockiert — eine Installation ist hier grundsätzlich
   nicht möglich. `make test-m68k` meldet das ausdrücklich als SKIP und
   **führt den Lauf nicht aus**; `make test-m68k REQUIRE_M68K=1` schlägt
   stattdessen fehl. Auf einem Rechner mit den Werkzeugen:

   ```sh
   sudo apt install gcc-m68k-linux-gnu qemu-user-static
   make test-m68k
   ```
   Das ist derselbe Testcode, der hier nativ läuft — er muss dort ebenfalls
   104/104 ergeben.

2. **Ersatzweise Endianness-Nachweis.** `make asm` prüft den **erzeugten
   m68k-Objektcode** (Apollo-GCC, `-m68080`) und stellt sicher:
   - die Helfer `v4p_put_u16le/u32le` und `v4p_get_u16le/u32le` erzeugen
     ausschließlich `move.b` auf den Puffer,
   - in den echten Frame-Funktionen (`v4p_build_*`, `v4p_check_*`) gibt es
     **keinen** 16/32-Bit-Store in einen Puffer,
   - `clr.l` zum Nullfüllen ist erlaubt (endian-neutral),
   - die Auswertung ist nicht vakuant: Instruction- und `move.b`-Zähler müssen
     über Schwellen liegen.

   Dass dieses Gate eine echte Verletzung erkennt, wurde nachgewiesen: ein
   eingebauter `*(uint16_t *)(void *)(f + 0) = …` in `v4p_build_write` erzeugt
   `move.w d1,(a2)` und lässt das Gate mit Exit 1 fehlschlagen. Zusammen mit
   dem Lint (der Casts, `memcpy` auf Frames, `packed` und Bitfelder verbietet)
   ist damit belegt, dass kein Frame-Byte anders als byteweise berührt wird.

3. **Die Amiga-Plattformschicht ist übersetzt, aber nicht gelaufen.**
   `v4_amiga_i2c.c` kompiliert sauber für m68k-amigaos; auf echter Hardware
   wurden `SendI2C`/`ReceiveI2C` und insbesondere die Verzögerung nicht geprüft.
   Dasselbe gilt für die **Logdatei**: die Haken sind auf dem Mock getestet
   (byte-gleiche Spiegelung) und der m68k-Code ist geprüft (`Open` −30,
   `Write` −48, `Close` −36, `Flush` −360 auf `_DOSBase`), aber ob auf der V4
   `Programs:test/` beschreibbar ist und ob `Flush()` dort wirklich jede Zeile
   durchschreibt, zeigt erst ein echter Lauf mit anschließendem `make log`.
   Geht die Datei nicht auf, meldet das Programm nur „nicht verfügbar" und
   läuft weiter — es stürzt dabei nicht ab.

4. **Die Verzögerung `t_wait` ist protokolltragend** (§1.3, Regel R3). Der Slave
   kann nicht clock-stretchen; ein zu früh gelesener Frame ist Müll. Auf der V4
   liefert `v4_plat_delay_us` die Zeit über `timer.device` mit `UNIT_MICROHZ`.
   Auf Hardware ist zu prüfen, ob die Granularität für 2000 µs reicht. Falls
   nicht: eine auf den 68080 kalibrierte Warteschleife einsetzen — aber die
   Wartezeit **nicht** verkürzen.

5. **`i2c.library` kennt keine Zeitüberwachung.** `SendI2C`/`ReceiveI2C`
   blockieren, bis die Transaktion durch ist. Hängt der Slave (z. B. nach einem
   Bus-Reset), blockiert der Master unbegrenzt. Die Spezifikation sagt dazu
   nichts. Als Absicherung wäre ein `SendIO` + `AbortIO` über `timer.device`
   nötig (offener Punkt, nicht umgesetzt).

6. **Bus-Takt und Pegel.** Die Spezifikation nennt 100 kHz als robust und
   400 kHz als möglich. `i2c.library` wird hier **nicht** über `SetI2CDelay`
   konfiguriert; der Vorgabewert der V4-Implementierung ist zu prüfen.
   Der Bus muss auf 3,3 V liegen.

7. **Dateigröße bei kleinem chunk.** `block` ist laut Spezifikation u16, bei
   `chunk = 128` sind das 8 MiB pro Datei. `v4_read_file` bricht an dieser
   Grenze mit `V4_ERR_ARG` ab, statt still umzulaufen.

## Protokollstand: Version 3.0 / 128-Byte-READ-Frame

`PROTOCOL_V4_MASTER.md` wurde zweimal nachgezogen: erst auf den 128-Byte-READ
(noch als „Version 2.0" mit `proto_ver` 2), dann auf **Version 3.0 mit
`proto_ver` 3**. Beide Schritte sind umgesetzt; maßgeblich ist jetzt
`proto_ver` 3. Was sich gegenüber der 64-Byte-Fassung geändert hat:

| Punkt | vorher (proto_ver 2) | jetzt (proto_ver 3) |
|---|---|---|
| `proto_ver` | 2 | **3** — kennzeichnet das 128-Byte-Format |
| READ-Frame (§4.2) | 64 Byte | **128 Byte** |
| READ-Nutzlast (§4.2) | 57 Byte | **121 Byte** |
| `flags` / CRC-8 im READ-Frame | +62 / +63 | **+126 / +127** |
| `GET_STATUS`-Antwort (§11) | 16 Byte | **17 Byte** (`sd_card_present` neu bei +16) |
| `audio` (Byte +5) | Ja/Nein | **Bitfeld `audio_flags`**: Bit 0 `A2DP_STREAMING`, Bit 1 `SD_PLAYBACK` |
| Namensgrenzen (§10) | 48 / 49 Zeichen | **113 Zeichen**, Puffer ≥ **114 Byte** |
| `PLAY_FILE` (0x60) | nicht implementiert (`BAD_CMD`) | **implementiert** (SD-Datei über Bluetooth abspielen), eigener §8.3-Ablauf |
| `STOP_PLAY` (0x61) | — | **neuer Befehl** (zurück auf I2S-Eingang) |
| Umbau-Budget (§8.3) | — | **~1 s ≈ 500 Runden à 2 ms** — `V4_PLAY_BUSY_TRIES` entspricht dem exakt |
| §14.0 | — | neu: eingebauter Firmware-Selbsttest (ESP32-Seite) |

Die Umsetzung: Konstanten `V4P_READ_FRAME_LEN`/`V4P_READ_PAYLOAD_MAX` und die
benannten Offsets `V4P_READ_OFF_FLAGS`/`V4P_READ_OFF_CRC`, `V4P_ST_OFF_*` und
`V4P_ST_LEN`, neue Wrapper `v4_play_file()`/`v4_stop_play()`, `audio_flags` und
`sd_card_present` in `v4p_status_t`. 121 = 8 + 113: die neue Nutzlast ist genau
auf die 113-Zeichen-Namen zugeschnitten.

### Die Umstellung ist jetzt am Protokoll erkennbar

Zwischenzeitlich war der Formatwechsel inkompatibel, ohne dass sich `proto_ver`
änderte — eine alte 64-Byte-Firmware war dann nur daran zu erkennen, dass jeder
128-Byte-Read scheiterte (`PING` → `V4_ERR_LINK`). Mit **Version 3.0 ist das
behoben**: §11 erklärt `proto_ver` 3 ausdrücklich zum Kennzeichen des
128-Byte-Formats und nennt `read_frame_len` als zweiten Prüfpunkt.

Umgesetzt ist beides:

- `v4_check_info()` wertet zuerst `proto_ver` aus und prüft zusätzlich
  `write_frame_len`/`read_frame_len`/`chunk`; bei Abweichung `V4_ERR_PROTO`.
  Die Konsole ruft das direkt nach dem `PING` auf und meldet alle gemeldeten
  Rahmengrößen, damit ein alter Stand sofort zuzuordnen ist.
- Der `PING`-Fehlerpfad der Konsole weist zusätzlich auf den 64-Byte-Fall hin,
  falls ein Slave gar nicht antwortet.

**Folgekosten des größeren Rahmens** (in §1.4/§1.5 des Dokuments mittlerweile
selbst dokumentiert): jede Steuerantwort kostet bei 100 kHz rund 12 ms statt
6 ms, und die frühere Abhilfe für USB-I2C-Bridges (`chunk = 16`) hilft nicht
mehr — die 32-Byte-Grenze trifft nun schon die Steuerframes. Mit so einem
Adapter läuft der Harness nicht; nötig ist ein Adapter ohne diese Grenze oder
gleich die echte V4.

## Adversariale Fremdprüfung

Der Code und die Testsuite wurden von zwei unabhängigen Prüfern gegen die
Spezifikation reviewt (read-only, ohne Kenntnis meiner Intention). Beide
Berichte sind vollständig abgearbeitet; der Stand danach ist oben dokumentiert.

**Bestätigt und behoben:**

| Fund | Behebung |
|---|---|
| `v4_read_file`: Puffergröße aus Momentaufnahme, Leselänge aber live → potenzieller Überlauf, wenn der Callback `chunk` ändert | Der Lesevorgang bricht bei geändertem `chunk` mit `V4_ERR_ARG` ab (`v4_read_file`), statt in einen zu kleinen Puffer zu lesen. Regressionstest mit 140-Byte-Puffer und Callback, der auf 1024 stellt |
| Nicht-idempotente Befehle (`PATH_APPEND`, `*_OPEN`) wurden nach verlorener Antwort blind wiederholt → doppeltes Anhängen bzw. Handle-Leck | `unsafe_retries` zählt Wiederholungen nach erfolgreichem Schreiben. `PATH_APPEND` baut den Slave-Puffer danach deterministisch aus dem Spiegel neu auf (`v4_path_rebuild`); schlägt auch das fehl, wird der Spiegel als ungültig markiert (`path_valid = 0`) und `L = 0` gesperrt. Bei `*_OPEN` zählt `possible_handle_leaks` das Risiko (mehr ist ohne ESP32-Mitarbeit nicht möglich, siehe Empfehlung unten) |
| `""` wurde als `L = 0` gesendet und öffnete damit den `PATH_*`-Puffer statt der Wurzel (§10 gegen §11) | `""` wird auf `"/"` abgebildet; Test mit belegt­em `PATH_*`-Puffer unterscheidet Wurzel von MUSIC |
| `v4_set_chunk` meldete bei ungültigem Wert `V4_ERR_ARG` statt `BAD_ARG` (§11) | liefert jetzt `V4P_ST_BAD_ARG` |
| `v4p_dec_dev`/`v4p_dec_dirent` ließen `out->flags` ungeschrieben | beide initialisieren `flags = 0` (die Flags kommen aus dem READ-Frame, nicht aus der Nutzlast) |
| Fuzzing-Suite war nicht verdrahtet (toter Code) | in `TEST_SRC`, `v4_test.h` und `test_main.c` eingebunden — 144 statt 126 Fälle |
| Vakuante Checks (`CHECK(1)` nach einer Schleife, die nur im Fehlerfall prüft; `CHECK_STR("A.MP3","A.MP3")`) | durch Fehlerzähler bzw. echte Zusicherungen ersetzt |
| §1.4-Zahlen und R3-Wartezeiten waren nicht gepinnt — ein Master ohne jede Pause hätte bestanden | Konstanten numerisch gepinnt plus Delay-Invariante (siehe Verifikationstabelle) |
| Lint-Gate ließ typedef-Casts (`u32`, `frame_t`), `union`-Zugriffe und Byte-Swapping durch; Allow-Logik blendete ganze Zeilen aus; fehlende Dateien galten als „sauber" | Muster generisch (jeder Cast auf einen Nicht-Byte-Zeiger), `union`/`swap` verboten, Neutralisieren statt Zeilenausblenden, fehlende/leere Dateien sind Fehler, Positivkontrollen inkl. Live-Test auf der echten Datei |
| m68k-Gate erkannte `move.w d0,8(a1)` nicht (fehlendes `.*`) und prüfte kein `movem` | Muster korrigiert, `movem` ergänzt, Positivkontrollen: zwei absichtlich falsche Probefunktionen **müssen** erkannt werden |
| Mock: Namensüberlauf im Datei-Pfad (`memcpy` ohne Klemme), Mount-Wartezeit unsichtbar, FIFO-Selbstheilung nicht modellierbar | Klemme ergänzt; der Mount wird erst nach `V4_MOUNT_WAIT_US` Wartezeit fertig (ein Master ohne Warten fällt jetzt durch); neuer Knopf `partial_read_once` bildet die halb konsumierte Antwort ab |
| `M.rx_frames` (R1) wurde nie geprüft; `busy_rounds >= 1` war durch Block-Runden trivial erfüllt; PATH_CLEAR/RESET prüften nur Rückgabecodes | R1-Invariante `rx == tx`, exakte BUSY-Deltas (513 für 65536 Byte), funktionale Prüfungen: nach `PATH_CLEAR` liefert `L = 0` die Wurzel, nach `RESET` ist der Blockcache nachweislich verworfen |

**Bestätigt, aber bewusst nicht geändert:**

- **`BAD_CRC`-Budget:** §5.1 fordert „gleiche SEQ", §6 erhöht SEQ nach jeder
  gültigen Antwort. Nach erschöpftem Budget (4) bleibt SEQ stehen. Begründung:
  der Befehl wurde nie angenommen; SEQ ist reines Echo ohne Dedup auf dem
  Slave. Beide Lesarten sind ununterscheidbar — im Code kommentiert.
- **`FILE_CLOSE` mit unbekanntem Handle:** die Spezifikation fordert „immer OK"
  nur für `DIR_CLOSE` (§11). Der Test akzeptiert daher `OK` **oder**
  `NO_HANDLE`, statt eine Semantik festzuschreiben.
- **EOF-BUSY:** §8.2 sagt eine BUSY-Runde, §14.3 sagt zwei. Der Test nagelt
  §8.2 fest; der Master wiederholt BUSY ohnehin bis 40×, ein Client sieht also
  nie BUSY. Mit der ESP32-Referenz abzugleichen.

**Empfehlung an die ESP32-Seite (Protokoll, nicht Master):** `PATH_APPEND` und
`DIR_OPEN`/`FILE_OPEN` sind die einzigen Befehle, die §7 („alle Befehle sind
gefahrlos wiederholbar") nicht erfüllen: geht nur die *Antwort* verloren,
hängt der Slave das Fragment erneut an bzw. belegt ein zweites Handle. §7.2
löst genau dieses Problem für `DIR_NEXT` mustergültig — der Befehl trägt den
erwarteten Zustand (Eintragszähler). Dasselbe Muster wäre hier angebracht:
`PATH_APPEND` mit erwarteter aktueller Länge, `DIR_OPEN`/`FILE_OPEN` mit einem
Handle-Wunsch oder einer Nonce, die ein zweites Anlegen erkennt und ablehnt.

## Abweichungen und Klärungen gegenüber der Spezifikation

Beim Umsetzen sind fünf Stellen aufgefallen, an denen die Spezifikation
mehrdeutig oder in sich widersprüchlich ist. Jede Entscheidung ist im Code an
der Fundstelle kommentiert.

### 1. §8.2 gegen §14.3: Anzahl der BUSY-Runden hinter dem Dateiende

§8.2 zeigt `FILE_READ(h, n+1) → BUSY(len=0)`, dann `→ END` („nach 2 Runden").
§14.3 schreibt dagegen „`FILE_READ` auf einen Block hinter dem Dateiende ⇒
**zweimal** BUSY, dann `END`".

Die Umsetzung folgt §8.2 (eine BUSY-Runde, dann `END`) und ist gegen beide
Lesarten robust: `v4_transact_bulk` wiederholt BUSY intern bis zu 40×, ein
Client sieht also nie BUSY. Test `§14.3 FILE_READ hinter dem Dateiende` prüft
die §8.2-Folge, der Test `Busy-Kette vor END` zusätzlich eine dreifache
BUSY-Kette. **Mit der ESP32-Referenz abzugleichen.**

### 2. §5.1 `BAD_CRC`: Anzahl der Wiederholungen nicht festgelegt

§5.1 verlangt für `BAD_CRC` „Befehl neu senden, gleiche SEQ", nennt aber keine
Obergrenze. §1.4 begrenzt nur die vom *Master erkannten* Framing-Fehler auf 4.
Eingeführt: `V4_BADCRC_TRIES = 4` (`v4_master.h`), gleiche Größenordnung wie die
Framing-Wiederholungen. Nach Erschöpfung wird der Status `0x02` an den Aufrufer
zurückgegeben. **Zu bestätigen.**

### 3. §8.1 gegen §6/§14.1: BUSY bei `SD_MOUNT`

§6 und §5.1 sagen, BUSY wird intern wiederholt (§14.1 testet ausdrücklich
„BUSY-Kette … intern wiederholt, Erfolg"). §8.1 beschreibt dagegen einen
Aufrufer, der `SD_MOUNT(force=1) → BUSY` sieht, ~200 ms wartet und dann
`SD_MOUNT(force=0)` sendet — dafür muss BUSY nach außen sichtbar sein.

Beides ist vorhanden:

| Funktion | Verhalten |
|---|---|
| `v4_sd_mount(m, force)` | BUSY wird intern wiederholt (§6) |
| `v4_sd_mount_probe(m, force)` | BUSY wird durchgereicht (§8.1) |
| `v4_sd_mount_wait(m, tries)` | fährt die §8.1-Runden selbst |

`v4_transact_ex(..., flags)` mit `V4_X_BUSY_RETRY` steuert das allgemein.

### 4. §4.2 Punkt 5: zu kurze Nutzlast bei gültigem Frame

Die Spezifikation verlangt generisch `len <= 57`, sagt aber nicht, was passiert,
wenn ein Frame zwar CRC-gültig ist, die Nutzlast für den konkreten Befehl aber
zu kurz ist (§11 legt pro Befehl implizit Mindestlängen fest). Der Master gibt
dafür `V4_ERR_FRAME` (0xE3) zurück und **wiederholt nicht** — der Frame war
gültig, eine Wiederholung lieferte dasselbe.

### 5. Pfadregeln §10: leerer String und Übergröße
- Ein Pfad über 27 Byte ohne `PATH_*` kann physisch nicht in das `len`-Feld
  passen. Der Master weist ihn lokal mit `TOO_LONG` ab, ohne Busverkehr. Test:
  „§14.3 Pfad > 27 Byte ohne PATH_* → TOO_LONG, kein Busverkehr".
- `v4_dir_open(m, "", …)` wird auf `"/"` abgebildet und öffnet damit die
  **Wurzel** — so verlangt es §10 („`""` und `"/"` bedeuten die Wurzel"),
  während §11 für `L = 0` den `PATH_*`-Puffer vorsieht. Nur `path == NULL`
  bedeutet `L = 0`. Ohne diese Trennung würde ein belegter `PATH_*`-Puffer
  stillschweigend den falschen Ordner öffnen.
- Der lokale Pfadspiegel des Masters und der Slave-Puffer können nicht
  auseinanderlaufen: beide starten leer, wachsen nur um erfolgreiche
  `PATH_APPEND` und sind gleich groß (128 Byte). Ein lokaler Überlauf impliziert
  daher den Überlauf beim Slave.

### 6. §5.2 `NAME_TRUNCATED`: wie erfährt der Aufrufer davon?

§5.2 definiert das Flag im READ-Frame (+62), §10/§11 verlangen es bei gekürzten
Namen — die Spezifikation sagt aber nicht, wie es die API nach außen gibt. Ohne
einen Weg dorthin würde ein Aufrufer mit einem **gekürzten Dateinamen
weiterarbeiten**, ohne es zu merken; bei `FILE_OPEN` wäre das ein Zugriff auf
die falsche Datei.

Deshalb tragen `v4p_dev_t` und `v4p_dirent_t` ein Feld `flags`, das
`v4_dev_get` und `v4_dir_next` aus dem READ-Frame befüllen (nicht aus der
Nutzlast — dort steht es nicht). Getestet für beide Richtungen: Name über 49
Zeichen (`DEV_GET`) und über 48 Zeichen (`DIR_NEXT`), inklusive Gegenprobe,
dass kurze Namen das Flag **nicht** setzen.

### 7. §7 (Idempotenz) gegen §6 (Wiederholung) — die schärfste Lücke

§7 erklärt **alle** Befehle für gefahrlos wiederholbar; §6 verlangt, nach einem
verlorenen oder unbrauchbaren Frame blind denselben Befehl erneut zu senden. Für
reine Lese- und Zustandsbefehle stimmt das. Für drei Befehle stimmt es nicht:

| Befehl | Zweite Ausführung bedeutet |
|---|---|
| `PATH_APPEND` | Fragment hängt **zweimal** an → Slave-Pfad ≠ Spiegel |
| `DIR_OPEN` | zweites Verzeichnis-Handle belegt (nur 2 vorhanden) |
| `FILE_OPEN` | zweites Datei-Handle belegt (nur 4 vorhanden) |

Der Master kann nicht unterscheiden, ob der Slave den Befehl nie gesehen hat
oder nur die Antwort verloren ging. Der Code macht daraus das Beste:

- `unsafe_retries` zählt jede Wiederholung nach **erfolgreichem** Schreiben.
- `PATH_APPEND` repariert sich danach selbst: `PATH_CLEAR` plus erneutes
  Anhängen des Spiegels ergibt deterministisch den gewollten Pfad. Schlägt auch
  das fehl, wird der Spiegel ungültig (`path_valid = 0`) und `L = 0` gesperrt —
  lieber ein sauberer Fehler als ein falscher Pfad.
- Bei `*_OPEN` zählt `possible_handle_leaks`; der Aufrufer kann reagieren. Mehr
  ist ohne Änderung auf der ESP32-Seite nicht möglich (siehe Empfehlung oben).

### 8. §5.2 nannte den falschen Flags-Offset — in Version 3.0 korrigiert

*(Der folgende Punkt ist mit Version 3.0 erledigt; er steht hier, weil er den
Grund für die Entscheidung dokumentiert.)*

Die Überschrift von §5.2 lautet „Flags (READ-Frame +62)" — das ist ein Rest der
alten 64-Byte-Fassung. Nach §4.2 liegen `flags` bei **+126** und CRC-8 bei
**+127**; der Golden Frame §12.4 bestätigt das (Byte 126 = `00`, Byte 127 =
`FD`). Umgesetzt ist +126 (`V4P_READ_OFF_FLAGS`).

### 9. §11-Tabelle für `GET_STATUS` — in Version 3.0 korrigiert

*(Ebenfalls erledigt: die Tabelle nennt jetzt `audio_flags` bei +5,
`scan_gen` ab +6 und `sd_card_present` bei +16, also genau die umgesetzten
Offsets. Der Punkt bleibt als Begründung stehen.)*

Die Tabelle in §11 zeigt für `GET_STATUS` ein „—" bei +6 und schiebt danach
alle Felder um ein Byte (`scan_gen` ab +7, `sd_free_kb` ab +9, …). Der Hinweis
unmittelbar darunter sagt dagegen ausdrücklich, **maßgeblich** seien die
`V4P_ST_OFF_*`-Offsets, und nennt `scan_gen` ab +6, `sd_free_kb` ab +8 — also
die alten Offsets plus `sd_card_present` als neues Byte bei +16.

Der Golden Frame §12.4 entscheidet die Frage: bei `len = 17` steht
`scan_gen = 0x000A` in den Bytes 11/12 (`+6/+7` der Nutzlast) und
`sd_card_present = 1` im letzten Nutzlastbyte. **Die Tabelle ist falsch, der
Hinweis richtig.** Die Umsetzung folgt dem Hinweis und nagelt alle Offsets als
`V4P_ST_OFF_*` fest; ein Test prüft sie einzeln.

### 10. §9 widerspricht §1.5 beim USB-Adapter (redaktionell, offen)

Der letzte Satz von §9 empfiehlt weiterhin „USB-I2C-Bridges limitieren oft auf
32 Byte ⇒ `chunk = 16`". §1.5 sagt dagegen (seit Version 3.0) zutreffend, dass
der Harness auf so einem Adapter **gar nicht** läuft, weil schon die
Steuerframes 128 Byte lesen. Gemeint ist in §9 offenbar nur noch die
Puffer-/Durchsatzbetrachtung für Bulk-Frames. Sachlich gilt §1.5; der Satz in §9
sollte entsprechend entschärft werden.

## Debugausgabe: den Schritt sehen, in dem es knallt

Weil auf der V4 kein Debugger zur Verfügung steht, protokolliert das Programm
jeden Transaktionsschritt. **Die Ausgabe ist standardmäßig an** und jede Zeile
wird sofort geflusht — bei einem Absturz bleibt nichts im Puffer, und die
**letzte Zeile benennt den Schritt, in dem es passiert ist.**

```sh
Programs:test/v4_console        # Trace an, Logdatei an (Vorgabe)
Programs:test/v4_console -x     # zusätzlich die ersten 32 Antwortbytes als Hexdump
Programs:test/v4_console -q     # still, nur die normalen Meldungen
```

So sieht eine gesunde Transaktion aus:

```
[trace] TX-Beginn    cmd=0x01 seq=  0 schreibe 32 Byte
[trace] TX-Ende      cmd=0x01 seq=  0 rc=0
[trace] Warten       cmd=0x01 seq=  0 2000 us warten
[trace] RX-Beginn    cmd=0x01 seq=  0 lese 128 Byte ...
[trace] RX-Ende      cmd=0x01 seq=  0 rc=0
[trace] Pruefung     cmd=0x01 seq=  0 ok (status=0x00 len=12)
[trace] Ergebnis     cmd=0x01 seq=  0 OK (status=0x00 len=12)
```

Die Ereignisse und was sie bedeuten:

| Letzte Zeile | Bedeutung |
|---|---|
| `TX-Beginn` | Absturz **in** `SendI2C` (die Library oder der Bus) |
| `TX-Ende rc=-1` | Der Slave hat nicht geantwortet; der Klartext kommt darunter |
| `Warten` | Absturz in `DoIO` (`timer.device`) |
| `RX-Beginn` | Absturz **in** `ReceiveI2C` |
| `RX-Ende rc=-1` | Lesefehler — Bus, Pegel, Adresse |
| `Pruefung` mit `CRC-Fehler`/`falsche Magic`/`len-Feld unzulässig` | Es kommen Bytes an, aber kein gültiger Frame (typisch: falsche Rahmengröße der Firmware, verschobener Strom) |
| `Pruefung` mit `cmd/seq nicht zurückgespiegelt` | Der Slave antwortet auf einen anderen Befehl — Synchronisation verloren |
| `WIEDERHOLUNG` | Vier davon hintereinander ergeben `V4_ERR_LINK` |
| `BUSY` | Der Slave arbeitet; wird intern wiederholt (40 bzw. 500 Runden) |
| `Ergebnis` | Der Befehl ist durch — der Status steht dahinter |

Dazu nennt jede Fehlerzeile den **rohen Busfehlercode** und seinen Klartext
(`v4_plat_last_error()` / `v4_plat_error_text()`): auf der Amiga-Seite
`I2CErrText` der Library, z. B. `Busfehler 0x00000200: no reply` = falsche
Adresse oder kein Slave am Bus.

Der Trace wird vom Master über einen Haken gerufen (`v4_master_t.trace`, siehe
`v4_master.h`); ohne Haken (`NULL`) kostet er nichts. Ein Test prüft, dass die
Ereignisse in der richtigen Reihenfolge und vollständig kommen — sonst würde das
Instrument beim Hardwarefehler Falsches zeigen.

### Jede Zeile zusätzlich in eine Logdatei — die überlebt den Absturz

Der Bildschirm ist nach einem Absturz weg. Deshalb schreibt das Programm seine
**komplette Ausgabe zusätzlich in eine Datei**; die liegt auf der Platte der V4
und ist danach mit `acp` abholbar. Die Logdatei ist **standardmäßig an**:

```sh
Programs:test/v4_console            # Logdatei: Programs:test/v4_console.log
Programs:test/v4_console -q         # nur die Meldungen, ohne Trace
Programs:test/v4_console -o ram:anderes.log   # anderer Pfad
Programs:test/v4_console -n         # keine Logdatei, nur Konsole
```

Nach einem Absturz — in einer Shell hier im Repository:

```sh
make log            # holt Programs/test/v4_console.log und zeigt sie an
```

Das Ziel macht genau das:

```sh
acp "192.168.178.50:Programs/test/v4_console.log" build/ && cat build/v4_console.log
```

Die letzten Zeilen sind die Diagnose: die letzte Zeile benennt den Schritt, in
dem es geknallt hat (dieselbe Bedeutung wie beim Trace, siehe Tabelle oben).

**Zwei Dinge, die hier zählen:**

1. **`Flush()` nach jeder Zeile.** AmigaDOS puffert Schreibvorgänge im
   FileHandle. Ohne `Flush()` stünde nach einem Absturz genau der Teil nicht in
   der Datei, auf den es ankommt. Die Amiga-Schicht ruft es deshalb nach jedem
   `Write()` (`v4_amiga_i2c.c`).
2. **`MODE_NEWFILE`.** Jeder Lauf beginnt mit einer frischen Datei, damit im Log
   nie Zeilen zweier Läufe vermischt sind.

Die Ausgabe ist ein Plattformhaken (`v4_master.h`), genau wie
`v4_plat_delay_us` — die Konsole kennt nur diese drei Funktionen:

| Haken | Amiga (`v4_amiga_i2c.c`) | Linux-Harness / Mock |
|---|---|---|
| `v4_plat_log_open(pfad)` | `Open(pfad, MODE_NEWFILE)`; `pfad == NULL` → `V4_LOG_DEFAULT_AMIGA` | `fopen(pfad, "w")`; ohne Pfad → `-1` (kein Standardpfad) |
| `v4_plat_log_write(s, len)` | `Write()` + `Flush()`; ohne offene Datei `0` | `fwrite()` + `fflush()`; ohne offene Datei `0` |
| `v4_plat_log_close()` | `Close()`; mehrfach aufrufbar | `fclose()`; mehrfach aufrufbar |

Geht die Datei nicht auf (Drawer fehlt, Volume gesperrt), ist das **kein Fehler**:
`v4_plat_log_open()` meldet nur
`Logdatei nicht verfuegbar (Standardpfad) -- nur Konsole.` und das Programm läuft
unverändert weiter. Das Schreiben ohne offene Datei ist ein No-op; ein Test prüft
diesen Vertrag (`tests/test_master.c`, Fall „log hooks"), und `make smoke`
vergleicht Konsole und Logdatei per `cmp` auf **Byte-Gleichheit** — bis auf die
eine Marke, die vor dem Öffnen auf die Konsole geht.

**Serielle Ausgabe gibt es weiterhin, aber nicht mehr als Vorgabe:** der Pfad ist
frei wählbar, `-o ser:` schreibt also nach wie vor auf `serial.device` Unit 0
(AmigaOS-Vorgabe 9600 8N1). Nur der *Standardpfad* ist jetzt die Datei — COM6
braucht damit keinen Adapter mehr und kann nichts blockieren.

## Stufenprobe auf der V4: `v4_probe`

Wenn auf der V4 „nichts kommt" oder das Programm sofort verschwindet, zerlegt
`v4_probe` den Start in acht nummerierte Schritte. Jeder Schritt schreibt genau
eine Zeile und flusht sofort; **bleibt die Ausgabe nach Schritt N stehen, sitzt
der Absturz in Schritt N+1** — ohne Debugger.

```sh
ram:v4_probe          # Schritte 1,2,4..8 -- oeffnet ser: NICHT an
ram:v4_probe -s       # zusaetzlich Schritt 3: ser: oeffnen, schreiben, schliessen
```

| Schritt | Was geprüft wird | Typischer Befund |
|---|---|---|
| 1/8 | Programm startet, Stackgröße | kommt nichts: Startproblem, nicht der Bus |
| 2/8 | `DOSBase`, `Output()` — trägt jedes `printf` | `DOSBase=NULL` → ohne dos.library geht keine Ausgabe |
| 3/8 | `ser:` erst mit `MODE_OLDFILE`, dann mit `MODE_NEWFILE`, inkl. `Write` und `IoErr()` | belegt/falsches Gerät = DOS-Fehler statt Absturz |
| 4/8 | `i2c.library` V39+ | fehlt sie, bricht der Master sonst schon in `v4_open()` ab |
| 5/8 | `timer.device` `UNIT_MICROHZ` — Träger von `t_wait` | |
| 6/8 | Byte-Order-Selbsttest | muss 1 sein |
| 7/8 | `v4_open()` + `PING` mit knappem Trace | Busfehler 0x…0200 = kein Slave/falsche Adresse |
| 8/8 | `v4_close()` und Programmende | |

Schritt 3 ist **absichtlich nicht** im Standardlauf und das Hauptprogramm benutzt
`ser:` überhaupt nicht mehr (Vorgabe ist die Logdatei). Er bleibt in der Probe,
weil ein zweites Öffnen von `ser:` einer Shell, die selbst auf dem seriellen
Anschluss läuft, die Konsole wegziehen kann — und weil man so prüfen kann, ob der
serielle Weg grundsätzlich trägt, wenn man ihn doch einmal braucht.

## Feldbefund: die Konvention von `i2c.library` ist umgekehrt zu „0 = OK"

Beim ersten Lauf auf der V4 meldete das Programm `PING: LINK-Fehler`. Ursache war
**kein** Protokollfehler, sondern die Auswertung des Library-Rückgabewerts in
`v4_amiga_i2c.c` — und das gleich in beiden Richtungen falsch herum:

```c
/* falsch: */
return ((err & 0xFFu) != 0u) ? -1 : 0;
```

`i2c.library` (V39+) meldet **Erfolg mit gesetztem Low-Byte**. Belegt in
`i2clib-master/src/i2c.generic.s`:

```asm
ReportAndFinish:
    moveq #0,d2
    move.b AllocError(a5),d2
    swap d2                         ; AA
    asl.w #8,d3                     ; BB
    seq d2                          ; CC = 0xFF, wenn KEIN Fehler
    or.w d3,d2
```

und im Kommentarkopf derselben Datei sowie in `I2CErrText`:

```
; $00AA0800, AA=1..6: allocation errors
; $0000BB00, BB=1..8: I/O errors
; $000000CC, CC<>0    OK
; $00000000:          error, somehow
```

Dieselbe Lesart benutzen die mitgelieferten Werkzeuge `src/SendI2C.c` und
`src/ReceiveI2C.c` (`if ((err & 0xff)==0) { /* Fehler */ }`). Mit der falschen
Prüfung galt **jede erfolgreiche Transaktion als Fehler**: der Master
wiederholte viermal und endete in `V4_ERR_LINK`.

Behoben und gegen Rückfall gesichert:

- Die Auswertung liegt jetzt in `v4_i2c_err_is_ok()` in `v4_master.h` — an einer
  Stelle, mit der Tabelle im Kommentar, und **unit-getestet** (Erfolg
  `0x000000FF`, `I2C_REJECT 0x00000100`, `I2C_NO_REPLY 0x00000200`,
  `I2C_HARDW_BUSY 0x00000800`, Allokationsfehler `0x00010800`, sowie
  `0x00000000` → alles Fehler).
- Neu: `v4_plat_last_error()` liefert den **rohen** Busfehlercode der letzten
  Operation (Amiga: Library-Code, Linux: `errno`, Mock: 0). Die Konsole gibt ihn
  bei `V4_ERR_LINK` aus — auf der V4 gibt es keinen Debugger, damit ist der
  Library-Code die wichtigste Diagnose. Beispiel: `letzter Busfehler:
  0x00000200` heißt `I2C_NO_REPLY`, also falsche Adresse oder kein Slave.

Geprüft und **unverdächtig** bei dieser Gelegenheit: der Stackbedarf (mit
`-fstack-usage` gemessen; tiefste Kette ≈ 1 KB, die CLI bekommt 4 KB) und die
Sprungtor­tabelle der Library (`AllocI2C` −30, `FreeI2C` −36, `SetI2CDelay` −42,
`InitI2C` −48, `SendI2C` −54, `ReceiveI2C` −60 … `BringBackI2C` −84) — sie
stimmt exakt mit `fd/i2c_lib.fd` und den LVO-Werten der Inline-Makros überein.
Ein `AllocI2C`/`InitI2C` vor dem ersten `SendI2C` ist nicht nötig: laut
`i2c.doc` geschieht die Allokation implizit beim Öffnen der Library.

## Vertrag mit der ESP32-Seite

§15 verweist auf `../v4_master/v4_master.h/.c` und `../main/v4_proto.h/.c` als
fertige Referenz. **Diese Dateien existieren in diesem Repository nicht** — ich
habe den ganzen Baum danach durchsucht (`v4_proto*`, `v4_master*`, `v4_link*`,
`i2c_slave_v2*`: keine Treffer). Die hier vorliegende `v4_proto.h/.c` ist damit
eine **Neuumsetzung aus dem Spezifikationstext**, kein Kopie der ESP32-Vorlage.

§15 verlangt ausdrücklich „`../main/v4_proto.h/.c` — **kopieren, nicht forken**".
Vor dem ersten Hardwareeinsatz sollte diese Datei daher gegen die ESP32-Vorlage
**diffed** werden. Bei Abweichungen gilt die Vorlage, nicht diese Fassung.

## Einbindung in die Apollo-Umgebung

Dieses Verzeichnis ist eigenständig und lässt sich bauen, ohne das bestehende
Projekt `Projects/I2C-test/` anzufassen: `i2cBTdevice.c` und das dortige
Makefile bleiben unverändert. Für einen Amiga-Build über das Projekt-Makefile:

1. `-I v4_master` in `C_INCL_ALL` ergänzen.
2. `v4_proto.c`, `v4_master.c`, `v4_amiga_i2c.c` und das gewünschte Programm in
   `C_SOURCEDIR` legen (das Makefile sammelt per `wildcard *.c`) — **nicht**
   `v4_mock.c`, `v4_linux_i2c.c` oder Dateien aus `tests/`.
3. `-DV4_PLAT_AMIGA=1` in `C_DEFINES` setzen.

Faustregel: pro Ziel genau **eine** Plattformschicht linken (`v4_amiga_i2c.c`
oder `v4_linux_i2c.c` oder `v4_mock.c`).

### Auf die V4 übertragen

Der Upload läuft über denselben Weg wie in allen anderen Projekten dieses
Repos: `apolloExplorer/acp` schickt die Datei **über das Netz** an den
ApolloExplorer-Server auf der V4 (`acp <datei> "<host>:ram/"`) — kein
Kabel, kein USB. Voraussetzung ist, dass der AE-Server auf der V4 läuft.

```sh
export AMIGAHOST=192.168.178.50   # IP der V4 (steht auch in tasks.json)
make upload                       # Programm + passende .info-Ikone
make upload-debug                 # zusaetzlich bgdbserver und Debug-Skript
make acp-hosts                    # nur lesend: Hosts im Netz suchen
make acp-ls                       # nur lesend: RAM-Disk der V4 auflisten
```

Zu einem Amiga-Programm gehört eine `.info`-Datei **gleichen Namens** — das ist
die Ikone. `make upload` legt das Binary deshalb als `v4_console` plus
`v4_console.info` ab (Name über `AMIGA_NAME` änderbar), analog zum Hausstil in
`Projects/_makefiles/Makefile`. `upload-debug` bildet zusätzlich das dortige
Debug-Muster nach: `bgdbserver` mit hochladen und ein Startskript
`v4_console_Debug` erzeugen, das `bgdbserver v4_console` aufruft — dazu passt
`miDebuggerServerAddress: ${env:AMIGAHOST}:2345` in `.vscode/launch.json`.

Ist `AMIGAHOST` nicht gesetzt oder `acp` nicht vorhanden, brechen die
Upload-Ziele mit einer klaren Meldung ab, statt eine falsche Adresse
anzusprechen.

**Stand: hochgeladen und byte-genau verifiziert.** Die V4 war zwischenzeitlich
nicht erreichbar; nach einem Neustart war das RAM-Disk leer (RAM-Inhalt ist
flüchtig), der Upload wurde also wiederholt und diesmal gegengeprüft: die Datei
wurde mit `acp` **zurückgelesen** und mit `cmp`/`md5sum` gegen die lokale Fassung
gestellt — identisch (`abd58775…`, 33 152 Byte), und die Trace-Strings sind in
der Kopie auf der V4 nachweisbar. Diese Rücklese-Prüfung ist die verlässlichste
Bestätigung und steht als Rezept unten.

**Ziel ist `Programs:test/`** — dort liegt auch die Logdatei, und dieser Ordner
überlebt einen Reset der V4 (anders als `ram:`, das nach jedem Neustart leer
ist).

**Stand des Uploads: hochgeladen und verifiziert.** `make upload` überträgt
`v4_console`, `v4_console.info` und `v4_probe` nach
`192.168.178.50:Programs/test/`; `make acp-ls` zeigt sie dort:

```
Programs:test/v4_console
Programs:test/v4_console.info
Programs:test/v4_probe
```

**Starten auf der V4:** aus einer Shell, nicht per Doppelklick — `make upload`
legt zusätzlich `v4_probe` mit ab, die Stufenprobe aus dem Abschnitt oben —

```
Programs:test/v4_console
```

Das Ziel ist über `V4_DIR` einstellbar (Vorgabe `Programs/test/`). **Schreibweise
mit Schrägstrich, nicht mit Doppelpunkt:** `acp` zerlegt sein Argument am ersten
Doppelpunkt in Host und Pfad — ein zweiter Doppelpunkt schneidet den Pfad ab, und
`host:Programs:test/` landet im Wurzelverzeichnis von `Programs:`. AmigaOS selbst
versteht beide Formen; im Programm steht deshalb die native Form
`Programs:test/v4_console.log`. `ram/` geht weiterhin.

Die hochgeladene `.info` ist die generische `ApolloIcon.info` aus
`Projects/_icons`; sie enthält **keine Tooltypes** (kein `CLI`, kein
`WINDOW`). Ein Doppelklick würde daher keinen Konsolenkanal öffnen. Wer den
Doppelklick will, muss der Ikone eine `CLI`-Tooltype geben (binäre
`.info`-Bearbeitung, absichtlich nicht automatisch gemacht).

**Upload verifizieren (Rücklese-Rezept):** `acp` kann auch *herunterladen* — die
Gegenrichtung als Quelle angeben und in ein **existierendes Verzeichnis mit
Schrägstrich** legen, dann vergleichen:

```sh
mkdir -p /tmp/back
acp "192.168.178.50:Programs/test/v4_console" /tmp/back/
cmp build/v4_console /tmp/back/v4_console && echo identisch
```

Ein Zielpfad ohne existierendes Verzeichnis (z. B. `/tmp/back/datei.bin`) wird
abgelehnt — `acp` erwartet dort ein Verzeichnis.

`acp` kann **keine** Programme starten — es kennt nur Übertragen, `-l` und
`-d`. Der Start erfolgt also immer auf der V4. Für den Debug-Weg mit
`bgdbserver` gibt es `make upload-debug` (noch nicht ausgeführt).

Erwartung beim Start: Meldet das Programm `I2C-Bus nicht verfuegbar`, fehlt
`i2c.library` oder der Bus; meldet es `PING: LINK-Fehler`, ist der ESP32 nicht
angeschlossen, nicht versorgt oder die Adresse stimmt nicht. Ein
`Protokoll passt nicht` heißt, dass die Firmware nicht `proto_ver` 3 spricht —
die Konsole nennt dann alle gemeldeten Rahmengrößen.

## Hardware-Inbetriebnahme (§14.2)

```sh
# 1. Bus scannen: muss 0x50 zeigen
i2cdetect -y 1
# 2. Einzeltest gegen die Golden Frames aus §12
make demo && ./build/v4_console_linux /dev/i2c-1
# 3. Dauerlauf: der Mock-Lauf ist kein Hardwareersatz (Timing!)
# 4. Störung: Takt auf 400 kHz, t_wait auf 500 µs — ab wann steigen die Fehler?
# 5. Massentransfer: 1-MB-Datei, CRC gegen den erwarteten Datei-CRC
# 6. Endianness: make test-m68k
```

**Zuerst das Firmware-Log ansehen.** Nach dem SD-Mount läuft auf dem ESP32
`v4_link_selftest()` (§14.0) und protokolliert alles mit dem Präfix `SELFTEST`:
Frame-Aufbau und -Prüfung in beide Richtungen, die BUSY-Runde, das Bulk-Frame
mit wechselnder Chunk-Größe, `DIR_OPEN`/`DIR_NEXT` samt Idempotenz-Test und die
Fehlerfälle `..` → `BAD_ARG`, ungültiges Handle → `NO_HANDLE`. Steht dort
durchweg OK und die V4 meldet trotzdem Fehler, liegt es an der
Transportschicht oder am Master — nicht am Protokoll. Nur den I2C-Bus selbst
erfasst der Selbsttest nicht.

**Bei `V4_ERR_LINK` auf PING zuerst an den Rahmen denken:** eine Firmware mit
dem alten 64-Byte-READ-Frame scheitert genau so (siehe „Protokollstand"). Die
Konsole gibt diesen Hinweis aus.

Bei `EMSGSIZE` oder ähnlichen Längenfehlern limitiert ein USB-I2C-Adapter die
Nachrichtenlänge auf 32 Byte. Die alte Abhilfe `v4_set_chunk(m, 16)` hilft seit
dem 128-Byte-READ-Frame **nicht mehr**: schon die Steuerframes sind 128 Byte
groß. Mit einem 32-Byte-Adapter läuft der Master nicht (§1.5).

Die Diagnosezähler in `v4_master_t` (`tx_frames`, `rx_frames`, `retries`,
`busy_rounds`, `badcrc_rounds`, `last_check`) sind für die Fehlersuche auf
Hardware gedacht: steigende `retries` bei laufendem Betrieb deuten auf Timing
oder Takt, `last_check` nennt den konkreten Ablehnungsgrund
(`v4_check_str(m->last_check)`).

## Hinweis zur Testsuite

Beim Protokollwechsel hat die Suite außerdem einen Fehler im **Makefile**
aufgedeckt: es fehlten Header-Abhängigkeiten, sodass eine Änderung an
`v4_proto.h` **keinen** Neubau auslöste — `make test` lief still mit dem alten
Binary weiter und meldete grün. `HDRS` steht jetzt bei allen Zielen als
Voraussetzung, und `make -n test` nach `touch v4_proto.h` beweist den Neubau.

Beim Protokollwechsel auf den 128-Byte-Frame hat die Suite einen weiteren
echten Fehler gefunden: der Mock legte die `GET_STATUS`-Nutzlast in einen
16-Byte-Puffer, während die Antwort auf 17 Byte wuchs — AddressSanitizer meldete
den Überlauf in `v4p_enc_get_status`. Alle lokalen Nutzlastpuffer im Mock sind
jetzt auf `V4P_READ_PAYLOAD_MAX` ausgelegt.

Die Suite hat beim ersten Lauf einen echten Fehler gefunden: im Mock war
`s_chunk` als `uint8_t` deklariert, wodurch `SET_CHUNK(1024)` zu 0 wurde und
jeder Bulk-Read mit `V4_ERR_LINK` abbrach. Genau solche Fehler — Typbreite gegen
Wertebereich — sind im Linux-Harness unsichtbar, wenn nicht alle Grenzen
angefahren werden. `chunk = 16` und `chunk = 1024` werden deshalb explizit
getestet.

Eine zweite Schwäche war struktureller Natur und wurde gezielt geschlossen: die
Nutzlast-Offsets der Antworten (GET_INFO, GET_STATUS, SD_INFO, DEV_GET,
DIR_NEXT, FILE_OPEN) wurden zunächst nur vom gemeinsamen Codec geprüft — ein
Offset-Fehler in Encoder **und** Decoder wäre unentdeckt geblieben, weil der
Mock dieselben Funktionen benutzt. `test_golden_payloads()` vergleicht deshalb
jetzt handabgeschriebene Byte-Folgen direkt aus den Tabellen in §11 gegen
Encoder und Decoder.

Drei weitere Lehren aus der Fremdprüfung, die als Testmuster übernommen wurden:

1. **Schleifen, die nur im Fehlerfall prüfen, sind vakuant.** Die
   Byte-Order-Schleife über 200 000 Werte erzeugte bei korrektem Code null
   Checks. Jetzt wird gezählt und der Zähler geprüft.
2. **Zähler gegen sich selbst zu vergleichen beweist nichts.**
   `tx_frames == 20 + retries` war per Konstruktion wahr; ersetzt durch die
   R1-Invariante `rx_frames == tx_frames` und die Delay-Invariante.
3. **Ein Gate, das nichts findet, findet auch keinen Verstoß.** Beide
   `tools/`-Skripte enthalten jetzt Gegenproben: der Lint kompiliert neun
   Verstoß-Dateien (inkl. typedef-Cast und `union`), das m68k-Gate zwei
   absichtlich falsche Probefunktionen — werden sie nicht erkannt, schlägt das
   Werkzeug selbst fehl.

