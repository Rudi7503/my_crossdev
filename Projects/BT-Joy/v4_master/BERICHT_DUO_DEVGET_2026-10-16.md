# Bericht 3: duo und DEV_GET-Layout (fw_ver 6)

Stand: 16.10.2026. Projekt: `Projects/BT-Joy/v4_master` (V4-Seite).
Nichts wurde hochgeladen; die Uebertragung auf die V4 macht der Auftraggeber.
Am ESP32-Projekt (`V4_ESP32_BT_Joy_BP32/`) wurde **nichts geaendert** (nur
gelesen) -- die Nummern unten sind woertlich von dort uebernommen.

Die Nummern stammen aus `V4_ESP32_BT_Joy_BP32/main/v4_proto.h` (dort
nachgelesen und gegen unsere `#define` gestellt):

| | |
|---|---|
| `GET_DUO` / `SET_DUO` | **0x0D / 0x0E** |
| Werte | **0 = aus**, **1 = Port 0 Maus / Port 1 Joystick**, **2 = umgekehrt** |
| Faehigkeitsbit | **0x0800** (`V4P_CAP_DUO`) |
| `DEV_GET` Verbunden-Byte | Faehigkeitsbit **0x1000** (`V4P_CAP_DEV_STATE`) |
| `V4P_FW_VERSION` | **0x06** (Rahmen und `proto_ver` unveraendert) |

---

## 1. duo: EIN Controller auf beiden Buchsen

### Was duo ist -- und was nicht (Belegstelle)

`V4_ESP32_BT_Joy_BP32/main/v4_platform.c` (Zeile 269 ff.):

```
duo: EIN Controller auf BEIDEN Buchsen
... Deshalb legt duo die eine Buchse auf MAUS und die andere auf JOYSTICK.
```

duo setzt **nur die Betriebsarten**. Die Geraetezuordnung wird **nicht**
angefasst: ein Geraet hat weiterhin genau eine Buchse
(`v4_devices.c`, `v4_devices_zuordnen()`, Zeile 413 ff.: `d->port = port;`), und
wenn duo endet, gilt sie sofort wieder. Wer dieselbe BDA beiden Buchsen
zuweist, **verschiebt** sie nur -- genau der Fall aus Bericht 2.

### Protokoll (`fw_ver` 6)

`v4_proto.h` / `v4_proto.c`:

* `V4P_FW_VER 6`, neu `V4P_FW_VER_DUO 6` und `V4P_FW_VER_DEV_STATE 6`.
* `V4P_CAP_DUO 0x0800`, `V4P_CAP_DEV_STATE 0x1000`; `V4P_CAP_*`-Klartext
  (`dbg_caps_text`, `caps_text` der Oberflaeche) nennt beide.
* `V4P_CMD_GET_DUO 0x0D`, `V4P_CMD_SET_DUO 0x0E`, `V4P_DUO_OFF_STATE 0`,
  `V4P_DUO_PAYLOAD_LEN 1`, `V4P_DUO_OFF/MOUSE_PORT0/MOUSE_PORT1 0/1/2`.
* Codec `v4p_enc_duo()` / `v4p_dec_duo()`; `v4p_duo_valid()` prueft **nur**
  0/1/2; `v4p_duo_name()`/`v4p_duo_text()` liefern den Klartext
  (`Port 0 Maus, Port 1 Joystick` bzw. `Port 1 Maus, Port 0 Joystick`).
  Der Decoder prueft den Wert ABSICHTLICH nicht -- so kann eine Anzeige auch
  einen unbekannten Wert ehrlich als `unbekannt (n)` zeigen.
* Rahmen und `proto_ver` bleiben unveraendert; `v4_check_info()` prueft
  weiterhin nur `proto_ver` und die Rahmengroessen. **Verbindlich ist das
  Faehigkeitsbit**, nicht die Versionsnummer.

### Master (`v4_master.h/.c`)

* `v4_get_duo(m, &duo)`: 1 Byte; ein Wert ausserhalb 0/1/2 ergibt
  `V4_ERR_FRAME` (lieber ein Fehler als eine erfundene Anzeige).
* `v4_set_duo(m, duo)`: prueft **lokal** (nur 0/1/2; sonst `V4P_ST_BAD_ARG`
  **ohne** Busrahmen) und prueft danach das **Echo** Byte fuer Byte
  (`V4_ERR_FRAME` bei Abweichung). `BAD_ARG` des Slaves kommt unveraendert an.
* Der Master merkt sich die zuletzt gemeldeten Faehigkeiten
  (`v4_master_t.caps`, gesetzt in `v4_info_common()`), weil das DEV_GET-Layout
  davon abhaengt (Punkt 2).

### Zustandsblock

`v4_state.h/.c`: `duo` + `duo_valid` (wie `mousecfg`), `v4_state_put_duo()`.
`duo_valid` bleibt 0, solange keine gueltige Antwort vorliegt -- der Schalter
zeigt dann `?` statt `aus`.

### Oberflaeche (`v4_gui.c`)

* **Seite `Ports`, neue Zeile `duo`**: Auswahlfeld
  (`aus` / `Port 0 Maus` / `Port 1 Maus`, Index = Wert) und daneben der
  Klartext (`aus (jeder Controller seine Buchse)` bzw.
  `Port 0 Maus, Port 1 Joystick`; ohne Faehigkeitsbit
  `nicht verfuegbar (fw 6)`, ohne Antwort `?`).
* Ohne `V4P_CAP_DUO` ist das Feld **gesperrt** und es wird **nichts** gesendet
  (`s_duo_ok`, wie bei den Maus-Reglern).
