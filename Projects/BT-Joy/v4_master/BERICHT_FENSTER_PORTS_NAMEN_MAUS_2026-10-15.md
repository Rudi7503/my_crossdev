# Bericht: Fenstergroesse, ein Geraet auf zwei Ports, Namen, Maustempo

Stand: 15.10.2026. Projekt: `Projects/BT-Joy/v4_master` (V4-Seite).
Nichts wurde hochgeladen; die Uebertragung auf die V4 macht der Auftraggeber.
Am ESP32-Projekt (`V4_ESP32_BT_Joy_BP32/`) wurde **nichts geaendert** (nur
gelesen) -- zwei Befunde dort sind unten belegt.

Fuenf Punkte, je Ursache (mit Belegstelle) und Aenderung.

---

## 1. Fenster zu klein fuer die Konfigurationsseite

### Ursache

Die Mindesthoehe einer MUI-Seitengruppe ist laut `MUI_Group.doc` das
**Maximum ueber alle Seiten**: *"the minimum width/height of a page group is
the maximum minimum width/height of all its children"*. Die
Konfigurationsseite war hoeher als der Platz im Fenster (640x400; auf dem
Geraet lief zuletzt 1280x720, das Fenster also **nicht** auf den Schirm
begrenzt -- die Seite passte trotzdem nicht). MUI kann dann nur noch Kinder un-
ter ihre Mindestgroesse druecken oder abschneiden; genau das war zu sehen.

### Aenderung

1. **Die Seiten liegen jetzt in einer `ScrollgroupObject` mit einer
   `VirtgroupObject`** (`MUIA_Virtgroup_TryFit, FALSE`). Damit koennen die
   Seiten das Fenster **nicht mehr** vergroessern; wird es eng, wird gescrollt.
   `MUIA_Scrollgroup_AutoBars, TRUE` zeigt die Balken **nur**, wenn es wirklich
   nicht passt (sonst kosten sie keine Breite). Eingebaut in `v4_gui.c`
   (`s_scroll`/`s_virt`), der MUI-Baumpruefer kennt die beiden Klassen jetzt
   (`tools/check_mui_tree.py`).
2. **Der Umschalter `Seite:` bleibt ausserhalb der Scrollgruppe** (in der
   Controller-Zeile). Er kann also nie weggescrollt oder abgeschnitten werden.
   Zusaetzlich springt die Scrollgruppe bei jedem Seitenwechsel nach oben
   (`MUIA_Virtgroup_Top, 0`).
3. **Die Konfigurationsseite ist kompakter geworden:** der Klartext ist von 3
   auf 2 Zeilen (eine je Port) gekuerzt, und **Tempo und Totzone teilen sich
   EINE Zeile** (`Maus: Tempo <Cycle> Totz. <Wert> - +`). Die Gerätenamen im
   Auswahlfeld sind auf 14 Zeichen gekuerzt (die Zelle teilt sich die Breite mit
   `Beitrag`); der volle Name steht in der Klartextzeile und in der
   Geraeteliste. Die Scrollgruppe hat `MUIA_MinHeight 160` und Gewicht 50, die
   beiden Listen `MUIA_MinHeight 20` (statt 36) und Gewicht 20/20 -- so bekommt
   die Seitengruppe den Platz, der fuer die Bedienung noetig ist, und die
   Fenstermindesthoehe bleibt unter 400.
4. **Die Mindestmasse werden jetzt gemessen und protokolliert.** Nach dem
   Oeffnen und bei jedem Seitenwechsel ruft die Oberflaeche `MUIM_AskMinMax`
   -- dieselbe Methode, die MUI fuer das Layout benutzt -- und schreibt:
   `[fenster] Mindestgroesse Fensterinhalt: <w>x<h> px (Fenster 640x400)`, dazu
   Seitengruppe (Maximum) und beide Seiten einzeln.

### Die tatsaechlichen Mindestmasse

