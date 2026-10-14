# Bericht 2: Seiten, Richtungsschalter, duo, Geraeteliste

> **Nachtrag:** Punkt 3 (duo) ist inzwischen **gebaut** -- die Nummern der
> Firmware (fw_ver 6) sind eingetroffen. Siehe
> `BERICHT_DUO_DEVGET_2026-10-16.md`. Aussagen unten, die duo noch als offen
> bezeichnen, sind damit ueberholt.

Stand: 16.10.2026. Projekt: `Projects/BT-Joy/v4_master` (V4-Seite).
Nichts wurde hochgeladen; die Uebertragung auf die V4 macht der Auftraggeber.
Am ESP32-Projekt (`V4_ESP32_BT_Joy_BP32/`) wurde **nichts geaendert** (nur
gelesen) -- die Firmware-Punkte sind unten als Auftrag/Erwartung notiert.

Vier Punkte, je Ursache (mit Belegstelle) und Aenderung.

---

## 1. Konfigurationsseite passt jetzt ohne Scrollen

### Ursache

Die Mindesthoehe einer MUI-Seitengruppe ist das **Maximum ueber alle Seiten**
(`MUI_Group.doc`: *"the maximum minimum width/height of all its children"*).
Mit **allen** Einstellungen auf einer Seite (Geraet, Beitrag, drei
Richtungsschalter, drei Knopfquellen-Zeilen, Maus Tempo, Totzone, Klartext,
Hinweis) war die Seite hoeher als der Platz, den das 640x400-Fenster nach
Kopfbereich und Listen uebrig laesst -- der Benutzer musste scrollen
(*"muss links scrollbar benutzen"*).

### Aenderung

1. **Drei Seiten statt zwei:** `Seite: Anzeige | Ports | Maus`
   (`gui_seiten_eintraege`, `act_seite()` in `v4_gui.c`).
   * `Anzeige` = Portdaten (unveraendert).
   * `Ports` = Geraet, Beitrag, Richtungen (Klartext), Knopfquellen.
   * `Maus` = Tempo und Totzone je Port.
   Damit ist die **hoechste** Seite die Anzeigeseite, nicht mehr die
   Konfiguration.
2. **Die drei Knopfquellen stehen in EINER Zeile** (`Knopf 6/9/5`, je Port drei
   Auswahlfelder nebeneinander). MUI rechnet die Breite eines Cycles aus dem
   **laengsten** Eintrag; `"Gruppe 1"`/`"Select"` waren fuer drei Felder in
   250 px zu breit. Dafuer gibt es jetzt `gui_taste_kurz_eintraege`
   (`"Gr.1"`, `"Gr.2"`, `"Gr.3"` ...) mit Uebersetzungszeit-Sicherung auf
   `V4P_TASTE_ANZAHL`; die **vollen** Namen stehen unveraendert in der
   Klartextzeile (`v4p_taste_name`) und im Protokoll.
3. **Richtungen und Knopfquellen sind zwei Zeilen** -- genau wie gefordert.
4. **Kuerzere Beschriftungen:** `Tempo`, `Totzone` (statt `Maus Tempo`,
   `Totz.`). Die Spaltenbreite der Beschriftung ist 96 px (vorher 80), damit
   `Knopf 6/9/5` nicht abgeschnitten wird; `96 + 250 + 250 = 596` passt in die
   Fensterbreite.
5. Die Scrollgruppe bleibt als **Sicherheitsnetz** (siehe Bericht 1): sie
   verhindert, dass eine Seite das Fenster vergroessert. Sie wird jetzt im
   Normalfall **nicht mehr gebraucht**.

### Die Mindestmasse (gezaehlt und gerechnet)

Gemessen wird auf dem Geraet: nach dem Oeffnen und bei jedem Seitenwechsel
schreibt die Oberflaeche die von MUI **berechneten** Werte ins Protokoll
(`MUIM_AskMinMax`, `gui_mindestmasse_melden()`), jetzt fuer **fuenf** Objekte:

```
[fenster] Mindestgroesse Fensterinhalt: <w>x<h> px (Fenster 640x400)
[fenster] Mindestgroesse Seitengruppe (Maximum ueber alle Seiten): <w>x<h> px ...
[fenster] Mindestgroesse Seite Anzeige: <w>x<h> px ...
[fenster] Mindestgroesse Seite Ports: <w>x<h> px ...
[fenster] Mindestgroesse Seite Maus: <w>x<h> px ...
```

Auf dem Wirt (Linux) laeuft MUI nicht, ein Amiga-Emulator ist hier nicht
vorhanden -- die pixelgenauen Werte kann ich daher nicht messen. Gezaehlt aus
dem Baum und mit der MUI-Vorgabeschrift gerechnet (Topaz 8: 8 px Zeile, 4 px
Abstand, Cycle ≈ 16 px):

| Seite | Zeilen | davon Cycle-Zeilen | gerechnet |
|---|---|---|---|
| Anzeige | 11 (Direkttabelle) + 1 (Modus) + 2 (Rohregister) = 14 | 1 | ≈ 136 px |
| Ports | 2 (Klartext) + 1 (Kopf) + Geraet + Beitrag + Richtungen + Knopf 6/9/5 + 3 (Hinweis) = 10 | 3 | ≈ 132 px |
| Maus | 1 (Kopf) + 1 (Tempo/Totzone) + 1 (Hinweis) = 3 | 1 | ≈ 40 px |

