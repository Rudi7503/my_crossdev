# Bericht: Geraete-Varianten (Beitrag), Maus-Einstellungen und die auf die Portdaten reduzierte Anzeige

Stand: 14.10.2026. Projekt: `Projects/BT-Joy/v4_master` (V4-Seite).
Nichts wurde hochgeladen; die Uebertragung auf die V4 macht der Auftraggeber.

Dieser Bericht beschreibt drei zusammenhaengende Aenderungen:

1. **Beitrag je Buchse** (analog / digital / analog+digital) als Bedienhilfe,
2. **Maus-Einstellungen je Buchse** (Tempo und Totzone) nach dem bestaetigten
   Vertrag `GET/SET_MOUSE_CFG`,
3. die **Anzeigeseite zeigt nur noch die direkten Portdaten**; der Vergleich
   direkt <-> I2C wandert ausschliesslich ins Protokoll.

---

## 1. Was gebaut ist

### 1.1 Der "Beitrag" -- drei Varianten je Buchse

Jede Buchse hat auf der Konfigurationsseite ein eigenes Auswahlfeld
**`Beitrag`** direkt unter der Geraeteauswahl. Es gibt vier Eintraege; drei sind
waehlbar, der vierte ist ein reiner Anzeigezustand:

| Eintrag | waehlbar | Bedeutung |
|---|---|---|
| `analog` | ja | Richtungen <- Analog-Stick links, Knoepfe L1/R1/keine -- die **Maus**-Belegung |
| `digital` | ja | Richtungen <- D-Pad, Knoepfe A/B/Start -- die **Joystick**-Belegung |
| `beide` | ja | alle Richtungen (Stick links, Stick rechts, D-Pad), Knoepfe Gruppe 1/2/3 -- zusammen **alle** Tasten |
| `eigene` | nein | die vorhandene Verteilung passt zu **keiner** der drei (z.B. `dir = 0` oder eine Handbelegung); es wird nichts gesendet |

Ein gewaehlter Beitrag setzt **nur** die bestehenden Routing-Werte ueber
`SET_PORT_ROUTING` (0x0A). Es gibt **keinen** neuen Protokollbefehl und **keine**
Firmware-Aenderung dafuer. Der Anzeigezustand `eigene` verhindert, dass das Feld
eine Variante behauptet, die es nicht gibt.

Beide neuen Auswahlfelder benutzen `MUIA_Cycle_Entries` als **statisches,
NULL-abgeschlossenes Zeigerarray** (`gui_beitrag_eintraege`,
`gui_speed_eintraege`) -- keine mit `|` getrennte Zeichenkette. Wie bei den
bestehenden Feldern sichert ein `typedef char ...[... ? 1 : -1]` den
NULL-Abschluss schon zur Uebersetzungszeit ab, und `check_mui_tree.py` prueft
es zusaetzlich.

### 1.2 Zuordnung Variante -> Routingwerte

| Variante | `dir` (Richtungen) | Pin 6 | Pin 9 | Pin 5 |
|---|---|---|---|---|
| `analog`  | `0x01` Stick links | `L1` (5) | `R1` (6) | `keine` (0) |
| `digital` | `0x04` D-Pad | `A` (1) | `B` (2) | `Start` (11) |
| `beide`   | `0x07` Stick links + Stick rechts + D-Pad | `Gruppe 1` (14) | `Gruppe 2` (15) | `Gruppe 3` (16) |

Begruendung der Knopfbelegung:

* **analog**: L1 = Maus links, R1 = Maus rechts; Pin 5 bleibt offen. Das ist
  die Belegung einer Maus-Buchse.
* **digital**: A/B/Start ist die uebliche Joystick-Vorgabe und damit die
  "sinnvolle Vorgabe" aus dem Auftrag.
* **beide**: die drei **bestehenden** Gruppen decken zusammen alle zwoelf
  Tasten ab, die das Protokoll kennt (A,L1,L2,L3 / B,R1,R2,R3 /
  X,Y,Start,Select; Home/Capture liegt in keiner Gruppe). Damit ist "alle
  Knoepfe" ohne eine vierte Leitung oder einen neuen Gruppencode erfuellt. Das
  ist der einzige Fall, in dem die drei Knoepfe **nicht** frei waehlbar sind --
  siehe 1.3.

Die Rueckrichtung (`v4_beitrag_aus_dir()`) ordnet eine **bestehende**
Richtungsmaske wieder einer Variante zu. Verglichen werden nur die Richtungen:
die Knopfquellen sind der "Wunsch" der Variante und duerfen einzeln
nachjustiert werden, ohne dass die Richtungsaussage falsch wird. `0x00` ergibt
`eigene`.

