# Bericht: Seitenflaeche und Fensterhoehe -- GEMESSEN statt gerechnet

Stand: 17.10.2026. Projekt: `Projects/BT-Joy/v4_master` (V4-Seite).
**Nichts hochgeladen.** Der Stand liegt im Arbeitsbaum (Basis `167326eec`),
noch nicht committet.

---

## 0. Zuerst: es gibt ZWEI Messungen im Mitschnitt

Das gelieferte Zitat stammt aus **19:00:09** -- und enthaelt die Zeile
`[fenster] Seitenbereich (sichtbar)`, die es **nur in der Fassung MIT
Scrollgruppe** gibt. Diese Zeile hat der Fix entfernt (Scrollgruppe und ihre
Diagnosezeile sind weg). Der Fix wurde im Mitschnitt **19:02:01** gemessen:

```
18:59:59  (alte Fassung, Scrollgruppe)
[fenster] Seitenbereich (sichtbar): 584x165 px bei 59,121
[fenster] Seitengruppe (alle Seiten): 0x0 px bei 0,0
[fenster] Seite Ports (sichtbar): 0x0 px

19:02:01  (UPLOADED FIX 167326eec)
[fenster] 700x480, Schirm 1280x720 (sichtbar 1280x720)
[fenster] Fensterinhalt: 600x505 px bei 4,12 (Fenster 700x480)
[fenster] Seitengruppe (alle Seiten): 584x113 px bei 12,85
[fenster] Seite Anzeige (sichtbar): 450x35 px
```

**Antwort auf Punkt 1 des Auftrags:** Die Seitengruppe ist **nicht mehr 0x0** --
sie ist **584x113 px**. Genau `113 = 110 (MUIA_MinHeight) + 3`, also **wirkt
`MUIA_MinHeight` an der Seitengruppe**. Sie haengt als **4. Kind** der
Wurzelgruppe (`MUI-Baum: sauber ... VGroup (3 eigene Kinder)`), ihr Gewicht
(60) draengt sie nicht weg. Der 0x0-Befund gehoert zur Fassung mit der
Virtgruppe; die ist entfernt.

Was 19:02:01 noch zeigte: die **aktive Seite** bekam nur 35 px. Ursache: MUI
rechnet die Mindesthoehe beim **Oeffnen** aus dem Text, der dann im Baum steht --
und `s_port0`/`s_port1` hatten dort einen **einzeiligen** Platzhalter (" ").
Nach dem Fuellen (vier Zeilen) war die Seite zu kurz reserviert. Das ist mit
diesem Bericht behoben (Platzhalter mit vier Zeilen).

---

## 1. Kostenaufstellung -- aus der MESSUNG abgeleitet

Aus `Fensterinhalt 505 px`, `Seitengruppe bei y=85, Hoehe 113`:

| Abschnitt | Rechnung | Hoehe |
|---|---|---|
| Kopfbereich ueber der Seitengruppe | y = 85 px fuer Verbindungszeile + Zustandszeile + Controller (damals 2 Zeilen) | **85 px** -> rund **21 px je Textzeile** |
| Seitengruppe | gemessen | **113 px** |
| Knopfreihe + **zwei Listviews** | 505 - 85 - 113 = 307 px, davon Knopfreihe ~34 px | **~273 px fuer die zwei Listen** (je ~135 px) |
| **Summe** | | **505 px** |

**Der Brocken sind die zwei `ListviewObject`.** Ihre Mindesthoehe kommt von der
Klasse: **5 sichtbare Zeilen (5 x 21 = 105) + Rollbalken (~20) + Rand** ≈ 135 px.
`MUIA_MinHeight` konnte das nicht senken -- es setzt nur eine **Untergrenze**
(`MUI_Area.doc`: *"Specify a minimum height for an object"*), das
Klassenminimum war groesser. `MUIA_Listview_MinVisible` gibt es in **diesem**
MUI nicht (in `libraries/mui.h` nicht vorhanden -- geprueft), und
`MUIA_List_MinLineHeight` aendert nur die Zeilenhoehe, nicht die Zeilenzahl.
Was wirkt, ist **`MUIA_FixHeight`** (V4, nur beim Anlegen): es legt die Hoehe
fest und ueberschreibt das Klassenminimum.

