# v4_master -- Vampire V4 als I2C-Master fuer den Bluetooth-Joystick-Adapter

Dieses Programm laeuft auf der Vampire V4 und spricht ueber I2C mit unserem
ESP32 (`V4_ESP32_BT_Joy_BP32`), der als Bluetooth-HID-Host Gamepads und Maeuse
annimmt und die Tasten- und Richtungssignale auf zwei DB9-Buchsen ausgibt.
Es ist das Gegenstueck zum Audio-Projekt (`Projects/I2C-test/v4_master`) --
gleicher Transaktionskern, gleiche Plattformschicht, anderer Befehlssatz.

Zwei Frontends, eine Protokollschicht:

| Programm        | Wofuer                                                        |
|-----------------|---------------------------------------------------------------|
| `v4_console.c`  | Textkonsole: Notfallweg auf der V4, Testlauf ohne Hardware     |
| `v4_gui.c`      | MUI-Oberflaeche: der eigentliche Zweck dieses Projekts         |

Beide benutzen `v4_master.c` / `v4_proto.c` unveraendert und rufen den Bus
**direkt aus ihrem Hauptfaden** auf: kein `CreateNewProc`, kein zweiter Faden,
kein Nachrichtenport, kein `Forbid()`. Das ist die Konstruktion, die auf der V4
nachweislich gelaufen ist (siehe "I2C direkt im Hauptfaden" weiter unten).

Jede Meldung geht **gleichzeitig** an drei Senken: Standardausgabe, den
Hardware-Debug-UART der V4 (115200 Baud, am PC ueber eine CH340 mitgelesen) und
die Logdatei. So lassen sich Mitschnitt und Logdatei nach dem Lauf Zeile fuer
Zeile vergleichen.

## Schnellstart

```sh
make test        # Portabilitaets-Lint + native Testsuite (Little Endian)
make demo        # Konsolenprogramm gegen den MOCK bauen UND durchlaufen
make verify      # alles, was ohne Hardware pruefbar ist
```

`make demo` braucht **keine** Hardware: es linkt `v4_mock.c` (den
ESP32-Emulator) statt `v4_linux_i2c.c` und schiebt die Befehle ueber eine Pipe
herein, damit der Lauf nicht an der Eingabeaufforderung stehenbleibt. Der Ziel
prueft die Ausgabe auf die Zeilen, auf die es ankommt (Rahmengroessen, Adresse,
Faehigkeiten, Controller, Portgruppen, gemessene Pegel samt Widerspruch
"kein Pull-up"/"unmoeglich", Rueckfallweg der alten 10-Byte-Firmware, Scan,
Verbinden, Modustausch, Fehlerwege) -- dazu, dass ein **toter Link**
ausdruecklich gemeldet wird und das Programm sich beendet, statt zu warten
(`V4_DEMO_LINK_TOT=1`), sowie seit dem Debugausbau, dass die Zeilen je
I2C-Transaktion samt Dauer und Antwort-Kurzform erscheinen und `-quiet` sie
wieder abschaltet.

Auf der V4 selbst:

```sh
make amiga       # Konsole   -> build/v4_console_amiga
make amiga-gui   # MUI       -> build/v4_gui_amiga   (braucht -lmui)
export AMIGAHOST=192.168.178.50
make upload      # beide Programme + Ikonen nach Programs:test/
make log         # Logdatei von der V4 holen (Vorgabe: Programs:test/v4_joy.log)
```

### Schalter

Die Belegung der Schalter ist bei beiden Programmen gleich, die **Vorgabe**
unterscheidet sich: die Konsole ist ein Arbeitsmittel (dort ist die Zeile je
Transaktion die Grundlage), die Oberflaeche laeuft im 200-ms-Takt und muss mit
der Ausgabe sparsam sein.

| Schalter    | Wirkung                                                        |
|-------------|----------------------------------------------------------------|
| (Vorgabe Konsole)   | eine Zeile je I2C-Transaktion, Debug-UART an           |
| (Vorgabe MUI)       | **keine** Zeile je Transaktion: im Ruhezustand EINE Zeile je Sekunde (Lebenszeichen mit dem Vergleich direkt/I2C), jede Aenderung sofort |
| `-quiet`    | keine Zeile je Transaktion; Start, Aenderungen, Fehler, Lebenszeichen, Vergleich bei Aenderung und Schlusszeile bleiben. Fuer die MUI ist das die Vorgabe |
| `-nodebug`  | keine Ausgabe ueber den Debug-UART der V4                      |
| `-trace`    | zusaetzlich die Zeile je Transaktion UND jeden Transaktionsschritt (tx/rx/pruefen/...). Sehr ausfuehrlich (etwa das Achtfache) und **hat Vorrang vor `-quiet`** |
| `-l <pfad>` | Logdatei (Vorgabe `Programs:test/v4_gui.log`)                  |
| `-nolog`, `-n` | keine Logdatei                                              |
| `-d <geraet>` | I2C-Geraet (Linux-Harness; auf der V4 wirkungslos)           |
| `-w <px>`, `-h <px>` | Fenstergroesse der MUI (Vorgabe **640x400**, mit der Maus aenderbar) |
| `-a`, `-x`  | nur Konsole: Trace an bzw. zusaetzlich Rahmenbytes             |
| `-w <us>`, `-r <zahl>`, `-o <pfad>`, `-t <ticks>` | nur Konsole: Wartezeit vor dem Lesen, Versuche je Transaktion, Logdatei, Wartezeit je Logzeile |

Ausgabe immer auf **allen** Kanaelen zugleich: `stdout`, Debug-UART der V4
(115200 Baud) und Logdatei. Nach dem Lauf lassen sich Mitschnitt und Logdatei
Zeile fuer Zeile vergleichen.

Am Linux-Rechner mit echtem Adapter am i2c-Bus:

```sh
make demo-i2c
./build/v4_console_i2c -d /dev/i2c-1
```

## Aufbau

| Datei                    | Inhalt                                                       |
|--------------------------|--------------------------------------------------------------|
| `v4_proto.h/.c`          | Rahmenformat, CRC-8, Nutzlast-Codecs, Klartexte/Ableitungen   |
| `v4_master.h/.c`         | Transaktionskern (SEQ, Wiederholung, BUSY, BAD_CRC) + Wrapper |
| `v4_state.h/.c`          | Zustandshilfe der Frontends: letzte gueltige Werte, Backoff   |
| `v4_joyport.h/.c`        | direkte Abfrage der Joystick-Register + Vergleich mit I2C     |
| `v4_debug.h/.c`          | gemeinsame Ausgabe: stdout + Debug-UART + Logdatei, Transaktionszeilen |
| `v4_amiga_i2c.c`         | Zielplattform: i2c.library + timer.device (m68k, Big Endian)  |
| `v4_linux_i2c.c`         | Linux-Harness: i2c-dev                                        |
| `v4_mock.h/.c`           | ESP32-Emulator fuer Tests und Demo, inkl. Fehlerbilder        |
| `v4_console.c`           | Textkonsole (beide Plattformen, mit Mock)                     |
| `v4_gui.c`               | MUI-Oberflaeche (nur Amiga)                                   |
| `tests/`                 | 158 Testfaelle / 1016 Checks, plus Demo-Rahmen (`demo_main.c`) |
| `tools/check_portability.sh`    | Quelltext-Lint: keine mehrbytigen Frame-Zugriffe      |
| `tools/check_m68k_byteaccess.sh`| Nachweis im erzeugten m68k-Code                       |
| `tools/check_mui_tree.py`       | Aufbaupruefer des MUI-Objektbaums in `v4_gui.c`        |

### Protokoll in Kurzform

Massgeblich ist die ESP32-Seite: `V4_ESP32_BT_Joy_BP32/main/v4_proto.h` (die
Zahlen) und `V4_ESP32_BT_Joy_BP32/docs/PROTOKOLL.md`, Abschnitt
"Eingangspayload" (die Beschreibung). Diese Datei hier ist nur die V4-Fassung
desselben Vertrags.

* WRITE-Rahmen 32 Byte: Magic `0xA5`@0, cmd@1, seq@2, len@3, Nutzlast@4..30,
  CRC-8@31.
* READ-Rahmen **128 Byte**: Magic `0x5A`@0, cmd@1, seq@2, status@3, len@4,
  Nutzlast@5..125, flags@126, CRC-8@127. Das Audio-Projekt las **64** Byte --
  eine Firmware mit der alten Rahmengroesse wuerde jede Antwort um 64 Byte
  verschieben. `v4_check_info()` lehnt sie ausdruecklich ab.
* CRC-8 nach SMBUS: Polynom 0x07, Init 0x00, keine Reflektion.
* I2C-Adresse des ESP32: **7 Bit 0x5E** (Schreibadresse 0xBC). Das Audio-Projekt
  benutzt 0x50 -- gleicher Bus, anderer Adapter.
* Alle Mehrbyte-Felder little endian, byteweise ueber `v4p_put_*_le` /
  `v4p_get_*_le`; `tools/check_portability.sh` erzwingt das.

