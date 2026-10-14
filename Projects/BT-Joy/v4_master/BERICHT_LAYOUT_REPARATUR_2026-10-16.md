# Bericht: Layout-Reparatur (duo bleibt, Ports passt wieder)

Stand: 16.10.2026. Projekt: `Projects/BT-Joy/v4_master` (V4-Seite).
**Nichts hochgeladen.** Der reparierte Stand liegt im Arbeitsbaum (Basis:
`2253d57dc`), noch nicht committet.

---

## 1. Ursache -- mit Belegstelle

**Der einzige Unterschied im MUI-Baum zwischen dem lesbaren Stand `e1cd82a77`
und dem zerschossenen `2253d57dc` ist die Zeile `duo` auf der Seite `Ports`.**

Beleg:

```
git diff e1cd82a77 2253d57dc -- Projects/BT-Joy/v4_master/v4_gui.c
```

liefert im Baum** genau einen** Hunk:

```
@@ -3601,6 +3833,40 @@ static int gui_build(void)
                                         End,
                                     End,
                                 End,
+                                Child, HGroup,          <- duo-Zeile
+                                    MUIA_Weight, 0,
+                                    Child, TextObject, "duo", FixWidth 96,
+                                    Child, HGroup, FixWidth 250, s_duo
+                                    Child, HGroup, FixWidth 250, s_duotext
+                                End,
                                 Child, s_cfghint = TextObject,
```

Alles andere in `2253d57dc` ist Protokoll/Logik (duo-Wrapper, DEV_GET-Layout,
Faerbung der Faehigkeiten) und **beruehrt kein Layout**. Nachgezaehlt:

| | lesbar `e1cd82a77` | zerschossen `2253d57dc` | repariert |
|---|---|---|---|
| `MUIA_FixWidth, 96` | 7 | **8** (+duo) | 8 |
| `MUIA_FixWidth, 250` | 14 | **16** (+2 duo) | 0 |
| `MUIA_FixWidth, 240` | 0 | 0 | 16 |
| Kinder der Seite `Ports` | 7 | **8** | **7** (wie lesbar) |
| Kinder der Seite `Maus` | 3 | 3 | 4 (+duo) |

### Warum das die Anzeige zerschiesst

**(a) Die Breite war schon auf der Kante.** `MUIA_FixWidth` legt die Breite
fest, unabhaengig von der Schrift. Jede Zeile der Konfigurationsseiten ist

```
96 (Beschriftung) + 250 (Port 0) + 250 (Port 1) + 2 x 4 (MUI-Gruppenabstand)
= 604 px
```

