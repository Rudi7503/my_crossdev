# Bericht -- Eingangsverteilung je Buchse (ESP32 `fw_ver` 4)

**Auftrag:** Die V4-Seite (`ApolloCrossDev/Projects/BT-Joy/v4_master/`) um die
drei neuen Kommandos `SET_DEVICE_PORT` (`0x08`), `GET_PORT_CONFIG` (`0x09`) und
`SET_PORT_ROUTING` (`0x0A`) sowie die geaenderte Bedeutung von `SET_PORT_MODE`
(`mode = 0xFF` = "nach Geraeteart") erweitern -- Protokollschicht, MUI-
Oberflaeche, Konsole, Tests, Doku.

**Vertrag:** `V4_ESP32_BT_Joy_BP32/docs/PROTOKOLL.md` (nur gelesen, nichts dort
geaendert) und `V4_ESP32_BT_Joy_BP32/main/v4_proto.h` (nur gelesen).
**Der ESP32 und das Audio-Schwesterprojekt wurden ausschliesslich gelesen.**
**Es wurde nichts hochgeladen und nichts geflasht.**

**Ergebnis:** `make verify` laeuft mit **Exit-Code 0** durch (Lint, Testsuite,
ASan/UBSan, Demo gegen den Mock, MUI-Baumpruefer, m68k-Disassemblierung, beide
Amiga-Builds mit `-Werror`). Testsuite: **158 Faelle / 1016 Checks / 0 Fehler**
(vorher 136/818). Demo: **10 Durchlaeufe** (vorher 8).

---

## 1. Neue und geaenderte Dateien

| Datei | Was |
|---|---|
| `v4_proto.h` | `V4P_FW_VER 3 -> 4` (+ `V4P_FW_VER_ROUTING`), Faehigkeitsbits `V4P_CAP_PORT_ROUTING 0x0100`/`V4P_CAP_DEVICE_PORT 0x0200`, `V4P_MODE_AUTO 0xFF`, `V4P_DIR_*`, `V4P_TASTE_*` (0..16), Nutzlast-Offsets und -Laengen der drei Befehle, Host-Struct `v4p_portcfg_t`, Codec-/Pruef-Deklarationen, Klartext-Helfer |
| `v4_proto.c` | Codecs `v4p_enc/dec_get_port_config`, `..._set_port_routing`, `..._set_device_port`; Pruefungen `v4p_dir_valid`, `v4p_mode_soll_valid`, `v4p_taste_valid`, `v4p_btn_set_valid`; Namen `v4p_port_mode_soll_name`, `v4p_dir_name`, `v4p_dir_text`, `v4p_taste_name`, `v4p_taste_name_short`; `v4p_dec_set_port_mode` laesst jetzt `0xFF` zu |
| `v4_master.h/.c` | `v4_get_port_config`, `v4_set_port_routing`, `v4_set_device_port` (jeweils mit Echo-Pruefung), `v4_info_has_caps()`; `v4_set_port_mode` akzeptiert `V4P_MODE_AUTO`; `v4_check_info()` prueft **weiterhin nur** `proto_ver` und die Rahmengroessen -- `fw_ver 4` wird ausdruecklich **nicht** abgelehnt |
| `v4_state.h/.c` | `v4p_portcfg_t portcfg` + `portcfg_valid`, `v4_state_put_portcfg()` (setzt `mode`/`applied` auf die **wirksame** Betriebsart); Sitzungs-Gedaechtnis der Geraetezuordnung (`v4_state_put_devport`, `v4_state_devport`, `v4_state_devport_at`) |
| `v4_debug.c` | Klartextnamen der drei neuen Befehle, Kurzform je Transaktionszeile (`GET_PORT_CONFIG` nennt `soll/ist/dir/btn`; `SET_PORT_ROUTING` und `SET_DEVICE_PORT` das bestaetigte Echo), zwei neue Faehigkeitsnamen ("Verteilung", "Geraetezuordnung") |
| `v4_mock.h/.c` | ESP32-Emulation: `fw_ver 4`, die neuen Faehigkeitsbits, `SET_PORT_MODE` mit `0xFF`, `GET_PORT_CONFIG`, `SET_PORT_ROUTING` (mit derselben Doppelbelegungs-Regel, **erst pruefen, dann uebernehmen**), `SET_DEVICE_PORT`; Zustand `s_mode_soll`/`s_dir`/`s_btn` mit den Werksvorgaben (`0xFF`, `0x07`, `14/15/16`); Stoerknoepfe `bad_routing_echo`, `legacy_fw3`; Setter/Getter fuer Tests |
| `v4_console.c` | neuer Befehl **`cfg`** (Anzeigen, `mode`, `dir`, `btn1..3`, `assign`), Anzeige der Verteilung im Anfangszustand, Menue-Hinweis |
| `v4_gui.c` | **Kern des Auftrags**: zweite Seite "Konfiguration" (`MUIA_Group_PageMode`) mit Geraet, drei Richtungsschaltern und drei Knopfquellen je Buchse; Betriebsart-Feld auf vier Eintraege (`auto`/`aus`/`Joystick`/`Maus`) mit `wirkt: ...` daneben; Aktionen `act_set_dev`/`act_set_dir`/`act_set_btn`/`act_mode_weiter`/`act_seite`; `gui_cfg_read()` + `gui_show_portcfg()` |
| `tools/check_mui_tree.py` | neuer positiver Selbsttestfall "Seitengruppe (`MUIA_Group_PageMode`) bleibt sauber" und neuer Fehlerfall "`MUIA_Group_PageMode` am TextObject"; Docstring ergaenzt. **Keine bestehende Pruefung abgeschwaecht.** |
| `tests/test_proto.c` | 10 neue Faelle: Codecs und Pruefungen der Verteilung, `0xFF` in `SET_PORT_MODE` (und Ablehnung in `GET_PORT_MODE`), Klartexte |
| `tests/test_master.c` | 9 neue Faelle gegen den Mock: `GET_PORT_CONFIG`, `SET_PORT_ROUTING` (gueltig, Doppelbelegung -> `BAD_ARG` **ohne** Rahmen und ohne Aenderung, unbekannte Werte, falsches Echo -> `V4_ERR_FRAME`), `SET_PORT_MODE 0xFF`, `SET_DEVICE_PORT`, `fw_ver 4` wird angenommen, `fw_ver 3` wird angenommen und die neuen Befehle melden `BAD_CMD` |
| `tests/test_state.c` | 3 neue Faelle: `mode_soll`/`mode_ist` bleiben getrennt, `mode[]` traegt die wirksame Betriebsart, Geraetezuordnung merken/ersetzen/nachlesen, Tabellengrenze |
| `tests/demo_main.c` | Stoerknopf `V4_DEMO_LEGACY_FW3=1` |
| `Makefile` | Demo-Erwartungen auf `fw=4`/`0x03FF` aktualisiert; **Demo 9** = das Abnahmebeispiel ueber `cfg`; **Demo 10** = `fw_ver 3` |
| `README.md` | Abschnitt "Eingangsverteilung je Buchse", Konsolenbefehle `cfg`, zwei Seiten der Oberflaeche, neue offene Punkte (9/10), Zaehler aktualisiert |
| `BERICHT_EINGANGSVERTEILUNG_2026-10-14.md` | dieser Bericht (neu) |

