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
| **`BST-002`** | **Optimiert Skalar: Base-Displacement Trick**<br>`slt` + `andi.w #4` + `4(a0, d2.w)` (kein `addi`!) | **5** | **2 Takte** | **7 Takte** | 🔥 **50% Netto-Einsparung (2 Takte schneller)** |
| **`BST-003`** | **Dual-EA Parallel Load + Bitwise Selection**<br>Dual `move.l` mit statischen Offsets + Bitmaske | 9 | **7 Takte** | **12 Takte** | Verhindert ALU-to-EA, aber zu viele Bit-Operationen |
| **`BST-004`** | **AMMX 4-Way SIMD Tree Node (B-Tree)**<br>64-Bit Quadword Key-Fetch + `pcmpgtw` | **3** | **-1 Takt** | **4 Takte** | 🚀 **4 Schlüssel parallel in 4 Takten (1 Takt/Vergleich)** |

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

### 3.2 Warum `BST-003` (Dual Load) langsamer war
In `BST-003` wurden beide Zeiger parallel geladen (`move.l 4(a0), d2` und `move.l 8(a0), d3`), um den 2-Takte-Stall zwischen `andi` und `(a0, d2)` zu umgehen. 
Allerdings erfordert die anschließende bitweise Maskenauswahl (`extb`, `and`, `not`, `and`, `or`) vier zusätzliche Takte, was den Vorteil des parallelen Ladevorgangs wieder aufzehrt.

### 3.3 Der nächste Schritt: AMMX 4-Way SIMD B-Tree (`BST-004`)
Wenn die Baumstruktur an die Hardware angepasst werden darf (B-Tree statt binärer Baum):
* Ein 64-Bit-Knoten enthält **4 Schlüssel gleichzeitig** (`load (a0), e1`).
* `pcmpgtw e0, e1, e2` vergleicht den Suchschlüssel in `e0` simultan mit allen 4 Schlüsseln in **einem einzigen Takt**!
* Die resultierende Maske liefert direkt die Ziel-Kante (Index 0 bis 4).
* **Ergebnis:** Anstatt 4 sequentielle Baum-Ebenen abzuarbeiten, erledigt der 68080 vier Vergleiche in nur **4 Roh-Takten** (Netto: -1 Takt, schlägt die Baseline).

---

## 4. Speicherorte & Reproduzierbarkeit

* **ASM-Sourcen:** `Projects/Tree_Branchless/BST-001.i` bis `BST-004.i`
* **V4-Messprotokolle (40 Rohlogs):** `cores/core-11350/suites/bst_logs/BST-001.run01.log` bis `BST-004.run10.log`
* **Runner-Script:** `/home/papa/run_opt_suite.py bst`
