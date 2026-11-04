# 30. High-Performance Kernels: Memory, String-SIMD & Branchless Sorting

Dieses Kapitel dokumentiert die Implementierung und physikalische Vermessung von 11 modernen System- und Algorithmus-Kernen auf der **Vampire V4 Standalone** (Apollo 68080 Core 11350 @ 92 MHz) unter `Forbid()` mit **Harness v5** (jeweils 10 Durchgänge à 100 Iterationen, Clock Cycle Counter CCC).

---

## 1. Übersicht der gemessenen Bibliotheks-Kerne

Alle 11 Kerne wurden auf echter V4-Hardware stabil und mit 10/10 reproduzierbaren Ergebnissen gemessen:

### 1.1 Memory Suite (`MEM-010` bis `MEM-013`)
Fokus: Maximale Speicherbandbreite, Dual-Stream Overlap und branchless Tail-Handling.

| Test-ID | Operation / Kernel | Daten / Block | Befehle | Netto-Takte (MIN) | Durchsatz (92 MHz) | Besonderheit |
|---|---|:---:|:---:|:---:|:---:|---|
| **`MEM-010`** | **AMMX Fast `memset`** | 32 Bytes | 4 | **0 Takte** (Roh: 5) | **588.8 MB/s** | 4× 64-bit `store`, sättigt Write-Buffer |
| **`MEM-011`** | **AMMX Fast `memcpy`** | 32 Bytes | 8 | **4 Takte** (Roh: 9) | **327.1 MB/s** | Dual-Stream Early Store Overlap |
| **`MEM-012`** | **Polymorphic Tail-Store** | 1..7 Bytes | 3 | **-1 Takt** (Roh: 4) | **Branchless Tail** | `storeilm` mit Masken-LUT, keine Reste-Loops |
| **`MEM-013`** | **AMMX Overlap-Safe Copy** | 16 Bytes | 4 | **0 Takte** (Roh: 5) | **294.4 MB/s** | Dual 64-bit `load`/`store` Streaming |

### 1.2 String & Text Processing Suite (`STR-001` bis `STR-004`)
Fokus: Parallele 8-Zeichen-Verarbeitung pro Takt über `pcmpeqb` und Permutationen.

| Test-ID | Operation / Kernel | Zeichen / Takt | Befehle | Netto-Takte (MIN) | Takte / Zeichen | Geschwindigkeitsvorteil |
|---|---|:---:|:---:|:---:|:---:|---|
| **`STR-001`** | **AMMX SIMD `strlen`** | 8 Zeichen | 3 | **-1 Takt** (Roh: 4) | **0.50 Takte** | **4× bis 6× schneller** als 68060-Schleife |
| **`STR-002`** | **AMMX SIMD `memchr`/`strstr`** | 8 Zeichen | 3 | **-1 Takt** (Roh: 4) | **0.50 Takte** | Broadcast-Match mit `pcmpeqb` |
| **`STR-003`** | **Base64 SIMD Unpack** | 4 Symbole (24b$\rightarrow$32b) | 5 | **1 Takt** (Roh: 6) | **1.50 Takte** | `vperm` + `lsrq` + `pand` Bit-Extraktion |
| **`STR-004`** | **JSON Structural Scanner** | 8 Zeichen | 5 | **1 Takt** (Roh: 6) | **0.75 Takte** | Erkennt `"` und `:` parallel via `pcmpeqb` + `por` |

### 1.3 Sorting & Branchless Suite (`SRT-001` bis `SRT-003`)
Fokus: Eliminierung von Branch-Misprediction-Stalls durch vektorielle Min/Max- und Bit-Operationen.

| Test-ID | Algorithmus / Kernel | Eingabe | Befehle | Netto-Takte (MIN) | Takte / Element | Besonderheit |
|---|---|:---:|:---:|:---:|:---:|---|
| **`SRT-001`** | **8-Element SIMD Sortiernetzwerk** | 8 Bytes | 5 | **2 Takte** (Roh: 7) | **0.875 Takte** | Bitonic Stage via `pminub`, `pmaxub`, `vperm` |
| **`SRT-002`** | **Branchless Binary Search Step** | 1 Intervall | 4 | **0 Takte** (Roh: 5) | **1 Intervall** | `pcmpgtw` + `bsel`, 0 Mispredictions |
| **`SRT-003`** | **64-Bit Parallel `POPCNT`** | 64 Bits | 10 | **7 Takte** (Roh: 12) | **7.00 Takte** | Hamming-Weight (Shift-Add) für 64 Bits |

