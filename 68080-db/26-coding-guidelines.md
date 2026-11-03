# 26 – Simply 68080 Coding Guidelines (Architektur- & Optimierungsregeln)

**Quelle:** Apollo Core Team / Gunnar von Boehn („Simply 68080 coding guidelines“); Apollo 68080 PRM (Juni 2026), S. 83–90.

Diese Richtlinien fassen die grundlegenden Architektur- und Scheduling-Prinzipien des Apollo 68080 (Vampire V4) zusammen. Sie dienen als verbindliche Entwurfs- und Optimierungsbasis für Compilerbauer, Hand-Assembly-Entwickler und KI-Assistenten.

---

## 1. Core-Architektur, Decoder & Durchsatz

* **Decoder-Konfiguration:** Je nach Core-Version sind **2 oder 3 Decoder** aktiv.
* **Operationen pro Takt:** Die CPU kann **2 bis 3 Operationen pro Takt** ausführen (superskalare Ausführung).
* **Instruction Fusing:** Häufig können 2 Instruktionen zu **einer Operation verschmolzen** („fused“) werden.
* **Spitzen-Durchsatz:** Durch Fusing erreicht der 68080 einen theoretischen Maximaldurchsatz von bis zu **6 Instruktionen pro Takt** (*Peak Issue Rate*).
* **Instruktionsgruppen:**
  * Instruktionen werden in Gruppen von **1 bis 6 Instruktionen** gemeinsam gebündelt und ausgeführt.
  * Eine Instruktionsgruppe ist maximal **16 Bytes lang**.
  * Instruktionen innerhalb einer Gruppe müssen entweder **vollständig unabhängig** sein oder als **Fusing-Bundles** zusammengehören.

---

## 2. Instruktionstypen & Ausführungseinheiten

Der 68080 trennt Branch-, Adressberechnungs- (EA) und Recheneinheiten (ALU/FPU) strikt voneinander. Jede Einheit besitzt eigene Register und Pfade:

### 1) Branch- / Jump- / Return-Einheit (BTC / ICache)
* **Befehle:** `BRA`, `BSR`, `BCC`, `DBRA`, `JSR $abs`, `JSR d16(pc)`, `JMP $abs`, `JMP d16(pc)`, `RTS`, `RTD`.
* **Ausführung:** Werden extrem früh in der Pipeline direkt durch den **Instruction Cache (ICache)** und den **Branch Target Cache (BTC)** verarbeitet.
* **Indirekte Sprünge:** `JMP (An)` muss über die EA-Einheit aufgelöst werden und dauert signifikant länger!
* **Return Address / RTS:**
  * `RTS` ist extrem schnell, da der Hardware-Return-Stack-Buffer (RSB) die Rücksprungadresse vorhersagt.
  * **Wichtig:** Niemals die Rücksprungadresse manuell im Stack-Speicher überschreiben! Dies zerstört die Vorhersage, leert die Pipeline und gilt als schlechte Programmierpraxis.

### 2) Effective Address (EA) Einheiten
* **Eigentümer:** Die beiden EA-Einheiten verwalten und besitzen die **16 Adressregister** (`A0`–`A15`).
* **Befehle:** 
  * `LEA (ea), An`
  * `ADDA.W #imm, An` / `SUBA.L An/Dn, An`
  * `ADDQ #1, An` / `SUBQ #1, An`
  * `MOVEA #imm, An`
* **Eingänge:** Register (`An`, `Dn`), Immediates (`#imm`).
* **Ausgänge:** Aktualisieren ausschließlich `An`-Register und erzeugen Adressen für den Speicher- und Daten-Cache-Zugriff.
* **Post-Inkrement / Pre-Dekrement:** Die EA-Einheiten aktualisieren `An` bei Adressierungsarten wie `(An)+` und `-(An)`.
* **Zero-Cost-Updates:** Die CPU kann einige EA-Updates pro Takt **völlig kostenlos** (0 Takte Overhead) miterledigen.
* **Kompaktheit:** `(An)+` und `-(An)` erzeugen besonders dichten Code (kürzeste Opcodes).

### 3) ALU-Einheiten
* **Eigentümer:** Die ALU-Einheiten besitzen die **Datenregister** (`D0`–`D7`, bzw. im AMMX-/E-Modus bis `D15`/`E23`).
* **Befehle:** Der Großteil aller arithmetischen und logischen 68k-Befehle sind ALU-Instruktionen.
* **Eingänge:** Register, Immediates (`#imm`), Speicher.
* **Ausgänge:** Aktualisieren `Dn`-Register oder den Speicher.

---

## 3. Die 9 Goldenen Coding-Regeln

### Regel 1: Weniger Instruktionen schlagen kürzere Instruktionslänge
* Ziel ist immer die **geringste Anzahl von Instruktionen**, selbst wenn die einzelne Instruktion im Byte-Encoding länger ist.
* *Beispiel:*
  * `add.l #9, d0` (1 Instruktion, 6 Bytes) ist **wesentlich besser** als:
  * `addq.l #8, d0` gefolgt von `addq.l #1, d0` (2 Instruktionen, 4 Bytes).