Verfuegbar sind im 640x400-Fenster rund **170-190 px** (400 minus Rahmen,
Kopfbereich und zwei Listen). **Damit passen alle drei Seiten ohne Scrollen**
(die hoechste ist die Anzeigeseite mit ≈ 136 px). Die exakten Zahlen stehen
nach dem naechsten Lauf in den fuenf `[fenster]`-Zeilen.

---

## 2. Checkboxen bei den Richtungen sind weg

### Ursache

Die drei Schalter (Stick links / Stick rechts / D-Pad) waren eine **zweite
Stelle fuer dieselbe Aussage**: welche Richtungen gelten, legt der **Beitrag**
fest (`analog` -> Stick links, `digital` -> D-Pad, `beides` -> alle). Widersprach
der Benutzer dem Beitrag, war nicht klar, was nun gilt -- und bei `beides`
mussten die Schalter zusaetzlich gesperrt werden. Zu Recht verwirrend.

### Aenderung

* Die Schalter, ihre Notifies (`ID_DIR0`/`ID_DIR1`), `act_set_dir()`,
  `gui_dir_aus_schaltern()` und die Helfer `gui_check()`/`gui_check_set()` sind
  **entfernt**. Die Kommandos `SET_PORT_ROUTING` werden weiterhin gesendet --
  jetzt ausschliesslich vom Beitrag (`act_set_beitrag()`), der die
  Richtungsmaske setzt.
* **Stattdessen zeigt die Seite `Ports` die Richtungen als Klartext an** --
  eine Zeile `Richtungen` je Port, gefuellt aus `v4p_dir_text(S.portcfg.dir[port])`
  (z. B. `Stick links`). Nur Anzeige, nicht bedienbar; die Werte bleiben im
  Klartext oben (`Richtungen Stick links`) und im Protokoll sichtbar.
* **Die drei Knopfquellen bleiben bedienbar** -- auch bei `beides`. Sie sagen,
  welche Taste auf Pin 6/9/5 liegt, und das ist eine echte Wahl; die
  "gegenstandslos"-Sperre und der zugehoerige Hinweistext sind damit weg.
* Der Hinweis auf der Seite `Ports` erklaert die Varianten:
  `analog = Stick links + L1/R1, digital = D-Pad + A/B/Start, beides = alle
  Richtungen + Gruppe 1/2/3, eigene = passt zu keiner Variante.`

---

## 3. Dasselbe Geraet zweimal (einmal analog, einmal digital)

### Ursache: die Firmware haelt je Geraet genau EINE Buchse

`V4_ESP32_BT_Joy_BP32/main/v4_devices.c`, `v4_devices_zuordnen()`, Zeile 413 ff.:

```c
if (d->port != port) {
    d->port = port;              /* EIN Port je Geraet */
    geaendert = true;
}
```

Dieselbe BDA fuer den anderen Port zu waehlen **verschiebt** die Zuordnung.
Die Oberflaeche hat dabei nichts falsch gemacht: sie sendet beide Male
`SET_DEVICE_PORT` (Mitschnitt 12:40:51 Port 0, 12:40:53 Port 1, 12:40:55
Port 0, 12:41:15 Port 1), und der ESP32 antwortet `OK`. Dass "die anderen
Kombinationen richtig sind", passt dazu: bei **verschiedenen** Geraeten gibt es
kein Verschieben -- nur bei **derselben** BDA.

### Der richtige Weg ist `duo`, nicht die doppelte Zuordnung

`V4_ESP32_BT_Joy_BP32/main/v4_platform.c` (Zeile 269 ff.): *"duo: EIN
Controller auf BEIDEN Buchsen ... Deshalb legt duo die eine Buchse auf MAUS und
die andere auf JOYSTICK."* duo braucht **eine** Verbindung, nicht zwei
Zuordnungen. Es ist eine Einstellung der ESP32-Seite und hat in
`V4_ESP32_BT_Joy_BP32/main/v4_proto.h` **keinen I2C-Befehl** (Kommandoliste
0x01..0x0C, 0x10.., 0x20.., kein `DUO`).

### Aenderung (Jetzt) und was fuer den duo-Schalter fehlt

* Beim **Verschieben** einer Zuordnung sagt die Oberflaeche das jetzt
  **konkret**: `[cfg] Hinweis: "8BitDo 64 BT" war Port 0 zugeordnet und ist
  jetzt Port 1 -- ein Geraet hat im ESP32 immer genau EINE Buchse. Fuer EINEN
  Controller auf beiden Buchsen ist der duo-Betrieb des ESP32 zustaendig (nicht
  ueber I2C einstellbar).`
* Der Hinweis auf der Seite `Ports` sagt es ebenfalls:
  `Geraet: genau EIN Port je Geraet -- beide Ports = duo im ESP32.`
