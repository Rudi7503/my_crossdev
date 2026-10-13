# Bericht: Absturz der MUI-Oberflaeche und Umbau der Ausgabe

Projekt: `Projects/BT-Joy/v4_master/` (Vampire V4, Bluetooth-Joystick ueber ESP32)
Stand: 14.10.2026
Auftrag: (1) Absturz beheben, (2) Ausgabemenge senken, (3) Vergleich behalten,
(4) kein `Forbid()`, (5) Fenster 640x400 + SizeGadget + `-w`/`-h`, (6) `make verify`
mit Exit 0, (7) nichts hochladen.

---

## 1. Die Ursache des Absturzes -- was belegt ist und was nicht

### 1.1 Der Befund am Geraet

Der Mitschnitt `Projects/BT-Joy/logs/v4_seriell.log` (350 KB, mitgeschrieben am
DebugUART) enthaelt **zwei** Laeufe der Fassung "0.2 (13.10.2026)":

| Zeit        | Lauf                                             | Ende |
|-------------|--------------------------------------------------|------|
| 14:36:05 - 14:45:11 | laeuft, 9 Minuten, sauber beendet (`[ende] ... i2c_zeilen=1665`) | vom Anwender beendet |
| 15:32:31 - ~15:32:32 | stirbt nach 14 Zeilen                        | keine Zeile mehr bis zum Neustart 15:37 |

Der letzte Satz des zweiten Laufs ist

```
15:32:32  EMPFANGEN : [ping] OK: proto=3 fw=3 write=32 read=128 caps=0x00FF
```

Der erste Lauf kennt weder `[direkt] Register beim Start` noch irgendeine
`[vergleich]`-Zeile (nachgezaehlt: `grep -c vergleich` = 0), der zweite hat die
`[direkt]`-Zeile und keine `[vergleich]`-Zeile.

### 1.2 Was daraus folgt (Belegstelle)

`v4_debug.c`, `dbg_senke()` (Zeilen 82-95): eine Zeile geht **zuerst** auf den
DebugUART (`ApolloDebugPutStr`) und **erst danach** in die Logdatei und in den
Zeilen-Haken der Logliste. Eine Zeile, die im Mitschnitt fehlt, ist also gar
nicht entstanden -- der Absturz liegt **zwischen `[ping] OK` und der ersten
Zeile der ersten Busabfrage**:

* `open_libs()` (`v4_gui.c`, main),
* `gui_build()` (Objektbaum),
* `timer_open()`,
* `set(s_live, MUIA_Selected, TRUE)`,
* `set(s_win, MUIA_Window_Open, TRUE)`,
* Anfang von `gui_tick()` bzw. `gui_poll()`.

Die Anzeige (`gui_show_vergleich`) und die Vergleichszeile kommen spaeter -- sie
sind als Ursache ausgeschlossen.

### 1.3 Geprueft: der MUI-Objektbaum ist NICHT die Ursache

Nicht geraten, sondern nachgezaehlt. `tools/check_mui_tree.py` zerlegt den
Aufbau in `gui_build()` an den Kommas der Tiefe 0 -- das ist exakt eine
MUI-Tag-Liste -- und prueft Klammerbalance, `Child`/Objekt, Tag/Wert und
Klassenzugehoerigkeit. Ergebnis:

```
MUI-Baum: sauber (v4_gui.c)
  Fensterinhalt: VGroup mit 10 Kindern
    - TextObject (0 eigene Kinder)
    - TextObject (0 eigene Kinder)
    - TextObject (0 eigene Kinder)
    - TextObject (0 eigene Kinder)
    - HGroup (2 eigene Kinder)
    - TextObject (0 eigene Kinder)
    - HGroup (6 eigene Kinder)
    - HGroup (4 eigene Kinder)
    - ListviewObject (1 eigene Kinder)
    - ListviewObject (1 eigene Kinder)
  Klammerbalance 0, WindowContents genau einmal, jedes Child mit genau einem
  Objekt, jedes Tag mit genau einem Wert, alle Tags aus der Klasse des Objekts.
```

Das neue Feld steht in `v4_gui.c` (Zeilen 1717-1720) als