Befehle (nur diese): `0x01` PING, `0x02` GET_STATUS, `0x03` GET_INFO,
`0x04` SET_PORT_MODE, `0x05` GET_PORT_MODE, `0x06` GET_INPUT, `0x07` GET_HID,
`0x08` SET_DEVICE_PORT, `0x09` GET_PORT_CONFIG, `0x0A` SET_PORT_ROUTING,
`0x10` SCAN_START, `0x11` SCAN_STOP, `0x12` DEV_COUNT, `0x13` DEV_GET,
`0x20` CONNECT, `0x21` CONNECT_BDA, `0x22` DISCONNECT, `0x23` FORGET,
`0x7E` RESET.

Portzustand-Bits (gleiche Belegung fuer Zug und Pegel): UP `0x01`, DOWN `0x02`,
LEFT `0x04`, RIGHT `0x08`, BTN1 `0x10` (Pin 6), BTN2 `0x20` (Pin 9),
BTN3 `0x40` (Pin 5). Betriebsarten: 0 aus, 1 Joystick, 2 Maus, `0xFF` "nach
Geraeteart" (nur als **Vorgabe**, siehe unten).

### Eingangsverteilung je Buchse (`fw_ver` 4)

Die Firmware meldet jetzt **`fw_ver = 4`**. Der Rahmen und `proto_ver` bleiben
**unveraendert** (proto_ver 3, 32 -> 128 Byte) -- eine Firmware mit `fw_ver` 4
wird von `v4_check_info()` deshalb **nicht** abgelehnt; geprueft werden dort nur
`proto_ver` und die Rahmengroessen. Ob die drei neuen Befehle vorhanden sind,
sagen die **Faehigkeitsbits** `0x0100` (`GET_PORT_CONFIG`/`SET_PORT_ROUTING`)
und `0x0200` (`SET_DEVICE_PORT`) -- `v4_info_has_caps()`. Fehlt eines, bleibt
das jeweilige Feld untaetig und sagt das ausdruecklich; ein Absenden ins Leere
(BAD_CMD) gibt es nicht.

| Befehl | Wert | Anfrage -> Antwort |
|---|---|---|
| `SET_DEVICE_PORT` | `0x08` | `[bda 6][port u8]` -> Echo (7 Byte); `BAD_ARG` bei port > 1 |
| `GET_PORT_CONFIG` | `0x09` | -- -> 12 Byte (siehe unten) |
| `SET_PORT_ROUTING` | `0x0A` | `[port][dir][btn1][btn2][btn3]` -> Echo (5 Byte); `BAD_ARG` bei port > 1, unbekanntem Code oder **doppelt vergebener** Knopfquelle |
| `SET_PORT_MODE` | `0x04` | wie bisher; **`mode = 0xFF`** setzt die Vorgabe auf "nach Geraeteart" zurueck |

`GET_PORT_CONFIG` (12 Byte): `mode_soll0`@0, `mode_soll1`@1 (Vorgabe, `0xFF` =
auto), `mode_ist0`@2, `mode_ist1`@3 (**wirksame** Betriebsart, nie `0xFF`),
`dir0`@4, `dir1`@5 (Richtungsquellen), `btn0`@6..8 und `btn1`@9..11
(Knopfquellen in der Reihenfolge **Pin 6, Pin 9, Pin 5**).

Richtungen (`dir`): `0x01` Stick links, `0x02` Stick rechts, `0x04` D-Pad;
`0x07` = alle drei = Vorgabe. Knopfquellen: `0` keine, `1` A, `2` B, `3` X,
`4` Y, `5` L1, `6` R1, `7` L2, `8` R2, `9` L3, `10` R3, `11` Start,
`12` Select, `13` Home/Capture, `14` Gruppe 1 (A,L1,L2,L3), `15` Gruppe 2
(B,R1,R2,R3), `16` Gruppe 3 (X,Y,Start,Select). Vorgabe: `dir = 0x07`,
`btn = 14, 15, 16` -- ohne neue Einstellung aendert sich also nichts.

Regeln (der Master prueft sie **lokal** mit denselben Funktionen wie der Mock
und schickt bei einem Fehler gar keinen Rahmen): nur bekannte Codes und
Richtungsbits; eine **nicht-leere** Quelle darf nur auf **einer** der drei
Leitungen stehen ("keine" darf mehrfach vorkommen). `BAD_ARG` heisst: der
ESP32 hat **nichts** geaendert -- die Anzeige bleibt auf dem letzten
bestaetigten Stand.

**Was die Gegenstelle nicht hergibt:** die Zuordnung BDA -> Buchse ist ueber
die Leitung **nicht lesbar**. `DEV_GET` liefert nur Name und Adresse,
`GET_PORT_CONFIG` nur die Buchsenfelder. Konsole und Oberflaeche zeigen deshalb
die Zuordnungen, die in **dieser Sitzung** per `SET_DEVICE_PORT` gesetzt und
bestaetigt wurden; die Zuordnung selbst liegt im NVS des ESP32 und bleibt dort.
Ein "Loesen" gibt es nicht -- der ESP32 kennt nur "BDA -> Buchse".

`GET_INPUT` liefert **12 Byte**: `state0`@0 und `state1`@1 (Zug-Maske,
Bit gesetzt = nach GND gezogen = aktiv, Ruhe `0x00`), `steps_x`@2 und
`steps_y`@6 (u32 LE) sowie ab der aktuellen Firmware `level0`@10 und
`level1`@11 (gemessene Pegel, Bit gesetzt = **High**, Ruhe `0x7F`). Die
Nutzlastlaenge ist die Versionsauskunft: **12 Byte = mit Pegeln, 10 Byte =
aeltere Fassung ohne**. Bei 10 Byte sind die Pegel unbekannt; die Anzeigen
zeigen dann einen Hinweis und keine Zahl.

`SCAN_START` schickt zwei Byte: `[dauer_s]` und ein optionales `[modus]`
(Bit 0 = Dauerbetrieb). Die Firmware liest das zweite Byte nur, wenn es da ist
(`mode = (plen >= 2) ? p[1] : 0`) -- beide Seiten sind sich also einig. Ohne
Dauerbetrieb endet die Suche nach dem Fenster; die Oberflaechen lesen die
Ergebnisse ohnehin ueber mehrere Takte ab.

## Unterschiede zum Audio-Projekt

| Punkt                  | Audio                            | hier                                  |
|------------------------|----------------------------------|---------------------------------------|
| READ-Rahmen            | 64 Byte (proto_ver 2)            | **128 Byte** (proto_ver 3)            |
| I2C-Adresse            | 0x50                             | **0x5E**                              |
| GET_INFO +10/+11       | reserviert (0)                   | **Faehigkeitsliste** (u16 LE)         |
| BULK / CRC-32 / chunk  | fuer Dateizugriff                | **entfaellt** (`bulk_payload_max=0`)  |
| SD-, Datei-, Audio-, EQ-Befehle | vorhanden               | **entfallen** (der ESP32 antwortet darauf mit `BAD_CMD`) |
| Buchsen                | --                               | `SET/GET_PORT_MODE`, `GET_INPUT` (12 Byte mit Pegeln), `GET_HID` |
| Logdatei auf der V4    | `Programs:test/v4_console.log`   | `Programs:test/v4_joy.log`            |

Der Transaktionskern ist bewusst derselbe geblieben: SEQ wird nur nach einer
gueltigen Antwort erhoeht, Wiederholungen benutzen dieselbe SEQ, BUSY wird mit
wachsendem Warte-Backoff erneut gesendet, BAD_CRC ebenso. Was am Audio-Projekt
im Feld gelernt wurde (Wartezeit vor jedem Lesen, Backoff, 32-Byte-Schreibrahmen),
gilt hier unveraendert.

## Zug und Pegel -- zwei verschiedene Dinge

`GET_INPUT` liefert beides, und beides bedeutet etwas anderes:

* **Zug** (`state0`/`state1`): was die Firmware nach GND **zieht**.
  Bit gesetzt = Taste/Richtung aktiv, Ruhezustand `0x00`.
* **Pegel** (`level0`/`level1`): was an der Leitung **wirklich anliegt**.
  Bit gesetzt = High, Ruhezustand `0x7F`.

Auseinander gehen sie genau dort, wo es elektrisch interessant wird:

| Zug | Pegel | Bedeutung |
|---|---|---|
| 0 | 1 | losgelassen und High -- Pull-up wirkt, der Normalfall |
| 1 | 0 | gezogen und Low -- richtig, die Leitung liegt auf GND |
| 0 | 0 | **losgelassen, liest aber 0** -- es zieht niemand hoch: der Pull-up fehlt oder die Platine hängt nicht an der Buchse |
| 1 | 1 | **gezogen, liest aber High** -- elektrisch unmöglich, Ausgangsstufe oder Pinbelegung prüfen |