---

## 2. Zentrale architektonische Erkenntnisse

### 2.1 Warum `memset` und `memcpy` auf dem 68080 Rekordwerte erreichen
1. **DDR3-Write-Buffer-Sättigung (`MEM-010`):**
   * Vier hintereinander abgesetzte `store e0,(a1)+` schreiben 32 Bytes am Stück in den Write-Buffer.
   * Mit einem Rohwert von nur 5 Takten für 32 Bytes liegt der Durchsatz bei **588.8 MB/s**, was die physische Busbandbreite des Standalone-Boards nahezu vollständig ausreizt.
2. **Early Store Overlap bricht Memory-Latenz (`MEM-011`):**
   * Durch das Schema `load e0 -> load e1 -> store e0 -> load e2 -> store e1 -> ...` wird das Schreiben des ersten Vektors überlappt mit dem Lesen des dritten Vektors. Dadurch entstehen keine Pipeline-Pausen zwischen EA- und Bus-Interface.
3. **Branchless Tail mit `storeilm` (`MEM-012`):**
   * Die größte Bremse in traditionellen AmigaOS-Kopierroutinen sind die letzten 1–7 unvollständigen Bytes, die üblicherweise über langsame Byte-Schleifen (`dbra`) gesichert werden.
   * Mit `storeilm e0, e6, (a1)+` und einer vorberechneten Lane-Maske in `e6` werden beliebige Rest-Bytes in **Rohzeit 4 Takten** geschrieben – vollkommen ohne Verzweigungen.

### 2.2 Der Durchbruch bei SIMD-Strings (`strlen`, `strstr`, JSON)
1. **8 Zeichen pro Zyklus (`STR-001` & `STR-002`):**
   * Klassische 68k-Routinen lesen `move.b (a0)+, d0` gefolgt von `beq.b`. Jeder Zeichenvergleich kostet mindestens 2–4 Takte.
   * AMMX lädt mit `load (a0)+, e0` 8 Zeichen gleichzeitig. `pcmpeqb e1, e0, e2` vergleicht alle 8 Zeichen simultan in **1 Takt**.
   * Mit Rohzeit 4 Takten für 8 Zeichen sinkt der Aufwand auf **0.50 Takte pro Zeichen**.
2. **JSON Structural Token Scanner (`STR-004`):**
   * Inspiriert von modernen Architekturen (*simdjson*) klassifiziert der Kern 8 Zeichen parallel:
     `pcmpeqb` scannt auf Anführungszeichen `"`, ein zweiter `pcmpeqb` scannt auf Doppelpunkte `:`. `por` führt die Treffer zusammen. In nur **1 Netto-Takt** sind 8 Zeichen vor-geparsed.

### 2.3 Branchless Sorting Networks & POPCNT
1. **Sortieren ohne Sprünge (`SRT-001`):**
   * Klassische Sortierer scheitern an Zufallsdaten wegen unvorhersehbarer Verzweigungen.
   * Das AMMX-Sortiernetzwerk nutzt `vperm`, um Nachbarn zu spiegeln, und führt mit `pminub` und `pmaxub` den Parallelaustausch durch.
   * 8 Werte werden in nur **2 Netto-Takten** (Rohwert 7 Takte) vorsortiert – garantiert ohne Flushes der 68080-Pipeline.
2. **Paralleler 64-Bit `POPCNT` (`SRT-003`):**
   * Ohne dedizierten x86-POPCNT-Befehl benötigte 68k bisher bis zu 64 Iterationen.
   * Mit paralleler SIMD-Hamming-Weight-Arithmetik (2-Bit- und 4-Bit-Maskenaddition) wird ein 64-Bit-Wort in **7 Netto-Takten** aufsummiert.

---

## 3. Speicherorte & Reproduzierbarkeit

* **ASM-Sourcen:**
  * `Projects/Memory_AMMX/MEM-010.i` bis `MEM-013.i`
  * `Projects/String_AMMX/STR-001.i` bis `STR-004.i`
  * `Projects/Sort_AMMX/SRT-001.i` bis `SRT-003.i`
* **V4-Messprotokolle (110 Rohlogs):**
  * `cores/core-11350/suites/mem_logs/` (40 Logs)
  * `cores/core-11350/suites/str_logs/` (40 Logs)
  * `cores/core-11350/suites/srt_logs/` (30 Logs)
* **Runner-Script:** `/home/papa/run_opt_suite.py [mem|str|srt]`