Unveraendert: `v4_joyport.*`, `v4_amiga_i2c.c`, `v4_linux_i2c.c`,
`tests/test_joyport.c`, `tests/test_debug.c`, `tests/test_main.c`,
`tools/check_portability.sh`, `tools/check_m68k_byteaccess.sh`.

---

## 2. Protokollschicht -- die I2C-Kommandos

Rahmen und `proto_ver` bleiben **unveraendert** (WRITE 32 Byte, READ 128 Byte,
`proto_ver 3`). Der ESP32 meldet jetzt `fw_ver = 4`; `v4_check_info()` prueft
weiterhin nur `proto_ver` und die Rahmengroessen und **lehnt `fw_ver 4` nicht
ab**. Verbindliche Auskunft ueber die neuen Befehle sind die Faehigkeitsbits
`0x0100`/`0x0200` (`v4_info_has_caps`).

| Befehl | Wert | Anfrage -> Antwort |
|---|---|---|
| `SET_DEVICE_PORT` | `0x08` | `[bda 6][port u8]` -> Echo (7 Byte); `BAD_ARG` bei port > 1 |
| `GET_PORT_CONFIG` | `0x09` | -- -> 12 Byte: `mode_soll0/1`, `mode_ist0/1`, `dir0/1`, `btn0[3]`, `btn1[3]` |
| `SET_PORT_ROUTING` | `0x0A` | `[port][dir][btn1][btn2][btn3]` -> Echo (5 Byte); `BAD_ARG` bei port > 1, unbekanntem Richtungsbit/-code oder **doppelt vergebener** Knopfquelle |
| `SET_PORT_MODE` | `0x04` | wie bisher; **`mode = 0xFF`** setzt die Vorgabe auf "nach Geraeteart"; Echo = `0xFF` |
| `GET_PORT_MODE` | `0x05` | unveraendert die **wirksame** Betriebsart (nie `0xFF`) |

