# Bericht: Keine Controller-Werte mehr ueber I2C -- und der Scan-Befund

Stand: 14.10.2026. Projekt: `Projects/BT-Joy/v4_master` (V4-Seite).
Nichts wurde hochgeladen; die Uebertragung auf die V4 macht der Auftraggeber.
Am ESP32-Projekt (`V4_ESP32_BT_Joy_BP32/`) wurde **nichts geaendert** (nur
gelesen) -- siehe Abschnitt 3.

Dieser Bericht behandelt zwei Auftraege:

1. **Die Oberflaeche fragt keine Controller-Eingabewerte mehr ueber I2C ab**
   (`GET_INPUT` entfaellt; Quelle sind die Joyport-Register der V4).
2. **"Scan funktioniert nicht"** -- Befund: die Ursache liegt in der
   **Firmware**, nicht in der Oberflaeche. Die Oberflaeche wurde trotzdem
   robuster (laengeres Fenster, Nachfassen, Diagnose in der Meldung).

---

## 1. Keine Controller-Werte mehr ueber I2C

### 1.1 Was genau weggefallen ist

| Weg | vorher | jetzt |
|---|---|---|
| `GET_INPUT` (0x06) aus der Oberflaeche | in **jedem** Takt (200 ms) | **entfaellt vollstaendig** |
| Anzeige der ESP32-Sicht (`state0/state1`, `level0/level1`) im Fenster | vorhanden | weg (war schon am 14.10. entfernt) |
| Vergleich "direkt <-> I2C" im Fenster | vorhanden | weg |
| Vergleich "direkt <-> I2C" im **Protokoll** (`[vergleich]`, `ABWEICHUNG`) | vorhanden | **weg** -- es gibt nichts zu vergleichen |
| Zug/Pegel-Widerspruchspruefung der Oberflaeche | ins Logfeld | weg |
| `[eingang]`-Zeile der Oberflaeche | je Eingangsantwort | weg |
| Startzustand `s_input_frisch`, `S.input` in `v4_gui.c` | benutzt | entfernt |

Die **Konsole** (`v4_console.c`) ist ausdruecklich **nicht** betroffen: sie ist
der Notfallweg und das Diagnosewerkzeug, hat keinen eigenen Registerzugriff und
traegt mehrere Demo-Pruefungen. Der Auftrag nennt "die Oberflaeche"; dort ist
`GET_INPUT` entfallen. Waere `GET_INPUT` auch in der Konsole entfallen, haetten
die bestehenden Pruefungen (Zug/Pegel, Widerspruch, 10-Byte-Rueckfall)
abgeschwaecht werden muessen -- genau das ist nicht erlaubt.

### 1.2 Welche I2C-Abfragen bleiben -- und in welchem Takt

**In jedem Takt (200 ms), OHNE Bus:** die Register der V4 (`JOY0DAT`/`JOY1DAT`,
`CIAAPRA`/`POTINP`) ueber `v4_joyport.c`. Das ist die Eingangsquelle.

**Im langsamen Takt (jeder fuenfte, rund 1 s), ueber I2C:**

| Befehl | wofuer |
|---|---|
| `GET_STATUS` | Verbindungszustand/Scan -- sonst weiss die Oberflaeche nicht, ob ein Controller da ist |
| `GET_PORT_MODE` | wirksame Betriebsart je Buchse |
| `GET_HID` | Name, VID/PID, Art, Transport, Batterie (Geraeteinfo, kein Eingabewert) |
| `GET_PORT_CONFIG` | Geraet/Vorgabe/Richtungen/Knopfquellen |
| `GET_MOUSE_CFG` | Tempo/Totzone je Buchse |

**Alle drei Takte, nur waehrend einer Suche:** `DEV_COUNT` (bei einem Treffer
dann `DEV_GET`).

**Auf Knopfdruck:** alle setzenden Befehle (`SET_PORT_ROUTING`,
`SET_PORT_MODE`, `SET_MOUSE_CFG`, `SET_DEVICE_PORT`, `SCAN_START`, `CONNECT`
usw.).