* **Bei aktivem duo wird der Begleiter-Port gekennzeichnet und gesperrt:**
  * Begleiter ist der Port, der **nicht** die gemerkte Buchse des einen
    Controllers ist (`gui_duo_begleiter()`, ueber `v4_state_devport_at()`);
    ist keine eindeutige Zuordnung bekannt, ist es der Port mit der zweiten
    Rolle (bei duo 1 also Port 1).
  * In der Klartextzeile steht dort `Port N: duo: Maus` bzw. `duo: Joystick`
    statt eines Geraetenamens, und das **Geraete-Auswahlfeld dieses Ports ist
    gesperrt** (`MUIA_Disabled`) -- eine Auswahl waere eine Luege, es bedient
    ihn derselbe Controller.
  * Der Hinweis sagt: `duo = EIN Controller auf beiden Ports; es setzt nur die
    Betriebsarten und sperrt die Geraeteauswahl des duo-Ports (kein zweiter
    Controller).`
* **Immer eine Protokollzeile je Aenderung** (auch ohne `-trace`), mit dem
  zurueckgelesenen Wert:
  `[duo] SET_DUO 1 (Port 0 Maus, Port 1 Joystick) bestaetigt -- vorher 0 (aus),
  gelesen 1 (Port 0 Maus, Port 1 Joystick) -- EIN Controller auf BEIDEN
  Buchsen` bzw. bei Ablehnung
  `[duo] SET_DUO 9: BAD_ARG -- erlaubt sind nur 0 (aus), 1 (Port 0 Maus) und
  2 (Port 1 Maus); die Einstellung bleibt ...`.
  Gelesen wird im **langsamen Takt** (`gui_duo_read(0)`, ereignisgesteuert wie
  bei den Maus-Werten) und **nach jeder Aenderung**.

### Konsole (`v4_console.c`)

* `cfg duo` zeigt den Zustand, `cfg duo aus|0|port0|1|port1|2` setzt ihn
  (lokale Grenzen, Echo, danach GET_DUO als Nachweis). Fehlt das
  Faehigkeitsbit: Klartext und **kein** Rahmen.
* `cfg` (die Uebersicht) nennt jetzt ebenfalls `duo = ...`.

---

## 2. DEV_GET-Layout: beide Staende lesen

### Der Layoutwechsel

`V4_ESP32_BT_Joy_BP32/main/v4_proto.h` (fw_ver 6):

| Offset | alt (< 6) | neu (6, `V4P_CAP_DEV_STATE`) |
|---|---|---|
| 0 | `idx` | `idx` |
| 1…6 | `bda[6]` | `bda[6]` |
| 7 | `name_len` | **`verbunden`** |
| 8 | `name` | **`name_len`** |
| 9… | -- | **`name`** |

Ohne Anpassung haette die Oberflaeche **Namen als Muell** gelesen -- genau die
eine Stelle, die bricht.

### Aenderung

* `v4_proto.h`: Offsets fuer **beide** Layouts
  (`V4P_DEV_OFF_CONNECTED/NAME_LEN/NAME` = 7/8/9 und
  `V4P_DEV_OFF_NAME_LEN_ALT/NAME_ALT` = 7/8); `v4p_dev_t` hat `int verbunden`
  (`V4P_DEV_VERBUNDEN_UNBEKANNT` = -1, wenn die Nutzlast das Feld nicht hat).
* `v4p_dec_dev(p, len, caps, &dev)`: **ausschliesslich das Faehigkeitsbit**
  `V4P_CAP_DEV_STATE` waehlt das Layout. Aus der Nutzlast allein laesst es sich
  **nicht** ableiten: ein Verbunden-Byte 0 sieht aus wie `name_len` 0 -- dieser
  Fall ist als eigener Test drin.
* `v4p_enc_dev()` schreibt das **neue**, `v4p_enc_dev_alt()` das **alte**
  Layout. Der Mock benutzt beides (Stoerknopf `legacy_fw5`); damit sind beide
  Staende ohne Hardware fahrbar.
* Der **Master** merkt sich die Faehigkeiten (`m->caps`, aus PING/GET_INFO);
  `v4_dev_get()` reicht sie an den Decoder durch. Die **Konsole** liest die
  Liste wie bisher ueber den Master -- kein Aufrufer muss die Offsets kennen.
* `v4_debug.c` merkt sich die Faehigkeiten aus `v4_dbg_info()` fuer die
  Rahmenkurzform, die nur die Nutzlast sieht (`DEV_GET ... verbunden=ja/nein/?`),
  und kennt die Befehlsnamen `GET_DUO`/`SET_DUO`.

### "verbunden" in der Liste -- EINE Wahrheit

* **Die Geraeteliste zeigt weiterhin `GET_STATUS.conn_index`** (live, ohne
  zusaetzliche Abfrage; Index in **genau der** Tabelle, aus der `DEV_GET`
  liest). Das ist die eine Quelle der Liste; die Liste wird nur bei einer
  **Aenderung** des Zustands neu gefuellt (Auswahl bleibt erhalten).
* **Das neue Feld wird dekodiert und in der Rahmenkurzform gezeigt**
  (`verbunden=ja/nein/?`), aber **nicht** parallel in derselben Anzeige --
  also keine zwei Wahrheiten. Der Nutzen des Feldes bleibt: Name **und**
  Zustand in EINER Antwort (fuer Konsole/Diagnose und fuer den Fall, dass
  `GET_STATUS` gerade fehlt).