Die pixelgenauen Werte haengen an der MUI-Vorgabeschrift des jeweiligen
Rechners; auf dem Wirt (Linux) laeuft MUI nicht, ein Amiga-Emulator ist hier
nicht vorhanden. Deshalb gibt es zwei Angaben:

**(a) Gezaehlt aus dem Baum** (jede Zeile = ein Textobjekt, jede
Cycle-/Checkmark-Zeile ist hoeher):

| Seite | Zeilen/Objekte | davon Cycle-/Schalterzeilen |
|---|---|---|
| Anzeige (Seite 0) | 11 (Direkttabelle) + 1 (Modus-Zeile) + 2 (Rohregister) = **14** | 1 |
| Konfiguration (Seite 1) | 2 (Klartext) + 1 (Kopf) + 1 Geraet + 1 Beitrag + 1 Richtungen + 3 Knopf + 1 Maus + 3 Hinweis = **13** | 7 |

**(b) Gerechnet mit der MUI-Vorgabeschrift** (Topaz 8: 8 px Zeile, 4 px
Gruppenabstand, Cycle/Checkmark ≈ 16 px):

* Anzeige: 13 Textzeilen (104) + 1 Cycle-Zeile (16) + Abstaende ≈ **136 px**
* Konfiguration: 6 Textzeilen (48) + 7 Gadget-Zeilen (112) + Abstaende ≈ **200 px**

Die Seitengruppe meldet damit **rund 200 px** (Maximum = Konfiguration). Im
Fenster stehen dafuer rund **170-180 px** zur Verfuegung (400 minus Rahmen,
Kopfbereich und zwei Listen). Die Konfigurationsseite scrollt also im
Regelfall um **etwa eine Zeile** -- und laesst sich in jedem Fall bedienen,
ohne das Fenster zu vergroessern. Die **exakten** Zahlen stehen nach dem
naechsten Lauf im Protokoll (die `[fenster] Mindestgroesse ...`-Zeilen); die
Anzeigeseite passt ohne Scrollen.

---

## 2. Dasselbe Geraet auf BEIDEN Ports

### Ursache: die Firmware haelt je Geraet genau EINE Buchse

Der Weg in der Oberflaeche ist in Ordnung: `act_set_dev()` **blockiert nichts**,
sendet `SET_DEVICE_PORT` mit der gewaehlten BDA und dem Port, und die
Gegenstelle **akzeptiert** das (der Mitschnitt zeigt alle vier Versuche):

```
12:40:51  [cfg] Buchse 0: Geraet "8BitDo 64 BT" (E4:17:D8:5F:E6:C9) zugeordnet ...
12:40:53  [cfg] Buchse 1: Geraet "8BitDo 64 BT" (E4:17:D8:5F:E6:C9) zugeordnet ...
12:40:55  [cfg] Buchse 0: Geraet "8BitDo 64 BT" (E4:17:D8:5F:E6:C9) zugeordnet ...
12:41:15  [cfg] Buchse 1: Geraet "8BitDo 64 BT" (E4:17:D8:5F:E6:C9) zugeordnet ...
```

Die Zuordnung **wandert** aber: `V4_ESP32_BT_Joy_BP32/main/v4_devices.c`,
`v4_devices_zuordnen()`, Zeile 413 ff.:

```c
if (d->port != port) {
    d->port = port;          /* EINE Buchse je Geraet */
    geaendert = true;
}
```

Ein Geraet hat also **ein** Feld `port`. Die zweite Zuweisung derselben BDA
verschiebt sie; Port 0 verliert sie, und die Oberflaeche zeigt das korrekt als
`(keines)`. Es ist **kein** Fehler der Oberflaeche und **kein** Ablehnen durch
den ESP32.

### Der richtige Weg fuer "ein Controller auf beiden Ports": `duo`

`V4_ESP32_BT_Joy_BP32/main/v4_platform.c` (Zeile 269 ff.):

