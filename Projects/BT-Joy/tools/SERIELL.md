# Serielle Verbindung zur V4

## Parameter (gemessen, nicht vermutet)

| | |
|---|---|
| Kabel | CH340 (USB-Seriell) → `/dev/ttyUSB1` |
| Baudrate | **115200** |
| Weg auf der V4 | direkter Hardware-UART (**SERPER**), **nicht** `ser:` |

Nachgewiesen am 13.10.: 39 Zeilen der V4 auf dem PC **und** 22 Zeichen vom PC
auf der V4 (Bildschirm der V4). Beide Richtungen tragen.

## Bedienung

```sh
# Fenster mit Live-Ausgabe (Vorgabe /dev/ttyUSB1, 115200)
tools/v4ser_fenster.cmd            # Doppelklick, oder aus WSL:
cmd.exe /c start "" "\\wsl.localhost\Ubuntu\...\tools\v4ser_fenster.cmd"

# ohne Fenster
python3 tools/v4ser.py
python3 tools/v4ser.py --dauer 60          # nach 60 s beenden
python3 tools/v4ser.py --sende "HALLO"     # einmal senden und beenden
python3 tools/v4ser.py --port /dev/ttyUSB0 # anderes Kabel
```

Befehle an die V4 werden **nicht** ueber ein zweites Programm geschickt, sondern
als Zeile in `logs/v4ser_cmd.txt` geschrieben. Nur so bleibt es bei **einem**
Prozess auf dem Port.

## Die Regel, an der es zuerst gescheitert ist

**Es darf nur ein Programm auf dem Port laufen.** Zwei Leser auf einer seriellen
Leitung ergeben Zeichenbrei oder Schweigen — und niemand sieht, wer schuld ist.
Das Programm fuehrt dafuer `/tmp/v4ser.lock` und verweigert den Start, wenn
schon eines laeuft.

## Pruefprogramm auf der V4

`Programs:test/serprobe [sekunden]` (Quelle: `Projects/BT-Joy/serprobe/`)

Es sendet jede Sekunde eine Zeile `SERPROBE <n>`, zeigt Empfangenes **sofort**
Zeichen fuer Zeichen an und schickt jede empfangene Zeile als
`V4-HAT-EMPFANGEN: ...` zurueck. Damit ist der Rundlauf in **einem** Fenster
sichtbar:

```
13:16:18  SENDEN    : PC->V4 HALLO 1                    ← vom PC
13:16:19  EMPFANGEN : SERPROBE 1                        ← von der V4
13:16:20  EMPFANGEN : V4-HAT-EMPFANGEN: PC->V4 HALLO 1  ← die V4 bestaetigt
```

## Wenn nichts ankommt

1. **Laeuft schon eines?** Meldung `Es laeuft bereits v4ser (PID ...)` beachten.
2. **Richtiges Kabel?** Der ESP32 ist die CP210x, die V4 die CH340.
3. **Durchgereicht?** `usbipd list` — der Zustand muss `Attached` sein.
4. **Sendet die V4 ueberhaupt?** `serprobe` meldet es auf dem Bildschirm:
   `gesendet: 1190 Zeichen, Sendebereitschaft 1190 mal, Wartegrenze 0 mal`.
   Die Wartegrenze muss **0** sein — sonst klemmt der Sender.

---

# Die Lehre: erst sammeln, dann ausgeben

**Das ist der Punkt, an dem es zuerst gescheitert ist. Wer das naechste Mal ein
Programm fuer die serielle Schnittstelle der V4 schreibt, muss ihn kennen.**

## Das Symptom

Der PC sendete `SINGLE A` — auf der V4 kamen abwechselnd nur `[53] [49]` an,
also die Buchstaben **„SI"**, die ersten zwei Zeichen. Der Rest fehlte.

Wichtig war die Beobachtung **im Hex**: `53 49` sind genau die ersten beiden
gesendeten Bytes. Die V4 **dekodierte also richtig** — sie bekam nur nicht mehr.

## Die Ursache

Die serielle Hardware der V4 hat **ein Byte** Empfangspuffer. Das Programm hatte
fuer **jedes empfangene Zeichen sofort** `Write(Output(), ...)` gerufen, um es
anzuzeigen. Ein solcher Systemaufruf dauert rund **100 µs** — und in dieser Zeit
fragt niemand `SERDATR` ab. Bei 115200 Baud ist ein Zeichen nach etwa **87 µs**
durch: alles, was waehrend der Ausgabe eintrifft, fällt aus dem Puffer.

## Die Loesung

**Zwischen zwei Lesevorgaengen darf nichts ausgegeben werden.**