Die dritte Zeile ist der Befund, für den die Pegel überhaupt über die Leitung
gehen: ohne sie sieht "kein Pull-up" wie "nichts gedrückt" aus. Konsole und MUI
markieren den Widerspruch je Leitung und melden ihn zusätzlich als `WARNUNG: ...`
(`v4p_level_missing_pullup()` / `v4p_level_impossible()` in `v4_proto.c`).

**Keine Ableitung mehr:** früher zeigte die Anzeige hier einen aus dem Zug
*abgeleiteten* Ersatzwert ("Pegel (erwartet)"). Seit die Firmware die Pegel
mitschickt, gibt es diesen Ersatz nicht mehr -- eine Zahl in der Pegelspalte ist
gemessen. Antwortet ein alter ESP32 mit der 10-Byte-Fassung, steht in der Spalte
`Pegel: Firmware ohne Pegel -- neu flashen` und je Zeile `(Pegel unbekannt)`.

## Direkt aus der Hardware -- der zweite Massstab

Die V4 kann ihre Joystick-Anschluesse **selbst** lesen. Die MUI-Oberflaeche tut
das seit diesem Durchgang und stellt die Werte **neben** die I2C-Antwort
(`v4_joyport.c`, geprueft in `tests/test_joyport.c`). Das ist der Zweck:

* I2C = was der **ESP32** meldet (`GET_INPUT`: Zug, Pegel),
* direkt = was die **V4** an ihren eigenen Registern sieht.

Stimmen beide ueberein, ist die Kette ESP32 -> DB9 -> V4 in Ordnung. Weichen sie
ab, sagt der danebenstehende Rohwert, wo zu suchen ist.

| Register   | Adresse   | Bedeutung                                                    |
|------------|-----------|--------------------------------------------------------------|
| `JOY0DAT`  | `$DFF00A` | Zaehler Buchse 0: Bit 0..7 X, Bit 8..15 Y                    |
| `JOY1DAT`  | `$DFF00C` | Zaehler Buchse 1, gleiche Aufteilung                         |
| `CIAAPRA`  | `$BFE001` | Knopf 1 (Pin 6): Bit 6 = Buchse 0, Bit 7 = Buchse 1 (aktiv low) |
| `POTINP`   | `$DFF016` | Knopf 2 (Pin 9): Bit 8 = Buchse 0, Bit 10 = Buchse 1         |

Zum Vergleich wird in **dieselbe Bitmaske** umgerechnet wie im Protokoll
(`UP 0x01, DOWN 0x02, LEFT 0x04, RIGHT 0x08, BTN1 0x10, BTN2 0x20`); die
deutschen Namen stehen daneben. So sehen die Zeilen im Fenster aus (und genau so
gehen sie je Takt mit Praefix `[vergleich]` auf den DebugUART):

```
Buchse 0  direkt: 0x00 -   roh 0x085C   I2C: state=0x00 level=0x7F   OK
Buchse 1  direkt: 0x11 VOR+BTN1   roh 0x0100   I2C: state=0x11 level=0x6E   OK
CIAAPRA=0x00F4  POTINP=0xFF02   Grundzustand CIAAPRA=0x00F4 POTINP=0xFF02
```

Bei Abweichung steht `ABWEICHUNG (Differenz 0x..)` -- die Differenzmaske sagt
unmittelbar, welche Leitung gemeint ist:

```
Buchse 1  direkt: 0x08 RECHTS   roh 0x0003   I2C: state=0x0A level=0x75   ABWEICHUNG (Differenz 0x02)
```

**Zwei Abtastungen je Buchse.** Der Zaehler der V4 liefert beim Umschalten
Zwischenwerte (`$0003`, `$0200` im Mitschnitt vom 13.10.). Deshalb wird jede
Buchse aus ZWEI Abtastungen mit kurzem Abstand bewertet: nur wenn beide
denselben Wert liefern, wird er uebernommen -- sonst bleibt der letzte gueltige
stehen (`(springt)` in der Zeile, `stabil=nein` auf dem UART). So flackert die
Anzeige nicht. Der Abstand ist eine kurze Rechenschleife, **kein** `Delay()`
(das waere ein Aufgabenwechsel und viel zu lang).

**Feuerknoepfe: Grundzustand statt geratener Polaritaet.** Am Geraet bewegte
sich beim Druecken an `CIAAPRA`/`POTINP` nichts (`$00F4` bzw. `$FF02`). Welches
Bit welche Taste ist, ist also offen. Deshalb gilt: als gedrueckt zaehlt, was
sich vom **beim Start gemerkten** Wert unterscheidet -- in beide Richtungen
(aktiv low oder aktiv high). Die Rohwerte beider Register stehen immer dabei,
damit sich die Belegung von Hand ablesen laesst. `BTN3` (Pin 5) hat keine
bekannte direkte Quelle und wird beim Vergleich **ausgelassen** -- ein
"ABWEICHUNG" daraus waere keine Messung.

**Wo die Dekodierung vom Wortlaut des Auftrags abweicht** (und warum das
Absicht ist): der Auftrag nennt `VOR = ((j >> 1) ^ j) & 0x0100` und
`ZURUECK = ((j << 1) ^ j) & 0x0100`. Gemessen ergibt das

* `$0100` -> `VOR ZURUECK` gleichzeitig (nicht nur beim Umschalten: der Wert lag
  Sekunden lang stabil an),
* `$0300` -> `LINKS ZURUECK`,
* `$0001` -> **keine** Richtung, obwohl dort "zurueck" anliegt.

Deshalb steht in `v4_joyport.c` die statische Form `VOR = (j & 0x0100) und LINKS
frei`, `ZURUECK = (j & 0x0001) und RECHTS frei`. Fuer alle nachgemessenen Werte
sagt sie genau das, was am Geraet zu sehen war (`$0002`/`$0003` = RECHTS,
`$0300`/`$0200` = LINKS, `$0100` = VOR, `$0001` = ZURUECK, `$0000` = Ruhe), und
fuer `$0003` **nicht** zusaetzlich "zurueck". Beides ist in
`tests/test_joyport.c` festgenagelt; die Entscheidung steht auch im Kopf von
`v4_joyport.h` mit dem Mitschnitt als Beleg.

**Grenze der Hardware** (kein Fehler der Anzeige): innerhalb EINER Achse sind
zwei Richtungen nicht gleichzeitig sichtbar. "unten+rechts" landet auf demselben
Zaehlerwert wie "rechts" allein (`$0003`), "oben+links" wie "links" allein. Eine
solche Diagonale zeigt deshalb eine Abweichung zum I2C-Wert -- zu Recht: die
direkte Quelle kann sie nicht sehen. Diagonalen ueber beide Achsen (z.B.
`oben+rechts` = `$0103` -> `0x09`) sind vollstaendig sichtbar.

## Die MUI-Oberflaeche

* **Oberste Zeile: der Verbindungszustand.** Im Normalfall
  `Verbindung steht (Adresse 0x5E).`, bei Stoerung ausdruecklich
  `ESP32 antwortet nicht -- Verkabelung/Strom pruefen` (mit dem Grund und dem
  naechsten Versuch in ms). Ein Wechsel wird genau einmal ins Logfeld
  geschrieben, nicht in jedem Takt.
* Kopfzeile: Protokoll- und Firmware-Version, Rahmengroessen, Adresse,
  Faehigkeitsliste im Klartext. Zustandszeile: `GET_STATUS`.
* Controller-Zeile: Name, Art (Pad/Maus/keine), Transport (BR/EDR/BLE),
  VID:PID, Report-ID/-laenge, gezaehlte Reports, Batterie (`GET_HID`) -- in
  DREI Zeilen, damit Name, Art und VID:PID auch bei 640 Pixel Fensterbreite
  sichtbar bleiben.
* **Der erste Takt ist ein vollstaendiger Takt** (`GET_PORT_MODE`, `GET_INPUT`,
  `GET_STATUS`, `GET_HID`), genau wie die Konsole ihren "Anfangszustand"
  sofort holt. Vorher wurde `GET_STATUS`/`GET_HID` nur in jedem fuenften Takt
  abgefragt, und der erste Takt war keiner: in der Zustands- und der
  Controller-Zeile standen dann nur die Anfangstexte (`" "` und `"Controller:
  (wird gelesen)"`) -- die Felder sahen leer aus, obwohl ein Controller
  verbunden war. Im Mitschnitt vom 13.10.2026 lagen dazwischen 25 s
  (`[mui] Fenster offen` 17:37:49 -> erste `[status]`-Zeile 17:38:14).
* **Eine erkannte Verbindung fuellt die Geraeteliste mit.** Steht laut
  `GET_STATUS` eine Verbindung und ist die Liste noch leer, holt der Takt sie
  genau einmal automatisch (`gui_liste_auto`/`act_scan_start(1)`, dieselbe
  Strecke wie der Knopf "Neu suchen"). Ohne das blieb die Auswahlliste leer,
  solange niemand den Knopf drueckte -- ein Controller, der sich von selbst
  verbindet (BluePad32 verbindet, sobald er werbend auftritt), tauchte dort nie
  auf.
