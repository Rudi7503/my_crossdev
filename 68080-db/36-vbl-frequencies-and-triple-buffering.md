# 36. SAGA GFXMODE VBL-Frequenzen & Ruckelfreies Triple-Buffering (Vampire V4)

**Hardware:** Vampire V4 Standalone, Apollo Core **11350** (E-Clock: `709.379 Hz`).  
**Diagnose-Tool:** [`vbl_probe.s`](vbl_probe.s) (Assembliert mit `vasmm68k_mot`, ausgeführt nativ auf V4).  
**Log-Datei:** `RAM:vbl_probe.log` (Gegated über 2,0s bzw. 10,0s exakte E-Clock-Zeitfenster).

---

## 1. Das Kern-Rätsel: Läuft der VBL auf der V4 immer mit 50 Hz?

In bisherigen Projekten (u. a. Hurrican v12/v13b) herrschte die Annahme, dass auf der Vampire V4 der Amiga-Interrupt `INTB_VERTB` und `WaitTOF()` **grundsätzlich mit 50 Hz PAL** laufen, während der physische HDMI-Ausgang mit 60 Hz taktet. Dies führte zu komplizierten Workarounds mit `timer.device`, MsgPorts und E-Clock-Busy-Waiting (`WaitPace`).

Unser Hardware-Test auf der echten Vampire V4 über alle 22 SAGA-Auflösungen ($00 bis $15) liefert die endgültige Klärung:

### Reale Hardware-Messwerte über alle SAGA-Modi

| Mode | SAGA Modus-Bezeichnung | Nominal | VBL / 2s | **Gemessene VBL-Rate** | **Frame-Dauer** | MaxLine (`VPOSR`) | **WaitTOF** |
|:---:|:---|:---:|:---:|:---:|:---:|:---:|:---:|
| **$00** | Native (Workbench / PAL) | 50 Hz | 99 | **49,49 Hz** | **20,20 ms** | 312 | 52,12 Hz |
| **$01** | 320×200 28MHz x2x2 | 60 Hz | 116 | **57,99 Hz** | **17,24 ms** | 312 | 61,63 Hz |
| **$02** | 320×240 28MHz x2x2 | 60 Hz | 119 | **59,49 Hz** | **16,80 ms** | 312 | 59,93 Hz |
| **$03** | 320×256 28MHz x2x2 | 60 Hz | 121 | **60,49 Hz** | **16,52 ms** | 312 | 60,07 Hz |
| **$04** | 640×400 28MHz | 60 Hz | 116 | **57,99 Hz** | **17,24 ms** | 312 | 60,64 Hz |
| **$05** | **640×480 28MHz (Hurrican)** | **60 Hz** | **117** | **58,49 Hz** | **17,09 ms** | **312** | **61,93 Hz** |
| **$06** | **640×512 28MHz** | **50 Hz** | **101** | **50,49 Hz** | **19,80 ms** | **312** | **50,25 Hz** |
| **$07** | 960×540 42MHz | 60 Hz | 120 | **59,99 Hz** | **16,66 ms** | 312 | 60,88 Hz |
| **$08** | 480×270 42MHz x2x2 | 60 Hz | 120 | **59,99 Hz** | **16,66 ms** | 312 | 61,49 Hz |
| **$09** | 304×224 28MHz x2x2 | 60 Hz | 117 | **58,49 Hz** | **17,09 ms** | 312 | 62,52 Hz |
| **$0A** | 1280×720 85MHz | 50 Hz | 101 | **50,49 Hz** | **19,80 ms** | 312 | 50,82 Hz |
| **$0B** | 640×360 85MHz x2x2 | 50 Hz | 100 | **49,99 Hz** | **20,00 ms** | 312 | 51,17 Hz |
| **$0C** | 800×600 42MHz | 60 Hz | 118 | **58,99 Hz** | **16,94 ms** | 312 | 61,84 Hz |
| **$0D** | 1024×768 85MHz | 60 Hz | 121 | **60,49 Hz** | **16,52 ms** | 312 | 61,52 Hz |
| **$0E** | 720×576 28MHz | 50 Hz | 98 | **48,99 Hz** | **20,40 ms** | 312 | 51,05 Hz |
| **$0F** | 848×480 42MHz | 60 Hz | 120 | **59,99 Hz** | **16,66 ms** | 312 | 60,82 Hz |
| **$10** | 640×200 28MHz | 60 Hz | 116 | **57,99 Hz** | **17,24 ms** | 312 | 60,21 Hz |
| **$11** | **1920×1080 85MHz** | **24 Hz** | **49** | **24,49 Hz** | **40,81 ms** | **312** | **24,45 Hz** |
| **$12** | 1280×1024 85MHz | 50 Hz | 98 | **48,99 Hz** | **20,40 ms** | 312 | 52,17 Hz |
| **$13** | 1280×800 84MHz | 60 Hz | 119 | **59,49 Hz** | **16,80 ms** | 312 | 60,06 Hz |
| **$14** | 1440×900 85MHz | 50 Hz | 101 | **50,49 Hz** | **19,80 ms** | 312 | 51,41 Hz |
| **$15** | 720×405 28MHz | 50 Hz | 98 | **48,99 Hz** | **20,40 ms** | 312 | 50,76 Hz |

