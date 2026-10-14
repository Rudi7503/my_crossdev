# Bericht 2: Seiten, Richtungsschalter, duo, Geraeteliste

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

<!-- VERIFY_AUSGABE -->

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