* **Die Anzeigefelder koennen nicht weggedrueckt werden.** Sie haben
  `MUIA_Weight, 0`; die Area-Autodokumentation sagt dazu: "An object with a
  weight of 0 will always stay at its minimum size." Ohne das verteilt MUI den
  fehlenden Platz bei einem zu kleinen Fenster nach Gewicht, und ein
  Textobjekt mit dem Vorgabegewicht 100 kann unter seine Mindesthoehe gedrueckt
  werden -- es sieht dann leer aus, obwohl der Inhalt gesetzt ist. Den Platz
  bekommen stattdessen die beiden Listen (Gewicht 30/40, `MUIA_MinHeight 36`).
  Die vier Kopffelder tragen zusaetzlich `MUIA_Text_SetMin, FALSE`: die
  Controller-Zeile ist lang, und ohne das macht MUI ihre Breite zur
  Mindestbreite des Fensterinhalts und zieht das Fenster ueber die zugesagten
  640 Pixel auf.
* Zwei Portgruppen (Buchse 0/1) mit Cycle fuer die Betriebsart und je Leitung
  `Zug` und `Pegel`; gezogene Leitungen sind fett gesetzt und mit
  `GND` markiert. Widersprueche stehen als `<- kein Pull-up!` bzw.
  `<- unmoeglich!` in der Zeile und werden einmalig ins Logfeld geschrieben.
  Kein Farbcode: die Zuordnung der MUI-Stifte haengt an der Palette des
  Anwenders, und ein "aktives Rot", das auf einem anderen Screen unsichtbar ist,
  waere schlechter als Text.
* **Zwei Seiten statt einer laengeren Liste (`fw_ver` 4).** Der Umschalter
  `Seite:` (neben der Controller-Zeile, damit er keine zusaetzliche Hoehe
  kostet) schaltet zwischen **`Anzeige`** (Ports + Vergleich, wie bisher) und
  **`Konfiguration`** um. Die Seitengruppe ist ein `VGroup` mit
  `MUIA_Group_PageMode`; `MUI_Group.doc` sagt dazu: *"the minimum width/height
  of a page group is the maximum minimum width/height of all its children"*.
  Die Konfigurationsseite ist kleiner als die Anzeigeseite -- die
  Mindestgroesse des Fensters aendert sich damit **nicht**, und nichts wird
  unter seine Mindesthoehe gedrueckt. Genau das war noetig: der Inhalt waere
  sonst hoeher als der Schirm (640x360), und MUI haette die Textfelder
  unsichtbar gemacht.
* **Die Konfigurationsseite** zeigt je Zeile eine Angabe, links die
  Beschriftung, rechts die beiden Buchsen (gleiche `MUIA_FixWidth`, damit die
  Spalten untereinander stehen): `Geraet` (Auswahlfeld aus der Scanliste),
  `Richtungen` (drei Schalter Stick l / Stick r / D-Pad) und `Knopf Pin 6`,
  `Pin 9`, `Pin 5` (je ein Auswahlfeld mit allen Quellen). Oben steht der
  Klartext beider Buchsen, z.B.
  `Buchse 0: 8BitDo 64 BT -- Vorgabe Maus, wirkt Maus, Richtungen Stick links,
  Pin6<-L1, Pin9<-R1, Pin5<-keine`. Jede Aenderung geht **sofort** als
  `SET_DEVICE_PORT` / `SET_PORT_ROUTING` zum ESP32; danach wird
  `GET_PORT_CONFIG` gelesen und die Felder werden auf den **echten** Zustand
  gestellt. Bei `BAD_ARG` (z.B. doppelt vergebene Knopfquelle) steht der Grund
  im Klartext im Fenster und im Protokoll, und die Anzeige springt auf den
  letzten bestaetigten Stand zurueck. Die drei Knopf-Auswahlfelder tragen
  Kurzfassungen ("Gruppe 1"), weil MUI die Knopfbreite aus dem laengsten
  Eintrag rechnet und drei Felder in eine Spalte passen muessen; die
  Erklaerung steht als Hinweiszeile darunter und im Klartext.
* **Die beiden Auswahlknopfe der Betriebsart** (`Buchse 0:` / `Buchse 1:`)
  bekommen ihre Eintraege als **NULL-abgeschlossenes Array** von Zeichenketten
  (`gui_modus_eintraege = { "auto", "aus", "Joystick", "Maus", NULL }`). `MUIA_Cycle_Entries`
  ist laut SDK-Header `STRPTR *` (`libraries/mui.h:3098`, Kommentar
  "V4 is. STRPTR *") und laut `MUI_Cycle.doc`: *"You must supply a pointer to a
  string array, containing one entry for each item and terminated with a
  NULL."* Eine mit `|` getrennte Zeichenkette liest MUI als Zeigerarray: der
  Knopf bleibt **leer** und wird ueber die ganze Spaltenbreite gezogen -- genau
  das war am 13.10.2026 auf der V4 zu sehen. `MUIA_Weight, 0` haelt ihn auf der
  Breite seines laengsten Eintrags ("Joystick"); die Area-Doku sagt dazu "An
  object with a weight of 0 will always stay at its minimum size". Der aktive
  Eintrag ist seit `fw_ver` 4 die **VORGABE** aus `GET_PORT_CONFIG.mode_soll`
  (mit `auto` als viertem Eintrag = `0xFF`); daneben steht `wirkt: <Modus>` mit
  der **wirksamen** Betriebsart aus `GET_PORT_MODE`/`mode_ist`. Die beiden
  Felder sind bewusst getrennt -- `0xFF` ist keine Betriebsart. Das Array ist
  `static` und traegt eine Uebersetzungszeit-Sicherung auf den NULL-Abschluss.
* **Vergleich direkt <-> I2C** in einem eigenen Feld ueber die volle
  Fensterbreite: eine Zeile je Buchse (`direkt: 0x11 VOR+BTN1 roh 0x0100` neben
  `I2C: state=0x11 level=0x6E`) mit `OK` bzw. `ABWEICHUNG (Differenz 0x..)`,
  darunter die Rohwerte von `CIAAPRA` und `POTINP` samt Grundzustand. Das Feld
  wird auch dann gezeichnet, wenn der ESP32 schweigt -- die direkte Abfrage
  haengt nicht am Bus. Verglichen wird nur, wenn die I2C-Werte aus DIESEM Takt
  stammen; sonst steht `(alte Antwort)` und kein Urteil.
* **Fenster und Schirmgroesse.** Vorgabe 640x400 mit SizeGadget; `-w`/`-h`
  aendern die Groesse, mit der Maus geht es ueber das Groessensymbol. Passt die
  gewuenschte Groesse **nicht** auf den oeffentlichen Schirm (die Workbench
  dieser V4 laeuft auf 640x360), wird sie auf den Schirm begrenzt und das
  ausdruecklich gemeldet: `[fenster] 640x400 passt nicht auf den Schirm
  (640x360) -- auf 624x336 begrenzt.` Ein Fenster, das nicht passt, ist
  unbrauchbar (die unterste Zeile, das Logfeld, ist nie zu sehen), und MUI
  rechnet beim Oeffnen eines Fensters mit SizeGadget mit einem Groessenbereich,
  den es auf so einem Schirm nicht geben kann. Passt 640x400, aendert sich
  nichts.
* **MUI-Objekte werden erst angefasst, wenn das Fenster wirklich offen ist.**
  `s_gui_up` -- der Schalter, der den Zeilen-Haken fuer die Logliste scharf
  macht -- wird erst NACH `set(s_win, MUIA_Window_Open, TRUE)` gesetzt, und das
  Ergebnis von `MUIA_Window_Open` wird geprueft (schlaegt es fehl, endet das
  Programm mit einer Zeile im Log statt weiterzurechnen). Vorher stand
  `s_gui_up` direkt hinter dem Aufbau des Objektbaums. Die Reihenfolge steht im
  Log: `[mui] Objektbaum steht ...` -> `[mui] Fenster offen (640x400). Ab hier
  werden MUI-Objekte angefasst.` -> `[takt] erste Abfrage ...`.
* Knoepfe: Neu suchen, Verbinden, Trennen, Vergessen, Aktualisieren,
  Port 0/1 Modus weiterschalten, Buchsen tauschen, Live-Aktualisierung.
* **Buchsen tauschen** benutzt bewusst keinen neuen Befehl: der ESP32 hat
  keinen, und zwei Wege zu einer Sache sind ein Weg zu viel. `v4_swap_port_modes()`
  liest erst beide Betriebsarten und schreibt sie dann vertauscht -- erst lesen,
  dann schreiben, sonst waere der alte Wert der ersten Buchse verloren.

### I2C direkt im Hauptfaden -- die laufende Konstruktion