Werte: `mode` 0 aus / 1 Joystick / 2 Maus / `0xFF` auto; `dir` `0x01` Stick
links, `0x02` Stick rechts, `0x04` D-Pad (`0x07` = Vorgabe); Knopfquellen
`0` keine, `1` A, `2` B, `3` X, `4` Y, `5` L1, `6` R1, `7` L2, `8` R2, `9` L3,
`10` R3, `11` Start, `12` Select, `13` Home/Capture, `14` Gruppe 1, `15`
Gruppe 2, `16` Gruppe 3. Vorgabe der Firmware: `dir = 0x07`, `btn = 14/15/16`.

**Lokale Vorpruefung:** `v4_set_port_routing()` und `v4_set_device_port()`
pruefen Port, Richtungsmaske, Knopfcode und Doppelbelegung **bevor** gesendet
wird (dieselben Funktionen, die auch der Mock benutzt -- eine zweite
Schreibweise der Regel gibt es nicht). Ein `BAD_ARG` aus dieser Vorpruefung
erzeugt **keinen** Busrahmen. Die Antwort wird Feld fuer Feld gegen die Anfrage
geprueft; jede Abweichung ist `V4_ERR_FRAME` und wird nicht als Bestaetigung
gewertet.

---

## 3. Konsole -- der Befehl `cfg`

Das alte Menue (`s h i p t a c d f r q`) bleibt unveraendert; `cfg` wird vor dem
Schalter abgefangen (sonst waere es "c" = verbinden).

```
cfg                                        GET_PORT_CONFIG lesen, beide Buchsen zeigen
cfg <0|1>                                  eine Buchse zeigen
cfg <0|1> mode auto|aus|joystick|maus      SET_PORT_MODE (auto = 0xFF)
cfg <0|1> dir alle|keine|stick_l|stick_r|dpad[+...]|0xNN
                                           SET_PORT_ROUTING mit dieser Richtungsmaske
cfg <0|1> btn1|btn2|btn3 <quelle>          SET_PORT_ROUTING (Quelle: keine,a,b,x,y,l1,r1,
                                           l2,r2,l3,r3,start,select,home,gruppe1..3,g1..g3,0..16)
cfg assign <index|BDA> <0|1>               SET_DEVICE_PORT (Index aus 'a', oder AA:BB:CC:DD:EE:FF)
```

Nach **jeder** Aenderung wird `GET_PORT_CONFIG` neu gelesen und die Zeile
gezeigt; bei `BAD_ARG` steht der Grund im Klartext da und die Anzeige bleibt
auf dem echten Zustand. Beispiel (echter Demo-Lauf, `make demo`, Demo 9):

```
Befehl> cfg 0 btn2 l1
SET_PORT_ROUTING: BAD_ARG -- "L1" ist auf dieser Buchse schon einer anderen Leitung zugeordnet
  (Pin6<-L1 Pin9<-R1 Pin5<-keine). Eine nicht-leere Quelle darf nur auf EINER Leitung stehen; nichts geaendert.
Befehl> cfg assign 1 0
Geraet [1] "Logitech MX Anywhere" -> Buchse 0 (bestaetigt).
Befehl> cfg
Buchse 0: Geraet Logitech MX Anywhere     Vorgabe Maus     wirkt Maus     Richtungen Stick links            Pin6<-L1 Pin9<-R1 Pin5<-keine
Buchse 1: Geraet F4:17:D8:11:22:33        Vorgabe Joystick wirkt Joystick Richtungen D-Pad                  Pin6<-A Pin9<-B Pin5<-Start
```

---

## 4. Die Oberflaeche

### 4.1 Aufbau

Die Oberflaeche hat jetzt einen Umschalter **`Seite:`** (neben der
dreizeiligen Controller-Zeile und damit **ohne zusaetzliche Hoehe**) mit zwei
Seiten (`MUIA_Group_PageMode`):

* **`Anzeige`** -- unveraendert: die beiden Buchsen mit `Vorgabe`-Auswahlfeld,
  `wirkt: <Modus>`, dem Zug/Pegel-Tableau und dem Vergleich direkt <-> I2C.
* **`Konfiguration`** -- neu: je Zeile eine Angabe, links die Beschriftung,
  rechts die beiden Buchsen in deckungsgleichen Spalten (`MUIA_FixWidth`):

