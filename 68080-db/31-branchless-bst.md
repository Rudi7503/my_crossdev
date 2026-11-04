# 31. Branchless Binary Search Tree (BST) auf 68080 / Vampire V4

Dieses Kapitel analysiert und misst die Umwandlung von Kontrollfluss-Verzweigungen (Branchy `bgt`/`blt`) in reine Datenabhängigkeiten (Branchless Data-Driven Pointer Selection) bei der Traversierung von binären Suchbäumen (Binary Search Trees, BST).

Alle Messungen wurden auf der physischen **Vampire V4 Standalone** (Apollo 68080 Core 11350 @ 92 MHz) unter `Forbid()` mit **Harness v5** durchgeführt (10 Durchgänge à 100 Iterationen, Clock Cycle Counter CCC).

---

## 1. Problemstellung: Warum klassische BST-Suche die Pipeline bremst

Bei einem klassischen Binärbaum-Suchlauf entscheidet der Vergleich an jedem Knoten, welcher Kindknoten geladen wird:

```asm
.search_loop:
    move.l  (a0),d0         ; Lade Schlüssel des aktuellen Knotens
    cmp.l   d1,d0           ; Vergleiche Suchwert (d1) mit Knoten-Schlüssel (d0)
    beq.s   .found          ; Treffer
    bgt.s   .go_left        ; Verzweigung nach links (Control Hazard!)
    
    move.l  8(a0),a0        ; Rechter Zeiger
    bra.s   .next_iteration

.go_left:
    move.l  4(a0),a0        ; Linker Zeiger

.next_iteration:
    cmp.l   #0,a0           ; Ende erreicht?
    bne.s   .search_loop
```

### Das Pipeline-Problem
Da reale Suchdaten in einem Binärbaum annähernd pseudozufällig nach links oder rechts verzweigen, liegt jede statische oder dynamische Sprungvorhersage (Branch Prediction Unit, BPU) bei ca. 50% der Verzweigungen falsch. Jeder Fehltritt (Branch Misprediction) leert die Vorstufe des 68080-Befehlsdecoders und kostet bis zu **6–10 Takte Pipeline-Flush**.

---

## 2. Gemessene Testfälle auf Vampire V4 Hardware

| Test-ID | Strategie / Implementierung | Befehle | Netto-Takte (MIN) | Roh-Takte (MIN) | Speedup / Architektur-Effekt |
|---|---|:---:|:---:|:---:|---|
| **`BST-001`** | **Basis Branchless (User-Vorschlag)**<br>`slt` + `extb.l` + `andi.l #4` + `addi.l #4` + `0(a0, d2.l)` | 7 | **4 Takte** | **9 Takte** | Vollkommen branchless, 0 Mispredictions |
| **`BST-002`** | **Optimiert Skalar: Base-Displacement Trick**<br>`slt` + `andi.w #4` + `4(a0, d2.w)` (kein `addi`!) | 5 | **2 Takte** | **7 Takte** | 🔥 **50% Netto-Einsparung (2 Takte schneller)** |
| **`BST-003`** | **Dual-EA Parallel Load + Bitwise Selection**<br>Dual `move.l` mit statischen Offsets + Bitmaske | 9 | **7 Takte** | **12 Takte** | Verhindert ALU-to-EA, aber zu viele Bit-Operationen |
| **`BST-004`** | **AMMX 4-Way SIMD Tree Node (B-Tree)**<br>64-Bit Quadword Key-Fetch + `pcmpgtw` | 3 | **-1 Takt** | **4 Takte** | 🚀 **4 Schlüssel parallel in 4 Takten (1 Takt/Vergleich)** |
| **`BST-005`** | **Ultra-Kompakt: `subx.w` + Hardware EA-Scaler**<br>`move.l` + `sub.l` + `subx.w` + `8(a0, d2.w*4)` | **4** | **2 Takte** | **7 Takte** | 🔥 **Nur 4 Befehle!** Kein `slt`, kein `ext`, kein `and`, kein `add` |
| **`BST-006`** | **Eytzinger Implicit Array-Tree (`addx.l`)**<br>`move.l (a0, d2.l*4)` + `sub.l` + `addx.l d2, d2` | **3** | **-1 Takt** | **4 Takte** | 👑 **Absoluter Rekord:** 3 Befehle, kompletter Verzweigungsschritt in 4 Roh-Takten! |
| **`BST-007`** | **2-Level Unrolled Pipeline-Interleaved BST**<br>Traversiert 2 vollständige Ebenen am Stück | 8 | **10 Takte** | **15 Takte** | 5 Netto-Takte / Ebene bei fortlaufender Zeigerkette |