**Nebeneffekt (ausdruecklich):** vorher liefen `GET_PORT_MODE` **und**
`GET_INPUT` in jedem Takt -- rund **zehn** Transaktionen je Sekunde, und
`GET_INPUT` war davon die haeufigste. Jetzt sind es im Ruhezustand **fuenf** je
Sekunde, und keine davon traegt Controller-Werte. Der Bus ist damit deutlich
ruhiger.

### 1.3 Wie die neue Herzzeile aussieht

Eine Zeile je Sekunde, mit den direkten Registerwerten beider Buchsen -- kein
Vergleich, kein Urteil:

```
[herz] takt=12s aufrufe=144 tx_frames=144 rx_frames=144 wiederholungen=0 verworfene_rahmen=0 geprueft=144 fehlerhaft=0 fehlerserie=0 letzter_status=OK | Buchse0 direkt=0x00 - stabil=ja roh=0x085C | Buchse1 direkt=0x18 RECHTS+BTN1 stabil=ja roh=0x0002 | CIAAPRA=0x00F4 POTINP=0xFF02
```

Bei **Aenderung** geht sofort eine eigene Zeile heraus:

```
[direkt] Buchse0 direkt=0x04 VOR stabil=ja roh=0x085C | Buchse1 direkt=0x00 - stabil=ja roh=0x0000 | CIAAPRA=0x00F4 POTINP=0xFF02
```

Beides kommt aus `v4_joy_direkt_protokoll_text()` (neu in `v4_joyport.c`) -- die
eine Stelle, die die Protokoll-/Herzzeile baut. Die Zeile ist auch im Test
festgenagelt (kein `I2C`, kein `state=`, kein `ABWEICHUNG`).

### 1.4 Wie es im Fenster aussieht

Unveraendert gegenueber dem letzten Stand: je Buchse die Direktwerte
(Leitung/Pin/Pegel/direkt), darunter die gezogenen Leitungen, der Rohzaehler und
die Rohregister. Der Hinweis unter den Rohregistern lautet jetzt:

```
Direkt aus den Registern der V4 (JOY0DAT/JOY1DAT, CIAAPRA/POTINP) -- die einzige Eingangsquelle.
```

Die **wirksame Betriebsart** bleibt daneben stehen (`Modus ... (ESP32)`): sie
ist eine Einstellung, kein Eingabewert.

### 1.5 Pruefungen dazu (nichts abgeschwaecht)

* **`make gui-lint` (neu, Teil von `make verify`):** liest `v4_gui.c` und
  schlaegt fehl, sobald dort `v4_get_input`, `v4_joy_debug_text`,
  `v4_joy_herz_text`, `v4_joy_vergleich_text`, `v4_joy_port_text`,
  `v4_joy_abweichung`, `s_input_frisch` oder `ABWEICHUNG` steht. Die Wache
  wurde mit einem absichtlichen Verstoss geprueft (sie meldet ihn und bricht
  ab) und danach wieder entfernt.
* **`v4_mock.input_reads` (neu):** zaehlt `GET_INPUT`-Aufrufe. Der neue
  Testfall in `tests/test_master.c` faehrt genau die I2C-Folge des langsamen
  Takts (`GET_STATUS`, `GET_PORT_MODE`, `GET_HID`, `GET_PORT_CONFIG`,
  `GET_MOUSE_CFG`) und prueft, dass `input_reads == 0` bleibt und der letzte
  Befehl nicht `GET_INPUT` ist; danach belegt ein ausdrueckliches
  `v4_get_input`, dass der Zaehler arbeitet.
