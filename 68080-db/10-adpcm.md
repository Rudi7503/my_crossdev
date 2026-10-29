# 10 – ADPCM Case Study: 2/3/4/5/6-Bit & Hybrid M/S Audio-Dekodierung

**Projekt:** [`Projects/ADPCM_Decoder/`](../Projects/ADPCM_Decoder/) (`decoder_4bit.s`, `decoder_2bit.s`, `decoder_3bit.s`, `decoder_5bit.s`, `decoder_6bit.s`, `decoder_3_2ms.s`, `decoder_4_2ms.s`, `decoder_5_3ms.s`)  
**Core-Referenz:** Apollo 68080 Core 1088 / Release 1 (`CORE;1088;1;0`, ApolloControl ID 11350) @ 85.0 MHz  
**Messmethode:** Hardware Clock Cycle Counter (CCC), 100 Warmup-Läufe + 100 Messläufe unter `Forbid()`, 1 Block = 15 Bytes (30 Samples). Verifikation: Bit-exakter Golden-Master-Vergleich über 512 Blöcke (15.360 Samples).

---

## 1. Übersicht & Benchmark-Ergebnisse

ADPCM (Adaptive Differential Pulse Code Modulation) komprimiert 16-Bit-PCM-Audio auf 2 bis 6 Bit pro Sample. Da jedes Sample vom vorherigen abhängt (rekursive Prädiktorschleife), lässt sich ADPCM nicht trivial vektorisieren. Die Optimierung auf der 68080 konzentriert sich daher auf die **Eliminierung von Branches**, **vorberechnete Lookup-Tabellen**, **superskalare Registerallokation** und **vollständiges Loop-Unrolling**.

### A. 4-Bit ADPCM Dekodierer (`decoder_4bit.s` – Fresh15)
Ein Block = 15 Bytes = 30 Samples (15 Stereo-Paare L/R bzw. 30 Mono-Samples):

| Modus | Baseline (C / Naiv) | Opt3 (Paar-Unroll) | Fresh9 (Signed-Table) | **Fresh15 (Produktiv)** | Netto-Takte / Sample | CPU-Last @ 44.1 kHz Stereo | Speedup vs. Baseline |
|---|---:|---:|---:|---:|---:|---:|---:|
| **Stereo (L/R)** | 1.546 Takte | 1.242 Takte | 848 Takte | **784 Takte** | **26,13 Takte** | **2,71 %** | **1,97x (−49,3 %)** |
| **Mid-Side (M/S)** | 1.876 Takte | 1.526 Takte | 922 Takte | **920 Takte** | **30,67 Takte** | **3,18 %** | **2,04x (−51,0 %)** |
| **Mono** | 1.624 Takte | 1.256 Takte | 732 Takte | **729 Takte** | **24,30 Takte** | **1,26 %** | **2,23x (−55,1 %)** |

*Alle 18 ADPCM-Modi sind über 512 Blöcke bit-exakt gegen den Golden Master verifiziert (PASS).*

### B. 2/3/5/6-Bit & Hybrid M/S Dekodierer

| Modus | Baseline (MIN / Med) | **Signed-Table Fresh15 (MIN / Med)** | Takte / Sample | Ersparnis |
|---|---:|---:|---:|---:|
| **2-Bit** Stereo / MS / Mono | 2.443 / 2.869 / 2.335 | **1.337 / 1.763 / 1.254** | 44,57 / 58,77 / 41,80 | **−45,3 % / −38,5 % / −46,3 %** |
| **3-Bit** Stereo / MS / Mono | 1.941 / 2.229 / 1.805 | **1.092 / 1.380 / 1.007** | 36,40 / 46,00 / 33,57 | **−43,7 % / −38,1 % / −44,2 %** |
| **5-Bit** Stereo / MS / Mono | 1.356 / 1.512 / 1.236 | **538 / 699 / 503** | 17,93 / 23,30 / 16,77 | **−60,3 % / −53,8 % / −59,3 %** |
| **6-Bit** Stereo / MS / Mono | 1.331 / 1.457 / 1.373 | **609 / 735 / 590** | 20,30 / 24,50 / 19,67 | **−54,2 % / −49,6 % / −57,0 %** |
| **Hybrid** 3/2, 4/2, 5/3 M/S | 2.411 / 2.254 / 1.755 | **1.248 / 1.358 / 958** | 41,60 / 45,27 / 31,93 | **−48,2 % / −39,8 % / −45,4 %** |