```
Konfiguration (GET_PORT_CONFIG) -- Vorgabe = Auswahlfeld, wirkt = was gerade laeuft
Buchse 0: Logitech MX Anywhere -- Vorgabe Maus, wirkt Maus, Richtungen Stick links, Pin6<-L1, Pin9<-R1, Pin5<-keine
Buchse 1: F4:17:D8:11:22:33 -- Vorgabe Joystick, wirkt Joystick, Richtungen D-Pad, Pin6<-A, Pin9<-B, Pin5<-Start
                       Buchse 0                       Buchse 1
Geraet           [ Logitech MX Anywhere v ]     [ F4:17:D8:11:22:33   v ]
Richtungen       [x] Stick l [ ] Stick r [ ] D-Pad   [ ] Stick l [ ] Stick r [x] D-Pad
Knopf Pin 6      [ L1 v ]                        [ A v ]
Knopf Pin 9      [ R1 v ]                        [ B v ]
Knopf Pin 5      [ keine v ]                     [ Start v ]
Gruppe 1 = A,L1,L2,L3 - Gruppe 2 = B,R1,R2,R3 - Gruppe 3 = X,Y,Start,Select;
Home/Capture ist in keiner Gruppe. Im Mausbetrieb bleiben L1 = links und R1 = rechts.
```

Dazu auf der Anzeigeseite die Betriebsart als **Auswahlfeld mit vier
Eintraegen** (`auto`, `aus`, `Joystick`, `Maus`) und daneben `wirkt: Maus` mit
der **wirksamen** Betriebsart aus `GET_PORT_CONFIG.mode_ist` -- Vorgabe und
Wirklichkeit sind bewusst zwei Felder, weil `0xFF` keine Betriebsart ist.

### 4.2 Verhalten

* **Jede Aenderung geht sofort zum ESP32**: Geraet -> `SET_DEVICE_PORT`,
  Auswahlfeld -> `SET_PORT_MODE`, Schalter/Knopfquelle -> `SET_PORT_ROUTING`
  (Richtungen und die drei Knopfquellen immer als **ganzes 5-Byte-Paket** --
  der ESP32 kennt nur diesen einen Befehl). Danach wird `GET_PORT_CONFIG`
  gelesen und die Felder werden auf den echten Zustand gestellt.
* **`BAD_ARG`** (z.B. dieselbe nicht-leere Quelle auf zwei Leitungen) wird im
  Klartext gemeldet -- mit den beteiligten Leitungen -- und die Anzeige springt
  auf den letzten **bestaetigten** Stand zurueck. Es wird nie eine Verteilung
  angezeigt, die es nicht gibt.
* **Rueckkopplungsschutz:** die Anzeige zieht die Felder im Takt nach; jeder
  Handler vergleicht deshalb zuerst mit dem Zustandsblock und sendet nur bei
  einer echten Abweichung (dieselbe Bauart wie beim bisherigen
  Betriebsart-Knopf).
* **Umschalten** (`act_seite`) setzt nur `MUIA_Group_ActivePage` und fuellt die
  neu sichtbare Seite sofort.
* **Geraeteauswahl:** die Eintraege kommen aus der Scanliste (`DEV_COUNT`/
  `DEV_GET`), auf 20 Zeichen gekuerzt (MUI rechnet die Knopfbreite aus dem
  laengsten Eintrag); der volle Name steht in der Zusammenfassung. Index 0 ist
  `(keines)`: ein **Loesen** einer Zuordnung gibt es nicht -- der ESP32 kennt
  nur "BDA -> Buchse" --, deshalb wird diese Auswahl zurueckgezogen und
  ausdruecklich gemeldet.
* **Was die Gegenstelle nicht hergibt:** welches Geraet welcher Buchse
  zugeordnet ist, ist ueber die Leitung **nicht lesbar** (`DEV_GET` nennt nur
  Name und Adresse, `GET_PORT_CONFIG` nur die Buchsenfelder). Gezeigt wird
  deshalb, was in **dieser Sitzung** per `SET_DEVICE_PORT` gesetzt und
  bestaetigt wurde; die Zuordnung selbst liegt im NVS des ESP32 und bleibt
  dort.
* **Ohne die Faehigkeitsbits** (aeltere Firmware) bleiben `GET_PORT_CONFIG`,
  `SET_PORT_ROUTING` und `SET_DEVICE_PORT` aus; die Felder zeigen einen
  Klartext-Hinweis statt erfundener Werte, und `auto` ist nicht waehlbar
  (`SET_PORT_MODE` mit `0xFF` kennt nur `fw_ver` 4).