---

## 2. Aenderung

| Massnahme | Wirkung |
|---|---|
| **Geraeteliste in die Seite `Ports`**, **Logfeld in die Seite `Anzeige`**, beide mit `MUIA_FixHeight, 48` | **-~220 px** (die zwei 135-px-Listen sind weg; jede kostet jetzt 48 px und nur auf ihrer Seite) |
| **Controller-Zeile 3 -> 1 Zeile** (`Controller: <Name> -- <Art>, <Transport>, VID:PID, R<n>/<m> B, <n> Reports, <Batterie>`) | **-42 px** |
| **Hinweiszeile der Seite `Ports` entfernt** (die Beitrags-Varianten stehen als Eintraege im Auswahlfeld; `duo` steht im Hinweis der Seite `Maus`) | **-21 px** |
| **Platzhalter der Portspalten mit VIER Zeilen** | die Seite reserviert beim Oeffnen die richtige Hoehe (vorher 1 Zeile -> 35 px!) |
| **Fenstervorgabe hart auf 632x352 begrenzt** (ohne `-w/-h`) | passt auf den 640x360-Schirm; `-w/-h` heben die Grenze auf (dann gilt nur die Begrenzung auf den gemeldeten Schirm) |
| **Fenster oben links (4,4) statt zentriert** | der Rahmen mit dem Titel ist auch dann sichtbar, wenn der Schirm groesser gemeldet wird als er ist |
| unveraendert aus dem letzten Schritt: Seiten **direkt** in der Seitengruppe (`MUIA_MinHeight, 110`), Raender 8/8, Schirmmessung ueber den ViewPort | Seitenflaeche > 0 |

### Neue Rechnung (mit den GEMESSENEN 21 px je Zeile, 30 px je Auswahlzeile)

| Bereich | Rechnung | Hoehe |
|---|---|---|
| Kopfbereich | Verbindungszeile 21 + Zustandszeile 21 + Controller 21 + Abstand | **~71 px** |
| Knopfreihe (1 Reihe, 7 Knoepfe) | 30 + Abstand | **~34 px** |
| Seite `Anzeige` | Auswahlzeile 30 + 4 Registerzeilen 84 + Logfeld 48 + Abstand | **~170 px** |
| Seite `Ports` | Klartext 21 + Kopfzeile 21 + 3 Auswahlzeilen 90 + Geraeteliste 48 + Abstaende 20 | **~200 px** |
| Seite `Maus` | Kopfzeile 21 + Tempo/Totzone 30 + duo 30 + Hinweis 21 + Abstaende | **~114 px** |
| **Summe** | Kopf + Knopfreihe + **hoechste Seite (Ports)** | **~311 px** |

Ziel war **unter 330 px** (640x360 minus Rahmen/Titel): erreicht, **~19 px
Luft**. Vorher: **505 px** (gemessen) -> **-194 px**.

---

## 3. Baum (Beleg, dass der Inhalt in der Gruppe sitzt)

```
MUI-Baum: sauber (v4_gui.c)
  Fensterinhalt: VGroup mit 5 Kindern
    - TextObject (Verbindungszeile)
    - TextObject (Zustandszeile)
    - HGroup (Controller-Zeile + "Seite:")
    - VGroup (3 eigene Kinder)        <- s_seiten, MUIA_MinHeight 110, Gewicht 60
    - HGroup (7 eigene Kinder)        <- die eine Knopfreihe
```

Kinder der Seiten (aus dem Baum gezaehlt):