* **Der duo-Zustand ist NICHT eingebaut, weil mir die Nummern fehlen.** Sobald
  mir der Firmware-Agent nennt: **Kommandonummer fuer `GET_DUO` und `SET_DUO`,
  das Faehigkeitsbit aus `GET_INFO`, die Bedeutung der Werte** (z. B. 0 = aus,
  1 = Port 0 Maus / Port 1 Joystick, 2 = umgekehrt) **und ob die Antwort das
  Echo enthaelt**, baue ich:
  * Anzeige und Schalter "Ein Controller auf beiden Ports (Port 0 Maus /
    Port 1 Joystick)" auf der Seite `Ports`,
  * bei aktivem duo gehoert die zweite Buchse **duo** und nicht der
    Geraeteliste -- die Oberflaeche **sagt** das dann (statt eine Zuordnung
    vorzutaeuschen), und die Geraeteauswahl fuer die zweite Buchse wird
    gesperrt/erklaert,
  * ohne Faehigkeitsbit bleibt der Schalter gesperrt mit Klartextgrund
    (dasselbe Muster wie bei `GET/SET_MOUSE_CFG`).

---

## 4. Zustand in der Geraeteliste

### Ursache

`DEV_GET` liefert nur `idx`, BDA, Namenslaenge, Flags und Namen
(`v4p_dev_t` in `v4_proto.h`) -- **kein** Feld "verbunden". Die Liste konnte
also nichts ueber den Zustand sagen.

### Aenderung: die vorhandene Quelle nutzen, nichts behaupten

* **`GET_STATUS.conn_index` ist genau der Index des verbundenen Geraets in der
  Tabelle, aus der `DEV_GET` liest.** Damit steht hinter der MAC jetzt
  `- verbunden` / `- nicht verbunden`
  (`gui_dev_verbunden_text()`, benutzt in `gui_devices_fill()`):
  `8BitDo 64 BT  (E4:17:D8:5F:E6:C9)  - verbunden`
* **Ohne gueltige Statusantwort steht dort `unbekannt`** -- eine fehlende
  Antwort ist keine Aussage; "nicht verbunden" waere eine Behauptung.
* Die Liste wird bei einer **Aenderung** des Verbindungszustands nachgezogen
  (`gui_liste_zustand_nachziehen()`, aufgerufen im langsamen Takt); der aktive
  Eintrag bleibt dabei erhalten. Im Ruhezustand passiert nichts -- die Regel
  "eine Debugzeile je Sekunde" bleibt.
* Sobald der Firmware-Agent das Feld in `DEV_GET` nachreicht, wird die Anzeige
  darauf umgestellt (dann kann sie auch "verbunden" sagen, wenn `GET_STATUS`
  gerade fehlt).

### "wenn verbunden laeuft kein scan mehr"

Das ist BluePad32-Verhalten (gesucht wird erst nach dem Trennen). Die
Zustandszeile **erklaert** das jetzt statt einen leeren Zustand zu zeigen:

```
Zustand 3 (verbunden), Verbindung aktiv, Scan aus (Verbindung aktiv -- Suche pausiert), Geraete 1, Scan-Generation 4
```

---

## 5. Geaenderte Dateien

| Datei | Was |
|---|---|
| `v4_gui.c` | drei Seiten (`Anzeige`/`Ports`/`Maus`), Knopfquellen in EINER Zeile mit kurzen Eintraegen, Richtungsschalter und ihre Helfer entfernt, Richtungen als Klartext, kuerzere Beschriftungen, Mindestmassen-Ausgabe fuer fuenf Objekte; Geraeteliste mit Verbindungszustand (aus `conn_index`) und Nachziehen bei Aenderung; Zustandszeile mit "Verbindung aktiv -- Suche pausiert"; duo-Hinweis konkret |
| `README.md` | drei Seiten, keine Richtungsschalter mehr, Geraeteliste-Zustand, "Suche pausiert" |
| `BERICHT_SEITEN_DUO_LISTE_2026-10-16.md` | dieser Bericht |

**Kein `Forbid()`** (alle Treffer sind Prosa), kein zweiter Faden, I2C im
Hauptfaden, Fenster 640x400 + SizeGadget + `-w`/`-h`, `V4_ESP32_BT_Joy_BP32/`
nur gelesen.

---

## 6. `make verify` -- wortwoertliche Ausgabe

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

## 7. Offen

* **Pixelgenaue Mindestmasse** der drei Seiten: stehen nach dem naechsten Lauf
  in den `[fenster]`-Zeilen (auf dem Wirt nicht messbar). Gezaehlt/gerechnet:
  Anzeige ≈ 136 px, Ports ≈ 132 px, Maus ≈ 40 px, verfuegbar ≈ 170-190 px.
* **`GET/SET_DUO`**: Nummern, Faehigkeitsbit, Werte und Echo werden erwartet
  (Punkt 3).
* **`DEV_GET`-Feld "verbunden"**: bis dahin zeigt die Liste den Zustand aus
  `GET_STATUS.conn_index` bzw. `unbekannt`.
* `make test-m68k` (qemu, Big Endian) bleibt SKIP (Cross-Werkzeuge fehlen).