**Die Vorgeschichte:** `i2c.library` hat keinen Timeout und keine
Wiederholungen -- `SendI2C`/`ReceiveI2C` blockieren, bis die Transaktion durch
ist (`SetI2CDelay` ist nur der SCL-Takt, keine Zeitbegrenzung). Deshalb gab es
zwischenzeitlich eine eigene Amiga-Aufgabe (`v4_worker*.c`, `CreateNewProc`),
die alle Buszugriffe machte und den Zustandsblock fuellte. Diese Fassung ist auf
der V4 **nie gestartet**: die Oberflaeche blieb nach ihrem Banner stehen, auch
nach einem Neustart, und der DebugUART zeigte nach der Logdatei-Zeile keine
einzige Zeile mehr. Zusaetzlich hat ihr `Forbid()` -- als Sperre um den
Zustandsblock -- den Aufgabenwechsel im ganzen System angehalten und die
Workbench eingefroren.

**Jetzt gilt wieder die Fassung, die am Geraet nachweislich lief:**

```
   Frontend (MUI-Task bzw. Konsole)          v4_master / v4_plat_* / i2c.library
   --------------------------------          -----------------------------------
   Knopf oder 200-ms-Takt                    v4_get_input() ...
   ruft v4_master DIREKT, synchron           -> SendI2C / ReceiveI2C
   legt das Ergebnis in v4_state_t           (blockiert, kein Timeout)
   zeichnet, kehrt zurueck
```

* **Kein `CreateNewProc`, kein zweiter Faden, kein Nachrichtenport fuer
  Auftraege, kein `Forbid()`.** `CreateMsgPort` gibt es nur noch fuer
  `timer.device` (dort ist er Pflicht, sonst kommt die Antwort des Geraets nie
  an) und fuer den 200-ms-Takt der MUI.
* **Der Zustandsblock** (`v4_state_t`, ortsfest) haelt die zuletzt gueltigen
  Werte. Weil alles in EINEM Faden laeuft, braucht er keine Sperre. Sollte
  spaeter doch einmal ein zweiter Faden entstehen: SignalSemaphore (der
  Wartende schlaeft, das System laeuft weiter) -- **niemals `Forbid()`**.
* **Daten bleiben stehen:** ein Feld wird nur bei einer GUELTIGEN Antwort
  ersetzt. Bleibt eine Antwort aus, zeigt die Oberflaeche die letzten gueltigen
  Werte weiter und schreibt ausdruecklich in die oberste Zeile:
  `ESP32 antwortet nicht -- Verkabelung/Strom pruefen`.
* **Wachsender Abstand:** nach einem Fehlversuch wird mit 0,5 s -> 1 s -> 2 s ->
  5 s (danach bleibt es bei 5 s) erneut gefragt. Ein fehlender Partner erzeugt so
  keinen Dauerlauf auf dem Bus.
* **Der Takt:** `GET_PORT_MODE`/`GET_INPUT` alle 200 ms, jeder fuenfte Takt
  zusaetzlich `GET_STATUS`/`GET_HID`. Live ist abschaltbar; dann fragt nur noch
  "Aktualisieren" ab. Gibt es kein `timer.device`, wird nach jedem Ereignis
  gezeichnet (Fallback ohne Takt).
* **Beenden:** die Plattform wird geschlossen, danach MUI abgebaut. Es gibt
  keinen zweiten Faden, der noch auf den Bus zugreifen koennte.

Eine klassische MUI-Falle ist dabei umschifft: `set(MUIA_Cycle_Active, ...)`
loest die Notify aus, auch wenn nur die Anzeige nachgezogen wird. Ohne
Gegenmassnahme schickte die Oberflaeche bei jeder Aktualisierung denselben
`SET_PORT_MODE`. `act_set_mode()` vergleicht deshalb mit der zuletzt
**bestaetigten** Betriebsart aus dem Zustandsblock und tut nichts, wenn beide
gleich sind.

Dieselbe Konstruktion benutzt die **Konsole**: sie ruft `v4_master` direkt aus
ihrem Hauptfaden und wartet auf das Ergebnis. Damit laeuft der echte
Konsolencode in `make demo` und in der Testsuite -- nur gegen den Mock statt
gegen `i2c.library`. Was das **nicht** beweist, steht unter "Offene Punkte".

### Konsolenbefehle

Das Menue (`s h i p t a c d f r q`) bleibt, wie es war. Neu ist **`cfg`** --
dieselben Moeglichkeiten wie die Auswahlfelder der Oberflaeche, damit sich die
Eingangsverteilung auch ohne MUI bedienen und pruefen laesst:

| Befehl | Wirkung |
|---|---|
| `cfg` | `GET_PORT_CONFIG` lesen und **beide** Buchsen anzeigen (Geraet, Vorgabe, wirkt, Richtungen, Pin6/Pin9/Pin5) |
| `cfg <0\|1>` | nur diese Buchse anzeigen |
| `cfg <0\|1> mode auto\|aus\|joystick\|maus` | `SET_PORT_MODE` (`auto` = `0xFF`) |
| `cfg <0\|1> dir alle\|keine\|stick_l\|stick_r\|dpad[\|+...]\|0xNN` | `SET_PORT_ROUTING` mit dieser Richtungsmaske |
| `cfg <0\|1> btn1\|btn2\|btn3 <quelle>` | `SET_PORT_ROUTING`; Quelle: `keine`, `a`, `b`, `x`, `y`, `l1`, `r1`, `l2`, `r2`, `l3`, `r3`, `start`, `select`, `home`, `gruppe1`..`3`, `g1`..`g3` oder `0`..`16` |
| `cfg assign <index\|BDA> <0\|1>` | `SET_DEVICE_PORT`; Index aus der Liste des Befehls `a`, oder eine Adresse `AA:BB:CC:DD:EE:FF` |

Nach **jeder** Aenderung wird `GET_PORT_CONFIG` neu gelesen und die Zeile
gezeigt. Ein `BAD_ARG` (z.B. dieselbe nicht-leere Knopfquelle auf zwei
Leitungen) wird im Klartext gemeldet, und die Anzeige bleibt auf dem echten
Stand. Beispiel (das Abnahmebeispiel aus dem Auftrag, `make demo`, Demo 9):

```
Befehl> cfg 0 mode maus
Buchse 0: Betriebsart Maus bestaetigt.
Buchse 0: Geraet (keines)   Vorgabe Maus     wirkt Maus     Richtungen Stick links+Stick rechts+D-Pad Pin6<-Gruppe 1 Pin9<-Gruppe 2 Pin5<-Gruppe 3
...
Befehl> cfg 0 btn2 l1
SET_PORT_ROUTING: BAD_ARG -- "L1" ist auf dieser Buchse schon einer anderen Leitung zugeordnet
  (Pin6<-L1 Pin9<-R1 Pin5<-keine). Eine nicht-leere Quelle darf nur auf EINER Leitung stehen; nichts geaendert.
...
Befehl> cfg assign 1 0
Geraet [1] "Logitech MX Anywhere" -> Buchse 0 (bestaetigt).
```

### DebugUART -- was wann geschrieben wird

Die V4 hat einen Hardware-Debug-UART (SERPER, 115200 Baud), der am PC ueber
eine CH340 mitgelesen wird (`Projects/BT-Joy/tools/SERIELL.md`). Beide Programme
schreiben dorthin **und** auf `stdout` **und** in die Logdatei -- immer denselben
Text. Je Meldung entsteht eine fertige Zeile, der UART bekommt sie mit einem
einzigen `ApolloDebugPutStr` (nicht Zeichen fuer Zeichen); `Delay()` gibt es dort
nicht.

| Ausgabestelle                              | Zeile (Anfang)                          | wann |
|--------------------------------------------|-----------------------------------------|------|
| Startbanner                                | Name, Version, Datum, Uhrzeit, Logdatei, I2C-Adresse | einmal |
| Plattform                                  | `[plattform] v4_plat_open -> 0 (OK)`    | vor/nach dem Oeffnen |
| Register beim Start                        | `[direkt] Register beim Start: JOY0DAT=0x085C JOY1DAT=0x0000 CIAAPRA=0x00F4 POTINP=0xFF02` | einmal (Grundzustand der Feuerleitungen) |
| **Vergleich direkt <-> I2C (Ruhe)**        | `[herz] takt=12s aufrufe=144 tx_frames=... geprueft=137 fehlerhaft=0 fehlerserie=0 letzter_status=OK \| Buchse0 direkt=0x00 stabil=ja roh=0x085C I2C state=0x00 level=0x7F OK \| Buchse1 ... \| CIAAPRA=0x00F4 POTINP=0xFF02` | **einmal je Sekunde** (in der Herzzeile) |
| **Vergleich direkt <-> I2C (Aenderung)**   | `[vergleich] Buchse0 direkt=0x04 stabil=ja roh=0x085C I2C state=0x00 level=0x7F ABWEICHUNG(Diff 0x04) \| ...` | **sofort**, wenn sich Richtung, Feuerknopf oder Urteil aendern |
| Erster PING                                | `[ping] ...`                            | einmal |
| **Jede I2C-Transaktion**                   | `[i2c] cmd=GET_INPUT seq=12 status=OK len=12 dauer=2900us \| state0=... ` | je Transaktion -- **nur mit `-trace`** |
| Antwort-Kopf                               | `[info] proto=3 ...`                    | einmal |
| Buchsenmodi                                | `[modus] Buchse 0 = Joystick, ...`      | bei Aenderung |
| Eingaenge / Tastendruck                    | `[eingang] state0=...` / `[taste] Port 0: gedrueckt=[UP,BTN3] ...` | bei Aenderung |
| Sitzvergabe                                | `[sitzung] Sitzung an Index 1 vergeben` | bei Wechsel |
| Controller                                 | `[hid] ...`                             | bei Aenderung |
| Scan                                       | `[scan] ...`                            | bei Scan/Liste |
| Fehler                                     | `[fehler] GET_INPUT: LINK-Fehler ... rc=0xE0` + `[fehler]   tx_frames=... wiederholungen=... geprueft=... fehlerhaft=... fehlerserie=... busfehler=0x...` | je Fehler |
| Lebenszeichen                              | `[herz] takt=12s aufrufe=144 ... verworfene_rahmen=2 geprueft=137 fehlerhaft=1 fehlerserie=0 letzter_status=OK \| <Vergleich>` | jede Sekunde (MUI); in der Konsole, wenn seit der letzten eine Sekunde vergangen ist |
| Schlusszeile                               | `[ende] ... beendet: i2c_zeilen=... verworfene_rahmen=2 geprueft=137 fehlerhaft=1 fehlerserie=0 letzter_status=...` | einmal |