* **`v4_joyport`:** die reinen Vergleichsfunktionen (`v4_joy_abweichung`,
  `v4_joy_port_text`, `v4_joy_debug_text`, `v4_joy_herz_text`,
  `v4_joy_vergleich_text`) bleiben als getestete Rechenfunktionen erhalten, aber
  **kein Frontend ruft sie mehr**; sie erzeugen keine Ausgabe. Die neuen
  Funktionen `v4_joy_direkt_text` (Fenster) und `v4_joy_direkt_protokoll_text`
  (Protokoll) sind die einzigen, die die Oberflaeche benutzt. Damit geht keine
  bestehende Testabdeckung verloren.

---

## 2. Scan: was die Oberflaeche robuster macht

**Ursache des Befunds ist die Firmware** (Abschnitt 3). Die Oberflaeche hatte
zusaetzlich zwei echte Schwaechen, die den Befund verstaerkt haben:

1. **Das Suchfenster war zu kurz.** Vorher 40 Takte (8 s nominal). Eine
   BluePad32-Suche braucht deutlich laenger (im Mitschnitt vom 13.10.2026 lagen
   25 s zwischen `SCAN_START` und der ersten Statusantwort). Jetzt:
   **150 Takte (30 s)** Regelwartezeit, **300 Takte (60 s)** harte Obergrenze.
2. **Kein Nachfassen.** Die Oberflaeche behauptete "keine Geraete gefunden",
   sobald ihr Fenster ablief -- auch dann, wenn `GET_STATUS` noch "Scan laeuft"
   meldete. Jetzt wird **nachgefasst**, solange die Gegenstelle sucht (bis zur
   harten Obergrenze), und die Meldung nennt den Stand der Gegenstelle:
   `[scan] keine Geraete gefunden (Scan der Gegenstelle beendet, Generation 0).`
3. **Dauerbetrieb entfernt.** `SCAN_START` wurde mit `continuous = 1` gesendet;
   der ESP32 scannte nach **einem** Knopfdruck endlos weiter (Funk/Strom), und
   `scan_active` taugte nicht als "fertig"-Signal. Jetzt **ein Fenster**
   (`continuous = 0`), die Fensterlaenge wird aus der Wartezeit gerechnet
   (24 Einheiten = 31 s), und beim Aufgeben wird `SCAN_STOP` geschickt.
4. **Bus ruhiger:** `DEV_COUNT` wird waehrend der Suche nur **alle drei Takte**
   gefragt (rund 1,7-mal je Sekunde statt 5-mal).

**Das behebt den Befund NICHT** -- solange die Firmware das gefundene Geraet
nicht in ihre Tabelle eintraegt, liefert `DEV_COUNT` weiter 0, und die
Oberflaeche meldet (dann zu Recht) "keine Geraete gefunden". Die Meldung ist
jetzt nur ehrlicher und laenger geduldig.

---

## 3. Der Scan-Befund: die Ursache liegt in der FIRMWARE

### 3.1 Belege aus den Mitschriften

`ApolloCrossDev/Projects/BT-Joy/logs/v4_seriell.log` (V4-Seite) -- die
Oberflaeche **schickt den Scan und fragt nach**:

```
21:04:08  [scan] Scan gestartet (bis zu 8 s) -- die Liste fuellt sich waehrend der Live-Aktualisierung
21:04:09  [status] Zustand 1 (SCANNING), Verbindung keine, Scan laeuft, Geraete 0, Scan-Generation 0
21:04:09  [herz] takt=22s aufrufe=288 ... letzter_status=OK (DEV_COUNT) |  ...
21:04:21  [scan] keine Geraete gefunden. Ist der Controller eingeschaltet und in Reichweite?
```

`V4_ESP32_BT_Joy/logs/monitor_session.log` (ESP32-Seite) -- der ESP32 **findet**
das Pad und traegt es trotzdem **nicht** ein:

```
I (553137) bt_mgr: DIAG merge: num=1 liste=0x3ffbb1c4
I (553138) bt_mgr: DIAG fund 0: e4:17:d8:5f:e6:c9 transport=1 usage=0 name=8BitDo 64 BT
W (553140) bt_mgr: DIAG tm: vorher=0 nachher=0 idx=255
I (553151) bt_mgr: Scan: 1 Fund(e), Tabelle jetzt 0
```