Nutzbar sind im 640er Fenster rund **620-630 px** (MUI-Rahmen). Sobald die
Scrollgruppe einen **senkrechten Balken** zeigt (≈16 px), bleiben **604-614 px**
-- die 604-px-Zeile passt dann **exakt oder gar nicht**. Und
`MUIA_Scrollgroup_FreeHoriz, FALSE` **verbietet** das Querscrollen
(`MUI_Scrollgroup.doc`: *"Specify if a scroll group should be horizontally
moveable. Defaults to TRUE."*) -- der Ueberstand wird also nicht erreichbar,
sondern beschnitten/gestaucht: die rechte Spalte (Port 1) sitzt am Rand bzw.
darueber. Genau das Bild "zerschossen".

**(b) Die zusaetzliche Zeile erzwingt den Balken.** Gezaehlt aus dem Baum, mit
der MUI-Vorgabeschrift (Topaz 8: Textzeile 8 px, Cycle/Button ≈16 px,
Gruppenabstand 4 px):

| Seite | vorher (zerschossen) | nachher (repariert) |
|---|---|---|
| Anzeige | 1 Cycle (16) + 11 Textzeilen (88) + Rohregister 2 (16) + Abstand (4) = **124 px** | **124 px** (unveraendert) |
| Ports | Klartext 2 (16) + Kopf (8) + 3 Cycles (48) + Richtungen (8) + **duo (16)** + Hinweis 4 (32) + 7 Abstaende (28) = **156 px** | Klartext 2 (16) + Kopf (8) + 3 Cycles (48) + Richtungen (8) + Hinweis 3 (24) + 6 Abstaende (24) = **128 px** |
| Maus | Kopf (8) + Maus-Zeile (16) + Hinweis (8) + Abstaende (8) = **40 px** | + duo (16) + Abstand (4) = **76 px** |

Reserviert waren der Scrollgruppe nur `MUIA_MinHeight, 160`. Die Ports-Seite
war mit **156 px** schon fast dort und durch die duo-Zeile **ueber** der
Anzeigeseite (124 px) -- der Balken wurde erzwungen, und damit griff (a).
(Sensitivitaet: bei einer 11-px-Schrift waeren es Anzeige ≈170, Ports ≈176,
Maus ≈105 -- dann bleibt ein kleiner Balken, der mit der neuen Breitenluft
aber unschaedlich ist.)

### Nebenbefund: meine eigene Messung war kaputt

Die Zeilen `[fenster] Mindestgroesse ...: 0x0 px` im Mitschnitt sind **kein**
Beleg fuer irgendetwas: `MUIM_AskMinMax` von aussen liefert auf dem Geraet
durchgehend 0 -- im **lesbaren** Lauf (13:05:07) genauso wie im zerschossenen
(13:35:30). Die Methode ist fuer das Layout **innerhalb** einer Klasse gedacht.
Das ist jetzt ersetzt (siehe 3.).

---

## 2. Aenderung (klein gehalten)

1. **Die duo-Zeile liegt jetzt auf der Seite `Maus`** (hinter der
   Tempo/Totzone-Zeile). Damit hat die Seite `Ports` **wieder genau dieselben
   7 Kinder** wie der am Geraet lesbare Stand `e1cd82a77` -- nachgezaehlt (siehe
   Tabelle oben). Kein Feld, keine Beschriftung und keine Zeile der
   Ports-Seite wurde sonst angefasst.
2. **Zellenbreite 250 -> 240** auf beiden Konfigurationsseiten (Kopfzeilen
   mitgezaehlt): Zeilenbreite **604 -> 584 px**, mit Balken 600 px -> **20-30 px
   Luft** statt "auf der Kante". Beschriftungsspalte bleibt 96 px.
3. **Die drei Knopfquellen sind noch kuerzer** (`Select` -> `Sel`,
   `Gruppe 1` -> `Grp1`): drei Auswahlfelder in 240 px passen damit auch bei
   breiterer Schrift. Die vollen Namen stehen unveraendert in der
   Klartextzeile (`v4p_taste_name`) und im Protokoll.
4. **`Totzone` -> `Totz.`** in der Maus-Zeile (240-px-Zelle).
5. **Der Hinweis der Seite `Ports` ist 4 -> 3 Zeilen**; die duo-Erklaerung
   steht jetzt im Hinweis der Seite `Maus`:
   `duo = EIN Controller auf beiden Ports (die eine Maus, die andere Joystick);
   es setzt nur die Betriebsarten und sperrt die Geraeteauswahl des duo-Ports
   (kein zweiter Controller).`
6. **Scrollgruppe: `MUIA_MinHeight 160 -> 175`, Gewicht 50 -> 60.** Die
   reservierte Hoehe deckt die hoechste Seite (Anzeige 124 px, Ports 128 px)
   bei der Vorgabeschrift ab -- dann erscheint **gar kein** Balken mehr.

Fachlich ist **nichts** verloren: duo-Auswahlfeld, duo-Klartext, Kennzeichnung
und Sperre des duo-Ports, `[duo]`-Protokollzeilen, drei Seiten,
`V4P_CAP_DEV_STATE`-Layoutwahl und die neuen Faehigkeitsbits sind unveraendert
vorhanden. Die Sperre/Kennzeichnung (`duo: Maus` / `duo: Joystick` in der
Klartextzeile der Ports-Seite) haengt nicht an der Zeile selbst.

---

## 3. Zwei zusaetzliche Auftraege (Punkt 5)

### 3.1 Die Liste fuellt sich jetzt OHNE Knopfdruck

**Bisher:** `gui_liste_auto()` holte die Tabelle nur, wenn eine **Verbindung**
erkannt wurde und die Liste leer war. Im Mitschnitt vom 16.10.2026 stand aber
`[status] Zustand 1 (SCANNING), Verbindung keine, Scan laeuft, Geraete 1` --
die Gegenstelle hatte also **gefunden**, ohne dass eine Verbindung bestand, und
es folgte **keine** `[scan]`-Zeile: die Liste blieb leer, bis jemand
"Neu suchen" drueckte.

**Jetzt:** meldet `GET_STATUS` **`dev_count > 0`** fuer eine **neue
Scan-Generation**, wird die Tabelle **von selbst** gelesen -- einmal je
Generation. Dabei wird **kein** `SCAN_START` und **kein** `SCAN_STOP` gesendet
(neues Kennzeichen `s_scan_nur_lesen`): laeuft die Suche auf der ESP32-Seite,
gehoert sie ihr; ein `SCAN_STOP` von hier waere ein Eingriff ohne Auftrag.
Nachweis im Mitschnitt (ohne Knopfdruck):

```
[scan] Gegenstelle meldet 1 Fund(e) (Scan-Generation 1180) -- die Liste wird automatisch gelesen
[scan] 1 Geraet(e) in der Liste
```

Der Knopf "Neu suchen" bleibt unveraendert (er startet weiter eine echte Suche
und holt danach die Liste).

### 3.2 Alter Stand (ohne `0x1000`) liefert weiter richtige Namen

* `v4p_dec_dev(p, len, caps, &dev)` waehlt das Layout **ausschliesslich** ueber
  `V4P_CAP_DEV_STATE`; ohne das Bit gilt `name_len`@7 / `name`@8 und
  `dev.verbunden` steht auf `V4P_DEV_VERBUNDEN_UNBEKANNT`.
* Die Faehigkeiten kommen aus `v4_master_t.caps`, gesetzt von
  `v4_info_common()` (PING/GET_INFO).
* **Zusaetzliche Absicherung im Takt:** ist vor dem Lesen der Tabelle
  `S.info_valid == 0` (kein INFO gelaufen), holt die Oberflaeche **einmal**
  `GET_INFO` nach -- sonst waere das Layout unbekannt und die Namen kaemen als
  Muell heraus (genau die Falle vom 16.10.2026).
* Geprueft: `tests/test_master.c` (`fw_ver 5 ... CHECK_STR(d.name, "8BitDo Pro
  2")`, `verbunden == V4P_DEV_VERBUNDEN_UNBEKANNT`) und Demo 15 (fw=5,
  `Faehigkeiten: 0x07FF`, **kein** `GET_DUO`/`SET_DUO` auf der Leitung, Namen
  richtig). Zusaetzlich in `tests/test_proto.c`: dasselbe Byte in beiden
  Layouts, das Faehigkeitsbit entscheidet.

### 3.3 Die Groessenmessung ist jetzt echt

`gui_mindestmasse_melden()` liest statt `MUIM_AskMinMax` die dokumentierten
Attribute `MUIA_Width/Height/LeftEdge/TopEdge` **nach** dem Oeffnen
(`MUI_Area.doc`: gueltig zwischen Show/Hide) und schreibt:

```
[fenster] Fensterinhalt: <w>x<h> px bei <l>,<t> (Fenster 640x400)
[fenster] Seitenbereich (sichtbar): <w>x<h> px bei ...
[fenster] Seitengruppe (alle Seiten): <w>x<h> px bei ...
[fenster] Seite Ports (sichtbar): <w>x<h> px
```

Damit steht nach dem naechsten Lauf **gemessen** da, ob die sichtbare Seite in
den Seitenbereich passt (`Seite ... < Seitenbereich` = kein Scrollen).

---

## 4. Wortwoertliche `make verify`-Ausgabe

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

## 5. Was der Benutzer nach dem Upload sehen soll

1. `Seite: Anzeige | Ports | Maus` -- die **Ports**-Seite ist jetzt wieder
   genau so hoch wie in der lesbaren Fassung (7 Zeilen, **ohne** duo).
2. Die **Maus**-Seite traegt zusaetzlich die Zeile `duo` (Auswahlfeld + Klartext
   `aus (jeder Controller seine Buchse)` bzw. `Port 0 Maus, Port 1 Joystick`).
3. Im Logfeld (und im Mitschnitt) erscheinen **ohne Knopfdruck**:
   `[scan] Gegenstelle meldet N Geraet(e) ... -- die Liste wird automatisch
   gelesen` und danach `[scan] N Geraet(e) in der Liste`; die Geraeteliste ist
   gefuellt.
4. Die `[fenster]`-Zeilen nennen jetzt **echte** Zahlen (nicht mehr 0x0).

**Falls es weiterhin nicht passt**, brauche ich genau diese `[fenster]`-Zeilen
und eine Angabe, welche Seite/welche Zeile betroffen ist -- daraus laesst sich
die Breite dann exakt nachrechnen (die Zeilenbreite steht mit 584 px fest).

---

## 6. Geaenderte Dateien

| Datei | Was |
|---|---|
| `v4_gui.c` | duo-Zeile von `Ports` auf `Maus` verschoben; Zellen 250 -> 240; Knopfquellen-Eintraege `Sel`/`Grp1..3`; `Totz.`; Hinweise angepasst; Scrollgruppe `MinHeight 175`, Gewicht 60; **automatisches** Lesen der Fundtabelle (`s_scan_nur_lesen`, `s_liste_gen`, Logzeile); `GET_INFO` nachholen, wenn die Faehigkeiten fehlen; Groessenmessung ueber `MUIA_Width/Height/LeftEdge/TopEdge` statt `MUIM_AskMinMax` |
| `BERICHT_LAYOUT_REPARATUR_2026-10-16.md` | dieser Bericht |

Protokoll, Master, Mock, Tests, Demo, Makefile und README sind in diesem
Schritt **unveraendert** geblieben (die duo-/DEV_GET-Faehigkeiten aus
`2253d57dc` sind unangetastet).
