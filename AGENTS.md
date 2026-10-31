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

## 5. Verifikation, Mess-Methodik & Git-Workflow (EISERNE PFLICHT: Immer Harness v5!)

1. **Strikte Verifikation:**
   - Jede Assembler-Optimierung muss **100 % bit-exakt** gegen eine C-Referenz verifiziert werden (`diff = 0`, `PASS`).

2. **Messungen IMMER mit Harness-v5 Methodik durchführen:**
   - **Niemals nur reine `CCC`-Zyklen messen!** Eine Messung ohne Pipeline-Auslastung ist unvollständig und unzureichend.
   - Es müssen **immer alle Hardware-Performance-Zähler** der 68080 erfasst und dokumentiert werden:
     - `CCC`: Clock Cycle Counter (`movec.l CCC, d0`)
     - `IEP1`: Instruction Execution Pipeline 1 / Decoder Anton (`movec.l IEP1, d0`)
     - `IEP2`: Instruction Execution Pipeline 2 / Decoder Bruno (`movec.l IEP2, d0`)
     - `IEP3`: Instruction Execution Pipeline 3 / Decoder Cesar (`dc.w $4e7a,$000c`)
     - `DCH`: Data Cache Hit (`movec.l DCH, d0`)
     - `DCM`: Data Cache Miss (`movec.l DCM, d0`)
     - `BPC`: Branch Predict Correct (`movec.l BPC, d0`)
     - `BPW`: Branch Predict Wrong (`movec.l BPW, d0`)
   - **Kennzahlen:** Stets `IPC` (Befehle / Zyklen) sowie die prozentuale Aufteilung der Befehle auf Anton (`IEP1 %`), Bruno (`IEP2 %`) und Cesar (`IEP3 %`) ausgeben und analysieren, ob die Superskalarität greift oder ob Bus-/Pipeline-Stalls vorliegen.
   - **Warmup-Pflicht:** Vor der Messung zwingend einen Warmup-Durchlauf durchführen (Code, Caches und Branch-Predictors primen), wie in [`68080-db/09-harness.md`](68080-db/09-harness.md) vorgegeben.

3. **Git-Workflow:**
   - Nach erfolgreichem Testen und Dokumentieren alle Änderungen committen und auf GitHub (`origin/main`) pushen.

---

## 6. Häufige Absturzursachen auf der 68080 / V4 & Pre-Flight-Prüfliste

Vor jedem Upload und Ausführen von Assembler-Code auf der echten V4-Hardware muss folgende Prüfliste strikt abgearbeitet werden, um Core-Halts, Line-F-Traps und Guru Meditations zu verhindern:

### 1. Stack-Symmetrie (Kritisch)
- **Problem:** Bedingte `push`/`pop`-Befehle in verzweigten Pfaden (z. B. `movem.l -(sp)` nur bei $j > 0$, aber gemeinsames `movem.l (sp)+` am Schleifenende) verschieben den Stack-Pointer. Beim abschließenden `rts` springt die CPU in beliebige Speicherbereiche -> sofortiger Double Bus Fault / Halt.
- **Regel:** Niemals dynamische Pushes in bedingten Schleifenzweigen durchführen. Wenn lokaler Speicher benötigt wird, immer einen festen Stack-Frame via `link.w a6,#-Size` / `unlk a6` anlegen oder Variablen in dedizierten Stack-Slots verwalten.

### 2. Register-Aliasing & Clobbering in geschachtelten Schleifen
- **Problem:** Ein Register, das eine Schleifenobergrenze oder einen Zähler der äußeren Schleife hält (z. B. `d4` für `half`), wird im inneren Rechenkern für temporäre Daten überschrieben. Dies führt zu Endlosschleifen oder unkontrollierten Speicherzugriffen.
- **Regel:** Vor dem Schreiben jedes Assembler-Blocks eine explizite Register-Belegungstabelle führen. Schleifenvariablen der äußeren Schleife müssen entweder in Callee-Saved Registern (`d2–d7`) verbleiben (die innen unberührt bleiben) oder im Stack-Frame (`-X(a6)`).

### 3. Callee-Saved Register (ABI-Erhalt) – *Realer Absturz-Vorfall `_mc_block_prefetch`*
- **Scratch (flüchtig – frei nutzbar):** `d0, d1, a0, a1, fp0, fp1`
- **Preserved (strikt zu sichern vor jeder Modifikation!):** `d2–d7, a2–a6, fp2–fp7`
- **Was beim `_mc_block_prefetch`-Crash passierte:**  
  In einer Prefetch-Routine wurden `d2` und `a2` verwendet, ohne sie via `movem.l` auf dem Stack zu sichern. Da GCC/libnix `a2` als Kontextzeiger für `printf`/I/O und `d2` für lokale Variablen hält, zerstörte der Aufruf den Stack-Frame von `main()`. Beim anschließenden `rts` sprang die CPU ins Nirvana -> sofortiger Systemabsturz und Freeze der V4.