### 4.3 Warum zwei Seiten (und keine Scrollgruppe, keine laengere Liste)

Der Inhalt war schon vor dieser Erweiterung an der Grenze: die Workbench dieser
V4 laeuft auf 640x360, das Fenster wird also auf 624x336 begrenzt. Sechs
zusaetzliche Zeilen (~70 px) haetten den Inhalt ueber die Schirmhoehe
getrieben -- MUI haette Textfelder unter ihre Mindesthoehe gedrueckt, und genau
das war der Befund "leere Felder" vom 14.10.2026.

Die Seitengruppe loest das **ohne** Zuwachs: `MUI_Group.doc` sagt *"the minimum
width/height of a page group is the maximum minimum width/height of all its
children"*. Die Konfigurationsseite ist **kleiner** als die Anzeigeseite, die
Mindestgroesse des Fensters bleibt also exakt wie bisher (der Umschalter liegt
neben der dreizeiligen Controller-Zeile und kostet keine Zeile). Ein Versuch
mit einer `Scrollgroup` wurde verworfen: deren Mindestgroesse haengt am Inhalt
bzw. an einer festen Hoehe, und beides waere auf einem Geraet, das hier nicht
zum Testen zur Verfuegung steht, ein Risiko ohne Not.

### 4.4 Randbedingungen (unveraendert eingehalten)

* Fenster **640x400 + SizeGadget**, `-w`/`-h`, Begrenzung auf den Schirm
  (`gui_fenster_an_schirm`) -- unveraendert.
* **`MUIA_Cycle_Entries` ist ueberall ein statisches, NULL-abgeschlossenes
  Zeigerarray** (`gui_modus_eintraege`, `gui_taste_eintraege`,
  `gui_seiten_eintraege`, `s_dev_eintraege`), mit Uebersetzungszeit-Sicherung
  auf den NULL-Abschluss bei den festen Arrays. Das Geraete-Array wird zur
  Laufzeit neu gebaut -- die Zeiger zeigen auf **statische** Puffer, nie auf
  den Stapel.
* `tools/check_mui_tree.py` laeuft sauber durch (Fensterinhalt: VGroup mit 9
  Kindern, darunter die Seitengruppe mit 2 Seiten).
* **Ausgabemenge:** im Ruhezustand weiterhin **eine** Zeile je Sekunde
  (Lebenszeichen), sofort bei Aenderung, Transaktionszeilen nur mit `-trace`.
  Die neue Konfiguration wird im **langsamen** Takt (rund einmal je Sekunde)
  gelesen -- `GET_PORT_CONFIG` ersetzt keine der bisherigen Abfragen.
* **Kein zweiter Faden, kein `CreateNewProc`, kein Nachrichtenport, kein
  `Forbid()`** -- siehe Abschnitt 8.

---

## 5. `make verify` -- wortwoertlich