### 3.2 Die Zeile, die es belegt

`W (553140) bt_mgr: DIAG tm: vorher=0 nachher=0 idx=255` zusammen mit
`Scan: 1 Fund(e), Tabelle jetzt 0`: **ein Fund, Tabelle leer, idx=255.**

### 3.3 Die Ursache im Quelltext (nur gelesen, nicht geaendert)

`V4_ESP32_BT_Joy_BP32/main/bt_manager.c`:

```c
static int find_index_by_bda(const uint8_t *bda)      /* Zeile 150 */
{
    ...
    return V4P_BT_NO_INDEX;                            /* Zeile 158 */
}
```

`V4P_BT_NO_INDEX` ist in `main/v4_proto.h:229` als **`0xFF`** definiert -- die
Funktion liefert also **255**, nicht `-1`. Der Aufrufer in `table_merge()`
prueft aber:

```c
int idx = find_index_by_bda(bda);

if (idx < 0) {                    /* Zeile 202 -- bei 255 FALSCH */
    ...                           /* neues Geraet eintragen */
    idx = s_dev_count++;
    ...
}
s_devs[idx].transport = ...;      /* idx == 255 */
```

**255 ist nicht kleiner als 0.** Damit passiert zweierlei:

1. Der Zweig fuer ein **neues** Geraet wird uebersprungen: `s_dev_count` bleibt
   **0** -> `DEV_COUNT` liefert 0, `GET_STATUS` meldet `Geraete 0` und
   `Scan-Generation 0`. Die Oberflaeche hat also recht.
2. Danach wird `s_devs[255]` beschrieben, obwohl `s_devs` nur
   `BT_MGR_MAX_DEVICES = 8` Eintraege hat -- ein **Schreibzugriff ausserhalb
   des Feldes** in den .bss (Speicherverfaelschung). Das ist der ernstere Teil
   des Befunds.

**Warum die Oberflaeche nicht die Ursache ist:** sie schickt `SCAN_START`
(der ESP32 geht in `Zustand 1 (SCANNING)`), sie fragt `DEV_COUNT` ueber die
ganze Suche ab (`letzter_status=OK (DEV_COUNT)` in jeder Herzzeile), und sie
liest keine Geraete, weil die Tabelle leer ist. Der Weg der Suche ist in
Ordnung; die Antwort ist es nicht.

**Weitere Stelle mit demselben Muster (zur Pruefung):**
`find_index_by_bda_locked()` gibt denselben Wert weiter; die Aufrufer in
`bt_manager.c` (u.a. Zeile 406 und 516) vergleichen ebenfalls `< 0`, und
`bt_mgr_connected_index()` kann dadurch 255 liefern -- was zufaellig wieder
`V4P_NO_INDEX` (`0xFF`) ergibt und deshalb bisher nicht auffiel.

**Vorschlag fuer den Firmware-Fix** (dort umzusetzen, nicht hier): in
`find_index_by_bda()` `-1` liefern **oder** in `table_merge()` auf
`idx == V4P_BT_NO_INDEX` pruefen; zusaetzlich jeden Index vor dem Schreiben
gegen `BT_MGR_MAX_DEVICES` begrenzen.

### 3.4 Antwort auf die vier moeglichen Stellen

1. **Weg der Suche:** in Ordnung. Die Oberflaeche sendet `SCAN_START` (der
   ESP32 geht in `Zustand 1 (SCANNING)`), fragt waehrend der Suche `DEV_COUNT`
   und holt bei einem Treffer `DEV_GET` 0..n-1. Die Frist war zu kurz (8 s) und
   ist jetzt 30 s/60 s -- der Befund bleibt aber auch damit bestehen, weil die
   Antwort 0 ist.