---

## 3. Nachweise

* **Tests:** 195 Faelle / 1291 Checks, 0 Fehler (vorher 188/1250). Neu:
  * `test_proto.c`: `test_codec_duo()` (Werte, Codec, Klartext),
    `test_codec_dev()` erweitert auf **beide** Layouts, den ambigen Fall
    (`V4P_CAP_DEV_STATE` entscheidet), abgeschnittene Nutzlast und die
    49-Zeichen-Grenze.
  * `test_master.c`: `test_duo()` -- Vorgabe 0, 0/1/2 setzen/lesen,
    unzulaessiger Wert lokal `BAD_ARG` **ohne** Rahmen
    (`v4_mock.writes` unveraendert), `BAD_ARG` des Slaves, falsches Echo ->
    `V4_ERR_FRAME`, und `fw_ver 5` ohne `0x0800` -> `BAD_CMD` **mit richtig
    gelesenem Namen** (altes DEV_GET-Layout).
  * `test_master.c`/`test_state.c`: lesen vor der Geraeteliste PING (die
    Faehigkeiten sind die Voraussetzung fuer das Layout) -- so macht es auch
    die Oberflaeche beim Start.
* **Demo:** jetzt **16** Durchlaeufe (vorher 13):
  * Demo 1 prueft `fw=6` und `Faehigkeiten: 0x1FFF` (inkl. `duo`).
  * Demo 14: `cfg duo` / `cfg duo 1` / `cfg duo 2` -- `cmd=GET_DUO` und
    `cmd=SET_DUO` werden abgesetzt und bestaetigt.
  * Demo 15: `legacy_fw5` -- `fw=5`, `Faehigkeiten: 0x07FF`, **kein**
    `GET_DUO`/`SET_DUO` auf der Leitung, Klartextgrund, und das **alte**
    DEV_GET-Layout liefert trotzdem die richtigen Namen (`8BitDo Pro 2`,
    `Logitech MX Anywhere`).
  * Demo 16: falsches Echo von `SET_DUO` wird als Fehler gemeldet.

---

## 4. Geaenderte Dateien

| Datei | Was |
|---|---|
| `v4_proto.h/.c` | `fw_ver 6`, `V4P_CAP_DUO`/`V4P_CAP_DEV_STATE`, `0x0D`/`0x0E`, duo-Werte und -Codec, `v4p_dev_t.verbunden`, beide DEV_GET-Layouts, `v4p_dec_dev(p,len,caps,&dev)`, `v4p_enc_dev_alt()` |
| `v4_master.h/.c` | `v4_get_duo`/`v4_set_duo` (lokale Pruefung + Echo), `v4_master_t.caps` |
| `v4_state.h/.c` | `duo` + `duo_valid`, `v4_state_put_duo()` |
| `v4_mock.h/.c` | fw_ver 6/caps 0x1FFF, duo-Zustand und -Befehle, DEV_GET in beiden Layouts, Knoepfe `legacy_fw5`/`bad_duo_echo`, `v4_mock_duo()`/`v4_mock_set_duo()` |
| `v4_debug.c` | Faehigkeitsbits im Klartext, Befehlsnamen `GET_DUO`/`SET_DUO`, duo-Kurzform, DEV_GET-Kurzform mit `verbunden=`, gemerkte Faehigkeiten fuer die Layoutwahl |
| `v4_console.c` | `cfg duo [...]`, duo-Zeile in der Uebersicht, Hilfetexte |
| `v4_gui.c` | duo-Zeile auf der Seite `Ports` (Anzeige + Schalter, gesperrt ohne Faehigkeitsbit), Begleiter-Port als `duo: Maus`/`duo: Joystick` gekennzeichnet und **gesperrt**, `[duo]`-Protokollzeilen, duo im langsamen Takt gelesen |
| `tests/*` | neue/erweiterte Faelle (duo, beide DEV_GET-Layouts), Demo-Knoepfe |
| `Makefile` | Demo 1 auf fw 6/0x1FFF, Demos 14-16, Zaehler 16 |
| `README.md` | duo-Abschnitt, DEV_GET-Layout-Tabelle, Register der Nachweise |
| `BERICHT_DUO_DEVGET_2026-10-16.md` | dieser Bericht |

**Kein `Forbid()`**, kein zweiter Faden, I2C im Hauptfaden, Fenster 640x400 +
SizeGadget + `-w`/`-h`, Ruhezustand eine Debugzeile je Sekunde.

---

## 5. `make verify` -- wortwoertliche Ausgabe