```
duo: EIN Controller auf BEIDEN Buchsen
... Deshalb legt duo die eine Buchse auf MAUS und die andere auf JOYSTICK.
```

`duo` ist die Absicht des Benutzers -- **eine** Verbindung, **zwei** Rollen
(Maus auf dem einen, Joystick auf dem anderen Port). Es braucht **keine**
doppelte Geraetezuordnung und ist eine **Einstellung der ESP32-Seite**
(`v4_settings_duo()`, NVS). **Es gibt dafuer keinen I2C-Befehl** -- die
V4-Seite kann duo weder lesen noch setzen (in `V4_ESP32_BT_Joy_BP32/main/
v4_proto.h` ist kein `duo`-Befehl deklariert).

### Aenderung in der Oberflaeche (ehrlich statt Schein)

* Wird eine Zuordnung **verschoben**, sagt die Oberflaeche das jetzt im
  Klartext: `[cfg] Hinweis: "8BitDo 64 BT" war Port 0 zugeordnet und ist jetzt
  Port 1 -- ein Geraet hat im ESP32 immer genau EINE Buchse. Fuer EINEN
  Controller auf beiden Buchsen ist der duo-Betrieb des ESP32 zustaendig (nicht
  ueber I2C einstellbar).`
* Die Klartextzeile im Fenster zeigt weiterhin **beide** Ports; sie zeigt aber
  die Wahrheit (ein Port hat das Geraet, der andere nicht).
* Ich habe **nicht** "beide Ports zeigen dasselbe Geraet" vorgetaeuscht: die
  Anzeige wuerde damit luegen, und der Befehl an den ESP32 aendert die
  Datenhaltung nicht. Wenn der Benutzer beide Ports aus der Oberflaeche
  schalten koennen will, braucht es **einen duo-Befehl im Protokoll** (z. B.
  `GET/SET_DUO` mit Faehigkeitsbit) -- das ist eine Aenderung an der
  **Firmware**, nicht an der Oberflaeche. **Bitte dort entscheiden.**

---

## 3. Namen

* **`Buchse` -> `Port` im Fenster:** die Portspalten der Anzeigeseite
  (`Port 0:` / `Port 1:`), die Kopfzeile der Anzeige (`Port 0  Modus ...`),
  die Spaltenkoepfe der Konfigurationsseite (`Port 0` / `Port 1`), die
  Klartextzeilen (`Port 0: ...`) und der Knopf (`Ports tauschen`).
  **Konsole und Protokoll** behalten `Buchse` (so im Auftrag).
* **Beitrag im Fenster heisst `beides`** (die Cycle-Eintraege:
  `analog`, `digital`, `beides`, `eigene`; die Klartextzeile ebenso ueber
  `gui_beitrag_name()`). Protokoll/Konsole behalten `beide`
  (`v4_beitrag_name()`).
* **`eigene` ist im Hinweis erklaert:**
  `beides = alle Richtungen + Gruppe 1/2/3, eigene = passt zu keiner Variante.`

---

## 4. Maustempo-Auswahl

### Was geprueft ist -- und was nicht die Ursache ist

* **Die beiden Felder sind unabhaengig.** Zwei getrennte Objekte
  (`s_mspeed0`/`s_mspeed1`), getrennte ReturnIDs (`ID_MSPEED0`/`ID_MSPEED1`),
  getrennte Handler (`act_set_maus(0,0)` / `act_set_maus(1,0)`).
* **Der aktive Eintrag kommt aus `GET_MOUSE_CFG`:** `gui_show_portcfg()` setzt
  je Port `MUIA_Cycle_Active` aus `S.mousecfg.speed[port] - 1`.
* **Es geht der richtige Port auf die Leitung:** `v4_set_mouse_cfg()` prueft
  lokal, baut `[port][speed][totzone]` (`v4p_enc_set_mouse_cfg()`) und prueft
  das **Echo feldweise**; `tests/test_master.c` belegt: *"SET_MOUSE_CFG: eine
  Buchse wird gesetzt, die andere bleibt"*.