### Regel 2: Kürzere Encodings bevorzugen, wenn semantisch gleichwertig
* Wenn dieselbe Operation mit kürzerer Kodierung existiert, diese wählen:
  * `MOVEQ` / `MOVI.L` vor generischem `MOVE.L #imm`
  * `ADDQ.L #imm` / `ADDIW.L #imm` vor langem `ADDI.L #imm`
  * Kurze Adressierungsarten: `(An)+` ist in der Regel besser als `d16(An)`.
* **Zu vermeidender Modus:** Niemals doppelt-indirekte Adressierung verwenden: `([$1234, An], $1234)`.
  * *Grund:* Dieser Modus ist extrem langsam und blockiert die Pipeline des Cores!
  * *Einzige Ausnahme:* Bei `JMP` oder `JSR` ist doppelte Indirektion akzeptabel (z. B. `jmp (xxx[An])`).

### Regel 3: Unabhängige Instruktionsgruppen bilden (Parallelität)
* Instruktionen so anordnen, dass aufeinanderfolgende Befehle unterschiedliche Zielregister nutzen, sodass sie parallel in den Pipes dekodiert und ausgeführt werden können.
* *Ausnahme:* **Fusing-Paare**. Wenn zwei Befehle vom Core verschmolzen werden können, müssen sie unmittelbar hintereinander stehen.

**Gutes Beispiel (unabhängige Ziele = parallele Pipes):**
```asm
    add.l   d0, d1          ; 3 Instruktionen mit verschiedenen Zielen
    add.l   d0, d2          ; -> laufen voll parallel in den Pipes!
    add.l   d0, d3
    and.w   #$ff, d1
    and.w   #$ff, d2
    and.w   #$ff, d3
    not.l   d1
    not.l   d2
    not.l   d3
```

**Schlechtes Beispiel (Kettung auf demselben Register = Datenabhängigkeit / Stalls):**
```asm
    add.l   d0, d1          ; 3 Instruktionen auf d1 hintereinander
    and.w   #$ff, d1        ; -> Abhängigkeits-Blasen (RAW Hazard)
    not.l   d1
    add.l   d0, d2
    and.w   #$ff, d2
    not.l   d2
    add.l   d0, d3
    and.w   #$ff, d3
    not.l   d3
```

### Regel 4: Schleifen-Design & Loop-Unrolling
* Nahezu alle Integer-Befehle des 68080 benötigen nur **1 Takt**.
* Es gibt **keine Befehlslatenzen**, die durch simples Schleifen-Unrolling überwunden werden müssten!
* Halte den Schleifencode kurz, damit er vollständig im ICache bleibt.
* Schleifen-Overhead ist beim 68080 oft **kostenlos oder extrem gering** (insbesondere mit `DBRA`).
* **Häufig ist KEIN Unrolling die beste Option!** Wenn ungerollt wird, dann nur sehr kontrolliert und begrenzt.
* *Kopier-Beispiel für größere Speicherblöcke:*
  ```asm
  ; Option A: 4 x 32-Bit = 16 Bytes pro Durchlauf
      move.l  (a0)+, (a1)+
      move.l  (a0)+, (a1)+
      move.l  (a0)+, (a1)+
      move.l  (a0)+, (a1)+

  ; Option B: 2 x 16-Byte = 32 Bytes pro Durchlauf
      move16  (a0)+, (a1)+
      move16  (a0)+, (a1)+
  ```
  *Nicht wesentlich weiter unrollen!* Größeres Unrolling verschwendet Cache und bringt keinen Durchsatzgewinn.

### Regel 5: Code- und Daten-Platzierung im Speicher
* Die Cacheline-Größe beträgt für Befehls- und Daten-Cache jeweils **64 Bytes**.
* **Code und Daten strikt trennen:**
  * Wenn Code und Read-Only-Konstanten dieselbe 4-KB-Page teilen müssen, teile sie in **zwei zusammenhängende Blöcke** (z. B. 2 KB Code, danach 2 KB Konstanten).
  * Feine Verschachtelung (z. B. 20 Bytes Code, 16 Bytes Konstanten, 50 Bytes Code, 32 Bytes Konstanten) ist Gift für die Caches!
* **Niemals schreibbare Daten (Variablen, Puffer) in eine Code-Page legen!**

### Regel 6: ALU-zu-EA-Abhängigkeitsblasen vermeiden
* Die ALU besitzt die Datenregister (`Dn`), die EA-Einheit besitzt die Adressregister (`An`).
* Wenn die ALU ein Register manipuliert und die EA-Einheit dieses Register unmittelbar danach für eine Adressberechnung benötigt, entsteht eine **2-Takt-Pipeline-Blase**:
  ```asm
      addq.l  #1, d0
      move.l  (a0, d0), d1    ; SEHR SCHLECHT: ALU hat d0 verändert,
                              ; EA-Einheit braucht d0 sofort -> 2 Takte Bubble!
  ```