```c
/* FALSCH - so gingen alle Zeichen ausser den ersten verloren: */
while ((c = ApolloDebugMayGetChar()) >= 0) {
    Write(Output(), &c, 1);            /* 100 us ohne Lesen! */
}

/* RICHTIG - sammeln, und erst danach ausgeben: */
while ((c = ApolloDebugMayGetChar()) >= 0) {
    if (puffer_len < PUFFER_MAX) puffer[puffer_len++] = (char)c;
}
if (laengere_zeit_kein_zeichen() && puffer_len > 0) {
    zeile_ausgeben();                  /* jetzt darf geredet werden */
}
```

Das Ende einer Nachricht erkennt das Programm an einer **kurzen Ruhe**: zaehlt
eine Abfrage Schleife mehrere hundert Mal „kein Zeichen", ist die Nachricht
durch. Ein Zeitgeber ist dafuer **nicht** noetig — ein Zaehler genuegt, weil
jede Abfrage nur wenige Mikrosekunden dauert.

## Was weiterhin gilt

* **Baudrate 115200** (von `ApolloDebugInit()` gesetzt, gilt fuer Senden **und**
  Empfangen). Die Rate war nie das Problem.
* **Kein `Delay()` in der Leseschleife.** Auch eine Pause von 200 ms kostet
  ganze Nachrichten.
* **Weg:** direkter Hardware-UART (SERPER) — kein AmigaDOS `ser:`.
* **Senderseite prüfen:** `serprobe` meldet am Bildschirm
  `gesendet: N Zeichen, Sendebereitschaft N mal, Wartegrenze 0 mal`.
  Greift die Wartegrenze, klemmt der Sender.
* **Restverluste:** auch mit dieser Fassung gehen gelegentlich einzelne Bytes
  verloren (im Mitschnitt standen `PC-HLO 18` und `PHALLO 19`), weil zwischen
  Ausgabe und naechster Abfrage immer noch eine Luecke liegt. Verlustfrei wird
  es erst mit einem **Empfangsinterrupt** (RBF-Interrupt ueber `INTREQ`/`INTENA`)
  oder groesserem Puffer auf der Gegenseite. Fuer ein Log ist das unerheblich,
  fuer ein Uebertragungsprotokoll nicht.

## Nachweis vom 13.10.

```
PC sendet:   PC-HALLO 1 .. 66     (alle 5 s)
V4 zeigt:    EMPFANGEN: [50][43][2D][48][41][4C][4C][4F][20][31][0A]  = PC-HALLO 1
V4 schickt:  V4-HAT-EMPFANGEN: PC-HALLO 1
PC sieht:    26 Rueckmeldungen im Mitschnitt logs/v4_seriell.log
```

Beide Richtungen tragen damit nachweislich.

---

# Regel fuer alle Tests: BEIDE Seiten mitschreiben

Jeder Test laeuft mit **zwei** Mitschriften, die dauerhaft offen sind:

| Seite | Kanal | Fenster | Datei |
|---|---|---|---|
| **V4** | DebugUART ueber die CH340 (`/dev/ttyUSB1`, 115200) | `V4 seriell` | `logs/v4_seriell.log` |
| **ESP32** | Konsole ueber die CP210x (`/dev/ttyUSB0`, 115200) | `V4 BT Joy - Monitor` | `../../V4_ESP32_BT_Joy/logs/monitor_session.log` |

**Warum:** am 13.10. hat der DebugUART zweimal den entscheidenden Hinweis
geliefert (die Oberflaeche blieb nach ihrem Banner stehen). Ohne ihn tippt man im
Dunkeln. Und ohne den ESP32-Mitschnitt fehlt die Gegenseite: man sieht dann zwar,
dass die V4 etwas sendet, aber nicht, was der ESP32 daraus macht.

**Beides vor dem Test starten, nicht danach.** Nachtraeglich ist das Ereignis weg.

**Beide Adapter durchreichen** (nicht vergessen, das faellt regelmaessig ab):

```sh
"/mnt/c/Program Files/usbipd-win/usbipd.exe" list
"/mnt/c/Program Files/usbipd-win/usbipd.exe" attach --wsl --busid 1-5   # CH340 = V4
"/mnt/c/Program Files/usbipd-win/usbipd.exe" attach --wsl --busid 2-3   # CP210x = ESP32
```

**Jede Seite nur EINMAL oeffnen.** Zwei Leser auf einer seriellen Leitung ergeben
Zeichenbrei oder Schweigen. Beide Programme fuehren dafuer eine Sperrdatei
(`/tmp/v4ser.lock`, `/tmp/v4_bt_joy_monitor.lock`).