**Die Rahmenpruefung wird GEZAEHLT, nicht als "letzter Zustand" gemeldet.**
Vorher stand in der Fehlerzeile `letzte_Rahmenpruefung=CRC-Fehler` aus
`v4_master_t.last_check`. Das ist ein haftenbleibender letzter Wert: ein
einziger Aussetzer -- etwa der Rahmen, der beim Beenden des Programms noch auf
dem Bus lag -- stand danach in JEDER Fehlerzeile und sah wie ein Dauerfehler
aus. Genau diese Fehldeutung hat es gegeben. Jetzt zaehlt `v4_debug.c` mit:

| Feld          | Bedeutung                                                     |
|---------------|---------------------------------------------------------------|
| `geprueft`    | READ-Rahmen, die durch die Rahmenpruefung liefen              |
| `fehlerhaft`  | davon ohne gueltige Pruefung (Magic/CRC/Echo/Laenge)          |
| `fehlerserie` | Fehler **in Folge**; ein gueltiger Rahmen setzt sie auf 0     |

Ein einzelner Aussetzer liest sich damit als `fehlerhaft=1 fehlerserie=1` und im
naechsten Herzschlag als `fehlerserie=0`; ein Dauerfehler waechst. Die drei
Felder stehen in der Herzzeile, der Fehlerzeile und der Schlusszeile (und in der
Konsole in ihrer Fehlerdiagnose) -- immer ueber `v4_dbg_rahmen_text()`, damit es
nur EINE Schreibweise gibt. Der bestehende Zaehler `verworfene_rahmen` bleibt
daneben stehen.

Die Zeile je Transaktion nennt Befehl, Sequenznummer, Status, Nutzlastlaenge,
Dauer in Mikrosekunden und in Kurzform den Inhalt der Antwort (z. B.
`GET_INPUT: state0=0x00 state1=0x7F level0=0x7F level1=0x7F`). Die Dauer misst
`v4_plat_now_us()` (Amiga: `timer.device` `TR_GETSYSTIME`, Linux:
`clock_gettime`, Mock: ein Zaehler); gemessen wird die ganze Transaktion,
inklusive Wiederholungen.

**Zahl der Zeilen (MUI, Vorgabe ohne `-trace`):**

| Betrieb                        | Zeilen je Sekunde | was                                        |
|--------------------------------|-------------------|---------------------------------------------|
| Ruhezustand (nichts passiert)  | **1**             | die Herzzeile: Zaehler, letzter Status, verworfene Rahmen, die Rahmenpruefung (`geprueft`/`fehlerhaft`/`fehlerserie`), Wiederholungen **und** der Vergleich direkt/I2C beider Buchsen mit `CIAAPRA`/`POTINP` |
| Aenderung                      | **+1 je Aenderung** | sofort eine fertige Zeile: Richtung, Feuerknopf, Buchsenmodus, Sitzvergabe, Fehler, Verbindungsverlust/-erholung, Abweichung direkt/I2C |
| `-trace`                       | ~100 und mehr     | zusaetzlich jede Transaktion und jeden Schritt |

Vorher waren es im ruhigen Betrieb **rund 18 Zeilen je Sekunde** (10
Transaktionszeilen, 2 Statuszeilen, 1 Herzschlag und 5 `[vergleich]`-Zeilen je
Takt) mit 150 bis 300 Zeichen je Zeile -- bei 115200 Baud ist das der groesste
Einzelposten auf dem UART. Jetzt ist es **eine** Zeile je Sekunde, und die
enthaelt alles, was man zum Mitlesen braucht.

**Warum der Vergleich in der Herzzeile steht:** er ist der Zweck des Programms,
und er soll auch im Ruhezustand sichtbar bleiben -- aber nicht fuenfmal je
Sekunde. `v4_dbg_herz_zusatz()` haengt ihn an DIESELBE Zeile an, statt eine
zweite aufzumachen (Einzelheiten in `v4_debug.h`). Zusaetzlich geht sofort eine
eigene Zeile heraus, wenn sich der direkte Stand oder das Urteil aendert -- denn
eine Abweichung ist genau das Ereignis, auf das man wartet.

**`-quiet` und `-trace`:** Fuer die Oberflaeche ist `-quiet` die **Vorgabe**:
Transaktionszeilen nur auf ausdrueckliche Anforderung (`-trace`). Die Konsole
behaelt ihre Vorgabe "Transaktionszeilen an", weil dort die Zeile je
Transaktion die Arbeitsgrundlage ist -- die Demo-Pruefungen in `make demo`
haengen daran und bleiben unveraendert. `-trace` hat Vorrang vor `-quiet`.

## Nachweis (Stand 14.10.2026)

| Pruefung                          | Ergebnis                                   |
|-----------------------------------|--------------------------------------------|
| `make test` (Lint + Testsuite)    | 158 Faelle, 1016 Checks, 0 Fehler          |
| `make test-san` (ASan + UBSan)    | `ERGEBNIS: OK`, keine Befunde              |
| `make demo` (Mock, Pipe)          | 10 Durchlaeufe geprueft, `Demo: OK`        |
| `make amiga` (m68k-amigaos)       | baut mit `-Wall -Wextra -Werror` fehlerfrei|
| `make amiga-gui` (MUI, `-lmui`)   | baut mit `-Wall -Wextra -Werror` fehlerfrei|
| `make asm` (m68k-Disassembly)     | Frame-Bauer nur `move.b` auf Puffer        |
| Zustandshilfe gegen Mock          | Ausfall, letzte Werte, Backoff 0,5->5 s, Erholung |
| Direktabfrage gegen Attrappe      | RECHTS/LINKS/VOR/ZURUECK/Diagonale/Ruhe, Stabilitaetstor, Feuerknoepfe, Zeilenformate, Urteil `v4_joy_abweichung` (OK / Abweichungsmaske / kein Urteil) |
| Registerzugriff im m68k-Code      | `move.w $dff00a`, `move.w $dff00c`, `move.b $bfe001`, `move.w $dff016` -- absolute Adressen, kein Umweg |
| Debugausgabe im Demo              | Transaktionszeile mit Dauer + Kurzform, `-quiet` schaltet sie ab |
| Ausgabeschicht (`tests/test_debug.c`) | Herzzeile traegt Zaehler, letzten Status, verworfene Rahmen, Wiederholungen UND den Vergleich mit `CIAAPRA`/`POTINP` -- und passt VOLLSTAENDIG in den Zeilenpuffer; ein Aufruf je Meldung; Aenderung wird gemeldet, Wiederholung nicht; Transaktionszeilen nur, wenn bestellt |
| MUI-Objektbaum (`make mui-lint`)   | Klammerbalance, `Child`/Objekt, Tag/Wert, Klassenzugehoerigkeit, Gruppen mit Kindern, `MUIA_Cycle_Entries` als NULL-Array statt Zeichenkette, Seitengruppe (`MUIA_Group_PageMode`) -- dazu ein Selbsttest an einem kuenstlichen Baum, der jeden dieser Fehler einzeln erzeugt |
| Protokollkonstanten gegen ESP32   | gemeinsame `#define` identisch (inkl. `0x08`/`0x09`/`0x0A`, `0x0100`/`0x0200`, `0xFF`, Richtungs- und Knopfquellen) |
| `make test-m68k` (qemu, Big Endian)| **SKIP** -- `m68k-linux-gnu-gcc`/`qemu-m68k` fehlen |

## Der Absturz vom 13.10.2026 -- Befund und was dagegen getan ist

Am 13.10.2026 um 15:32 ist die Oberflaeche auf der V4 beim Start gestorben. Der
Mitschnitt (`Projects/BT-Joy/logs/v4_seriell.log`) endet mit

