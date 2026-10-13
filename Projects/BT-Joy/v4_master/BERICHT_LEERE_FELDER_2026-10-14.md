# Bericht: leere Anzeigefelder bei bestehender Verbindung + Rahmenpruefung als Zaehler

Projekt: `Projects/BT-Joy/v4_master/` (Vampire V4, Bluetooth-Joystick ueber ESP32)
Stand: 14.10.2026
Auftrag: (1) leere Auswahl-/Anzeigefelder bei bestehender Verbindung beheben,
(2) die Rahmenpruefung von "letzter Zustand" auf Zaehler umbauen, (3) kein
`Forbid()`, (4) Fenster bleibt 640x400 + SizeGadget + `-w`/`-h`, (5)
Ausgabemenge nicht erhoehen, (6) `make verify` mit Exit 0, (7) nichts hochladen.

**Nachtrag vom selben Tag:** der Benutzer hat die Fassung aus den Abschnitten
1-6 auf der V4 gefahren (Mitschnitt ab 19:09:57) -- Verbindungs- und
Controller-Zeile sind gefuellt, die automatische Geraeteliste und die
Rahmenzaehler arbeiten. Offen waren danach nur noch die beiden
**Auswahlknopfe der Betriebsart** (`Buchse 0:` / `Buchse 1:`): zu gross und
leer. Ursache, Beleg und Korrektur stehen in **Abschnitt 7**.

Vorgaengerbericht: `BERICHT_ABSTURZ_2026-10-14.md` (Absturz der Oberflaeche,
Umbau der Ausgabe). Dieser Bericht baut darauf auf und aendert an der dort
beschriebenen Bauart nichts: I2C bleibt direkt im Hauptfaden, kein zweiter
Faden, kein `Forbid()`.

---

## 1. Die leeren Anzeigefelder -- Befund und Ursache

### 1.1 Was der Mitschnitt belegt

Grundlage ist `Projects/BT-Joy/logs/v4_seriell.log` (DebugUART der V4,
6676 Zeilen). Der letzte vollstaendige Lauf der Oberflaeche darin ist
**17:37:47 - 17:43:08** (Fassung `0.2 (13.10.2026)`, also der zuletzt
uebersetzte Stand):

| Zeile | Zeit | Inhalt |
|-------|------|--------|
| 5709 | 17:37:49 | `[mui] Fenster offen (640x400). Ab hier werden MUI-Objekte angefasst.` |
| 5711 | 17:37:49 | `[modus] Buchse 0 = aus, Buchse 1 = aus -- GET_PORT_MODE` |
| 5712 | 17:37:49 | `[eingang] state0=0x00 state1=0x00 level0=0x7F level1=0x7F steps=0/0` |
| **5719** | **17:38:14** | **erste `[status]`-Zeile** (`Zustand 0 (IDLE), Verbindung keine, Scan aus, Geraete 0, Scan-Generation 0`) |
| 5721 | 17:38:14 | erste `[hid]`-Zeile (`kein Controller verbunden`) |
| 5735 | 17:38:23 | `[hid] 8BitDo 64 BT, Art Pad, Transport BLE, VID:PID 2DC8:3019, ...` (Controller verbindet sich von selbst) |
| 6075 | 17:41:52 | `[fehler]   tx_frames=1427 rx_frames=1424 wiederholungen=8 letzte_Rahmenpruefung=CRC-Fehler busfehler=0x00000200 (no reply)` -- der haftenbleibende Wert aus Aufgabe 2 |

Damit ist zweierlei belegt:

1. **Der Datenweg ist in Ordnung.** `GET_STATUS` und `GET_HID` antworten mit
   `status=OK`, der Controller wird mit Name, Art und VID:PID geliefert und in
   `v4_state_t S` abgelegt (`v4_gui.c`, `gui_poll`, `v4_state_put_status` /
   `v4_state_put_hid`). An der Auswertung oder den Feldnamen liegt es NICHT.
2. **Die Anzeige kam 25 Sekunden zu spaet** und die Geraeteliste wurde nie
   geholt (im ganzen Mitschnitt gibt es **keine einzige** `[scan]`-Zeile der
   Oberflaeche: `grep -c "die Liste fuellt sich\|SCAN_START" v4_seriell.log`
   = 0).

### 1.2 Ursache A (belegt): der Zwischenspeicher wurde erst im 5. Takt gefuellt

Die Oberflaeche zeichnet `s_state` (Verbindung) und `s_ctrl` (Controller) aus
dem Zustandsblock. Gefuellt wurde der nur im langsamen Zweig von `gui_poll`:

```c
v4_gui.c:1127      if (slow != 0) {
v4_gui.c:1129          rc2 = v4_get_status(&M, &st);
v4_gui.c:1110          v4_state_put_status(&S, &st);      (alt: Zeile 1110)
v4_gui.c:1118          rc2 = v4_get_hid(&M, &hid);
v4_gui.c:1120          v4_state_put_hid(&S, &hid);         (alt: Zeile 1120)
```

und `slow` wurde allein aus dem Taktzaehler gebildet:

```c
v4_gui.c:1394 (alt)   slow = ((s_tick % GUI_SLOW_EVERY) == 0u || s_connect_left > 0u) ? 1 : 0;
```

`s_tick` wird direkt davor erhoeht und startet bei 1 -- **der erste Takt nach
dem Oeffnen des Fensters war also kein langsamer Takt.** Bis zum fuenften Takt
standen in den beiden Feldern nur die Anfangstexte aus `gui_build`:

```c
v4_gui.c:1671   Child, s_state = TextObject,
v4_gui.c:1672       MUIA_Text_Contents, (IPTR)" ",
v4_gui.c:1675   Child, s_ctrl = TextObject,
v4_gui.c:1676       MUIA_Text_Contents, (IPTR)"Controller: (wird gelesen)",
```

Genau das sieht wie ein **leeres Anzeigefeld** aus, obwohl ein Controller
verbunden ist. Im Mitschnitt dauerte dieser Zustand **25 s** (17:37:49 ->
17:38:14), weil der ESP32 in dieser Zeit beschaeftigt ist: die BluePad32-Fassung
sucht fortlaufend (`V4_ESP32_BT_Joy_BP32/docs/PROTOKOLL.md`: "Kein Suchfenster.
BluePad32 sucht fortlaufend"), und `GET_STATUS`/`GET_HID` kamen erst mit dem
ersten langsamen Takt durch.

**Der Unterschied zur Konsole -- die Spur des Auftrags:** die Konsole holt und
zeigt Status, Controller und Buchsen SOFORT beim Start, ohne Zwischenspeicher:

```c
v4_console.c:791   v4_msg("\n-- Anfangszustand --\n");
v4_console.c:792   (void)act_status();
v4_console.c:793   (void)act_hid();
v4_console.c:794   (void)print_ports();
```

`print_status` (`v4_console.c:204`) und `print_hid` (`v4_console.c:227`) drucken
die frisch empfangene Struktur; die Oberflaeche zeigt denselben Wert erst,
wenn der langsame Takt ihn in den Zustandsblock geschrieben hat.

### 1.3 Ursache B (belegt): die Auswahlliste wurde nur per Knopf gefuellt

Die Geraeteliste (`s_devs`) wurde ausschliesslich von `act_scan_start()`
(Knopf "Neu suchen") und `gui_scan_tick()` gefuellt. Ein Controller, der sich
von selbst verbindet (BluePad32 verbindet, sobald das Geraet werbend
auftritt -- im Mitschnitt zweimal passiert, Zeile 5735 und 5954), tauchte dort
**nie** auf, weil der Knopf nie gedrueckt wurde. Die Liste blieb also leer --
"die Listen" aus der Fehlermeldung.

### 1.4 Vorsorge C (nicht auf Hardware bewiesen, aber ausgeschlossen statt vermutet)

Zwei MUI-Eigenschaften koennen ein *gefuelltes* Feld ebenfalls leer aussehen
lassen. Beide sind jetzt ausgeschlossen:

* **MUI kann Kinder unter ihre Mindestgroesse druecken.** Der Fensterinhalt
  (4 Kopffelder, zwei 12-zeilige Buchsengruppen, Vergleichsfeld, zwei
  Knopfreihen, zwei Listen) ist lang, das Fenster ist auf 640x400 festgelegt.
  Ein Textobjekt hat ohne eigenes `MUIA_Weight` das Vorgabegewicht **100**;
  reicht der Platz nicht, verteilt MUI den fehlenden Platz nach Gewicht und
  kann ein Textobjekt unter seine Mindesthoehe druecken -- es sieht dann leer
  aus, obwohl der Inhalt gesetzt ist. Die Area-Autodokumentation sagt dazu
  eindeutig (lokale MUI-Doku,
  `Compilers/GCC-6.50-Dev/m68k-amigaos/doc/MUI_Area.doc:1302`):
  *"An object with a weight of 0 will always stay at its minimum size."*
* **Ein langer Text zieht das Fenster breit.** `MUIA_Text_SetMin` ist per
  Vorgabe TRUE (`MUI_Text.doc:305-311`): *"Boolean value to indicate whether
  the object's minimal width shall be calculated to fit the string given with
  MUIA_Text_Contents."* Die Controller-Zeile ist rund 100 Zeichen lang
  (Art, Transport, VID:PID, Reports, Batterie) -- ihre Breite waere damit die
  Mindestbreite des Fensterinhalts, und MUI zieht das Fenster ueber die
  zugesagten 640 Pixel auf.

Beide Punkte sind **nicht** als bewiesene Ursache des Benutzerbefunds
behauptet: der Datenweg war nachweislich in Ordnung, und ob MUI auf dem Geraet
tatsaechlich gequetscht hat, laesst sich ohne Blick auf das Fenster nicht
messen. Die Aenderungen schliessen die Moeglichkeit aus, statt sie zu
diskutieren.

### 1.5 Was jetzt im Fenster steht

Nach dem ersten Takt (also sofort nach dem Oeffnen) und danach bei jeder
Aenderung:

* **Buchsengruppen:** `Buchse 0  Modus Joystick` samt Zug/Pegel je Leitung und
  die Cycle-Knoepfe auf dem zuletzt bestaetigten Modus,
* **Zustandszeile:** `Zustand 3 (CONNECTED), Verbindung aktiv, Scan aus,
  Geraete 1, Scan-Generation 4`,
* **Controller-Zeile** (drei Zeilen, damit Name/Art/VID:PID auch bei 640 Pixeln
  sichtbar bleiben):
  ```
  Controller: 8BitDo 64 BT
  Art Pad, Transport BLE, VID:PID 2DC8:3019
  Report-ID 0, Reportlaenge 0 Byte, Reports 17, Batterie unbekannt
  ```
* **Geraeteliste:** fuellt sich jetzt von selbst, sobald eine Verbindung steht
  und die Liste leer ist (einmal je Verbindung, derselbe Verkehr wie ein
  Knopfdruck) -- der verbundene Controller steht mit Name und Adresse darin.

---

## 2. Geaenderte Dateien

| Datei | Was |
|-------|-----|
| `v4_gui.c` | **Aufgabe 1.** Der erste Takt ist ein vollstaendiger Takt (`s_tick == 1u` in der `slow`-Bildung, `gui_tick`) -- Status und Controller stehen sofort im Fenster wie in der Konsole. Neuer Automatismus `gui_liste_auto()` (gerufen aus `gui_poll`): steht laut `GET_STATUS` eine Verbindung und ist die Geraeteliste leer, wird sie genau einmal automatisch geholt (`act_scan_start(1)`, neue Meldung `[scan] Verbindung erkannt -- die Geraeteliste wird automatisch geholt`). `act_scan_start()` hat dafuer den Parameter `automatisch`. Controller-Zeile auf drei Zeilen umgestellt (wichtige Angaben vorn). **MUI-Aufbau:** die Kopffelder, beide Buchsengruppen, das Vergleichsfeld und beide Knopfreihen haben `MUIA_Weight, 0`, die vier Kopffelder zusaetzlich `MUIA_Text_SetMin, FALSE`; die beiden Listen haben `MUIA_MinHeight, 36` und bleiben die einzigen Kinder mit Gewicht (30/40). Fenster unveraendert 640x400 + SizeGadget + `-w`/`-h`. |
| `v4_debug.h/.c` | **Aufgabe 2.** Drei Zaehler `geprueft` / `fehlerhaft` / `fehlerserie` (Serie wird von einem gueltigen Rahmen auf 0 gesetzt). Sie erscheinen in Herzzeile, Fehlerzeile und Schlusszeile. `letzte_Rahmenpruefung=%s` aus `m->last_check` ist aus der Fehlerzeile **entfernt**. Neue oeffentliche Helfer `v4_dbg_rahmen_zaehler()` und `v4_dbg_rahmen_text()` (EINE Schreibweise fuer alle Zeilen). `v4_dbg_init()` setzt die Zaehler dieser Schicht auf 0 (Programmstart). Der bestehende Zaehler `verworfene_rahmen` bleibt. |
| `v4_console.c` | Die Fehlerdiagnose der Konsole benutzt dieselben Zaehler (`print_failure`, `v4_dbg_rahmen_text`) statt `letzte Rahmenpruefung: %s`. `Versuche=`/`Wiederholungen=` bleiben. |
| `tests/test_debug.c` | **neu:** Testfall "Rahmenpruefung: geprueft/fehlerhaft/fehlerserie statt letztem Zustand" -- gueltiger Rahmen, einzelner Aussetzer (Serie 1), zwei in Folge (Serie 2), Erholung (Serie 0), Textform, Vorkommen in Herz-/Fehler-/Schlusszeile, und dass `letzte_Rahmenpruefung` **nicht** mehr vorkommt. |
| `Makefile` | `demo` prueft zusaetzlich: die Fehlerzeile traegt `geprueft=`, `letzte_Rahmenpruefung` kommt nicht mehr vor, die Schlusszeile traegt die drei Zaehler. |
| `README.md` | Rahmenpruefung als Zaehler (Tabelle der Felder, Beispielzeilen), Controller-Zeile in drei Zeilen, erster Takt vollstaendig, automatische Geraeteliste, `MUIA_Weight, 0` / `MUIA_Text_SetMin, FALSE`. |
| `tools/check_mui_tree.py` | **nicht geaendert** -- die neuen Tags (`MUIA_Weight`, `MUIA_MinHeight`, `MUIA_Text_SetMin`) waren bereits erlaubt; der Pruefer laeuft unveraendert sauber. |

Nicht angetastet: `v4_proto.*`, `v4_master.*`, `v4_state.*`, `v4_mock.*`,
`v4_amiga_i2c.c`, `v4_linux_i2c.c`, das Audio-Schwesterprojekt
(`Projects/I2C-test/v4_master/`) und `V4_ESP32_BT_Joy_BP32/` (nur gelesen).

---

## 3. Die Rahmenpruefung als Zaehler (Aufgabe 2)

### 3.1 Vorher / Nachher

Vorher stand in jeder Fehlerzeile der **letzte** Pruefbefund:

```
[fehler]   tx_frames=1427 rx_frames=1424 wiederholungen=8 letzte_Rahmenpruefung=CRC-Fehler busfehler=0x00000200 (no reply)
[fehler]   tx_frames=1435 rx_frames=1424 wiederholungen=16 letzte_Rahmenpruefung=CRC-Fehler busfehler=0x00000200 (no reply)
[fehler]   tx_frames=1443 rx_frames=1424 wiederholungen=24 letzte_Rahmenpruefung=CRC-Fehler busfehler=0x00000200 (no reply)
```

(Mitschnitt 13.10.2026, Zeilen 6075/6081/6084.) Ein einziger CRC-Fehler -- der
Rahmen, der beim Beenden des Programms noch auf dem Bus lag -- erschien danach
in **jeder** Zeile und sah wie ein Dauerzustand aus.

Jetzt zaehlt die Ausgabeschicht mit: wie viele READ-Rahmen geprueft wurden, wie
viele davon fehlerhaft waren und wie viele Fehler **in Folge** auftraten (ein
gueltiger Rahmen setzt die Serie auf 0):

```
[i2c] Rahmen verworfen: cmd=GET_INPUT grund=CRC-Fehler (-3)
[herz] takt=1s aufrufe=0 tx_frames=1427 rx_frames=1424 wiederholungen=8 verworfene_rahmen=1 geprueft=6 fehlerhaft=1 fehlerserie=0 letzter_status=noch keiner
[fehler] GET_STATUS: LINK-Fehler (keine gueltige Antwort) (rc=0xE0)
[fehler]   tx_frames=1427 rx_frames=1424 wiederholungen=8 geprueft=6 fehlerhaft=1 fehlerserie=0 busfehler=0x00000000 (Mock)
[ende] BT Gamecontroller GUI 0.2 beendet: i2c_zeilen=0 tx_frames=1427 rx_frames=1424 wiederholungen=8 verworfene_rahmen=1 geprueft=6 fehlerhaft=1 fehlerserie=0 letzter_status=LINK-Fehler (keine gueltige Antwort) (0xE0)
```

Das Beispiel zeigt genau die gewuenschte Aussage: sechs gepruefte Rahmen, einer
davon fehlerhaft -- aber `fehlerserie=0`, weil danach wieder ein gueltiger
Rahmen kam. Ein einzelner Aussetzer ist damit nicht mehr als Dauerfehler zu
lesen.

### 3.2 Wo die Felder stehen

| Zeile | Felder |
|-------|--------|
| `[herz]` | `... verworfene_rahmen=N geprueft=N fehlerhaft=N fehlerserie=N letzter_status=...` |
| `[fehler]` (Diagnosezeile) | `tx_frames=N rx_frames=N wiederholungen=N geprueft=N fehlerhaft=N fehlerserie=N busfehler=0x... (...)` |
| `[ende]` | `... verworfene_rahmen=N geprueft=N fehlerhaft=N fehlerserie=N letzter_status=...` |
| Konsole, Fehlerdiagnose | `Versuche=N, Wiederholungen=N, geprueft=N fehlerhaft=N fehlerserie=N` |

Die Ausgabemenge aendert sich dadurch nicht: die Zaehler stehen in Zeilen, die
es schon gab (eine Herzzeile je Sekunde, eine Zeile je Fehler, eine
Schlusszeile). Der bestehende Zaehler `verworfene_rahmen` bleibt daneben
stehen; er zaehlt dieselben Ereignisse, aber aus Sicht der Ausgabeschicht.

---

## 4. `make verify` -- wortwoertlich, mit Exit-Code

```
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
 - Rahmenpruefung: geprueft/fehlerhaft/fehlerserie statt letztem Zustand

-----------------------------------------------
Faelle : 136
Checks : 818
Fehler : 0
-----------------------------------------------
ERGEBNIS: OK
cc -std=gnu17 -Wall -Wextra -Werror -O1 -g -fsanitize=address,undefined \
    -fno-omit-frame-pointer -I. -o build/v4_tests_san v4_proto.c v4_master.c v4_state.c v4_joyport.c v4_debug.c v4_mock.c tests/test_main.c tests/test_proto.c tests/test_master.c tests/test_state.c tests/test_joyport.c tests/test_debug.c
== Testlauf mit ASan + UBSan ==
-----------------------------------------------
Faelle : 136
Checks : 818
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
```

**Exit-Code: 0** (`make verify; echo EXIT=$?` -> `EXIT=0`).

Vorher: 135 Faelle / 791 Checks. Jetzt: **136 Faelle / 818 Checks, 0 Fehler**
(derselbe Lauf unter ASan+UBSan ebenfalls 0 Fehler). Keine bestehende Pruefung
wurde abgeschwaecht; die neuen Pruefungen sind der Testfall in
`tests/test_debug.c` und drei zusaetzliche `grep`-Pruefungen im `demo`-Ziel des
Makefiles.

---

## 5. Offen -- uebersetzt, aber NICHT auf echter Hardware geprueft

* **Die Ursache der leeren Felder ist mit dem Mitschnitt belegt, aber nicht am
  Fenster nachgemessen.** Belegt ist: die Oberflaeche hat Status und Controller
  erst mit dem fuenften Takt geholt (25 s im Mitschnitt), und die Geraeteliste
  wurde ohne Knopfdruck nie gefuellt. Ob zusaetzlich MUI die Felder gequetscht
  hat (Abschnitt 1.4), laesst sich nur auf der V4 durch Draufschauen
  entscheiden -- die Aenderungen schliessen diesen Fall aus, statt ihn zu
  beweisen.
* **Kein Durchlauf auf der V4 in diesem Arbeitsgang.** Alles hier ist
  uebersetzt, lint-geprueft, gegen den Mock getestet und mit dem
  MUI-Baumpruefer geprueft; gestartet wurde die Oberflaeche nicht.
* **Der automatische Listen-Scan** (`gui_liste_auto`) ist eine
  Verhaltensaenderung: einmal je erkannter Verbindung und nur bei leerer Liste
  geht ein `SCAN_START` auf den Bus (dieselbe Strecke wie der Knopf). Auf der
  V4 ist nicht gemessen, wie lange der ESP32 bei laufender Suche dafuer
  braucht; er erzeugt genau eine zusaetzliche Meldung (`[scan] Verbindung
  erkannt ...`) und danach die ueblichen `[scan]`-Zeilen.
* **Die drei Zaehler sind auf dem Wirt geprueft**, nicht gegen einen echten
  CRC-Fehler auf dem Bus. Auf der V4 ist die Aussage "ein Aussetzer, keine
  Serie" erst nach einem solchen Lauf sichtbar.
* **`make test-m68k`** (Big Endian unter qemu) meldet weiterhin SKIP, weil
  `m68k-linux-gnu-gcc`/`qemu-m68k` fehlen. Die beiden Amiga-Builds (Konsole und
  MUI) laufen mit `-Werror` durch.
* **Die Mindestbreite des Fensterinhalts ist nicht nachgemessen.** Die beiden
  Buchsengruppen sind auf `GUI_LINE_W` = 44 Zeichen aufgefuellt (damit MUI die
  Gruppen nicht bei jeder Aktualisierung zappeln laesst); zusammen mit dem
  Beschriftungs-/Cycle-Kopf sind das bei einem 8-Pixel-Zeichen rund 700 Pixel.
  Ob MUI das Fenster beim Oeffnen auf diese Mindestbreite aufzieht -- und damit
  ueber die zugesagten 640 Pixel -- haengt von der MUI-Fassung und der
  Bildschirmschrift ab und ist hier nicht gemessen. An den Fensterwerten
  (`MUIA_Window_Width/Height` 640x400, SizeGadget, `-w`/`-h`,
  Schirmbegrenzung) wurde nichts geaendert; die vier Kopffelder haben
  `MUIA_Text_SetMin, FALSE`, damit wenigstens die lange Controller-Zeile das
  Fenster nicht aufzieht.

---

## 6. `Forbid()`

**Nein.** Im gesamten Projekt steht kein `Forbid()` und kein `Permit()` im
Code; der Begriff kommt ausschliesslich in Kommentaren vor, die erklaeren,
warum er nicht benutzt wird (`v4_gui.c`, `v4_debug.c`, `v4_joyport.c`,
`v4_state.c`, `v4_master.h`, `v4_amiga_i2c.c`). Ebenso kein `CreateNewProc`,
kein `CreateTask`, kein zweiter Faden. Nachweis (am Ende dieses Arbeitsgangs
wiederholt):

```
$ grep -rn "Forbid(\|Permit(\|CreateNewProc\|CreateTask(" *.c *.h tests/*.c | wc -l
21
$ grep -rn "Forbid(\|Permit(\|CreateNewProc\|CreateTask(" *.c *.h tests/*.c \
      | grep -v "^\S*:[0-9]*: *\(\*\|/\*\)" | grep -v "^\S*:[0-9]*: \*"
$ echo $?
1
```

Die 21 Treffer sind also samt und sonders Kommentare. `CreateMsgPort` /
`CreateIORequest` gibt es nur fuer `timer.device` (den 200-ms-Takt der
Oberflaeche und die Zeitbasis der Amiga-Plattform) -- dort ist ein
Nachrichtenport Pflicht, sonst kommt die Antwort des Geraets nie an. Der
I2C-Verkehr laeuft direkt im Hauptfaden.

---

# 7. Nachtrag (14.10.2026): die beiden Auswahlknopfe der Betriebsart

## 7.1 Der Befund am Geraet

Wortlaut des Benutzers:

> "auswahlfelder nach buchse 0: und Buchse 1: viel zu groß und es steht nichts
> drin auch wenn getrennt und neu verbunden wurde"

Bestaetigt wurde ausdruecklich: **die Zeilen ueber den Auswahlfeldern
(Verbindungszustand und Controller mit Name, Art, VID:PID) sind gefuellt** --
die Korrekturen aus den Abschnitten 1-6 wirken. Betroffen sind **nur** die
beiden Cycle-Knoepfe der Betriebsart.

Der Mitschnitt desselben Laufs (neue Fassung, ab 19:09:57) zeigt beides:

```
19:11:39  [modus] Buchse 0 = aus, Buchse 1 = Joystick -- GET_PORT_MODE
19:11:40  [status] Zustand 3 (CONNECTED), Verbindung aktiv, Scan aus, Geraete 1, Scan-Generation 2
19:11:40  [scan] Verbindung erkannt -- die Geraeteliste wird automatisch geholt (bis zu 8 s)
19:11:40  [hid] 8BitDo 64 BT, Art Pad, Transport BLE, VID:PID 2DC8:3019, ...
19:11:57  [scan]   [0] 8BitDo 64 BT (E4:17:D8:5F:E6:C9)
19:11:57  [scan] 1 Geraet(e) in der Liste
19:15:51  [herz] takt=121s ... verworfene_rahmen=4 geprueft=1208 fehlerhaft=4 fehlerserie=0 letzter_status=OK (GET_HID) | ...
19:16:12  [modus] Buchse 0 = Joystick, Buchse 1 = Maus -- GET_PORT_MODE
```

`GET_PORT_MODE` liefert die Betriebsarten also korrekt, und die Oberflaeche
bekommt sie -- sie hat sie nur nicht in den Auswahlknopf geschrieben (er hatte
gar keine Eintraege).

## 7.2 Ursache (belegt)

`MUIA_Cycle_Entries` ist **kein** mit `|` getrennter Text, sondern ein
**NULL-abgeschlossenes Array von Zeichenketten** (`STRPTR *`). Belege:

* SDK-Header `Compilers/GCC-6.50-Latest/m68k-amigaos/include/libraries/mui.h:3098`:
  ```
  #define MUIA_Cycle_Entries  0x80420629  /* V4  is. STRPTR *  */
  ```
* `Compilers/GCC-6.50-Latest/m68k-amigaos/doc/MUI_Cycle.doc`,
  `MUIA_Cycle_Entries`: *"You must supply a pointer to a string array,
  containing one entry for each item and terminated with a NULL."* Und das
  Beispiel dort zeigt ein `static const char *CYA_...[] = { ..., NULL };`.

`v4_gui.c` uebergab dagegen eine Zeichenkette (alte Zeilen 1804/1820):

```c
Child, s_mode0 = CycleObject,
    MUIA_Cycle_Entries,
        (IPTR)"Aus|Joystick|Maus",
End,
```

MUI liest die ersten vier Bytes (`"Aus|"`) als Zeiger auf ein Zeigerarray, also
`0x4175737C` -- das ist kein Eintragsarray. Folge, genau wie am Geraet zu sehen:

* es kommen **keine Eintraege** heraus, der Knopf bleibt **leer**;
* MUI rechnet seine Breite aus den Eintraegen -- ohne brauchbare Eintraege und
  mit dem Vorgabegewicht 100 in der Zeile wird das Feld ueber die **ganze
  Spaltenbreite** gezogen ("viel zu groß").

Die beiden Vermutungen des Benutzers waren damit beide richtig: das Array-Format
fehlte, und ohne `MUIA_Weight, 0` dehnt die Gruppe das Feld.

## 7.3 Was geaendert wurde (nur die Auswahlknopfe)

| Stelle | Was |
|--------|-----|
| `v4_gui.c`, neu `gui_modus_eintraege` | `static const char *const gui_modus_eintraege[] = { "aus", "Joystick", "Maus", NULL };` -- ortsfest, NULL-abgeschlossen, in der Reihenfolge `V4P_PORT_OFF/JOY/MOUSE` und mit denselben Namen wie `v4p_port_mode_name()`. |
| `v4_gui.c`, beide Cycle-Knoepfe | `MUIA_Cycle_Entries, (IPTR)gui_modus_eintraege` statt der Zeichenkette, dazu `MUIA_Weight, 0` -- der Knopf bleibt so breit wie sein laengster Eintrag ("Joystick"). |
| `v4_gui.c`, Beschriftungen `Buchse 0:`/`Buchse 1:` | ebenfalls `MUIA_Weight, 0`, damit die Beschriftung den Platz der Zeile nicht fuer sich nimmt und den Knopf an den Rand schiebt. |
| `v4_gui.c`, Sicherung zur Uebersetzungszeit | Ein `typedef char ...[...]` vergleicht die Anzahl der Array-Elemente mit `V4P_PORT_MODE_MAX + 2u` (drei Betriebsarten + NULL). Wer den NULL-Abschluss entfernt, bekommt einen Uebersetzungsfehler statt eines leeren Felds. Gegenprobe gemacht: ohne `NULL` bricht der Amiga-Build ab. |
| `tools/check_mui_tree.py` | Neue Regel: `MUIA_Cycle_Entries` mit einer **Zeichenkette** ist ein Befund (Text + Begruendung). Neuer Selbsttestfall "MUIA_Cycle_Entries als Zeichenkette statt NULL-Array"; der kuenstliche Gut-Baum benutzt jetzt selbst die Array-Form. |

**Nicht angefasst** wurden in diesem Nachtrag der Verbindungszustand, die
Controller-Zeile und die Geraeteliste -- der Diff dieses Schritts enthaelt nur
die beiden Cycle-Knoepfe, die beiden Beschriftungen, das neue Array samt
Sicherung und Kommentare. Die Aenderungen aus den Abschnitten 1-6 (erster
Takt vollstaendig, automatische Liste, `MUIA_Weight, 0` /
`MUIA_Text_SetMin, FALSE` an den Anzeigefeldern) bleiben unveraendert.

Die Wirkung ist unveraendert: der aktive Eintrag kommt jeden Takt aus
`GET_PORT_MODE` (`v4_state_put_modes` -> `S.applied[]` -> `set(MUIA_Cycle_Active)`),
eine Bedienung des Knopfes geht ueber `act_set_mode()` -> `SET_PORT_MODE`, und
`v4_state_put_mode()` uebernimmt die Bestaetigung des ESP32.

## 7.4 Der Baumpruefer kennt die Fehlerklasse jetzt

```
$ python3 tools/check_mui_tree.py v4_gui.c
MUI-Baum: sauber (v4_gui.c)
  Fensterinhalt: VGroup mit 10 Kindern
  ...
$ python3 tools/check_mui_tree.py --selftest | tail -4
  ok     : MUIA_Cycle_Entries als Zeichenkette statt NULL-Array
  ok     : WindowContents fehlt
Selbsttest: OK
```

Der neue Fall erzeugt den Befund im kuenstlichen Baum:

```
Zeile 18: MUIA_Cycle_Entries bekommt eine Zeichenkette ("a|b|c") -- MUI
erwartet ein NULL-abgeschlossenes Array von Zeichenketten (STRPTR *, siehe
libraries/mui.h und MUI_Cycle.doc)
```

## 7.5 `make verify` (Nachtrag) -- wortwoertlich, mit Exit-Code

```
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
  Zeile 7: die Wurzel ist keine einzelne Anwendung (0 Wurzelknoten)
  ok     : fehlendes End am TextObject
MUI-Baum: BEFUND in /tmp/v4mui-selftest-03/v4_gui.c
  Zeile 29: ueberzaehliges 'End' -- der Baum ist hier schon zu
  ok     : ueberzaehliges End (VGroup zweimal geschlossen)
MUI-Baum: BEFUND in /tmp/v4mui-selftest-04/v4_gui.c
  Zeile 12: Tag MUIA_Text_Contents (Zeile 12) hat keinen Wert bekommen -- stattdessen folgt Tag MUIA_Text_PreParse
  Zeile 13: Tag MUIA_Text_PreParse (Zeile 12) hat keinen Wert bekommen -- stattdessen folgt Tag MUIA_Text_PreParse
  ok     : Tag ohne Wert (MUIA_Text_Contents ohne Inhalt)
MUI-Baum: BEFUND in /tmp/v4mui-selftest-05/v4_gui.c
  Zeile 13: Tag MUIA_Cycle_Entries gehoert nicht zu TextObject (Objekt in Zeile 11)
  Zeile 13: MUIA_Cycle_Entries bekommt eine Zeichenkette ("a|b|c") -- MUI erwartet ein NULL-abgeschlossenes Array von Zeichenketten (STRPTR *, siehe libraries/mui.h und MUI_Cycle.doc)
  ok     : Tag einer fremden Klasse am TextObject
MUI-Baum: BEFUND in /tmp/v4mui-selftest-06/v4_gui.c
  Zeile 12: zwei 'Child' hintereinander (Zeile 11 wartet noch auf sein Objekt)
  ok     : Child ohne Objekt
MUI-Baum: BEFUND in /tmp/v4mui-selftest-07/v4_gui.c
  Zeile 15: HGroup hat kein einziges Kind
  ok     : Gruppe ohne Kinder
MUI-Baum: BEFUND in /tmp/v4mui-selftest-08/v4_gui.c
  Zeile 18: MUIA_Cycle_Entries bekommt eine Zeichenkette ("a|b|c") -- MUI erwartet ein NULL-abgeschlossenes Array von Zeichenketten (STRPTR *, siehe libraries/mui.h und MUI_Cycle.doc)
  ok     : MUIA_Cycle_Entries als Zeichenkette statt NULL-Array
MUI-Baum: BEFUND in /tmp/v4mui-selftest-09/v4_gui.c
  Zeile 7: WindowContents kommt 0 mal vor (erwartet: genau einmal)
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
 - Rahmenpruefung: geprueft/fehlerhaft/fehlerserie statt letztem Zustand

-----------------------------------------------
Faelle : 136
Checks : 818
Fehler : 0
-----------------------------------------------
ERGEBNIS: OK
== Testlauf mit ASan + UBSan ==
-----------------------------------------------
Faelle : 136
Checks : 818
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
```

**Exit-Code: 0** (`make verify; echo EXIT=$?` -> `EXIT=0`), unveraendert
**136 Faelle / 818 Checks, 0 Fehler** (auch unter ASan+UBSan), Demo OK,
MUI-Baumpruefer sauber, Selbsttest OK, beide Amiga-Builds mit `-Werror`.

## 7.6 Offen

* **Kein Lauf dieser Nachtrags-Fassung auf der V4** -- alles uebersetzt,
  gelintet, gegen den Mock getestet und mit dem Baumpruefer geprueft. Ob der
  Knopf auf dem Schirm wirklich kompakt neben "Buchse 0:" steht, ist die
  naechste Sichtpruefung; die Breite folgt jetzt aber der dokumentierten Regel
  "Gewicht 0 = Mindestgroesse".
* **Die Eintraege sind dreisprachig-unabhaengig**: "aus"/"Joystick"/"Maus"
  stehen als Array im Code, nicht mehr als Zeichenkette. Die Namen sind
  dieselben wie in `v4p_port_mode_name()`; eine Aenderung dort muesste hier
  nachgezogen werden (der Baumpruefer prueft die Form, nicht die Namen).
* Unveraendert offen bleiben die Punkte aus Abschnitt 5 (keine
  Hardwaremessung der Zaehler, `make test-m68k` SKIP, Mindestbreite des
  Fensterinhalts).

## 7.7 `Forbid()`

**Nein** -- unveraendert. Auch dieser Nachtrag fuegt keinen Betriebssystem-
oder Fadenaufruf hinzu (nur MUI-Tags und ein statisches Array).