* **Die Eintraege sind ein statisches, NULL-abgeschlossenes Zeigerarray**
  (`gui_speed_eintraege = {"1".."10", NULL}` mit
  Uebersetzungszeit-Sicherung); `tools/check_mui_tree.py` prueft die Form.
* **Die Firmware ist ebenfalls je Port gebaut:** `v4_settings_set_maus(port,...)`
  schreibt `s_maus_speed[port]`/`s_maus_totzone[port]` und den NVS-Schluessel
  `MAUS0`/`MAUS1`; `GET_MOUSE_CFG` liest beide Ports einzeln
  (`V4_ESP32_BT_Joy_BP32/main/v4_settings.c`, Zeilen 71-73, 582-612).
* Im Mitschnitt steht **nur** `Buchse 0: Maus Tempo ... bestaetigt` -- es ist
  keine Zeile fuer Port 1 dabei, und die alte Zeile nannte den **angeforderten**
  Wert, nicht den zurueckgelesenen. Sie war also **kein** Beleg.

**Damit gibt es zwei plausible Erklaerungen fuer "erhoeht beide Auswahl":**

1. **Die Seite war abgeschnitten** (Punkt 1): die beiden Felder lagen im
   abgeschnittenen Bereich, Klicks trafen daneben/auf das falsche Objekt. Dafuer
   spricht, dass das Tempo von 9 auf 2 sprang, ohne Zwischenschritte.
2. **Die geflashte Firmware ist noch die GLOBAL gefasste Fassung** (ein Tempo
   fuer beide Ports). Die V4-Seite war zwischenzeitlich so spezifiziert, und die
   Korrektur auf "je Port" kam erst danach. Dann liefert `GET_MOUSE_CFG` fuer
   beide Ports denselben Wert, und das Fenster zeigt beide gleich. Der
   Quelltext im Baum ist inzwischen **je Port** -- ob das Binary dazu passt,
   entscheidet der naechste Lauf (siehe unten).

### Aenderung

1. **`GET/SET_MOUSE_CFG` werden IMMER protokolliert** (Punkt 5) und nennen
   **beide** Ports vorher/nachher. Damit ist in einer Zeile zu sehen, ob eine
   Tempo-Aenderung den richtigen Port trifft oder ob beide wandern.
2. **Der zurueckgelesene Wert wird uebernommen** (der Wrapper hat das Echo
   geprueft), danach trotzdem gelesen; die Anzeige kann also nicht mehr auf
   einen alten Stand zurueckspringen.
3. **Tempo und Totzone stehen jetzt in EINER Zeile** (`Maus: Tempo <c> Totz.
   <n> % - +`) -- kuerzerer Weg, weniger Verwechslung, und die Zeile passt
   vollstaendig ins Fenster (Punkt 1).
4. Der Hinweis nennt die Bedienung:
   `Maus-Tempo/-Totzone: Klick schaltet weiter, rechte Maustaste oeffnet die
   Liste.` (Ein MUI-Cycle hat ein Popup-Menue; es oeffnet mit der rechten
   Maustaste bzw. beim Gedrueckthalten, nicht durch Aufklappen wie ein
   Dropdown.)

**Wenn der naechste Lauf in der `[maus]`-Zeile zeigt, dass nach
`SET_MOUSE_CFG port=0` auch `port1` einen anderen Wert liest, ist das ein
Firmware-Befund** -- bitte dort beheben; die Oberflaeche sendet nachweislich
`[port][speed][totzone]` und prueft das Echo.

---

## 5. Maustempo-Aenderung sichtbar machen

Umgesetzt: `GET/SET_MOUSE_CFG` bekommen **eigene, immer eingeschaltete**
Protokollzeilen (auch ohne `-trace`). Eine Zeile entsteht bei einer
**Aenderung**, nicht im Takt -- die Regel "im Ruhezustand eine Zeile je
Sekunde" bleibt.