```
Portabilitaets-Lint (PROTOCOL_V4_MASTER.md §0/§2)
Streng geprueft (alle Regeln inkl. memcpy-Bann): v4_proto.h v4_proto.c v4_master.h v4_master.c
Mock (alle Regeln ausser memcpy-Bann): v4_mock.c
Plattformschichten (nur Mehrbyte-Integerzeiger): v4_linux_i2c.c v4_amiga_i2c.c
Geprueft: 4540 Codezeilen (ohne Kommentare)
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
 - Rahmenpruefung: geprueft/fehlerhaft/fehlerserie statt letztem Zustand

-----------------------------------------------
Faelle : 158
Checks : 1016
Fehler : 0
-----------------------------------------------
ERGEBNIS: OK
== Testlauf mit ASan + UBSan ==
-----------------------------------------------
Faelle : 158
Checks : 1016
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
Demo: OK (10 Durchlaeufe, Mock, ohne Hardware)
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

**Exit-Code: 0.**

---

## 6. Was geprueft ist

| Pruefung | Inhalt |
|---|---|
| `make test` | 158 Faelle / 1016 Checks / 0 Fehler (vorher 136/818) |
| `make test-san` | derselbe Lauf unter ASan + UBSan, keine Befunde |
| `make demo` | 10 nichtinteraktive Durchlaeufe des **echten** Konsolenprogramms gegen den Mock; Demo 9 faehrt das Abnahmebeispiel ueber `cfg` (inkl. `BAD_ARG` bei doppelter Quelle und `cfg assign`), Demo 10 faehrt `fw_ver 3` |
| `make mui-lint` | MUI-Baumpruefer sauber + Selbsttest (neu: Seitengruppe) |
| `make asm` | m68k-Code: Frame-Bauer und LE-Helfer nur `move.b` |
| `make amiga`, `make amiga-gui` | beide mit `-Wall -Wextra -Werror` fehlerfrei |
| Lint | Portabilitaetsregeln (keine mehrbytigen Frame-Zugriffe, kein `memcpy` auf Frames, kein packed/Bitfeld/Swap) |

Fachlich neu abgedeckt: `0xFF` nur in `SET_PORT_MODE` (nicht in
`GET_PORT_MODE`); Codecs und Laengenpruefungen der drei Befehle; Doppelbelegung
-> `BAD_ARG` **ohne** Busrahmen und ohne Zustandsaenderung; falsches Echo ->
`V4_ERR_FRAME`; `mode_soll`/`mode_ist` bleiben getrennt; `fw_ver 4` wird
angenommen; `fw_ver 3` wird angenommen und die neuen Befehle melden `BAD_CMD`.

---

## 7. Was offen ist bzw. nur uebersetzt und **nicht** auf echter Hardware geprueft

1. **Die neue Konfigurationsseite ist nicht auf der V4 geoeffnet worden.** Auf
   dieser Maschine gibt es keinen Amiga-Emulator, und hochgeladen/flasht wird
   laut Auftrag nicht. Geprueft sind: Amiga-Build mit `-Werror`, der
   MUI-Baumpruefer (inkl. Selbsttest der Seitengruppe) und die Groessenrechnung
   laut `MUI_Group.doc`. **Nicht gesehen** ist, wie MUI die zwei Seiten auf dem
   Geraet tatsaechlich legt. Das Risiko ist bewusst klein gehalten: die
   Seitengruppe ist eine Kernklasse von `Group.mui` (kein Zusatz-MCC), der
   Umschalter kostet keine Zeile, und die Anzeigeseite ist inhaltlich
   unveraendert.
2. **Der echte ESP32 wurde nicht angesprochen.** `GET_PORT_CONFIG`,
   `SET_PORT_ROUTING` und `SET_DEVICE_PORT` sind gegen den Mock gefahren --
   inklusive der unangenehmen Faelle. Dass die echte Firmware `fw_ver = 4`
   meldet, die Verteilung im NVS haelt und ein verbundenes Geraet bei
   `SET_DEVICE_PORT` sofort umhaengt, steht in der Doku und ist **nicht**
   nachgemessen.
3. **Die Geraetezuordnung ist ueber die Leitung nicht lesbar** (siehe 4.2).
   Nach einem Neustart des V4-Programms ist die Anzeige der Zuordnungen wieder
   leer, obwohl sie im NVS des ESP32 steht. Ein "Loesen" gibt es nicht.
4. **Der Big-Endian-Lauf fehlt** (`make test-m68k` = SKIP, Cross-Werkzeuge
   fehlen); Ersatz ist `make asm`.
5. **`make upload` ist nicht gelaufen** -- es wurde nichts auf die V4
   geschrieben (Auftrag: der Auftraggeber laedt hoch und flasht).
6. Ungeprueft bleibt wie bisher das Verhalten von `i2c.library` bei einem
   **haengenden** Bus (kein Timeout) -- der Mock stellt das nicht dar.

---

## 8. `Forbid()`, `Permit()`, `CreateNewProc`

**Nein -- nirgends.** Es gibt im gesamten Projekt **keinen einzigen Aufruf** von
`Forbid()`, `Permit()`, `CreateNewProc()`, `CreateTask()` oder `AddTask()`.
Alle Treffer im Quelltext sind Kommentare, die erklaeren, warum das so ist
(die frueher gebaute eigene I2C-Aufgabe mit `CreateNewProc` kam auf der V4
nicht durch, und ihr `Forbid()` hat die Workbench eingefroren).

Nachweis (Kommentare vorher entfernt, sonst waeren die Warntexte Treffer):

```
Treffer im CODE (ohne Kommentare): 0
```

Der I2C-Verkehr laeuft **direkt im Hauptfaden**: Konsole und MUI rufen
`v4_master` (und damit `v4_plat_*` und `i2c.library`) aus ihrem eigenen
Ereignisablauf auf. Fuer eine spaetere Sperre gilt weiter: SignalSemaphore --
niemals `Forbid()`.