* **Index-Modi mit Vorsicht nutzen:**
  * Modi wie `d8(A0, Am)` oder `(d8, An, Dn)` haben ein hohes Stall-Risiko, wenn ein beteiligtes Register zuvor von der ALU bearbeitet wurde.
  * Diese Modi sind sicher und optimal, wenn alle beteiligten Register durch die **EA-Einheit** verwaltet werden (`An` als Counter nutzen; Inkremente via `ADDQ`, `ADDA` oder `LEA`).
  * Ein Index-Modus mit `Dn` ist sicher, wenn die letzte Manipulation durch die ALU mindestens **2 Zyklen** zurückliegt (bei bis zu 6 Instruktionen pro Takt kann dies bis zu 12 Instruktionen Abstand bedeuten!).
* **Grundsatz:**
  * **Adressregister (`An`) für Pointer und Adresszähler** verwenden.
  * **Datenregister (`Dn`) für ALU-Operationen und Schleifenzähler** verwenden.

### Regel 7: Beste Schleifeninstruktion ist DBRA / DBRA.L
* `DBRA` unterstützt 16-Bit-Counter und im 68080-Erweiterungsmodus als **`DBRA.L`** auch volle 32-Bit-Loop-Counter.
* **Standard-Schleifenkonstrukt mit Vorab-Prüfung:**
  ```asm
          bra.s   .start
  .loop
          ; Schleifenkörper
  .start
          dbra.l  d0, .loop
  ```
* **Optimal entkoppelte Schleife (Zero-Overhead):**
  ```asm
  .loop
          clr.l   (a0)+           ; EA-Einheit aktualisiert Pointer (unabhängig von ALU)
          dbra.l  d0, .loop       ; ALU/Branch führt Schleifenzähler & Sprung aus
  ```

### Regel 8: FPU-Parallelität & Latenzen
* Die FPU läuft parallel zu den Integer- und EA-Pipes.
* FPU-Instruktionen haben eine spürbare Latenz (ca. **6 Zyklen Latenz** für `FADD` oder `FMUL`).
* **FPU-Code unrollen:** Nutze die **32 FPU-Register** (`FP0`–`FP31`), um unabhängige Berechnungen verschachtelt zu pipelinen und Rechenlatenzen vollständig zu verstecken.

### Regel 9: Daten-Cache, Memory-Streaming & Speicherzugriff
* **Cache-Reads sind kostenlos:** Pro Takt kann **1 Daten-Cache-Read kostenlos** erfolgen (bei Cache-Hit).
* **Ausrichtung (Alignment):** Speicherlesezugriffe müssen nicht zwingend ausgerichtet sein. Der Core kann Daten sogar aus **zwei verschiedenen Cachelines parallel** ohne Latenzstrafe lesen.
* **Streaming-Prefetcher:**
  * Der Daten-Cache überwacht Zugriffsmuster und führt automatische Hardware-Prefetches durch.
  * Sequenzielle Lesezugriffe mit aufsteigenden Adressen (`(An)+`) werden am besten vorhergesagt.
  * Typische Speicherkopierschleifen eliminieren die DRAM-Latenz dank Prefetching nahezu vollständig.
  * `(An)+` (aufsteigend) ist extrem schnell; `-(An)` (absteigend) ist weniger optimal; rein zufällige Zugriffe (*Pointer Chasing*) sind am langsamsten.
* **Vermeide Look-up-Tabellen (LUTs):**
  * Je nach Größe verursachen Tabellenzugriffe Cache-Misses (~16 Takte Stall).
  * Für mathematische Berechnungen ist es oft deutlich schneller, **10 Takte für Rechenbefehle** aufzuwenden, als einen Speicherzugriff auf eine Tabelle zu wagen.
* **Streaming-Einheit-Limitierung:**
  * Die Prefetch-Streaming-Einheit ist auf das Verfolgen **eines einzelnen Streams** optimiert.
  * Parallele Streams (z. B. simultaner Vergleich zweier großer Speicherbereiche) können den Prefetcher verwirren.
* **Write-Through mit Write-Combining:**
  * Der Speicherbus arbeitet als Write-Through mit automatischer Schreibbündelung (*Write Combine*).
  * Schreibzugriffe werden über mehrere Takte gesammelt und als Block an den Speichercontroller übergeben.
  * Macht `memclr`- und `memcpy`-Schleifen maximal effizient.
* **Gleichzeitiger Durchsatz:** Pro Takt sind bis zu **1 Cached-Memory-Read UND 1 Memory-Write** parallel möglich.

---

## 4. Querverweise im 68080-Archiv

* [`01-fusing.md`](01-fusing.md) – Detaillierte Übersicht aller Fusing-Befehlspaare
* [`02-bonding.md`](02-bonding.md) – Superskalares Dual- und Triple-Issue-Verhalten
* [`05-timing.md`](05-timing.md) – Latenzen, ALU->EA-Blasen und Timing-Fallstricke
* [`06-memory.md`](06-memory.md) – Quad-Alignment, Cachelines und Burst-Übertragungen
* [`08-antipatterns.md`](08-antipatterns.md) – Gemessene Negativ-Beispiele (NOPs, 060-Scheduling, unbedachtes Unrolling)