* nach einer Aenderung:
  `[maus] SET_MOUSE_CFG port=0 Tempo 8/10 Totzone 12% bestaetigt -- vorher
  port0 Tempo 7/10 Totzone 12%, port1 Tempo 7/10 Totzone 12%, gelesen port0
  Tempo 8/10 Totzone 12%, port1 Tempo 7/10 Totzone 12%`
* bei Ablehnung:
  `[maus] SET_MOUSE_CFG port=0 Tempo 11/10 Totzone 12% -> BAD_ARG (abgelehnt);
  unveraendert gelesen: port0 Tempo 7/10 Totzone 12%, port1 Tempo 7/10
  Totzone 12%`
* bei jeder gelesenen Aenderung (auch von aussen, und einmal beim Start):
  `[maus] GET_MOUSE_CFG: port0 Tempo 8/10 Totzone 12%, port1 Tempo 7/10
  Totzone 12%`

Die Zeile nennt in **jedem** Fall beide Ports -- genau das, was zur Klaerung
von Punkt 4 gefehlt hat.

---

## 6. Geaenderte Dateien

| Datei | Was |
|---|---|
| `v4_gui.c` | Scrollgroup + Virtgroup um die Seitengruppe; Mindestmassen-Ausgabe (`MUIM_AskMinMax`); Konfigurationsseite kompakter (Klartext 2 Zeilen, Maus Tempo+Totzone in EINER Zeile, Geraetenamen 14 Zeichen, Listen `MUIA_MinHeight 20`); Fenstertexte `Buchse`->`Port`; Beitrag im Fenster `beides` (`gui_beitrag_name`), Hinweis um `eigene` und die Maus-Bedienung erweitert; Hinweis beim **Verschieben** einer Geraetezuordnung; `[maus]`-Protokollzeilen fuer GET/SET mit beiden Ports; Ruecklesewert wird uebernommen |
| `tools/check_mui_tree.py` | kennt `ScrollgroupObject` und `VirtgroupObject` samt ihrer Tags (Selbsttest unveraendert sauber) |
| `README.md` | Scrollgruppe/Mindestmasse, ein Geraet je Port + duo, Namen, `[maus]`-Zeilen |
| `BERICHT_FENSTER_PORTS_NAMEN_MAUS_2026-10-15.md` | dieser Bericht |

**Kein `Forbid()`** -- weder neu noch sonst im Code (alle Treffer sind Prosa;
nach Entfernen der Kommentare kein Code-Treffer). Kein zweiter Faden, I2C
direkt im Hauptfaden, Fenster 640x400 + SizeGadget + `-w`/`-h`.

---

## 7. `make verify` -- wortwoertliche Ausgabe

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
== Testlauf mit ASan + UBSan ==
-----------------------------------------------
Faelle : 182
Checks : 1216
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

## 8. Was offen ist

* **Die pixelgenauen Mindestmasse** beider Seiten: sie stehen nach dem
  naechsten Lauf im Protokoll (`[fenster] Mindestgroesse ...`). Die Schaetzung
  oben (Anzeige ~136, Konfiguration ~200 px) ist aus dem Baum gezaehlt und mit
  der MUI-Vorgabeschrift gerechnet -- auf dem Wirt ist MUI nicht lauffaehig.
* **Punkt 4** ist erst mit der neuen `[maus]`-Zeile endgueltig zuordenbar
  (Fenster-Clipping vs. globale Firmware). Die Oberflaeche sendet pro Port und
  prueft das Echo.
* **Ein Controller auf beiden Ports** braucht `duo`; dafuer fehlt ein
  I2C-Befehl. **Firmware-Entscheidung.**
* `make test-m68k` (qemu, Big Endian) bleibt SKIP (Cross-Werkzeuge fehlen).