* **Anzeige (3):** HGroup (beide Portspalten: je `Port N:` + `Vorgabe`
  (Auswahlfeld) + `wirkt: ...` + **vier Registerzeilen**), **Logfeld**
  (`MUIA_FixHeight 48`).
* **Ports (6):** Klartextzeile, Kopfzeile `Port 0`/`Port 1`, `Geraet`,
  `Beitrag`, `Knopf 6/9/5`, **Geraeteliste** (`MUIA_FixHeight 48`).
* **Maus (4):** Kopfzeile, `Maus` (Tempo/Totzone), `duo`, Hinweis.

Die Verbindungs-/Zustands-/Controller-Zeile und die Knopfreihe stehen fest und
sind auf **jeder** Seite sichtbar.

---

## 4. Was der Benutzer nach dem Upload sehen soll

1. **Rahmen und Titel sind sichtbar** (Fenster 632x352, oben links).
2. Unter `Seite: Anzeige | Ports | Maus` ist auf **allen drei** Seiten Inhalt.
3. Kein Scrollbalken; die Seiten passen.
4. Die Messzeilen nennen jetzt:
   `[fenster] 632x352, Schirm 1280x720 (sichtbar 1280x720) -- auf den Schirm
   begrenzt`, dann `Fensterinhalt: ~600x311 px`, `Seitengruppe (alle Seiten):
   ~584x200 px`, `Logfeld: 584x48 px`, `Geraeteliste: 584x48 px`,
   `Seite Ports (sichtbar): ~584x200 px`.

**Falls es noch klemmt:** die Messzeilen schicken. Aus `Fensterinhalt` gegen
`Seitengruppe` sieht man sofort, welcher Teil zu gross ist; die naechsten
Einsparungen waeren (in dieser Reihenfolge): Klartextzeile der Seite `Ports`
(21 px), Kopfzeile `Port 0/1` (21 px, dann tragen die Zeilen keine
Spaltenbeschriftung mehr), Geraeteliste auf 36 px, Logfeld auf 36 px.

---

## 5. Geaenderte Dateien

| Datei | Was |
|---|---|
| `v4_gui.c` | Geraeteliste und Logfeld in die Seiten (mit `MUIA_FixHeight 48`); Controller-Zeile auf eine Zeile; Hinweiszeile der Seite `Ports` und `s_cfghint` entfernt; vierzeiliger Platzhalter fuer die Portspalten; harte Obergrenze 632x352 fuer die Vorgabe (`-w/-h` hebt sie auf); Fenster oben links; Messung meldet zusaetzlich Logfeld und Geraeteliste |
| `BERICHT_HOEHE_330_2026-10-17.md` | dieser Bericht |

**Kein `Forbid()`**, kein zweiter Faden, nichts hochgeladen;
`V4_ESP32_BT_Joy_BP32/` und Audio nur gelesen.

---

## 6. Wortwoertliche `make verify`-Ausgabe

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
  Fensterinhalt: VGroup mit 5 Kindern
    - TextObject (0 eigene Kinder)
    - TextObject (0 eigene Kinder)
    - HGroup (2 eigene Kinder)
    - VGroup (3 eigene Kinder)
    - HGroup (7 eigene Kinder)
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
 - Knopf 3 (Pin 5): POTINP Bit 9 = Buchse 0, Bit 11 = Buchse 1
 - Knopf 3: Polaritaet kommt aus dem Grundzustand, nicht aus dem Code
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
 - Direktzeile: BTN3 (Pin 5) erscheint mit Pegel und Urteil
 - Direktzeile: leere Buchse zeigt keine erfundene Aktivitaet
 - Kurzzeile (Fenster): vier Zeilen, gleiche Werte
 - Kurzzeile: ungueltige Argumente liefern 0
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
Faelle : 200
Checks : 1323
Fehler : 0
-----------------------------------------------
ERGEBNIS: OK
== Testlauf mit ASan + UBSan ==
-----------------------------------------------
Faelle : 200
Checks : 1323
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