```
15:32:32  [ping] OK: proto=3 fw=3 write=32 read=128 caps=0x00FF
```

danach kam bis zum Neustart um 15:37 keine einzige Zeile mehr -- und **keine
einzige `[vergleich]`-Zeile**. Dieselbe Maschine hatte um 14:36 dieselbe
Fassung "0.2 (13.10.2026)" neun Minuten lang laufen lassen (mit `[herz]`,
`[i2c]`, `[status]`, `[hid]`, `[eingabe]`), nur ohne die Direktabfrage und ohne
das Vergleichsfeld. Zwischen beiden Laeufen sind nur `v4_gui.c`,
`v4_joyport.c/.h` geaendert worden (Zeitstempel).

### Was der Mitschnitt beweist

`v4_debug.c` schreibt jede Zeile ZUERST auf den Debug-UART und erst danach in
die Logliste. Eine Zeile, die im Mitschnitt fehlt, ist also **gar nicht
entstanden**. Daraus folgt:

* Der Absturz liegt **zwischen `[ping] OK` und der ersten Zeile der ersten
  Busabfrage** des ersten Takts (`[i2c] cmd=GET_PORT_MODE`) -- also in
  `open_libs()`, `gui_build()`, `timer_open()`, `set(s_live, ...)`,
  `set(s_win, MUIA_Window_Open, TRUE)`, im Anfang von `gui_tick()` oder in
  `gui_joy_abtasten()`. Nicht in der Anzeige und nicht in der Vergleichszeile:
  die kommen spaeter.
* Der I2C-Weg, `v4_plat_open`, PING/INFO und das Lesen der vier Register
  (`[direkt] Register beim Start: JOY0DAT=0xF7C8 ...`) haben funktioniert.

### Was geprueft wurde -- und mit welchem Ergebnis

* **Der MUI-Objektbaum ist NICHT die Ursache.** Das ist nicht geraten, sondern
  nachgezaehlt: `tools/check_mui_tree.py` zerlegt den Aufbau an den Kommas der
  Tiefe 0 (genau das ist eine MUI-Tag-Liste), baut den Baum nach und prueft
  Klammerbalance, `Child`/Objekt, Tag/Wert und Klassenzugehoerigkeit. Ergebnis
  fuer `v4_gui.c`: Fensterinhalt `VGroup` mit **10 Kindern** (4 Textzeilen,
  Portgruppe, das neue Vergleichsfeld, 2 Knopfzeilen, 2 Listen), Balance 0,
  jedes Tag mit genau einem Wert und aus der Klasse seines Objekts. Das neue
  `Child, s_vergleich = TextObject, MUIA_Text_Contents, ..., MUIA_Text_PreParse,
  ..., End,` ist Zeichen fuer Zeichen dasselbe Muster wie die sechs Textfelder
  daneben -- und die sind auf der V4 gelaufen.
* **Byte-Vergleich mit dem letzten auf dieser V4 gelaufenen Programm.**
  `Projects/BT-Joy/joyprobe` und die Binaries unter
  `Vampire_V4/wiederherstellen/` enthalten noch Symboltabellen. Ein Vergleich
  aller MUI-Attribut-IDs im Maschinencode der laufenden `BT Gamecontroller GUI`
  (v0.1, 11.10., auf der V4 gestartet und sauber beendet) mit der jetzigen
  Fassung ergibt **genau einen** Unterschied:
  `MUIA_Window_SizeGadget` -- und das neue Textfeld. Alles andere
  (`MUIA_Cycle_Entries`, `MUIA_Weight`, `MUIA_Listview_List`, `MUIA_Text_Contents`,
  `MUIA_Text_PreParse`, alle `MUIM_Notify`-Aufrufe, `MUIA_Window_ID`,
  `MUIA_Application_Base`, ...) ist identisch. Ein falsch verschachtelter Baum
  haette sich hier gezeigt.
* **Die Reihenfolge "Takt vor Fenster" war wirklich falsch** -- nur nicht so,
  wie vermutet: `gui_poll(1)` laeuft erst aus der Hauptschleife (ueber
  `ID_REFRESH`) und `gui_tick()` hat ein `if (!s_gui_up) return;`. Wohl aber
  stand `s_gui_up = 1` (der Schalter fuer den Zeilen-Haken in die MUI-Logliste)
  **direkt hinter dem Bauen des Objektbaums, also vor dem Oeffnen des
  Fensters**. Jede Meldung dazwischen haette in eine Liste eines noch nicht
  offenen Fensters geschrieben. Das ist jetzt korrigiert.

### Was NICHT bewiesen ist -- die verbleibenden Moeglichkeiten

**Die Ursache ist nicht sicher gefunden.** Der Mitschnitt endet vor der ersten
Zeile, und ohne die verlorene Quellfassung laesst sich der letzte Schritt nicht
mehr nachstellen; auf der Maschine gibt es keinen Debugger. Es bleiben zwei
Moeglichkeiten, und beide sind entschaerft worden:

1. **Das Oeffnen des Fensters.** Der einzige MUI-Unterschied zur nachweislich
   gelaufenen Fassung ist `MUIA_Window_SizeGadget`, und der Absturz liegt genau
   am Fensteraufbau. Auf dieser V4 laeuft die Workbench auf **640x360**
   (`Cybergraphics`, Modus `0x10000300`), das Fenster ist aber **640x400** --
   also hoeher als der Schirm. Genau daraus kann MUI beim Berechnen des
   Groessenbereichs fuer ein Fenster mit SizeGadget einen Bereich bauen, den es
   nicht gibt. Dagegen steht jetzt: die Groesse wird an den Schirm begrenzt (und
   das gemeldet), das Ergebnis von `MUIA_Window_Open` wird geprueft, und
   `s_gui_up` wird erst danach gesetzt. **Nicht gemessen** ist, ob MUI auf
   640x400/640x360 wirklich abstuerzt -- das laesst sich nur auf der V4 zeigen.
2. **Ein Speicher- oder Laufzeitproblem beim Aufbau** (MUI-intern, Stack,
   Allokation). Dagegen sprechen: der Baum ist geprueft, die Stapelgroesse ist
   bei allen Hausprogrammen dieselbe (kein `__stack`, auch nicht im laufenden
   Audio-Programm), und `v4_joy_abtasten()` rechnet nur auf einem Feld fester
   Groesse.

**Was in jedem Fall hilft:** die Reihenfolge ist jetzt so, dass der Mitschnitt
den naechsten Absturz eindeutig verortet. Kommt `[mui] Fenster offen` und dann
nichts, stirbt es im Takt (Direktabfrage/Bus); kommt die Zeile nicht, stirbt es
beim Aufbau oder beim Oeffnen des Fensters. Und im ersten Fall steht die erste
Buszeile **vor** der ersten Zeile der Direktabfrage.

## Offene Punkte (nicht beschoenigt)