### 1.3 "analog+digital" sperrt die Einzelschalter sichtbar

Bei `beide` sind die drei Richtungsschalter (Stick l / Stick r / D-Pad) und die
drei Knopfquellen-Auswahlfelder **gegenstandslos**. Sie werden deshalb mit
`MUIA_Disabled, TRUE` **gesperrt**; MUI zeichnet ein gesperrtes Gadget mit
seinem Raster ("ghost pattern") und nimmt keine Eingaben mehr an
(`MUI_Area.doc` zu `MUIA_Disabled`: *"Setting this attribute causes a gadget to
become disabled, it gets a ghost pattern and doesn't respond to user input any
longer."*). Beim Wechsel auf `analog` oder `digital` werden sie mit
`MUIA_Disabled, FALSE` wieder bedienbar.

Zusaetzlich nennt der dynamische Hinweis unter der Tabelle den Fall im
Klartext: `Buchse N: analog+digital -- die Einzelschalter (Richtungen und
Knopfquellen) sind gegenstandslos und deshalb ausgegraut.`

**Warum gesperrt und nicht nur ein Hinweis:** der Auftrag laesst beides zu. Die
Sperre ist der ehrlichere Weg -- es gibt dann keine Schalterstellung, die einer
anderen Anzeige widerspricht, und der Anwender kann nichts einstellen, was
sofort wieder verworfen wuerde. Der zusaetzliche Hinweis bleibt, damit die
Sperre nicht wie ein Fehler aussieht.

### 1.4 Maus-Einstellungen je Buchse (fw_ver 5)

Der Vertrag ist so uebernommen, wie ihn die Firmware-Seite am 14.10.2026
gemeldet hat. **Alle Nummern, Laengen und Grenzen stehen ausschliesslich in
`v4_proto.h`**; eine Korrektur ist damit eine Zeile. Der Mock benutzt dieselben
Makros.

| Befehl | Wert | Anfrage -> Antwort |
|---|---|---|
| `GET_MOUSE_CFG` | `0x0B` | -- -> 4 Byte: `speed0`@0, `totzone0`@1, `speed1`@2, `totzone1`@3 |
| `SET_MOUSE_CFG` | `0x0C` | `[port][speed][totzone]` -> Echo (3 Byte); `BAD_ARG` bei port > 1 oder unzulaessigem Wert |
| Faehigkeitsbit | `0x0400` (`V4P_CAP_MOUSE_CFG`) | verbindliche Auskunft, nicht `fw_ver` |
| `V4P_FW_VER` | `5` (vorher 4) | Rahmen und `proto_ver` (3) unveraendert |

* `speed` ist eine **Stufe 1..10** (1 = langsamste, 10 = schnellste, Vorgabe 7).
* `totzone` ist ein **Prozentwert 0..100** (Vorgabe 12). Die Umrechnung
  `totzone_roh = prozent * 512 / 100` macht **ausschliesslich die Firmware**;
  die Oberflaeche zeigt und setzt Prozent und rechnet nichts um.
* `fw_ver 5` wird von `v4_check_info()` **nicht** abgelehnt -- geprueft werden
  dort nur `proto_ver` und die Rahmengroessen. Ob die Befehle da sind, sagt das
  Faehigkeitsbit.

**Regler in der Oberflaeche:** auf der Konfigurationsseite je Buchse eine
eigene Zeile `Maus Tempo` (Cycle `1`..`10`, Index = Stufe-1) und eine Zeile
`Totzone` (Text `NN %` mit `-`/`+`, Schritt 1). Beide liegen in **derselben
Buchsen-Spalte** wie Geraet, Beitrag, Richtungen und Knopfquellen -- alles zu
dieser Buchse steht an einer Stelle.

* Warum **Cycle** fuer Tempo: 10 Werte, ein Klick = eine Stufe = ein
  I2C-Auftrag -- genau das Muster der uebrigen Felder.
* Warum **Text mit `-`/`+`** fuer die Totzone und kein Cycle mit 101
  Eintraegen: ein Auswahlfeld mit 101 Eintraegen ist unbrauchbar, und ein
  Slider (`MUI_Slider`) sendet waehrend des Ziehens viele Notifies -- das waere
  ein Dauerlauf auf dem Bus (jede Aenderung ist ein `SET_MOUSE_CFG` plus ein
  `GET_MOUSE_CFG`, rund 230 ms). `-`/`+` mit Schritt 1 ist exakt und sendet
  genau einen Auftrag je Klick. Der Preis: von 12 % auf 50 % sind es 38 Klicks
  (steht unter "Offen").
* Die Werte werden **beim Start und nach jeder Aenderung** gelesen
  (`GET_MOUSE_CFG`), **nicht** im 200-ms-Takt: sie aendern sich nur, wenn jemand
  sie aendert.
* Fehlt das Faehigkeitsbit `0x0400`, sind beide Regler **gesperrt und
  gerastert**, der Hinweis darunter nennt den Grund (`fw_ver 5 noetig`), und es
  wird **nichts** gesendet.

### 1.5 Vorher/nachher im Fenster

**Vorher** zeigte die Anzeigeseite je Buchse eine Tabelle mit `Zug` (I2C) und
`Pegel` (I2C) samt Bewertung und darunter das Vergleichsfeld
`direkt <-> I2C` mit `OK`/`ABWEICHUNG`.

**Jetzt** zeigt die Anzeigeseite je Buchse **nur** die direkten Registerwerte:

```
Buchse 1  Modus Maus (ESP32)
direkt aus den Registern:
Leitung  Pin   Pegel    direkt
UP       Pin 1  -        aktiv
DOWN     Pin 2  -        -
LEFT     Pin 3  -        -
RIGHT    Pin 4  -        -
BTN1     Pin 6  0 (Low)  gedrueckt
BTN2     Pin 9  1 (High) -
BTN3     Pin 5  -        nicht lesbar
gedrueckt (direkt): VOR+BTN1
Roh: Zaehler=0x0100 (stabil=ja)
CIAAPRA=0x0074 POTINP=0xFF02
```

Darunter ueber die volle Breite das Rohregister-Feld mit dem Satz
`Vergleich direkt/I2C nur im Protokoll (DebugUART/Logdatei) -- das Fenster
zeigt die Portdaten.`

Der Vergleich selbst steht weiterhin im **Protokoll** (DebugUART, Logdatei,
Logfeld): einmal je Sekunde in der Herzzeile und sofort als eigene Zeile, wenn
sich Richtung, Feuerknopf oder Urteil aendern. Die dauerhafte `ABWEICHUNG` auf
Buchse 0 (Maus-Register `$DFF00A` = `APOLLO_MOUSE_GET_Y`, kein Fehler) ist
damit aus dem Fenster verschwunden, im Protokoll aber weiter sichtbar.

Die **wirksame Betriebsart** bleibt daneben stehen (`Modus ... (ESP32)`), denn
sie beschreibt, was der ESP32 an den Leitungen tut -- eine Einstellung, kein
Portdatum. Fehlt sie, steht `Modus ? (ESP32 antwortet nicht)`, und die
Portdaten werden trotzdem gezeichnet (die Registerabfrage haengt nicht am Bus).

### 1.6 Konsolenbefehle (unveraendert textbasiert)

| Befehl | Wirkung |
|---|---|
| `cfg` | `GET_PORT_CONFIG` und `GET_MOUSE_CFG` lesen, beide Buchsen anzeigen (inkl. Beitrag und Tempo/Totzone) |
| `cfg <0\|1> beitrag analog\|digital\|beide` | setzt die Routing-Werte der Variante |
| `cfg <0\|1> mouse speed <1..10>` | `SET_MOUSE_CFG` dieser Buchse |
| `cfg <0\|1> mouse totzone <0..100>` | `SET_MOUSE_CFG` dieser Buchse (Prozent) |

Nach jeder Aenderung wird neu gelesen und die Zeile gezeigt; `BAD_ARG` wird im
Klartext gemeldet, und die Anzeige bleibt auf dem echten Stand. Die Zeile der
Konsole nennt jetzt am Ende `Beitrag <variante> Tempo N/10, Totzone N%` und bei
`beide` den Zusatz `(Einzelschalter gegenstandslos)`.

---

## 2. Geaenderte und neue Dateien

| Datei | Was |
|---|---|
| `v4_proto.h` | `V4P_FW_VER` 4 -> **5**, `V4P_FW_VER_MOUSE`, Faehigkeitsbit `V4P_CAP_MOUSE_CFG` `0x0400`, Befehle `0x0B`/`0x0C`, Offsets/Laengen (`V4P_MCFG_*` 4 Byte, `V4P_SMC_*` 3 Byte), Grenzen und Vorgaben (Tempo 1..10 / 7, Totzone 0..100 % / 12), Typ `v4p_mousecfg_t` (Felder **je Buchse**), Prototypen. **Alle Nummern nur hier.** |
| `v4_proto.c` | Encoder/Decoder fuer `GET_MOUSE_CFG` (beide Buchsen) und `SET_MOUSE_CFG` (eine Buchse, Echo), Pruefungen `v4p_mouse_speed_valid` / `v4p_mouse_totzone_valid` / `v4p_mouse_cfg_valid` (mit Uebersetzungszeit-Sicherung auf die Untergrenze 0), Klartext `v4p_mousecfg_text` |
| `v4_master.h/.c` | Wrapper `v4_get_mouse_cfg` (0x0B) und `v4_set_mouse_cfg` (0x0C, lokale Pruefung + Echo-Pruefung), Beitrags-Bedienhilfe `v4_beitrag_name` / `v4_beitrag_dir` / `v4_beitrag_btn` / `v4_beitrag_aus_dir` / `v4_set_port_beitrag` mit der **einen** Zuordnungstabelle |
| `v4_mock.h/.c` | Maus-Einstellungen nachgebildet (Vorgaben 7/12 je Buchse, `BAD_ARG` ohne Aenderung), Stoerknoepfe `legacy_fw4` (fw_ver 4, kein `0x0400`, 0x0B/0x0C -> `BAD_CMD`) und `bad_mousecfg_echo`, Setter `v4_mock_set_mouse_cfg`, Faehigkeiten-/Versionsmeldung nachgezogen |
| `v4_state.h/.c` | `mousecfg` + `mousecfg_valid` im Zustandsblock, `v4_state_put_mousecfg` |
| `v4_joyport.h/.c` | neu `v4_joy_direkt_text` (mehrzeilige Anzeige **nur** aus den eigenen Registern, ohne jeden I2C-Wert) und `V4_JOY_DIREKT_MAX`; Kopfprosa praezisiert |
| `v4_debug.c` | Befehlsnamen `GET_MOUSE_CFG`/`SET_MOUSE_CFG`, Kurzform der beiden Antworten, Faehigkeitsliste um `Maus-Einstellungen` ergaenzt |
| `v4_console.c` | `cfg <0\|1> beitrag ...`, `cfg <0\|1> mouse speed\|totzone ...`, `cfg_beitrag_parse`/`cfg_zahl_parse`, `mousecfg_verfuegbar`/`mousecfg_lesen`, Zeile und Hilfe erweitert, Faehigkeitsliste ergaenzt |
| `v4_gui.c` | Anzeigeseite auf Direktwerte umgebaut (`gui_show_regtext`, `gui_build_port_text`), Konfigurationsseite um `Beitrag` und `Maus Tempo`/`Totzone` erweitert, `MUIA_Disabled`-Sperre bei `beide` und ohne Maus-Faehigkeit, dynamischer Hinweis, Handler `act_set_beitrag`/`act_set_maus`, Maus-Lesen beim Start/Refresh/nach Aenderung |
| `tests/test_proto.c` | neuer Fall `test_codec_mouse_cfg` (4-Byte-GET, 3-Byte-SET, Grenzen, zu kurze Nutzlast, Text) |
| `tests/test_master.c` | neu `test_beitrag` (Tabelle, Rueckrichtung, Setzen gegen den Mock, `eigene` ohne Rahmen) und `test_mousecfg` (Vorgaben, je Buchse, lokale Grenzen, `BAD_ARG`, falsches Echo, fw_ver 4 ohne Bit) |
| `tests/test_state.c` | neu `test_state_mauscfg` |
| `tests/test_joyport.c` | neu `test_joy_direkt_text` (Direktzeile ohne `I2C`/`state=`/`level=`/`ABWEICHUNG`, BTN3 "nicht lesbar", Rohwerte), Randfaelle ergaenzt |
| `tests/demo_main.c` | Schalter `V4_DEMO_LEGACY_FW4`, `V4_DEMO_BAD_MOUSE_ECHO` |
| `Makefile` | Demo-Schritte 11-13 (Beitrag/Maus, fw_ver 4 ohne Maus, falsches Echo); erwartete Werte `fw=5` und `Faehigkeiten: 0x07FF` nachgezogen |
| `README.md` | Beitrag, Maus-Einstellungen, Direktanzeige, Konsolenbefehle, Nachweis |
| `BERICHT_BEITRAG_2026-10-14.md` | dieser Bericht |

**Kein Firmware- und kein Protokollumbau:** die Varianten sind reine
Bedienhilfe; die Maus-Befehle sind der bestaetigte Vertrag. Am ESP32-Projekt
(`V4_ESP32_BT_Joy_BP32/`) wurde nichts geaendert und nichts gelesen.

**Protokoll-Beobachtung (nur berichtet, nichts geaendert):** fuer die
Beitrags-Varianten **fehlt nichts** -- `SET_PORT_ROUTING` traegt Richtungen und
die drei Knopfquellen in einem Auftrag, und "keine" auf einer Leitung ist ein
gueltiger Wert. Die drei Varianten brauchen deshalb genau **einen** Busauftrag
plus das gewohnte `GET_PORT_CONFIG`; eine Firmware-seitige "Variante" waere nur
eine zweite Schreibweise derselben Werte. Zwei Asymmetrien im Vertrag sind
bewusst so und hier nur benannt: `GET_PORT_CONFIG` liest **beide** Buchsen,
`SET_PORT_ROUTING` setzt **eine**; und `GET_MOUSE_CFG` liefert **beide**
Buchsen, `SET_MOUSE_CFG` setzt **eine** -- das ist dieselbe Aufteilung wie
`GET_PORT_MODE` gegen `SET_PORT_MODE` und war so abgestimmt. Ein Befehl zum
**Loesen** einer Geraetezuordnung existiert weiterhin nicht (nur BDA ->
Buchse); das ist bekannt und wird in der Oberflaeche als "nicht moeglich"
gemeldet.

### Beispielzeile aus dem Fenster (Klartext der Konfigurationsseite)

```
Buchse 0: 8BitDo 64 BT -- Vorgabe Maus, wirkt Maus, Richtungen Stick links,
Pin6<-L1, Pin9<-R1, Pin5<-keine, Beitrag analog, Tempo 7/10, Totzone 12%
Buchse 1: 8BitDo 64 BT -- Vorgabe Joystick, wirkt Joystick, Richtungen D-Pad,
Pin6<-A, Pin9<-B, Pin5<-Start, Beitrag digital, Tempo 7/10, Totzone 12%
```

(Im Fenster stehen die beiden Buchsen als **Spalten** nebeneinander; oben steht
jeweils die Zeile der Buchse.)

---

## 3. `Forbid()`

**Es kommt kein `Forbid()` im Code vor.** Alle 15 Treffer des Wortes stehen in
Kommentaren/Prosa (`NIEMALS Forbid()`, Warnungen zum eingefrorenen Workbench).
Eine Pruefung mit entfernten Kommentaren ueber alle `.c`/`.h` in
`v4_master/` und `tests/` liefert **keinen** Code-Treffer. Die Neuerungen
halten sich daran: alles laeuft im Hauptfaden (MUI-Task), es gibt keinen
zweiten Faden und keine Sperre. I2C wird weiterhin direkt im Hauptfaden
gerufen, auch das Maus-Lesen.

---

## 4. `make verify` -- wortwoertliche Ausgabe

```
Portabilitaets-Lint (PROTOCOL_V4_MASTER.md §0/§2)
Streng geprueft (alle Regeln inkl. memcpy-Bann): v4_proto.h v4_proto.c v4_master.h v4_master.c
Mock (alle Regeln ausser memcpy-Bann): v4_mock.c
Plattformschichten (nur Mehrbyte-Integerzeiger): v4_linux_i2c.c v4_amiga_i2c.c
Geprueft: 5123 Codezeilen (ohne Kommentare)
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
 - Randfaelle: NULL, unbekannte Buchse, kleiner Puffer
== Ausgabeschicht: Lebenszeichen, eine Zeile je Meldung, Sofortmeldung bei Aenderung ==
 - Herzzeile: eine Zeile je Sekunde, Vergleich und Rohwerte mit drin
 - Zeilen-Haken: genau ein Aufruf je Meldung, ohne Umbruch
 - Tastendruck: Aenderung wird sofort gemeldet (frueher nie)
 - Transaktionszeilen: mit -quiet aus, ohne -quiet an
 - Urteil direkt <-> I2C: OK, Abweichung, kein Urteil
 - Rahmenpruefung: geprueft/fehlerhaft/fehlerserie statt letztem Zustand

-----------------------------------------------
Faelle : 178
Checks : 1192
Fehler : 0
-----------------------------------------------
ERGEBNIS: OK
cc -std=gnu17 -Wall -Wextra -Werror -O1 -g -fsanitize=address,undefined \
    -fno-omit-frame-pointer -I. -o build/v4_tests_san v4_proto.c v4_master.c v4_state.c v4_joyport.c v4_debug.c v4_mock.c tests/test_main.c tests/test_proto.c tests/test_master.c tests/test_state.c tests/test_joyport.c tests/test_debug.c
== Testlauf mit ASan + UBSan ==
-----------------------------------------------
Faelle : 178
Checks : 1192
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

## 5. Was geprueft ist -- und was nicht

**Geprueft (ohne Hardware):**

* Portabilitaets-Lint sauber (inkl. Selbsttest des Lints).
* 178 Testfaelle / 1192 Checks, 0 Fehler -- nativ und unter ASan+UBSan.
* Demo gegen den Mock: 13 Durchlaeufe, darunter die Beitrags-Varianten, die
  Maus-Einstellungen, `fw_ver 4` ohne Maus-Faehigkeitsbit und ein falsches Echo.
* MUI-Baumpruefer (`tools/check_mui_tree.py`, auch `--selftest`).
* m68k-Disassemblierung (Frame-Bytes nur byteweise).
* Beide Amiga-Builds mit `-Wall -Wextra -Werror` (Konsole und MUI).

**Keine bestehende Pruefung abgeschwaecht.** Angepasst wurden nur zwei
Erwartungswerte, die die Firmware-Seite geaendert hat: die PING-Zeile nennt
`fw=5` statt `fw=4`, und die Faehigkeiten sind `0x07FF` statt `0x03FF`. Beide
Pruefungen sind damit **scharfer** als vorher (sie enthalten das neue Bit
`0x0400`); alle uebrigen Pruefungen sind unveraendert und laufen weiter.

**Nur uebersetzt, NICHT auf echter Hardware geprueft:**

* Das **Aussehen** der neuen Zeilen und der Sperre (`MUIA_Disabled` -> Raster)
  auf der V4. Die Sperre ist durch die MUI-Dokumentation belegt, aber nicht am
  Geraet gesehen.
* Die **Fensterhoehe** mit drei zusaetzlichen Zeilen auf der
  Konfigurationsseite. Rechnerisch bleibt die Mindesthoehe in derselben
  Groessenordnung (die Anzeigeseite ist durch den Wegfall des Vergleichs
  kuerzer geworden), und die Fenstergroesse bleibt 640x400 + SizeGadget +
  `-w`/`-h`; gemessen auf dem 640x360-Schirm ist es nicht.
* Das **Maus-Tempo** und die **Totzone** in ihrer Wirkung: die Firmware rechnet
  die Stufe/Prozent um, und diese Umrechnung ist hier bewusst nicht
  nachgebildet. Geprueft ist nur, dass die Werte unveraendert auf die Leitung
  gehen und das Echo geprueft wird.
* `GET/SET_MOUSE_CFG` gegen die echte Firmware (der Vertrag ist abgestimmt, die
  Gegenstelle war zum Zeitpunkt dieses Berichts nicht erreichbar).
* `make test-m68k` (qemu, Big Endian) -- die Cross-Werkzeuge fehlen weiterhin.

**Offen / bewusst nicht getan:**

* Der Beitrag setzt **nur** die Routing-Werte, **nicht** die Betriebsart. Fuer
  eine Maus-Buchse muss die Betriebsart weiterhin auf `Maus` gestellt werden
  (Anzeigeseite `Vorgabe` bzw. `cfg <port> mode maus`). Das ist die Vorgabe des
  Auftrags ("die Varianten sind nur eine Bedienhilfe, die die bestehenden
  Kommandos setzt"); ein automatisches Mitstellen waere eine zweite Wahrheit
  und wurde bewusst nicht gebaut.
* Die Totzone ist mit `-`/`+` (Schritt 1) eingestellt. Von 12 % auf 50 % sind
  das 38 Klicks. Ein Slider waere bequemer, sendet aber waehrend des Ziehens
  viele Auftraege; wenn das am Geraet stoert, waere ein Slider mit
  `MUIA_Slider_Quiet`/Senden erst beim Loslassen die naechste Ausbaustufe.
* Bei `beide` sind die Knopfquellen auf die drei Gruppen festgelegt. Einzelne
  Tasten sind dann nicht waehlbar; das ist die Bedeutung von "alle Knoepfe".
* Der Hinweis "Vergleich direkt/I2C nur im Protokoll" steht als Text im
  Fenster; wer den Vergleich sehen will, liest den DebugUART oder die Logdatei
  (`make log`).
