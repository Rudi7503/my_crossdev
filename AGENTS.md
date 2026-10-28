# Apollo 68080 CrossDev – Agent Guide & V4 Operating Manual

Dieses Dokument ist die zentrale Arbeitsanweisung für alle Arbeiten im Repository `ApolloCrossDev` auf der Vampire V4 Standalone.

---

## 1. Zielsystem & Netzwerk-Verbindung

- **Zielmaschine:** Apollo Vampire V4 Standalone (Core 11350, ApolloOS 9.5).
- **IP-Adresse:** `192.168.178.50`
- **Dateitransfer (ACP):**
  ```powershell
  & "C:\Program Files\ApolloExplorer\acp.exe" <LokaleDatei> 192.168.178.50:RAM/
  ```
- **Remote-Ausführung (ASH):**
  ```powershell
  & "C:\Program Files\ApolloExplorer\ash.exe" 192.168.178.50 "RAM:<Programm>"
  ```
- **Dateibereinigung:** Nach Tests immer temporäre Binaries/Screenshots auf `RAM:` löschen (`delete RAM:<file> quiet`).

---

## 2. Fernsteuerung der GUI & Maus (amiagent)

Auf der V4 läuft `C:amiagent.020` als Hintergrunddienst auf Port **7846**.
Die Authentifizierung erfolgt mit dem lokalen Token (definiert in `S:Startup-Sequence`).

Das Python-CLI [`Tools/amiga_tool.py`](file:///home/papa/Vampire_V4/ApolloCrossDev/Tools/amiga_tool.py) steuert die V4 vollautomatisch:

- **GUI-Baum inspizieren:**
  ```bash
  ./Tools/amiga_tool.py ui-tree
  ```
- **Button im aktiven Fenster klicken:**
  ```bash
  ./Tools/amiga_tool.py ui-click <ButtonText> [--window "<WindowName>"]
  ```
- **Beliebige Mauskoordinaten anklicken:**
  ```bash
  ./Tools/amiga_tool.py click <x> <y> --button left
  ```
- **Screenshot erstellen:**
  ```bash
  ./Tools/amiga_tool.py screenshot /tmp/v4.png
  # oder auf Amiga-Seite:
  ash.exe 192.168.178.50 "grabrtgwb bmp RAM:shot.bmp"
  ```
- **Software Failure / Guru-Requester behandeln:**
  1. `./Tools/amiga_tool.py ui-tree` ausführen.
  2. Falls ein Fenster `Recoverable Alert!` über `Software Failure!` liegt:
     `./Tools/amiga_tool.py ui-click Continue`
  3. Danach im `Software Failure!`-Fenster den Reboot auslösen:
     `./Tools/amiga_tool.py ui-click Reboot --window "Software Failure!"`
     (Reboot dauert ca. 5 Sekunden).

---

## 3. Serielles Logging & UART-Framework (RS232)

- **Hardware-Schnittstelle:** Port **`COM9`** (`USB-SERIAL CH340`), 115.200 Baud, 8N1.
- **Framework-Dateien:**
  - [`uart_asm.s`](file:///home/papa/Vampire_V4/ApolloCrossDev/Projects/Matrix_Mul/uart_asm.s): Schreibt direkt in Custom-Chip-Register `$dff030` (`SERDAT`) mit `$dff032` (`SERPER = 30`).
  - [`lifecycle.h`](file:///home/papa/Vampire_V4/ApolloCrossDev/Projects/Matrix_Mul/lifecycle.h): Stellt Makros für Testläufe bereit:
    - `TEST_START("prog")` → Initialisiert UART, sendet `START;prog;dev\n`.
    - `test_alive("prog", n)` → Sendet `ALIVE;prog;n\n` über UART und `printf`.
    - `TEST_DONE("prog")` → Sendet `DONE;prog;dev\n`.
- **Seriellen Port auf dem Host abfragen:**
  Vor oder während der Programmausführung den COM-Port öffnen (z. B. via Python `serial.Serial('COM9', 115200)` oder `Projects/ADPCM_Decoder/tests/serial_log.py`), damit Daten nicht im Hardware-FIFO verfallen.

---

## 4. Compiler- & Assembler-Regeln (Kritisch!)

1. **GCC 6.5.0 Bug mit `-m68080 -O2`:**
   - **NIEMALS** `-m68080 -O2` an `m68k-amigaos-gcc` für C-Code übergeben!
   - GCC emittiert fehlerhafte Kurz-Immediates (`moviw.l`) und ungerade `dbf`-Sprungziele, die auf der V4 sofort zum Guru-Absturz führen.
   - **Pflicht für C-Quellen:** Immer `-O2 -m68020 -m68881 -ffast-math -fomit-frame-pointer -noixemul` verwenden und gegen Assembler-Objekte linken, die mit `vasmm68k_mot -m68080` übersetzt wurden.

2. **ABI-Registererhalt (Callee-Saved Register):**
   - **Scratch (flüchtig):** `d0, d1, a0, a1, fp0, fp1`
   - **Preserved (müssen zwingend gerettet werden):** `d2–d7, a2–a6, fp2–fp7`
   - **Achtung `a2`:** `a2` ist **kein** Scratch-Register! Wer `a2` in Assembler für Pointer nutzt, muss es mit `move.l a2,-(sp)` sichern und vor `rts` restaurieren, sonst werden Frame-Pointer von `printf`/`vfprintf` zerstört (`Illegal instruction 0x80000004`).

3. **Stack-Argumente:**
   - Parameter immer **vor** dem Pushen von Registern in die Adressregister laden (`move.l 4(sp),a0`, `8(sp),a1`, etc.), um variable Offsets durch `fmovem` zu verhindern.

4. **FPU-Instruktionen auf der 68080:**
   - Standard 68881/68882 Mnemonics nutzen (`fmul.s`, `fadd.s`, `fadd`).
   - Keine 68040-spezifischen Aliase wie `fsmul` oder `fsadd` in vasm verwenden (werden als `0x4463`/`0x4462` assembliert und lösen Unimplemented Opcode Line-F aus).

---

## 5. Verifikation & Git-Workflow

- Jede Assembler-Optimierung muss **100 % bit-exakt** gegen eine C-Referenz verifiziert werden.
- CCC-Hardware-Zykluszähler (`movec ccc,d0`) zur Zeitmessung heranziehen.
- Nach erfolgreichem Testen alle Änderungen committen und auf GitHub (`origin/main`) pushen.