```c
                Child, s_vergleich = TextObject,
                    MUIA_Text_Contents, (IPTR)"Vergleich direkt/I2C ...",
                    MUIA_Text_PreParse, (IPTR)"\33l",
                End,
```

und ist Zeichen fuer Zeichen dasselbe Muster wie die sechs Textfelder daneben --
die auf dieser V4 gelaufen sind. Der Uebersetzer kann diese Fehlerklasse nicht
finden (ein Tag ohne Wert uebersetzt einwandfrei); deshalb gibt es jetzt den
Pruefer und seinen Selbsttest (`--selftest` erzeugt jeden Fehler einzeln an einem
kuenstlichen Baum und belegt, dass er erkannt wird).

### 1.4 Zweiter Beleg: Byte-Vergleich mit dem letzten auf dieser V4 gelaufenen Programm

`Vampire_V4/wiederherstellen/BT Gamecontroller GUI` (41024 Byte, 13.10. 14:05)
ist dieselbe Datei wie `Vampire_V4/BT-Joy_V4_Upload/v4_gui` (11.10. 17:15) und
traegt noch eine Symboltabelle. `v4_logs/last_v4_gui.log` zeigt, dass dieses
Programm auf der V4 gestartet und ueber den Close-Gadget beendet wurde
("v4_gui beendet"). Ein Vergleich **aller MUI-Attribut-IDs im Maschinencode**
dieses laufenden Programms mit dem jetzigen `build/v4_gui_amiga` ergibt genau
**einen** Unterschied:

```
8042E33D MUIA_Window_SizeGadget      <- nur im jetzigen Programm
```

Alles andere ist identisch: `MUIA_Application_*`, `MUIA_Window_ID`,
`MUIA_Window_Width/Height/Title`, `MUIA_Cycle_Entries`, `MUIA_Weight`,
`MUIA_Listview_List`, `MUIA_Text_Contents`, `MUIA_Text_PreParse`,
`MUIA_List_Quiet`, `MUIM_Notify`, `MUIM_Application_ReturnID`,
`MUIM_Application_NewInput`, `MUIM_List_*`. Ein falsch verschachtelter Baum
haette hier andere Tag-Mengen ergeben. Das neue Textfeld benutzt nur Tags, die
in dem laufenden Programm schon vorkommen.

### 1.5 Die Reihenfolge "Takt vor Fenster" war wirklich falsch

`gui_poll(1)` laeuft **nicht** vor dem Fensteraufbau: der Aufruf steht in
`act_refresh()`, und die wird erst aus der Hauptschleife ueber `ID_REFRESH`
gerufen; `gui_tick()` hat ein `if (!s_gui_up) return;`. Wohl aber stand in
`gui_build()`:

```c
    s_gui_up = 1;       /* ALT: direkt nach dem Bauen des Objektbaums */
```

`s_gui_up` schaltet den Zeilen-Haken `gui_loglist_add()` scharf, und der ruft
`set(s_logl, MUIA_List_Quiet, ...)` und `DoMethod(s_logl, MUIM_List_InsertSingle,
...)`. Das geschah also **bevor** `set(s_win, MUIA_Window_Open, TRUE)` das
Fenster oeffnete: jede Meldung zwischen Baumaufbau und Fensteraufbau haette in
die Liste eines noch nicht offenen Fensters geschrieben. Das ist korrigiert (siehe
Abschnitt 3).

### 1.6 Die Ursache ist NICHT sicher gefunden -- verbleibende Moeglichkeiten

Ohne die verlorene Quellfassung und ohne Debugger auf der V4 laesst sich der
letzte Schritt nicht nachstellen. Es bleiben zwei Moeglichkeiten, beide sind
entschaerft:

1. **Das Oeffnen des Fensters mit SizeGadget auf einem zu kleinen Schirm.**
   Der einzige MUI-Unterschied zur nachweislich gelaufenen Fassung ist
   `MUIA_Window_SizeGadget`, und der Absturz liegt genau am Fensteraufbau. Im
   Mitschnitt waehlt CyberGraphX fuer die Workbench **640x360** (Modus
   `0x10000300`, bei jedem Start: 13:50:47, 14:00:37, 14:06:15, 15:37:03),
   das Fenster ist aber **640x400** -- also 40 Pixel hoeher als der Schirm und
   mit Rahmen breiter. Genau daraus kann MUI beim Oeffnen eines Fensters mit
   Groessensymbol einen Groessenbereich bauen, den es auf diesem Schirm nicht
   geben kann. **Nicht gemessen** ist, dass MUI daran wirklich stirbt; das
   laesst sich nur auf der V4 zeigen. Dagegen getan: die Groesse wird auf den
   Schirm begrenzt (und gemeldet), das Ergebnis von `MUIA_Window_Open` wird
   geprueft, `s_gui_up` kommt erst danach. Mit `-w 640 -h 400` laesst sich die
   Begrenzung umgehen -- dann steht im Log, dass nicht begrenzt wurde.
2. **Ein MUI-internes Problem beim Aufbau** (Allokation, Stack). Dagegen
   sprechen: der Baum ist geprueft (1.3), die Stapelgroesse ist bei **allen**
   Hausprogrammen dieselbe (kein Programm definiert `__stack`, auch nicht das
   laufende Audio-Programm -- nachgesehen mit `m68k-amigaos-nm`), und
   `gui_joy_abtasten()` rechnet nur auf einem Feld fester Groesse.

**Was unabhaengig von der Ursache hilft:** der Mitschnitt verortet den naechsten
Absturz eindeutig. Kommt `[mui] Fenster offen (640x400). Ab hier werden
MUI-Objekte angefasst.`, stirbt es danach im Takt; kommt die Zeile nicht, stirbt
es beim Objektaufbau oder beim Oeffnen. Und im ersten Fall steht die erste
Buszeile (`[i2c] cmd=GET_PORT_MODE`) **vor** der ersten Zeile der neuen
Direktabfrage -- die Direktabtastung ist im Takt hinter die Busabfrage gezogen
worden (vorher stand sie davor, `v4_gui.c` Zeile 949 alt).

---

## 2. Geaenderte Dateien

| Datei | Was |
|-------|-----|
| `v4_gui.c` | Ausgabemenge; `s_gui_up` erst nach dem Oeffnen des Fensters; Ergebnis von `MUIA_Window_Open` wird geprueft; Fenstergroesse wird an den Schirm angepasst (`gui_fenster_an_schirm`); Direktabtastung im Takt hinter die Busabfrage; neue Sofortmeldung bei Aenderung (`gui_joy_aenderungen`); Vergleich wandert in die Herzzeile (`gui_joy_herz_zusatz`); Phasenmarken im Log; Dokumentation |
| `v4_debug.h/.c` | `v4_dbg_herz_zusatz()` (Zusatz fuer DIESELBE Herzzeile); Zeilenpuffer 300 -> 420 (`V4_DBG_ZEILE_MAX`, oeffentlich); **Fehler behoben:** in `v4_dbg_input()` wurde `s_in_alt` VOR der Differenzbildung ueberschrieben -- die Zeile `[taste]` konnte nie erscheinen; Schalter-Doku |
| `v4_joyport.h/.c` | neu `v4_joy_abweichung()` (die EINE Stelle fuer "OK / Abweichung / kein Urteil"), `v4_joy_herz_text()`; `tx_urteil()` benutzt jetzt `v4_joy_abweichung()` statt eigener Rechnung |
| `tests/test_debug.c` | **neu:** Suite fuer die Ausgabeschicht (Herzzeile vollstaendig, ein Aufruf je Meldung, Aenderung vs. Wiederholung, Transaktionszeilen nur wenn bestellt, Urteil direkt/I2C) |
| `tests/test_main.c`, `tests/v4_test.h`, `Makefile` | neue Suite eingebunden; neues Ziel `mui-lint` in `verify` |
| `tools/check_mui_tree.py` | **neu:** Aufbaupruefer des MUI-Objektbaums + Selbsttest |
| `README.md` | Schalter, Ausgabemenge, Fenster/Schirm, Baumpruefer, Nachweis, neuer Abschnitt "Der Absturz vom 13.10.2026" |