2. **Gesperrte Geraete:** **nicht** die Ursache. Die Sperre verhindert nur das
   automatische Verbinden; `DEV_COUNT`/`DEV_GET` wuerden das Geraet liefern,
   wenn es in der Tabelle staende. Es steht dort nicht -- wegen des
   Index-Fehlers in `table_merge()`. Es ist also **kein** "Ausblenden durch die
   Firmware", sondern ein Eintrag, der nie stattfindet.
3. **Index/Reihenfolge:** in Ordnung. `DEV_GET` wird mit 0 beginnend bis
   `DEV_COUNT-1` gelesen; eine andere Reihenfolge braucht es nicht. Auffaellig
   ist `DIAG fund 0` -- der Fund ist also der erste; nur die Tabelle bleibt
   leer.
4. **Nachfassen:** umgesetzt. Solange `GET_STATUS` "Scan laeuft" meldet, wartet
   die Oberflaeche weiter (bis zur harten Obergrenze) und behauptet nicht
   "keine Geraete".

### 3.5 Was die Oberflaeche NICHT tut

Sie blendet **kein** Geraet aus und umgeht **nichts**: sie zeigt, was
`DEV_COUNT`/`DEV_GET` liefern. Ob "gesperrte" Geraete sichtbar und auswaehlbar
bleiben, haengt an derselben Tabelle -- auch sie bleibt leer, solange der
Eintrag nicht stattfindet. Sobald die Firmware den Index-Fehler behebt, findet
die Oberflaeche das Geraet ohne weitere Aenderung (die Suchfrist ist jetzt
genuegend lang, und die drei Knoepfe/`DEV_GET`-Reihenfolge sind unveraendert).

---

## 4. Geaenderte und neue Dateien

| Datei | Was |
|---|---|
| `v4_gui.c` | `GET_INPUT` entfaellt; `gui_poll` ist jetzt der **langsame Takt** und macht `GET_STATUS`/`GET_PORT_MODE`/`GET_HID`/`GET_PORT_CONFIG`/`GET_MOUSE_CFG`; in den schnellen Takten **kein** I2C. Anzeige/Logzeilen auf `v4_joy_direkt_protokoll_text` umgestellt, Vergleich und Zug/Pegel-Warnungen entfernt, `s_input_frisch`/`s_level_warn_*`/`s_joy_abw_alt` entfernt. Scan: 30 s Fenster, 60 s harte Grenze, Nachfassen bei `scan_active`, `SCAN_STOP`, `DEV_COUNT` alle drei Takte, Diagnose in der "keine Geraete"-Meldung. |
| `v4_joyport.h/.c` | neu `v4_joy_direkt_protokoll_text()` (beide Buchsen, nur Register, kein I2C/Urteil) |
| `v4_mock.h/.c` | Zaehler `input_reads` (GET_INPUT-Aufrufe) |
| `tests/test_master.c` | neu `test_ohne_i2c_eingang` (die I2C-Folge des langsamen Takts enthaelt kein `GET_INPUT`; Zaehler arbeitet) |
| `tests/test_joyport.c` | neu `test_joy_direkt_protokoll` (Protokollzeile ohne `I2C`/`state=`/`level=`/`ABWEICHUNG`/`OK`, Randfaelle) |
| `Makefile` | neues Ziel **`gui-lint`** (Wache ueber die Eingangsquelle) und Aufnahme in `verify` |
| `README.md` | Abschnitt "Geraetesuche (Scan)" mit dem Firmware-Befund, Takt-/I2C-Tabellen, Herzzeilen-Beispiel, Nachweis |
| `BERICHT_OHNE_I2C_EINGANG_UND_SCAN_2026-10-14.md` | dieser Bericht |

**Kein `Forbid()`** -- weder neu noch sonst im Code: alle 15 Treffer des Wortes
sind Prosa. Nach Entfernen der Kommentare ueber alle `.c`/`.h` in `v4_master/`
und `tests/` bleibt **kein** Code-Treffer. Alles laeuft weiter im Hauptfaden
(MUI-Task), I2C direkt, kein zweiter Faden.