- **Eiserne Regel:** Assembler-Subroutinen für C dürfen **ausschließlich** Scratch-Register (`d0-d1`, `a0-a1`) anfassen. Werden weitere Register benötigt, MUSS am Funktionskopf zwingend:
  ```asm
  movem.l d2-d7/a2-a6,-(sp)
  ; ... Code ...
  movem.l (sp)+,d2-d7/a2-a6
  rts
  ```
  stehen. Keine Ausnahmen!

### 4. Instruktions-Restriktionen der 68k / 68080
- **`BFLYW`-Privilegien-Falle (`0x80000008`):**  
  Auf Apollo Core 11350 / PCR 1088/1 ist `bflyw` im User-Mode gesperrt oder unvollständig implementiert. Ein Aufruf erzeugt sofort eine *Privilege Violation* (`Software Failure 0x80000008`).  
  **Regel:** Für Schmetterlinge/Butterflies immer standardkonformes AMMX `paddw` + `psubw` einsetzen!
- **`TOUCH`-Syntax in vasm:**  
  `touch (An)` ist in vasm syntaktisch ungültig. Es MUSS immer die indizierte Form `touch (An, Dn.w)` oder `touch (An, Dn.l)` verwendet werden (z. B. `moveq #0,d1` gefolgt von `touch (a0,d1.w)`).
- **Adressregister als Arithmetik-Ziel:** `muls`, `mulu`, `asr`, `lsr`, `spl` dürfen **nicht** auf Adressregister (`An`) zielen. Vor der Zuweisung immer Datenregister nutzen oder über temporäre Variablen im Stack-Frame arbeiten.
- **`subq.l #1, An`:** Verändert die Condition Codes (Z-Flag) **nicht**! Ein nachfolgendes `bne` prüft ein altes Flag und führt zu einer Endlosschleife.
- **Immediate-Shifts:** `asr.l #imm, Dn` / `lsl.l #imm, Dn` akzeptieren nur Werte von `1..8`. Größere Shifts müssen über ein Register (`asr.l Dx, Dy`) oder kaskadiert (`asr.l #8`, `asr.l #7`) realisiert werden.
- **Memory-Shifts:** `lsl.l #1, (ea)` ist im 68k-Befehlssatz ungültig (nur Word `lsl.w (ea)` ist erlaubt). Long-Shifts müssen über Datenregister abgewickelt werden.
- **68040-Mnemonics:** Niemals `fsmul` oder `fsadd` verwenden; immer standardkonformes `fmul.s` / `fadd.s` bzw. 3-Operanden `fmul fp0,fp2,fp4`.

### 5. Modale Intuition Requesters ("Software Failure") & amiagent Entstörung
- **Problem:** Stürzt ein AmigaOS-Prozess ab (z. B. durch Privilege Violation oder Nullzeiger), öffnet Intuition ein modales Systemfenster („Software Failure: Press left mouse button to continue“ bzw. Suspend/Reboot-Knöpfe). Solange dieser Requester offen ist, schließt der Kindprozess seine Streams nicht, und amiagent blockiert im Wartezustand.
- **Lösung ohne Reboot:**  
  Nicht den Netzstecker ziehen! Über `amiga_tool.py` kann der Dialog softwareseitig geschlossen werden:
  ```bash
  # Option A: UI-Klick auf den Suspend-Button
  ./Tools/amiga_tool.py ui-click Suspend
  # Option B: Direkter Mausklick auf den Suspend-Button (typische Koordinaten 446, 570)
  ./Tools/amiga_tool.py click 446 570 --button left
  ```
  Dadurch wird die abgestürzte Task sauber beendet, amiagent wird sofort wieder frei, und ApolloOS läuft ohne Datenverlust weiter.

---

## 7. Datenbank-Integration & Standard-Messframework (`harness-v5`)

Jede erfolgreiche und verifizierte Optimierung (wie z. B. Matrix-Multiplikation, FFT, Audio/Signalverarbeitung) muss **immer** auch in die zentrale Wissensdatenbank `68080-db/` eingepflegt und mit dem Standard-Framework getestet werden:
1. **Sequenzen erstellen:** Relevante Kernsequenzen (Butterflies, Transformationen) als `seq/<ID>.i` anlegen.
2. **Mit `harness-v5` bauen:** Via `tools/build_all.sh` nach `bin-v5/` kompilieren (automatisches Inlining in `harness-v5.asm`).
3. **Auf Vampire V4 messen:** Ausführung auf der Hardware unter `Forbid()` (100 Warmup-Läufe, 100 Messläufe, CCC-Takterfassung, IEP1/IEP2/IEP3 Pipeslot-Auslese).
4. **Logs sichern:** Messlogs nach `measurements/v5-files/<ID>.run01.log` herunterladen und prüfen (`tools/check_log.py`).
5. **Dokumentation pflegen:** Ein eigenes Kapitel (z. B. `13-fft.md`) erstellen, Ergebnisse protokollieren und in `68080-db/index.md` sowie `68080-db/README.md` verlinken.