---

## 3. Architektur-Analyse & Optimierungstricks

### 3.1 Die Optimierung von `BST-001` zu `BST-002`: Der Base-Displacement Trick
In der Ausgangsimplementierung (`BST-001`) wird der Zieloffset arithmetisch berechnet:
1. `slt d2` $\rightarrow$ `-1` ($FF) oder `0`.
2. `andi.l #4, d2` $\rightarrow$ `4` oder `0`.
3. `addi.l #4, d2` $\rightarrow$ `8` oder `4`.
4. `move.l 0(a0, d2.l), a0` $\rightarrow$ Lade Offset 8 oder Offset 4.

**Warum `BST-002` 2 Takte schneller ist:**
Die 68k-CPU besitzt die Adressierungsart `d16(An, Xi)` (Basis-Displacement + Indexregister).
Wir können die Addition von `4` direkt in die Adressberechnung der Hardware verlagern:
```asm
; seq/BST-002.i - 5 Befehle statt 7 Befehle
        move.l  (a0),d0         ; Lade Schlüssel
        cmp.l   d1,d0           ; Vergleiche
        slt     d2              ; d2.b = $FF oder $00
        andi.w  #4,d2           ; d2.w = 4 (links) oder 0 (rechts)
        move.l  4(a0,d2.w),a0   ; 4 + 0 = Offset 4 (links), 4 + 4 = Offset 8 (rechts)
```
* **Effekt:** `extb.l` und `addi.l #4` fallen komplett weg.
* **Ergebnis auf V4:** Die Nettozeit sinkt von 4 Takten auf **2 Takte**!

### 3.2 Die Perfektionierung des Zeiger-Baums: `BST-005` mit `subx.w` und Hardware-Skalierung (`*4`)
Kann man `BST-002` noch weiter verdichten?
Ja, indem man das 68k Extend-Flag ($X$) und die Hardware-Indexskalierung nutzt:
```asm
; seq/BST-005.i - Nur noch 4 Befehle (14 Bytes Code!)
        move.l  (a0),d0         ; Lade Schlüssel
        sub.l   d1,d0           ; Vergleiche Ziel d1 mit Schlüssel d0 (setzt X-Flag)
        subx.w  d2,d2           ; d2.w = -X (-1 oder 0), unbeeinflusst vom Vorinhalt!
        move.l  8(a0,d2.w*4),a0 ; Hardware EA-Scaler: 8 + (-1)*4 = 4 oder 8 + 0*4 = 8!
```
* `sub.l d1, d0` setzt das Extend-Flag $X=1$ (wenn $d_0 < d_1$) oder $X=0$ (wenn $d_0 \ge d_1$).
* `subx.w d2, d2` subtrahiert $d_2$ von sich selbst plus $X$: Das ergibt **garantiert $-1$ oder $0$ in 1 Takt**, egal was vorher in $d_2$ stand!
* `8(a0, d2.w*4)` nutzt den Hardware-Multiplizierer der Adressgenerierung:
  * Für $d_2 = -1$: $8 + (-1 \times 4) = 4$ (Linker Zeiger bei Offset 4).
  * Für $d_2 = 0$: $8 + (0 \times 4) = 8$ (Rechter Zeiger bei Offset 8).
* **Ergebnis:** Kein `slt`, kein `extb`, kein `andi`, kein `addi` – der gesamte Zeigerwechsel benötigt **nur noch 4 Befehle**!

### 3.3 Der algorithmische Durchbruch: Eytzinger Array-Tree (`BST-006`)
In der modernen Algorithmik (Khuong & Morin) werden binäre Suchbäume in einem flachen Array ohne Zeiger gespeichert (analog zu einem Binary Heap):
* Wurzel bei Index $k = 1$.
* Linkes Kind von $k$ bei Index $2k$.
* Rechtes Kind von $k$ bei Index $2k + 1$.