Nicht angetastet: `v4_proto.*`, `v4_master.*`, `v4_state.*`, `v4_console.c`,
`v4_amiga_i2c.c`, `v4_mock.*`, das Audio-Schwesterprojekt und
`V4_ESP32_BT_Joy_BP32/` (nur gelesen).

---

## 3. Debug-Zeilen je Sekunde

Vorgabe ohne `-trace`:

| Betrieb | Zeilen je Sekunde | Inhalt |
|---------|-------------------|--------|
| Ruhezustand | **1** | `[herz]`: Taktzaehler, Aufrufe, tx/rx_frames, Wiederholungen, verworfene Rahmen, letzter Status **und** der Vergleich direkt/I2C beider Buchsen mit `CIAAPRA`/`POTINP` |
| je Aenderung | **+1** | sofort eine fertige Zeile: Richtung, Feuerknopf, Abweichung direkt/I2C (`[vergleich] ...`), Buchsenmodus (`[modus]`), Sitzvergabe (`[sitzung]`), Fehler (`[fehler]`), Verbindungsverlust/-erholung (`[verbindung]`), Tastendruck (`[taste]`), Scan (`[scan]`) |
| `-trace` | ~85-100 | zusaetzlich jede Transaktion und jeden Schritt |

Vorher: 10 Transaktionszeilen + 2 Statuszeilen + 1 Herzschlag + 5
`[vergleich]`-Zeilen = **rund 18 Zeilen je Sekunde** mit 150 bis 300 Zeichen --
das war die Zahl aus dem Auftrag. Jetzt ist es **eine** Zeile je Sekunde, und die
enthaelt alles, was man zum Mitlesen braucht. Beim Start kommen einmalig die
Aenderungszeilen dazu (`[modus]`, `[eingang]`, eine `[vergleich]`-Zeile).

Eine fertige Zeile je Meldung bleibt: `dbg_senke()` schreibt jede Zeile mit
**einem** `ApolloDebugPutStr`; die Vergleichszeile bekommt ihren Umbruch im
eigenen Puffer (kein zweiter Aufruf). `Delay()` gibt es auf dem GUI-Weg nicht --
`v4_plat_log_settle()` (die einzige `Delay()`-Stelle, `v4_amiga_i2c.c:146`) wird
nur von der Konsole gerufen.

---

## 4. `make verify` -- wortwoertlich, mit Exit-Code