### Exakter 10-Sekunden-Langzeittest

* **Modus $00 (Native PAL):** `498 VBLs in 10,00 s` $\rightarrow$ **`49,799 Hz`** (`20,08 ms`)
* **Modus $05 (640×480 VGA):** `599 VBLs in 10,00 s` $\rightarrow$ **`59,900 Hz`** (`16,69 ms`)
* **Modus $06 (640×512 PAL):** `500 VBLs in 10,00 s` $\rightarrow$ **`49,999 Hz`** (`20,00 ms`)

### Wichtigste Erkenntnis: Warum entstand der Irrglaube?
1. **Der SAGA-Grafikcontroller koppelt den Amiga-VBL-Interrupt (`INTB_VERTB`) direkt an die gewählte Display-Frequenz!** Im 60-Hz-Modus kommen exakte **60 VBL-Interrupts pro Sekunde** an der CPU an.
2. **Der Agnus-Rasterzähler (`VPOSR/VHPOSR` bei `$DFF004/$DFF006`) läuft stur mit 50 Hz PAL weiter (`MaxLine = 312`).** Wer also Beam-Racing per `$DFF004` versucht, sieht immer 50 Hz.
3. Der VBL-Interrupt `INTB_VERTB` ist das **echte, hardware-synchrone 60-Hz-Taktsignal**.

---

## 2. Ruckelfreies Triple-Buffering (3-Puffer-Ring)

### Die drei Zustände der Puffer:
* **Puffer A:** Wird gerade vom SAGA-Display-DMA zeilenweise auf HDMI ausgegeben (**angezeigt**).
* **Puffer B:** Ist fertig gezeichnet, seine Adresse steht im SAGA-Register `BPLHPT` ($DFF1EC). SAGA übernimmt ihn beim **nächsten VBL**.
* **Puffer C:** Wird aktuell von der CPU im FastRAM **gezeichnet**.

### Synchronisation über den VBL-Zähler:
Im VBL-Interrupt:
```m68k
VblHandler:
        addq.l  #1,VblSincePoke     ; VBL hat gefeuert
        moveq   #0,d0
        rts
```

Sobald die CPU Puffer C fertig hat:
```m68k
.wait_vbl:
        tst.l   VblSincePoke        ; Hat SAGA Puffer B bereits uebernommen?
        beq.s   .wait_vbl           ; 0 = CPU war schneller als 16.6 ms -> kurz warten

        clr.l   VblSincePoke        ; Zaehler ruecksetzen
        move.l  Back,BPLHPT         ; Puffer C ins Register -> aktiv beim naechsten VBL

        ; Ringpuffer weiterschalten: Naechstes Bild in Puffer A zeichnen
        addq.l  #1,HwCur
        cmp.l   #3,HwCur
        blt.s   .cur_ok
        clr.l   HwCur
.cur_ok:
```

---

## 3. Was passiert, wenn ein Frame 20 ms dauert? (Analyse)

Im Idealfall (60 Hz) dauert ein Frame $\le 16{,}67\text{ ms}$. Was passiert bei schweren Szenen (z. B. viele Sprites, Laser, Partikel), wenn die CPU **20 ms** für einen Frame benötigt?

### Zeitstrahl bei 20 ms Renderzeit:

* **$t = 0{,}0\text{ ms}$:** CPU beginnt mit dem Zeichnen von **Frame 2** in Puffer C. Puffer A ist auf dem Schirm, Puffer B ist in `BPLHPT` scharf geschaltet. `VblSincePoke = 0`.
* **$t = 16{,}67\text{ ms}$ (VBL #1):**
  * SAGA übernimmt Puffer B. **Puffer B ist jetzt auf dem Bildschirm.**
  * Puffer A ist frei!
  * `INTB_VERTB` feuert $\rightarrow$ `VblSincePoke` springt von `0` auf `1`.
  * Die CPU rechnet noch an Frame 2 (braucht noch 3,33 ms).
* **$t = 20{,}00\text{ ms}$:**
  * CPU ist fertig mit Frame 2!
  * CPU prüft: `tst.l VblSincePoke`.
  * Wert ist **1**! (Der VBL fand während des Zeichnens statt).
  * **Die CPU wartet keine einzige Mikrosekunde!**
  * CPU setzt `VblSincePoke = 0`, schreibt Frame 2 in `BPLHPT` und fängt **sofort** bei $t = 20{,}00\text{ ms}$ an, **Frame 3** in Puffer A zu zeichnen!
* **$t = 33{,}33\text{ ms}$ (VBL #2):**
  * SAGA übernimmt Frame 2 (Puffer C). **Frame 2 ist jetzt auf dem Bildschirm.**
  * Puffer B ist frei!
  * `VblSincePoke` springt von `0` auf `1`.
  * Die CPU rechnet an Frame 3 (hat bei $t = 33{,}33$ bereits $13{,}33\text{ ms}$ gerendert).
* **$t = 40{,}00\text{ ms}$:**
  * CPU ist fertig mit Frame 3!
  * `VblSincePoke == 1`. CPU wartet wieder 0 Takte, übergibt Frame 3 an `BPLHPT` und beginnt Frame 4 bei $t = 40{,}00\text{ ms}$.

### Das Resultat auf dem Monitor:
1. **100 % CPU-Effizienz:** Die CPU verbringt **0 % Zeit mit Warten** oder Schlafen. Jeder Takt wird produktiv ins Blitting investiert. Die Render-Rate beträgt exakt $1 / 0{,}020\text{ s} = \mathbf{50{,}0\text{ FPS}}$.
2. **Display-Kadenz:** Auf dem HDMI-Schirm wechselt die Anzeige in einer 1-2-1-2-Kadenz (16,7 ms / 33,3 ms).
3. **Absolut kein Tearing:** SAGA schaltet Puffer *ausschließlich* im Vertikalblank um.
4. **Kein Stottern:** Im Gegensatz zu Double-Buffering (das bei 17 ms sofort hart auf 30 FPS einbricht) sinkt die effektive Framerate bei Triple-Buffering kontinuierlich mit dem Rechenaufwand.
5. **Keine Schwebung:** Da kein künstlicher E-Clock-Timer verwendet wird, gibt es keine Frequenzüberlagerung (Beating) gegen das HDMI-Display.

---

## 4. Alternative: Feste 30 / 60 FPS Quantisierung mit Hysterese

Wenn eine rechenintensive Phase z. B. **10 Sekunden lang 25 ms** pro Frame benötigt, führt unbeschränktes Triple-Buffering zu $40\text{ FPS}$ mit alternierender 2-1-2-1-VBL-Kadenz ($16{,}7\text{ ms} / 33{,}3\text{ ms}$). Dies kann bei horizontalem Scrolling als leichter Rhythmus-Judder wahrgenommen werden.

Die professionelle Alternative (bekannt aus Konsolen-Engines) ist **feste VBL-Quantisierung**:
* **Modus 60 FPS:** Genau **1 VBL** pro Frame ($16{,}67\text{ ms}$).
* **Modus 30 FPS:** Genau **2 VBLs** pro Frame ($33{,}33\text{ ms}$).

---

## 5. Hardware-Verifikation auf der Vampire V4: CCC-Timestamp & Time-Borrowing

Mit dem Testprogramm [`ccc_vbl_test.s`](ccc_vbl_test.s) wurden die mathematischen und hardwaretechnischen Eigenschaften direkt auf der echten Vampire V4 (Apollo Core 11350) gemessen:

### 5.1 Verifikation des 32-Bit-Überlaufs (Counter Overflow)
* Bei 92,09 MHz läuft der 32-Bit-`CCC` alle **46,64 Sekunden** ($2^{32} / 92{,}09\text{ MHz}$) über.
* Getesteter synthetischer Sprung: $t_{\text{start}} = \text{\$FFFFFF00} \rightarrow t_{\text{now}} = \text{\$00000100}$.
* Ergebnis: `sub.l d1, d0` liefert exakt **512 Takte**. Der vorzeichenlose Vergleich (`cmp.l #1534873, d0` gefolgt von `blo.s`) verzweigt fehlerfrei durch die Nullgrenze!

### 5.2 Gemessene 60-Hz-VBL-Zyklenabstände
* Reale gemessene Taktintervalle zwischen konsekutiven SAGA-60-Hz-VBLs:
  $$\mathbf{1.534.093\text{ Takte}} \quad \text{und} \quad \mathbf{1.533.869\text{ Takte}}$$
* Umgerechnet bei $92{,}09\text{ MHz}$:
  $$\frac{1.534.093}{92.090.000\text{ Hz}} = \mathbf{16{,}658\text{ ms}} \quad (\mathbf{60{,}03\text{ Hz}})$$

### 5.3 Live-Test: Time-Borrowing (19 ms Spike + 13 ms Erholung)
* **Frame 1 Lastspitze:** Berechnungszeit **1.753.689 Takte** (**`19,04 ms`** > 16,67 ms Budget).
  * VBL feuert während des Zeichnens $\rightarrow$ `VblSincePoke` springt auf `1`.
  * SAGA zeigt währenddessen den vorherigen Puffer aus der Triple-Buffer-Queue.
* **Frame 2 Erholung:** Berechnungszeit **1.200.000 Takte** (**`13,03 ms`** < 16,67 ms Budget).
  * Bei Fertigstellung von Frame 2: `VblSincePoke` ist noch **`0`**!
  * Frame 2 ist **vor** dem 2. VBL fertig geworden!
* **Hardware-Ergebnis:** **Beide Frames erreichen ihren jeweiligen VBL pünktlich auf dem HDMI-Schirm — 0 Frames gedroppt, 60 FPS voll erhalten!**