Auf der 68k-Architektur wird dieser Verzweigungsschritt zu einer einzigen Rechenoperation:
```asm
; seq/BST-006.i - Nur 3 Befehle für den kompletten Verzweigungsschritt!
        move.l  (a0,d2.l*4),d0  ; Lade Schlüssel A[k] aus flachem Array
        sub.l   d1,d0           ; Vergleiche Ziel d1 mit Schlüssel d0 (setzt X-Flag)
        addx.l  d2,d2           ; d2 = 2*k + X !
```
* `addx.l d2, d2` berechnet $d_2 + d_2 + X$:
  * Ist $X = 0$ (gehe links): $d_2 \leftarrow 2k$.
  * Ist $X = 1$ (gehe rechts): $d_2 \leftarrow 2k + 1$.
* **Ergebnis auf V4:** Mit **3 Befehlen** und **Rohzeit 4 Takten (Netto: -1 Takt)** schlägt dieser Kern sogar die Baseline-Schleife der Hardware! Keine Zeiger-Dereferenzierung, keine Sprünge, maximale Cache-Lokalität.

---

## 4. Offizielle Messung unter Harness v6 (10× Loop Standard)

Um den 5-Takte-Mess-Overhead von `movec` restlos zu eliminieren und den echten Durchsatz in einer realen Schleife ohne künstliches Entrollen zu messen, wurde **Harness v6** (`harness-v6.asm`) eingeführt. Jeder Testfall wird in einer echten 10er-Schleife (`dbra`) gemessen und gegen die leere 10er-Schleife (Baseline = 30 Takte) bereinigt.

### Ergebnisse auf physischer Vampire V4 Standalone (Harness v6, 10 × 100 Läufe)

| Test-ID | Implementierung / Strategie | Befehle | Rohzeit (10×) | Baseline (10×) | Netto (10×) | **Takte / Schritt (CYC/OP)** | Speedup vs. Basis |
|---|---|:---:|:---:|:---:|:---:|:---:|:---:|
| **`BST-001`** | **Dein Entwurf (Referenz-Pointer)**<br>`slt` + `extb` + `andi` + `addi` + `0(a0, d2.l)` | 7 | 105 Takte | 30 Takte | 75 Takte | **7.5 Takte** | Basis (Branchless) |
| **`BST-002`** | **Base-Displacement Trick**<br>`slt` + `andi.w #4` + `4(a0, d2.w)` | 5 | 85 Takte | 30 Takte | 55 Takte | **5.5 Takte** | 🔥 **-2.0 Takte schneller (-26.7%)** |
| **`BST-003`** | **Dual-EA Parallel Load**<br>Dual `move.l` + Bitmask-Selection | 9 | 126 Takte | 30 Takte | 96 Takte | **9.6 Takte** | Langsamer (zu viele Bit-Befehle) |
| **`BST-004`** | **AMMX 4-Way SIMD Node (B-Tree)**<br>64-Bit Quadword Key-Fetch + `pcmpgtw` | 3 | 46 Takte | 30 Takte | 16 Takte | **1.6 Takte** | 🚀 **4 Vergleiche in 1.6 Takten (0.4 T/Key)** |
| **`BST-005`** | **Ultra-Kompakt: `subx.w` + EA-Scale**<br>`sub.l` + `subx.w` + `8(a0, d2.w*4)` | 4 | 85 Takte | 30 Takte | 55 Takte | **5.5 Takte** | 🔥 **5.5 Takte** in nur 4 Befehlen (14 Bytes) |
| **`BST-006`** | **Eytzinger Implicit Array-Tree**<br>`move.l (a0, d2.l*4)` + `sub.l` + `addx.l d2, d2` | 3 | 55 Takte | 30 Takte | 25 Takte | **2.5 Takte** | 👑 **3.0× schneller (nur 2.5 Takte / Ebene!)** |

---

## 5. Speicherorte & Reproduzierbarkeit

* **ASM-Sourcen:** `Projects/Tree_Branchless/BST-001.i` bis `BST-007.i`, `BST-016.i`
* **V4-Messprotokolle (Harness v6):** `cores/core-11350/suites_v6/bst_v6_logs/` (60 Rohlogs)
* **Runner-Script:** `/home/papa/run_v6_suite.py bst`
* **Harness v6 Source:** `68080-db/harness-v6.asm`