---

## 5. `make verify` -- wortwoertliche Ausgabe

```
Portabilitaets-Lint (PROTOCOL_V4_MASTER.md §0/§2)
Streng geprueft (alle Regeln inkl. memcpy-Bann): v4_proto.h v4_proto.c v4_master.h v4_master.c
Mock (alle Regeln ausser memcpy-Bann): v4_mock.c
Plattformschichten (nur Mehrbyte-Integerzeiger): v4_linux_i2c.c v4_amiga_i2c.c
Geprueft: 5127 Codezeilen (ohne Kommentare)
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
    - VGroup (2 eigene Kinder)
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
cc -std=gnu17 -Wall -Wextra -Werror -O2 -I. -o build/v4_tests v4_proto.c v4_master.c v4_state.c v4_joyport.c v4_debug.c v4_mock.c tests/test_main.c tests/test_proto.c tests/test_master.c tests/test_state.c tests/test_joyport.c tests/test_debug.c

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
Faelle : 182
Checks : 1216
Fehler : 0
-----------------------------------------------
ERGEBNIS: OK
cc -std=gnu17 -Wall -Wextra -Werror -O1 -g -fsanitize=address,undefined \
    -fno-omit-frame-pointer -I. -o build/v4_tests_san v4_proto.c v4_master.c v4_state.c v4_joyport.c v4_debug.c v4_mock.c tests/test_main.c tests/test_proto.c tests/test_master.c tests/test_state.c tests/test_joyport.c tests/test_debug.c
== Testlauf mit ASan + UBSan ==
-----------------------------------------------
Faelle : 182
Checks : 1216
Fehler : 0
-----------------------------------------------
ERGEBNIS: OK
Sanitizer: keine Befunde
cc -std=gnu17 -Wall -Wextra -Werror -O2 -I. -DV4_CONSOLE_NO_MAIN -o build/v4_console_demo v4_proto.c v4_master.c v4_state.c v4_joyport.c v4_debug.c v4_mock.c v4_console.c tests/demo_main.c
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
Demo: OK (13 Durchlaeufe, Mock, ohne Hardware)
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

## 6. Was geprueft ist -- und was nicht

**Geprueft (ohne Hardware):**

* Portabilitaets-Lint sauber; **182 Testfaelle / 1216 Checks, 0 Fehler**;
  ASan+UBSan ohne Befund; Demo 13 Durchlaeufe; MUI-Baumpruefer sauber;
  **gui-lint** sauber (inkl. Selbsttest des Pruefers); m68k-Nachweis; beide
  Amiga-Builds mit `-Wall -Wextra -Werror`.
* Die neue Protokollzeile ist im Test festgenagelt (keine I2C-Reste, keine
  `ABWEICHUNG`).
* Die I2C-Folge des langsamen Takts kommt im Mock ohne `GET_INPUT` aus
  (`input_reads == 0`).

**Nur uebersetzt/plausibilisiert, NICHT auf Hardware geprueft:**

* Dass die Oberflaeche auf der V4 tatsaechlich ohne `GET_INPUT` laeuft (der
  Quelltext und die Mock-Folge sind geprueft, das Fenster nicht).
* Die neue Scanfrist (30 s/60 s) am echten BluePad32 -- sie ist aus dem
  Mitschnitt abgeleitet, nicht am Geraet nachgemessen.
* Die Herzzeile auf dem DebugUART (sie passt rechnerisch in den Zeilenpuffer,
  gesehen auf der V4 wurde sie nicht).

**Offen -- in der FIRMWARE zu beheben:** der `0xFF`-Index in
`bt_manager.c/table_merge()` (Abschnitt 3). Erst danach fuellt sich die
Geraeteliste, und die Suche findet das Pad. Die Oberflaeche ist darauf
vorbereitet und meldet bis dahin korrekt "keine Geraete gefunden" mit dem Stand
der Gegenstelle.