```
Portabilitaets-Lint (PROTOCOL_V4_MASTER.md §0/§2)
Streng geprueft (alle Regeln inkl. memcpy-Bann): v4_proto.h v4_proto.c v4_master.h v4_master.c
Mock (alle Regeln ausser memcpy-Bann): v4_mock.c
Plattformschichten (nur Mehrbyte-Integerzeiger): v4_linux_i2c.c v4_amiga_i2c.c
Geprueft: 5463 Codezeilen (ohne Kommentare)
Ergebnis: sauber
Selbsttest des Lints:
  ok: bad_std_cast.c wird erkannt
  ok: bad_typedef_cast.c wird erkannt
  ok: bad_struct_cast.c wird erkannt
  ok: bad_mixed.c wird erkannt
  ok: bad_union.c wird erkannt
  ok: bad_memcpy.c wird erkannt
  ok: bad_bitfield.c wird erkannt
  ok: bad_packed.c wird erkannt
  ok: bad_swap.c wird erkannt
  ok: good.c bleibt sauber
  ok: good_ternary.c bleibt sauber
  ok: good_comment.c bleibt sauber
  ok: good_byte_casts.c bleibt sauber
  ok: Byte-Casts bleiben erlaubt
  ok: bad_typedef_cast.c wird auch mit Allow-Logik erkannt
  ok: bad_mixed.c wird auch mit Allow-Logik erkannt
MUI-Baum: sauber (v4_gui.c)
  Fensterinhalt: VGroup mit 9 Kindern
    - TextObject (0 eigene Kinder)
    - TextObject (0 eigene Kinder)
    - TextObject (0 eigene Kinder)
    - HGroup (2 eigene Kinder)
    - ScrollgroupObject (1 eigene Kinder)
    - HGroup (6 eigene Kinder)
    - HGroup (4 eigene Kinder)
    - ListviewObject (1 eigene Kinder)
    - ListviewObject (1 eigene Kinder)
  Klammerbalance 0, WindowContents genau einmal, jedes Child mit genau einem Objekt, jedes Tag mit genau einem Wert, alle Tags aus der Klasse des Objekts.
Selbsttest des MUI-Baumpruefers (kuenstlicher Baum):
  ok     : unveraenderter Baum bleibt sauber
  ok     : Seitengruppe (MUIA_Group_PageMode) bleibt sauber
MUI-Baum: BEFUND in /tmp/v4mui-selftest-03/v4_gui.c
  Zeile 1: ApplicationObject wird nicht geschlossen ('End' fehlt)
  Zeile 7: die Wurzel ist keine einzelne Anwendung (0 Wurzelknoten)
  ok     : fehlendes End am TextObject
MUI-Baum: BEFUND in /tmp/v4mui-selftest-04/v4_gui.c
  Zeile 29: ueberzaehliges 'End' -- der Baum ist hier schon zu
  ok     : ueberzaehliges End (VGroup zweimal geschlossen)
MUI-Baum: BEFUND in /tmp/v4mui-selftest-05/v4_gui.c
  Zeile 12: Tag MUIA_Text_Contents (Zeile 12) hat keinen Wert bekommen -- stattdessen folgt Tag MUIA_Text_PreParse
  Zeile 13: Tag MUIA_Text_PreParse (Zeile 12) hat keinen Wert bekommen -- stattdessen folgt Tag MUIA_Text_PreParse
  ok     : Tag ohne Wert (MUIA_Text_Contents ohne Inhalt)
MUI-Baum: BEFUND in /tmp/v4mui-selftest-06/v4_gui.c
  Zeile 13: Tag MUIA_Cycle_Entries gehoert nicht zu TextObject (Objekt in Zeile 11)
  Zeile 13: MUIA_Cycle_Entries bekommt eine Zeichenkette ("a|b|c") -- MUI erwartet ein NULL-abgeschlossenes Array von Zeichenketten (STRPTR *, siehe libraries/mui.h und MUI_Cycle.doc)
  ok     : Tag einer fremden Klasse am TextObject
MUI-Baum: BEFUND in /tmp/v4mui-selftest-07/v4_gui.c
  Zeile 12: zwei 'Child' hintereinander (Zeile 11 wartet noch auf sein Objekt)
  ok     : Child ohne Objekt
MUI-Baum: BEFUND in /tmp/v4mui-selftest-08/v4_gui.c
  Zeile 15: HGroup hat kein einziges Kind
  ok     : Gruppe ohne Kinder
MUI-Baum: BEFUND in /tmp/v4mui-selftest-09/v4_gui.c
  Zeile 18: MUIA_Cycle_Entries bekommt eine Zeichenkette ("a|b|c") -- MUI erwartet ein NULL-abgeschlossenes Array von Zeichenketten (STRPTR *, siehe libraries/mui.h und MUI_Cycle.doc)
  ok     : MUIA_Cycle_Entries als Zeichenkette statt NULL-Array
MUI-Baum: BEFUND in /tmp/v4mui-selftest-10/v4_gui.c
  Zeile 7: WindowContents kommt 0 mal vor (erwartet: genau einmal)
  ok     : WindowContents fehlt
MUI-Baum: BEFUND in /tmp/v4mui-selftest-11/v4_gui.c
  Zeile 27: Tag MUIA_Group_PageMode gehoert nicht zu TextObject (Objekt in Zeile 26)
  ok     : MUIA_Group_PageMode am TextObject
Selbsttest: OK
gui-lint: v4_gui.c ohne GET_INPUT und ohne I2C-Vergleich -- sauber

v4_master Testsuite -- Bluetooth-Joystick-Adapter (proto_ver 3)
Wirt: little Endian, sizeof(int)=4 sizeof(long)=8 sizeof(void*)=8
Rahmen: WRITE 32 Byte, READ 128 Byte (Nutzlast 121)
Selbsttest Byte-Order: OK
u16 nativ: 02 01 | Leitung (LE): 02 01
== Rahmenformat, Pruefsumme und Codecs (proto_ver 3) ==
 - CRC-8/SMBUS: Standardpruefwert 0xF4
 - CRC-8: leere Eingabe ist 0x00
 - CRC-8: ein Byte 0x00 -> 0x00, 0x01 -> 0x07
 - WRITE: 32 Byte, Magic 0xA5, cmd/seq/len an 1/2/3
 - WRITE: zu lange Nutzlast wird abgewiesen
 - WRITE: 27 Byte Nutzlast passen genau
 - READ: 128 Byte, Flags an +126, CRC an +127
 - READ: gueltiger Rahmen wird angenommen
 - READ: 64 Byte (Audio-Fassung) wird abgewiesen
 - READ: falsche Magic
 - READ: CRC-Fehler
 - READ: falsche SEQ (verschobener Strom)
 - READ: len 122 ist unzulaessig (Grenze 121)
 - READ: 121 Byte Nutzlast passen genau
 - GET_INFO: 12 Byte, Faehigkeiten als u16 LE ab +10
 - GET_INFO: zu kurze Nutzlast wird abgewiesen
 - GET_INFO: Caps werden nicht mit dem alten Reservefeld verwechselt
 - GET_STATUS: 17 Byte, alle Offsets little endian
 - GET_STATUS: zu kurze Nutzlast wird abgewiesen
 - GET_INPUT: 12 Byte -- Zug, Schritte LE und gemessene Pegel
 - GET_INPUT: 10 Byte (aeltere Fassung) -- Pegel UNBEKANNT
 - GET_INPUT: 11 Byte ist keine gueltige Fassung
 - GET_INPUT: zu kurze Nutzlast wird abgewiesen
 - GET_INPUT: laengere Nutzlast ist vorwaertskompatibel
 - GET_INPUT: Bit 7 gehoert keiner Leitung und wird verworfen
 - Pegel: Ruhe mit Pull-up ist 0x7F, gezogen liest Low
 - Pegel: losgelassen, liest 0 -> fehlender Pull-up
 - Pegel: gezogen, liest High -> elektrisch unmoeglich
 - Pegel: Bit 7 traegt keine Leitung und aendert nichts
 - GET_HID: Name, VID/PID, Reports und Batterie
 - GET_HID: Name laenger als die Nutzlast wird abgewiesen
 - GET_HID: 13 Byte (Kopf unvollstaendig) wird abgewiesen
 - GET_HID: name_len 0 ist gueltig und ergibt ""
 - GET_HID: unmoegliche name_len wird abgewiesen
 - DEV_GET fw6: idx, bda[6], VERBUNDEN, name_len, name
 - DEV_GET fw6: nicht verbunden und 'nicht verbunden' lesbar
 - DEV_GET ALT (fw5): name_len liegt bei 7
 - DEV_GET: dasselbe Byte, beide Layouts -- das Bit entscheidet
 - DEV_GET: abgeschnittene Nutzlast wird abgewiesen
 - DEV_GET: 49 Zeichen (Leitungsgrenze) passen
 - DEV_GET: 50 Zeichen werden auf 49 gekuerzt
 - duo: erlaubte Werte 0/1/2
 - duo: Codec, 1 Byte in beide Richtungen
 - duo: Klartext nennt BEIDE Rollen
 - GET_PORT_MODE: [mode0][mode1]
 - GET_PORT_MODE: unmoegliche Betriebsart wird nicht geraten
 - GET_PORT_MODE: zu kurze Nutzlast wird abgewiesen
 - SET_PORT_MODE: Anfrage und Antwort sind [port][mode]
 - SET_PORT_MODE: port 2 und mode 3 werden abgewiesen
 - SET_PORT_MODE: mode 0xFF (auto) ist erlaubt, GET_PORT_MODE lehnt es ab
 - GET_PORT_CONFIG: 12 Byte, Vorgabe UND wirksame Betriebsart
 - GET_PORT_CONFIG: zu kurz / unmoegliche Werte werden abgewiesen (nicht geraten)
 - SET_PORT_ROUTING: 5 Byte [port][dir][b1][b2][b3]
 - SET_PORT_ROUTING: port 2, unbekannter Code und Doppelbelegung werden abgewiesen
 - SET_DEVICE_PORT: 7 Byte [bda 6][port]
 - Prueffunktionen: dir, mode_soll, taste, Knopfsatz
 - Der Knopfsatz steht in der Reihenfolge Pin 6, Pin 9, Pin 5
 - GET_MOUSE_CFG: 4 Byte [speed0][totzone0][speed1][totzone1]
 - SET_MOUSE_CFG: 3 Byte [port][speed][totzone]
 - Maus-Wertebereiche: Tempo 1..10, Totzone 0..100
 - Maus-Codec: unmoegliche Werte und zu kurze Nutzlast
 - Maus-Einstellungen als Text (mit Einheit)
 - Klartexte der Eingangsverteilung
 - Richtungsmaske als Text
 - v4p_bits_text: nichts gezogen ergibt "-"
 - v4p_bits_text: UP und BTN1
 - v4p_bits_text: alle sieben Leitungen
 - v4p_bits_text: Bit 7 wird ignoriert
 - v4p_bits_text: zu kleiner Puffer endet mit "..."
 - v4p_level_expected gibt es nicht mehr (Pegel sind gemessen)
 - v4p_port_mode_next: aus -> Joystick -> Maus -> aus
 - Klartexte sind nicht leer
== Transaktionskern und Wrapper gegen den Mock ==
 - v4_init: seq 0, t_wait 2000 us, Versuche 4, Zaehler 0
 - v4_transact: NULL-Ziele und zu lange Nutzlast -> V4_ERR_ARG
 - PING: Antwort wird dekodiert, check_info akzeptiert sie
 - GET_INFO: gleiche Nutzlast wie PING
 - v4_check_info: 64-Byte-Firmware wird erkannt
 - GET_STATUS: Zustand, Geraetezahl, Reserviertfelder 0
 - GET_HID: Name, VID/PID, Art, Transport, Reports
 - GET_HID: kein Controller -> kind 0, leerer Name
 - GET_PORT_MODE: Buchse 0 Joystick, Buchse 1 aus
 - GET_INPUT: Zug-Masken, Quadraturzaehler und Pegel
 - GET_INPUT: fehlender Pull-up wird als Widerspruch erkannt
 - GET_INPUT: alte 10-Byte-Firmware -> Pegel unbekannt
 - SET_PORT_MODE: gueltig -> Bestaetigung wird geprueft
 - SET_PORT_MODE: port 2 und mode 3 werden lokal abgewiesen
 - SET_PORT_MODE: falsche Bestaetigung -> V4_ERR_FRAME
 - Buchsen tauschen: Joystick/aus -> aus/Joystick
 - Buchsen tauschen (zweites Mal) stellt den Zustand wieder her
 - RESET: beide Buchsen aus, Verbindung bleibt
 - PING: fw_ver 4 wird angenommen, die neuen Faehigkeitsbits sind da
 - GET_PORT_CONFIG: Vorgaben ab Werk (dir 0x07, Knopfquellen = die drei Gruppen)
 - SET_PORT_ROUTING: gueltige Verteilung wird gesetzt und bestaetigt
 - SET_PORT_ROUTING: doppelte Knopfquelle -> BAD_ARG, NICHTS geaendert, kein Rahmen
 - SET_PORT_ROUTING: unbekannter Code, unbekanntes Richtungsbit, port 2 -> BAD_ARG
 - SET_PORT_ROUTING: falsches Echo -> V4_ERR_FRAME
 - SET_PORT_MODE 0xFF: Vorgabe zurueck auf "nach Geraeteart", wirksame Betriebsart bleibt
 - SET_DEVICE_PORT: BDA -> Buchse, Echo geprueft
 - fw_ver 3 (ohne die neuen Befehle): PING wird ANGENOMMEN, die neuen Befehle melden BAD_CMD
 - Beitrag: Tabelle Variante -> Richtungen und Knopfquellen
 - Beitrag: Richtungsmaske -> Variante (alle acht Masken)
 - Beitrag: Klartext
 - Beitrag analog/digital/beide setzen nur SET_PORT_ROUTING
 - Beitrag "eigene"/unbekannt -> V4_ERR_ARG, kein Rahmen
 - GET_MOUSE_CFG: Vorgaben der Firmware (Tempo 7, Totzone 12)
 - SET_MOUSE_CFG: eine Buchse wird gesetzt, die andere bleibt
 - SET_MOUSE_CFG: unzulaessige Werte lokal -> BAD_ARG, kein Rahmen, nichts geaendert
 - SET_MOUSE_CFG: BAD_ARG des Slaves kommt durch, nichts geaendert
 - SET_MOUSE_CFG: falsches Echo -> V4_ERR_FRAME
 - fw_ver 4 (ohne Maus-Einstellungen): PING wird ANGENOMMEN, 0x0B/0x0C melden BAD_CMD
 - GET_DUO: Vorgabe ist 0 (aus)
 - SET_DUO: 0/1/2 werden gesetzt und gelesen
 - SET_DUO: unzulaessiger Wert lokal -> BAD_ARG, kein Rahmen, nichts geaendert
 - SET_DUO: BAD_ARG des Slaves kommt durch, alt bleibt stehen
 - SET_DUO: falsches Echo -> V4_ERR_FRAME
 - GET_DUO: Wert ausserhalb 0/1/2 ist ein Formatfehler
 - fw_ver 5 (ohne duo): PING wird ANGENOMMEN, 0x0D/0x0E melden BAD_CMD, DEV_GET benutzt das ALTE Layout
 - Oberflaechen-Takt: die I2C-Folge enthaelt KEIN GET_INPUT
 - Zaehler arbeitet: ein ausdrueckliches GET_INPUT zaehlt
 - DEV_COUNT/DEV_GET: drei Geraete mit Namen und Adresse
 - DEV_GET: Index ausserhalb -> NOT_FOUND
 - SCAN_START/SCAN_STOP: Scan laeuft und endet
 - Scan laeuft asynchron: DEV_COUNT bleibt erst leer
 - CONNECT: Index 1 -> CONNECTING, dann CONNECTED
 - CONNECT: unbekannter Index -> NOT_FOUND
 - CONNECT_BDA: bekannte Adresse verbindet, fremde nicht
 - CONNECT_BDA: unbekannte Adresse -> BAD_ARG
 - DISCONNECT und FORGET
 - Bereits verbunden: v4_wait_state findet CONNECTED sofort
 - v4_wait_state: falscher Zielzustand laeuft in den Timeout
 - Unbekannter Befehl (Audio-Code 0x50) -> BAD_CMD
 - Link tot: alle Versuche scheitern -> V4_ERR_LINK
 - Lesen scheitert: Wiederholung, dann Erfolg
 - Halbe Antwort: Master wiederholt und heilt sich
 - Falsche CRC im ersten Rahmen: Master verwirft und wiederholt
 - Jeder zweite Rahmen mit falscher CRC: Befehl kommt durch
 - SEQ wird nicht gespiegelt: Master merkt es und wiederholt
 - len-Feld unzulaessig: Master verwirft den Rahmen
 - BUSY: wird intern wiederholt (2 Runden)
 - Dauer-BUSY: Budget laeuft ab -> V4_ERR_BUSY
 - BAD_CRC-Antwort: Befehl wird mit gleicher SEQ wiederholt
 - Erzwungener Fehlerstatus wird durchgereicht
 - Bus-Reset: naechster Befehl liefert wieder saubere Daten
 - SEQ zaehlt je gueltiger Antwort genau eins weiter
 - SEQ laeuft bei 255 auf 0 ueber
 - Fehlversuch erhoeht die SEQ NICHT
 - Framing-Fehler verlaengern t_wait (Lernen aus Fehlern)
== Zustandshilfe: letzte Werte, Backoff, Klassifikation ==
 - v4_state_init: keine Verbindung, Takt 200 ms, Klartext da
 - Start: PING fuellt die Kopfdaten, Verbindung steht
 - Poll liefert Buchsen, der langsame Durchlauf Status und HID
 - Ausfall: letzte gueltige Werte bleiben stehen, Klartext da
 - Backoff: 0,5 s -> 1 s -> 2 s -> 5 s, danach bleibt 5 s
 - v4_state_rc_is_down: nur echte Verbindungsstoerungen
 - Protokollstatus (BAD_ARG) laesst die Leitung stehen
 - Der erste Ausfall aus dem Ruhezustand meldet keinen Wechsel
 - SET_PORT_MODE und Buchsentausch, als bestaetigt uebernommen
 - Scan: Geraeteliste direkt gelesen und uebernommen
 - GET_PORT_CONFIG uebernommen: mode_soll UND mode_ist getrennt, mode[] traegt die WIRKSAME Betriebsart
 - Der falsche Slave antwortet nicht mit 0xFF in mode_ist
 - Geraetezuordnung: merken, ersetzen, nachlesen
 - GET_MOUSE_CFG uebernommen: Werte beider Buchsen, mousecfg_valid erst nach gueltiger Antwort
 - v4_state_put_mousecfg: NULL ist kein Wert
== Joystick-Anschluesse direkt: Dekodierung, Stabilitaet, Vergleich ==
 - Ruhe: $0000 ergibt keine Richtung, Text ist '-'
 - RECHTS: $0002 und der nachgemessene Wert $0003
 - LINKS: $0300 (nachgemessen) und der Zwischenwert $0200
 - VOR: $0100 ergibt genau VOR (nicht VOR+ZURUECK)
 - ZURUECK: $0001 -- der Wert, den die Auftragsformel verliert
 - Diagonale: je eine Richtung pro Achse, beide Achsen
 - Stabilitaet: zwei gleiche Abtastungen werden uebernommen
 - Verwerfen: ungleiche Abtastungen -- letzter gueltiger Wert bleibt stehen
 - Feuerknoepfe: losgelassen ist losgelassen (Grundzustand)
 - Knopf 1: CIAAPRA Bit 6 (Buchse 0) und Bit 7 (Buchse 1)
 - Knopf 2: POTINP Bit 8 (Buchse 0) und Bit 10 (Buchse 1)
 - Polaritaet: der Grundzustand entscheidet, nicht eine Verdrahtung im Code
 - Feuerleitungen: ungleiche Abtastung wird verworfen, der Richtungszaehler laeuft weiter
 - Registerhaken: ohne Haken Nullen, mit Haken die Attrappe
 - Vergleichszeile: gleiche Werte ergeben OK
 - Vergleichszeile: Abweichung wird ausdruecklich gemeldet
 - Vergleichszeile: Feuerknopf aus CIAAPRA/POTINP erscheint in der Maske
 - Vergleichszeile: ohne I2C-Antwort und ohne stabile Abtastung steht kein Urteil da
 - Vergleichszeile: 10-Byte-Firmware -- keine Zahl, wo keine Messung ist
 - Rohwertzeile: CIAAPRA und POTINP in vier Hexziffern
 - Debugzeile: Praefix [vergleich], beide Buchsen und die Feuerregister in EINER Zeile
 - Anzeigetext: zwei Buchsenzeilen und die Rohwertzeile
 - Direktzeile: nur Register, KEIN I2C-Wert, kein Urteil
 - Direktzeile: leere Buchse zeigt keine erfundene Aktivitaet
 - Protokollzeile: beide Buchsen direkt, KEIN I2C, kein Urteil
 - Protokollzeile: Randfaelle NULL/leerer Puffer
 - Randfaelle: NULL, unbekannte Buchse, kleiner Puffer
== Ausgabeschicht: Lebenszeichen, eine Zeile je Meldung, Sofortmeldung bei Aenderung ==
 - Herzzeile: eine Zeile je Sekunde, Vergleich und Rohwerte mit drin
 - Zeilen-Haken: genau ein Aufruf je Meldung, ohne Umbruch
 - Tastendruck: Aenderung wird sofort gemeldet (frueher nie)
 - Transaktionszeilen: mit -quiet aus, ohne -quiet an
 - Urteil direkt <-> I2C: OK, Abweichung, kein Urteil
 - Rahmenpruefung: geprueft/fehlerhaft/fehlerserie statt letztem Zustand

-----------------------------------------------
Faelle : 195
Checks : 1291
Fehler : 0
-----------------------------------------------
ERGEBNIS: OK
== Testlauf mit ASan + UBSan ==
-----------------------------------------------
Faelle : 195
Checks : 1291
Fehler : 0
-----------------------------------------------
ERGEBNIS: OK
Sanitizer: keine Befunde
== Demo 1: Zustand, Controller, Ports, dann Ende ==
== Demo 2: gezogene Leitungen an beiden Buchsen ==
== Demo 3: Scan, Verbinden, Portmodus, Tauschen ==
== Demo 4: laufende Verbindung und Maus-Controller ==
== Demo 5: Pegel -- Widerspruch und alte Firmware ==
== Demo 6: Fehlerwege und Schalter ==
== Demo 7: toter Link -- die Konsole meldet das und wartet nicht ==
== Demo 8: Debugausgabe -- Transaktionszeilen und -quiet ==
== Demo 9: Eingangsverteilung (fw_ver 4) -- das Abnahmebeispiel ==
== Demo 10: fw_ver 3 -- die V4 nimmt sie an, die neuen Felder bleiben aus ==
== Demo 11: Beitrag (analog/digital/beide) und Maus-Einstellungen ==
== Demo 12: fw_ver 4 -- Maus-Regler nicht verfuegbar, nichts gesendet ==
== Demo 13: falsches Echo von SET_MOUSE_CFG wird nicht geglaubt ==
== Demo 14: duo -- EIN Controller auf beiden Buchsen (fw_ver 6) ==
== Demo 15: fw_ver 5 -- kein duo, ALTES DEV_GET-Layout, Namen stimmen ==
== Demo 16: falsches Echo von SET_DUO wird nicht geglaubt ==
Demo: OK (16 Durchlaeufe, Mock, ohne Hardware)
== 1. Helfer-Probe (v4p_put/get_u16le/u32le) ==
  _probe_put   Instruktionen=16   move.b auf Puffer=4   breite Zugriffe(a[0-46])=0
  _probe_get   Instruktionen=29   move.b auf Puffer=6   breite Zugriffe(a0)=0
== 1b. Gegenprobe: die Muster MUESSEN bekannte Verstoesse finden ==
  ok: breiter Store mit Displacement wird erkannt
  ok: breiter Load mit Displacement wird erkannt
== 2. Echte Frame-Funktionen aus v4_proto.c ==
  -- Frame-Bauer (strikt: keine breiten Stores):
  _v4p_build_write   Instruktionen=79   move.b auf Puffer=11  breite Stores=0
  _v4p_build_read    Instruktionen=106  move.b auf Puffer=14  breite Stores=0
  -- Prueffunktionen (byteweiser Frame-Zugriff strikt, Rest Bericht):
  _v4p_check_read    Instruktionen=101  move.b auf Puffer=9   breite Zugriffe=3 (Ausgabestruktur)
== 3. Nullfuellen per clr.* ist erlaubt (endian-neutral) ==
  clr-Zugriffe in _v4p_build_write: 16
Ergebnis: Frame-Bauer schreiben ausschliesslich byteweise; Helfer byteweise in beide Richtungen
../../../Compilers/GCC-6.50-Latest/bin/m68k-amigaos-gcc -std=gnu99 -Wall -Wextra -Werror -O2 -m68020 -m68881 -noixemul \
    -DREGPARM=4 -DV4_PLAT_AMIGA=1 -I. -I../../I2C-test/i2clib-master/include -I../../ApolloLib \
    -o build/v4_console_amiga v4_proto.c v4_master.c v4_state.c v4_joyport.c v4_debug.c v4_amiga_i2c.c v4_console.c \
    ../../ApolloLib/ApolloCrossDev_Debug.c
Fertig: build/v4_console_amiga (m68k-amigaos)
../../../Compilers/GCC-6.50-Latest/bin/m68k-amigaos-gcc -std=gnu99 -Wall -Wextra -Werror -Wno-pointer-sign -O2 -m68020 -m68881 -noixemul \
    -DREGPARM=4 -DV4_PLAT_AMIGA=1 -I. -I../../I2C-test/i2clib-master/include -I../../ApolloLib \
    -o build/v4_gui_amiga v4_proto.c v4_master.c v4_state.c v4_joyport.c v4_debug.c v4_amiga_i2c.c \
    v4_gui.c \
    ../../ApolloLib/ApolloCrossDev_Debug.c -lmui
Fertig: build/v4_gui_amiga (m68k-amigaos, MUI)

verify: Lint, Tests, Sanitizer, Demo gegen den Mock, MUI-Baumpruefer,
        m68k-Nachweis, Amiga-Konsole und MUI-Oberflaeche sind durch.
EXIT=0
```