0. **Auf echter Hardware ist in diesem Durchgang nichts gelaufen -- und der
   Absturz vom 13.10. ist NICHT sicher erklaert.** Der Busweg
   (`i2c.library`, `timer.device`, `TR_GETSYSTIME`) und der DebugUART haben auf
   der V4 funktioniert (Mitschnitt 14:36 und 15:32); alles, was hier sonst
   steht, ist uebersetzt und gegen den Mock geprueft. Was der Mitschnitt ueber
   den Absturz hergibt und welche zwei Moeglichkeiten bleiben, steht oben unter
   "Der Absturz vom 13.10.2026". Der Nutzen des Umbaus liegt darin, dass die
   Konstruktion wieder die ist, die am Geraet nachweislich lief -- und dass der
   naechste Mitschnitt den Ort eindeutig nennt. Geprueft ist die *Logik* gegen
   den Mock (`tests/test_state.c`): Ausfall, stehenbleibende Werte,
   Backoff-Leiter, Erholung, Klassifikation der Rueckgabewerte und die
   Geraeteliste.
   * **Die direkte Registerabfrage ist auf der V4 GELESEN worden, aber nicht
     unter MUI verglichen.** Der Mitschnitt vom 15:32 enthaelt
     `[direkt] Register beim Start: JOY0DAT=0xF7C8 JOY1DAT=0x0000 CIAAPRA=0x00F4
     POTINP=0xFF02` -- die vier Zugriffe funktionieren auf dieser Maschine also.
     Ungeprueft bleibt, ob der Zaehler beim ZWEIMALIGEN Lesen
     (Stabilitaetstor) stabil genug ist. Die Vorgabe der Warteschleife steht in
     `gui_joy_pause()` (`v4_gui.c`); eine stabile Abtastung entsteht nur, wenn
     beide Abtastungen denselben Wert liefern, sonst bleibt der letzte gueltige
     stehen und das Urteil lautet "-- (noch keine stabile Abtastung)".
   * **`JOY0DAT` ist buchstaeblich der Zaehler von Buchse 0 -- und auf einem
     Amiga ist Buchse 0 die MAUS.** Der Wert `0xF7C8` im Mitschnitt dekodiert
     nach der Tabelle in `v4_joyport.h` als `LINKS` auf Buchse 0, obwohl am
     I2C-Wert nichts gezogen ist. Die Ursache ist keine Fehldeutung, sondern die
     Hardware: der Apollo-Kern nennt `$DFF00A`/`$DFF00B`
     `APOLLO_MOUSE_GET_Y`/`_X` (`Projects/ApolloLib/ApolloCrossDev_Defines.h`),
     und `$DFF00C` ist der Zaehler von Buchse 1 -- genau dort hat der Auftrag
     die Aenderung beim Druecken gemessen. Solange eine Maus an Buchse 0 haengt
     (bzw. die V4 die Maus dort abbildet), zeigt die Direktspalte von Buchse 0
     deren Bewegung, und **jede Bewegung ist eine echte Aenderung** -- sie
     erzeugt eine Sofortzeile. Im Ruhezustand (Maus nicht bewegt) bleibt es bei
     der einen Zeile je Sekunde. Die Rohwerte stehen in der Anzeige und in jeder
     Vergleichszeile daneben, damit sich das am Geraet entscheiden laesst.
   * **Die Feuerknopf-Belegung ist geraten, nicht gemessen.** `CIAAPRA` Bit 6
   (Buchse 0) und Bit 8/10 in `POTINP` stammen aus dem Auftrag; Bit 7 fuer
   Buchse 1 (`/FIR1`) ist die klassische Amiga-Belegung und am Geraet **nicht**
   geprueft. Deshalb ist die Polaritaet nicht verdrahtet (Grundzustand vom
   Start), und deshalb stehen die Rohwerte immer daneben. Wird ein Knopf schon
   beim Start gehalten, ist er der Grundzustand und erscheint beim Loslassen als
   "gedrueckt" -- der bewusst in Kauf genommene Preis dafuer, die Polaritaet
   nicht zu raten. `BTN3` (Pin 5) bleibt ohne direkte Quelle.
   * **`VOR`/`ZURUECK` folgen nicht woertlich der Formel des Auftrags.** Die
   Begruendung samt Mitschnitt steht in `v4_joyport.h` und im README; festge-
   nagelt ist sie in `tests/test_joyport.c`. Sollte sich am Geraet zeigen, dass
   die Formel doch anders gemeint war, ist das genau EINE Zeile in
   `v4_joy_richtungen()` -- die Rohwerte in der Anzeige und auf dem UART sagen,
   welche.
   * **Diagonalen innerhalb einer Achse sind direkt nicht sichtbar** (`unten+
   rechts` landet auf demselben Zaehlerwert wie `rechts` allein). Solche Faelle
   melden `ABWEICHUNG`, obwohl die Verkabelung in Ordnung ist. Das ist eine
   Grenze der Hardware, nicht der Anzeige.
1. **Keine Hardware-Verifikation.** Auf einer echten V4 wurde weder die Konsole
   noch die MUI-Oberflaeche gestartet. Ungeprueft bleibt insbesondere die
   tatsaechliche Verzoegerung von `v4_plat_delay_us` (R3) und damit die
   Wartezeit vor dem 128-Byte-Read. Aus dem Audio-Projekt ist bekannt, dass der
   ESP32 im Feld deutlich mehr als 2000 us brauchte; dafuer gibt es `-w <us>` in
   der Konsole und `M.t_wait_us` als Stellschraube. Ungeprueft ist auch die
   Genauigkeit von `v4_plat_now_us()` (`TR_GETSYSTIME`): sie geht nur in die
   Daueranzeige der Debugzeilen ein, nicht in das Protokoll.
2. **Der Big-Endian-Lauf fehlt** (`make test-m68k` meldet SKIP). Ersatzweise
   prueft `make asm` den erzeugten m68k-Code: die Frame-Bauer und die Helfer
   greifen ausschliesslich byteweise zu. Der Mock/Test-Lauf auf Little Endian
   kann einen Byte-Order-Fehler nicht finden.
3. **Die Pegel im Test und in der Demo sind simuliert.** Dass die Anzeige einen
   echten fehlenden Pull-up erkennt, ist nur mit echter Hardware zu sehen; der
   Mock stellt den Fall absichtlich her (`set_port_level`,
   `V4_DEMO_LEVEL0/1`, `legacy_input10`).
4. **Der Mock ist nicht der ESP32.** Er bildet die Regeln nach, die das
   Protokoll tragen; Timing, Scanverhalten und Fehlerbilder des echten
   Bluetooth-Stacks sind nur grob nachgebildet (`connect_rounds`, `scan_delay`).
   Insbesondere bildet er **kein Haengen** von `SendI2C` nach: der Fall "der
   Buszugriff steckt in `i2c.library`" ist gegen den Mock nicht darstellbar.
5. **Was passiert, wenn der ESP32 mitten im Betrieb wegfaellt** (Stecker
   gezogen, gerade geflasht): erwartet wird, dass der Aufruf in `V4_ERR_LINK`
   laeuft, die Anzeige auf "ESP32 antwortet nicht" umschaltet, die letzten Werte
   stehen laesst und mit wachsendem Abstand weiterversucht. Ob `i2c.library` in
   diesem Zustand sofort mit `I2C_NO_REPLY` zurueckkommt oder lange in ihren
   Bit-Bang-Schleifen bleibt, ist **nicht gemessen**. Im zweiten Fall steht das
   Fenster so lange still -- das ist der bewusst in Kauf genommene Preis der
   Direktaufrufe (die frueher dafuer gebaute eigene Aufgabe hat den Start des
   Programms auf der V4 verhindert und ist deshalb wieder entfallen).
6. **Die Fenstergroesse auf kleinen Schirmen ist eine Annahme.** Die Begrenzung
   auf den Schirm (640x360 -> 624x336) ist gerechnet, nicht auf der V4 gesehen:
   ob MUI mit 640x400 auf einem 640x360-Schirm wirklich Probleme macht, ist
   **nicht gemessen**. Wer es wissen will, startet einmal mit `-w 640 -h 400`
   (dann wird nicht begrenzt) und einmal ohne -- die Zeile `[fenster] ...`
   nennt in beiden Faellen die tatsaechliche Groesse. Der Rand von 16x24 Pixeln
   (`GUI_SCHIRM_RAND_W/H`) ist die Schaetzung fuer Rahmen und Titelleiste.
7. **`make upload` ist in dieser Umgebung nicht durchgelaufen**: `AMIGAHOST`
   zeigt auf `192.168.178.50`, `acp` meldete "Failed to connect to host". Die
   Pakete sind gebaut und lokal bereitgestellt, auf die V4 wurde nichts
   geschrieben.
8. **Die Eingangsverteilung (`fw_ver` 4) ist nur gegen den Mock geprueft.** Der
   echte ESP32 wurde nicht angesprochen: `GET_PORT_CONFIG`,
   `SET_PORT_ROUTING` und `SET_DEVICE_PORT` sind in `tests/test_master.c` gegen
   den Mock gefahren (inklusive Doppelbelegung -> `BAD_ARG` ohne Aenderung,
   falsches Echo -> `V4_ERR_FRAME` und einer Firmware mit `fw_ver` 3 ohne die
   neuen Faehigkeitsbits). Dass die echte Firmware `fw_ver = 4` meldet und die
   Verteilung im NVS haelt, steht in `V4_ESP32_BT_Joy_BP32/docs/PROTOKOLL.md`
   und ist **nicht** am Geraet nachgemessen. Ebenso ungemessen: ob ein
   verbundenes Geraet beim `SET_DEVICE_PORT` wirklich sofort umgehaengt wird
   (die Doku sagt "sofort, wenn die Zielbuchse frei ist").
9. **Die neue Konfigurationsseite ist uebersetzt und baumgeprueft, aber nicht
   auf der V4 geoeffnet.** Geprueft sind: der Amiga-Build mit `-Werror`, der
   MUI-Baumpruefer (`make mui-lint`, inkl. Selbsttest der Seitengruppe) und die
   Groessenrechnung der Seitengruppe laut `MUI_Group.doc` (Mindestgroesse =
   Maximum ueber die Seiten, die Konfigurationsseite ist kleiner). **Nicht**
   gesehen: dass MUI die Seite auf dem Geraet wirklich so legt. Der
   Umschalter sitzt neben der dreizeiligen Controller-Zeile, damit er keine
   zusaetzliche Hoehe kostet -- das Fenster soll so hoch bleiben wie bisher.
10. **Die Geraetezuordnung ist ueber I2C nicht lesbar.** `DEV_GET` liefert nur
    Name und Adresse, `GET_PORT_CONFIG` nur die Buchsenfelder. Konsole und
    Oberflaeche zeigen deshalb, was in **dieser Sitzung** per
    `SET_DEVICE_PORT` gesetzt und bestaetigt wurde; nach einem Neustart des
    Programms ist die Anzeige wieder leer, obwohl die Zuordnung im NVS des
    ESP32 steht. Ein "Loesen" einer Zuordnung gibt es nicht.