```
$ make verify
Portabilitaets-Lint (PROTOCOL_V4_MASTER.md §0/§2)
Streng geprueft (alle Regeln inkl. memcpy-Bann): v4_proto.h v4_proto.c v4_master.h v4_master.c
Mock (alle Regeln ausser memcpy-Bann): v4_mock.c
Plattformschichten (nur Mehrbyte-Integerzeiger): v4_linux_i2c.c v4_amiga_i2c.c
Geprueft: 3648 Codezeilen (ohne Kommentare)
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
  Fensterinhalt: VGroup mit 10 Kindern
    - TextObject (0 eigene Kinder)
    - TextObject (0 eigene Kinder)
    - TextObject (0 eigene Kinder)
    - TextObject (0 eigene Kinder)
    - HGroup (2 eigene Kinder)
    - TextObject (0 eigene Kinder)
    - HGroup (6 eigene Kinder)
    - HGroup (4 eigene Kinder)
    - ListviewObject (1 eigene Kinder)
    - ListviewObject (1 eigene Kinder)
  Klammerbalance 0, WindowContents genau einmal, jedes Child mit genau einem Objekt, jedes Tag mit genau einem Wert, alle Tags aus der Klasse des Objekts.
Selbsttest des MUI-Baumpruefers (kuenstlicher Baum):
  ok     : unveraenderter Baum bleibt sauber
MUI-Baum: BEFUND in /tmp/v4mui-selftest-02/v4_gui.c
  Zeile 1: ApplicationObject wird nicht geschlossen ('End' fehlt)
  Zeile 5: die Wurzel ist keine einzelne Anwendung (0 Wurzelknoten)
  ok     : fehlendes End am TextObject
MUI-Baum: BEFUND in /tmp/v4mui-selftest-03/v4_gui.c
  Zeile 28: ueberzaehliges 'End' -- der Baum ist hier schon zu
  ok     : ueberzaehliges End (VGroup zweimal geschlossen)
MUI-Baum: BEFUND in /tmp/v4mui-selftest-04/v4_gui.c
  Zeile 12: Tag MUIA_Text_Contents (Zeile 12) hat keinen Wert bekommen -- stattdessen folgt Tag MUIA_Text_PreParse
  Zeile 13: Tag MUIA_Text_PreParse (Zeile 12) hat keinen Wert bekommen -- stattdessen folgt Tag MUIA_Text_PreParse
  ok     : Tag ohne Wert (MUIA_Text_Contents ohne Inhalt)
MUI-Baum: BEFUND in /tmp/v4mui-selftest-05/v4_gui.c
  Zeile 13: Tag MUIA_Cycle_Entries gehoert nicht zu TextObject (Objekt in Zeile 11)
  ok     : Tag einer fremden Klasse am TextObject
MUI-Baum: BEFUND in /tmp/v4mui-selftest-06/v4_gui.c
  Zeile 12: zwei 'Child' hintereinander (Zeile 11 wartet noch auf sein Objekt)
  ok     : Child ohne Objekt
MUI-Baum: BEFUND in /tmp/v4mui-selftest-07/v4_gui.c
  Zeile 15: HGroup hat kein einziges Kind
  ok     : Gruppe ohne Kinder
MUI-Baum: BEFUND in /tmp/v4mui-selftest-08/v4_gui.c
  Zeile 5: WindowContents kommt 0 mal vor (erwartet: genau einmal)
  ok     : WindowContents fehlt
Selbsttest: OK

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
 - DEV_GET: idx, bda[6], name_len, name
 - DEV_GET: abgeschnittene Nutzlast wird abgewiesen
 - DEV_GET: 49 Zeichen (Leitungsgrenze) passen
 - DEV_GET: 50 Zeichen werden auf 49 gekuerzt
 - GET_PORT_MODE: [mode0][mode1]
 - GET_PORT_MODE: unmoegliche Betriebsart wird nicht geraten
 - GET_PORT_MODE: zu kurze Nutzlast wird abgewiesen
 - SET_PORT_MODE: Anfrage und Antwort sind [port][mode]
 - SET_PORT_MODE: port 2 und mode 3 werden abgewiesen
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
 - Randfaelle: NULL, unbekannte Buchse, kleiner Puffer
== Ausgabeschicht: Lebenszeichen, eine Zeile je Meldung, Sofortmeldung bei Aenderung ==
 - Herzzeile: eine Zeile je Sekunde, Vergleich und Rohwerte mit drin
 - Zeilen-Haken: genau ein Aufruf je Meldung, ohne Umbruch
 - Tastendruck: Aenderung wird sofort gemeldet (frueher nie)
 - Transaktionszeilen: mit -quiet aus, ohne -quiet an
 - Urteil direkt <-> I2C: OK, Abweichung, kein Urteil

-----------------------------------------------
Faelle : 135
Checks : 791
Fehler : 0
-----------------------------------------------
ERGEBNIS: OK
== Testlauf mit ASan + UBSan ==
-----------------------------------------------
Faelle : 135
Checks : 791
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
Demo: OK (8 Durchlaeufe, Mock, ohne Hardware)
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

**Exit-Code: 0** (`make verify; echo EXIT=$?` -> `EXIT=0`)

---

## 5. Offen -- uebersetzt, aber NICHT auf echter Hardware geprueft

* **Die Ursache des Absturzes ist nicht bewiesen** (Abschnitt 1.6). Alles, was
  hier steht, ist uebersetzt und gegen den Mock/Attrappen geprueft; die
  Oberflaeche ist in diesem Durchgang **nicht** auf der V4 gestartet worden.
* **Die Schirmbegrenzung ist gerechnet, nicht gesehen.** Ob MUI mit 640x400 auf
  640x360 wirklich Probleme macht, ist nicht gemessen. Der Rand von 16x24 Pixeln
  (`GUI_SCHIRM_RAND_W/H`) ist die Schaetzung fuer Rahmen und Titelleiste. Mit
  `-w 640 -h 400` wird nicht begrenzt; die Zeile `[fenster] ...` nennt in beiden
  Faellen die tatsaechliche Groesse.
* **`JOY0DAT` ist der Zaehler von Buchse 0 -- und auf einem Amiga ist Buchse 0
  die Maus.** Der Wert `0xF7C8` aus dem Mitschnitt dekodiert nach der Tabelle in
  `v4_joyport.h` als `LINKS` auf Buchse 0, obwohl am I2C-Wert nichts gezogen
  ist. Der Apollo-Kern nennt `$DFF00A`/`$DFF00B`
  `APOLLO_MOUSE_GET_Y`/`_X` (`Projects/ApolloLib/ApolloCrossDev_Defines.h`,
  Zeilen 220-221), und `$DFF00C` ist der Zaehler von Buchse 1 -- genau dort hat
  der Auftrag die Aenderung beim Druecken gemessen. Solange eine Maus an Buchse 0
  haengt, zeigt die Direktspalte von Buchse 0 deren Bewegung, und jede Bewegung
  ist eine echte Aenderung (also eine Sofortzeile). Im Ruhezustand bleibt es bei
  einer Zeile je Sekunde. Die Rohwerte stehen daneben.
* **Die Stabilitaetsschwelle (`gui_joy_pause()`, 200 leere Schleifendurchlaeufe)
  ist nicht am Geraet nachgemessen.** Liefert der Zaehler beim zweimaligen Lesen
  keinen gleichen Wert, bleibt das Urteil bei "-- (noch keine stabile
  Abtastung)". Der Zaehler dafuer steht im Herzschlag (`verworfene_rahmen`) und
  in der Schlusszeile.
* **Feuerknopfbelegung und `VOR`/`ZURUECK`-Formel** bleiben wie im Auftrag
  beschrieben offen (siehe README, "Offene Punkte").
* **`make test-m68k`** (Big Endian unter qemu) meldet weiterhin SKIP, weil
  `m68k-linux-gnu-gcc`/`qemu-m68k` fehlen. Keine bestehende Pruefung wurde
  abgeschwaecht; die beiden Schranken in `tests/test_joyport.c`, die sich auf
  "300" als Zeilenpuffer bezogen, vergleichen jetzt gegen die oeffentliche
  Konstante `V4_DBG_ZEILE_MAX` -- das ist dieselbe Aussage, nur an der echten
  Grenze.

---

## 6. `Forbid()`

**Nein.** Im gesamten Projekt steht kein `Forbid()` und kein `Permit()` im Code;
der Begriff kommt ausschliesslich in Kommentaren vor, die erklaeren, warum er
nicht benutzt wird (`v4_gui.c`, `v4_debug.c`, `v4_joyport.c`, `v4_state.c`,
`v4_master.h`, `v4_amiga_i2c.c`). Ebenso kein `CreateNewProc`, kein `CreateTask`,
kein zweiter Faden: der I2C-Verkehr laeuft direkt im MUI-Task. Nachweis:

```
$ grep -rn "Forbid(\|Permit(\|CreateNewProc\|CreateTask(" *.c *.h tests/*.c | wc -l
21
$ grep -rn "Forbid(\|Permit(\|CreateNewProc\|CreateTask(" *.c *.h tests/*.c \
      | grep -v "^\S*:[0-9]*: *\(\*\|/\*\)" | grep -v "^\S*:[0-9]*: \*"
$ echo $?
1
```
Die 21 Treffer liegen **alle** in Kommentaren (`v4_amiga_i2c.c:17,21`,
`v4_console.c:13,14,20,727`, `v4_debug.c:11`, `v4_gui.c:68,69,72,1944`,
`v4_joyport.c:6,14`, `v4_joyport.h:136`, `v4_master.h:19,26`,
`v4_state.c:7`, `v4_state.h:17,21,26,27`) und erklaeren, warum `Forbid()`
**nicht** benutzt wird. Ohne Kommentare bleibt kein einziger Treffer.