---

## 2. Schlüsseloptimierungen im Detail (mit Code-Snippets)

### A. ADP-001: Die vorberechnete Signed-32-Bit-Delta-Tabelle (`Fresh9`)

#### Das naive 68k-Verfahren (vorher):
Im traditionellen IMA-ADPCM wird das Delta bitweise mit Shiften und bedingten Sprüngen berechnet:
```asm
; --- NAIVER ANSATZ: Bis zu 4 Verzweigungen und 4 Shifts pro Sample! ---
        move.w  step,d1         ; d1 = step
        move.w  d1,d2
        lsr.w   #3,d2           ; delta = step >> 3
        btst    #0,d0           ; Teste Bit 0
        beq.s   .b0
        lsr.w   #2,d1           ; step >> 2
        add.w   d1,d2
.b0:    btst    #1,d0           ; Teste Bit 1
        beq.s   .b1
        lsr.w   #1,d1           ; step >> 1
        add.w   d1,d2
.b1:    btst    #2,d0           ; Teste Bit 2
        beq.s   .b2
        add.w   step,d2
.b2:    btst    #3,d0           ; Vorzeichenbit?
        beq.s   .pos
        neg.w   d2              ; negativer Delta-Wert
.pos:   add.w   d2,pcm          ; PCM-Prädiktor aktualisieren
```
*Nachteile:* Bis zu 4 Branch-Mispredictions pro Nibble, starke Serialisierung der CPU-Pipeline.

#### Der 68080-Ansatz (`Fresh9` / `Fresh15`):
Eine vorberechnete Tabelle (`89 Step-Indizes × 16 Nibbles = 1.424 Longwords = 5.696 Bytes`) enthält das **fertige, vorzeichenbehaftete 32-Bit-Delta**:
$$\text{delta\_table}[\text{index}][\text{nibble}] = \text{sign} \cdot \left( \left\lfloor\frac{\text{step}}{8}\right\rfloor + b_0 \left\lfloor\frac{\text{step}}{4}\right\rfloor + b_1 \left\lfloor\frac{\text{step}}{2}\right\rfloor + b_2 \cdot \text{step} \right)$$

```asm
; --- OPTIMIERTER 68080-ANSATZ: 1 Tabellenzugriff, 0 Branches! ---
; a4 = Zeiger auf delta_table (1424 Longwords im L1-Cache)
; d3 = Step-Index (0..88)
; d0 = Nibble (0..15)
; d2 = PCM-Prädiktor (32-Bit signed)

        moveq   #0,d6
        move.w  d3,d6           ; d6 = Step-Index (0..88)
        lsl.w   #4,d6           ; d6 = index * 16
        move.l  d0,d7
        add.w   d6,d7           ; d7 = index * 16 + nibble (0..15)
        move.l  (a4,d7.w*4),d6  ; 1 LOAD liefert das exakte vorzeichenbehaftete Delta!
        add.l   d6,d2           ; PCM += delta in 1 Takt!
```
*Gewinn:* Spart 130 Takte pro Block allein im Stereo-Dekoder (**848 Takte** statt 978 Takte)!

---

### B. ADP-002: 16-Bit-Overflow-Matrix mit `BVC` (`Fresh7`)

Bei Mid-Side Stereo werden Mid ($M$) und Side ($S$) zurück in Links ($L$) und Rechts ($R$) dekodiert:
$$L = M + S, \quad R = M - S$$
Traditioneller 32-Bit-Code führt zwei separate Clamping-Kaskaden mit `cmp.l`, `bge`, `ble` durch.

#### Der 68080-Ansatz:
Da $M$ und $S$ 16-Bit-Werte sind, löst eine 16-Bit-Addition `add.w` bzw. `sub.w` nur im seltenen Überlauffall das **Overflow-Flag (V)** aus:

```asm
; --- 16-Bit Overflow Fast-Path ---
        move.w  d2,d0           ; d2 = Mid (16-Bit)
        add.w   d4,d0           ; d4 = Side -> d0 = L = Mid + Side
        bvc.s   .left_ok        ; Zu 98% KEIN Überlauf -> direkter Sprung!
        ; Seltener Überlauf:
        spl     d0
        ext.w   d0
        eori.w  #$7fff,d0       ; Setzt auf +32767 bzw. -32768
.left_ok:
        move.w  d2,d1
        sub.w   d4,d1           ; d1 = R = Mid - Side
        bvc.s   .right_ok
        spl     d1
        ext.w   d1
        eori.w  #$7fff,d1
.right_ok:
        ; d0 = fertiges L, d1 = fertiges R
```
*Ergebnis:* Reduzierte die M/S-Dekodierzeit von **1.046** auf **922 Takte**!

---

### C. ADP-003: Vollständiges Loop-Unrolling des 15-Byte-Blocks (`Fresh15`)

Ein ADPCM-Block umfasst 15 Bytes:
- **3 × 4-Byte-Bursts:** Geladen via `movex.l (a0)+, e0` (endian-swapped direkt in 64-Bit E-Register).
  - Jedes `movex.l` liefert 8 Nibbles = 4 Stereo-Paare bzw. 8 Mono-Samples.
- **1 × 3-Byte-Rest:** Geladen als 3 einzelne Bytes.
- **Keine innere Schleife (`dbra`):** Alle 15 Stereo-Paare werden linear hintereinander im I-Cache ohne Branch-Instruktionen dekodiert.
*Ergebnis:* Reduzierte Stereo von **829** auf **784 Takte**!

---

## 3. Was funktioniert hat vs. Was gescheitert ist

### Was gut war (übernommen):
1. **Exakte 89×16-Delta-Tabelle:** Ersetzt 3 Bit-Test/Branch-Kaskaden pro Sample durch einen 1-Takt-Load.
2. **Endian-Swapped `MOVEX.L`:** Lädt 4 Bytes (8 Nibbles) und dreht das Byte-Ordering in einem einzigen Befehl.
3. **16-Bit Overflow-Clamping (`bvc`):** Nutzt das ALU-V-Flag für M/S-Matrixing und spart 32-Bit-Vergleiche.
4. **Vollständiges Unrolling bei Stereo:** Eliminiert Schleifen-Overhead vollständig.

### Was schlecht war (verworfen & Fallstricke):
1. **`step * mult >> 3` (Mathematischer Fehler):**  
   Falsch! Die IMA-Spezifikation verlangt, dass *jeder Teilschritt einzeln abgerundet* wird ($\lfloor step/8 \rfloor + \lfloor step/4 \rfloor \dots$). Die Multiplikation erzeugt Rundungsdifferenzen (Off-by-One), die den Prädiktor driften lassen.
2. **`MIN.L` / `MAX.L` (Hardware-Crash):**  
   Vasm akzeptiert die Befehle, die Vampire V4 stürzt jedoch mit sofortigem Hardware-Reset ab, da der 68080-Core keine 32-Bit-Skalarversionen dieser Opcodes besitzt.
3. **Branchless Sign mit `btst / sne / ext / eor / sub / add` (`Fresh5`):**  
   Benötigte 1.047 Takte statt 1.001 Takte. **Lehre:** Branchless Code mit 5 Instruktionen ist auf der 68080 langsamer als ein gut vorhergesagter Branch!
4. **GCC `-m68080 -O2` Compiler-Bug (`ADP-106`):**  
   GCC 6.5.0 erzeugt mit `-m68080` defekte Kurz-Immediates (`moviw.l`) und ungerade Branch-Ziele in `dbf`-Schleifen (Address-Error-Crash). C-Code immer mit `-m68020 -O2` übersetzen!

---

## 4. Offene Potenziale & Challenge

- **AMMX SIMD `PAVGB` für Dithering:** `pavgb` kann Stereo-Kanäle in 1 Takt mitteln.
- **AMMX `VPERM` für Nibble-Extraction:** Ein 64-Bit Vektor `vperm` könnte alle 8 Nibbles aus 4 Bytes parallel entpacken.
- **Challenge:** Kann jemand den 4-Bit Stereo-Dekoder unter **700 Takte pro 15-Byte-Block** (< 23,3 Takte/Sample) bringen? Einreichungen willkommen!