---

## 6. Offen / am Geraet zu pruefen

* **duo am Geraet:** Schalter auf der Seite `Ports`, Kennzeichnung/Sperre der
  zweiten Buchse und die `[duo]`-Zeilen sind uebersetzt und baumgeprueft, aber
  **nicht** auf der V4 gesehen. Die Mindesthoehe der Seite `Ports` waechst um
  eine Zeile (duo) auf gezaehlt ~11 Zeilen/4 Auswahlzeilen; die Scrollgruppe
  faengt es ab, falls es bei 640x400 doch nicht reicht -- die exakten Zahlen
  stehen nach dem naechsten Lauf in den `[fenster]`-Zeilen.
* **DEV_GET-Layout:** beide Wege sind getestet (mit/ohne `0x1000`); am Geraet
  ist das neue Layout erst mit der fw_ver-6-Firmware zu sehen.
* **Maus-Nachlauf:** auf der ESP32-Seite behoben (Rest beim Loslassen verworfen,
  gestaute Schritte verworfen statt nachgeholt; Tempo unveraendert). Auf der
  V4-Seite war dafuer nichts zu tun. Kriecht der Zeiger weiter, ist es der
  Ruhewert des Pads gegenueber der Totzone -- dann Totzone groesser stellen
  (`cfg <port> mouse totzone <wert>`, auf der ESP32-Konsole
  `maus <port> totzone <wert>`).
* `make test-m68k` (qemu, Big Endian) bleibt SKIP (Cross-Werkzeuge fehlen).
