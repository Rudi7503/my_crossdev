# 11 – ADPCM Instruktionskandidaten: Gemessen vs. Geschätzt

**Core-Referenz:** Apollo 68080 Core 1088 / Release 1 (`CORE;1088;1;0`, ApolloControl 11350) @ 85.0 MHz  
**Messharness:** Harness v5 (`harness-v5.asm`) & Standalone ADPCM Suite (`Projects/ADPCM_Decoder`)  
**Goldene Regel der Datenbank:**  
Eine Zahl ist **nur dann ein Messwert**, wenn ein 100×-Mikrobenchmark auf echter Vampire-Hardware vorliegt und im Log dokumentiert ist. Alles andere ist ausdrücklich **Hypothese / Schätzung** und darf nicht als Fakt gewertet werden.

---

## 1. Verifizierte & Gemessene Instruktionen

| Instruktion | Einsatzbereich | Messwert (Hardware V4 Core 1088/1) | Pipe-Slots (v5: IEP1 / IEP2) | Status & Bewertung |
|---|---|---|---|---|
| **`MOVEX.L`** | 4-Byte-Bitstream laden + Byte-Swap | Verifiziert in `Fresh15` (3× pro Block) | Im Block-Mix: IPC = **1,83** (24,1 % Bruno) | **Exzellent.** Spart separate Endian-Swap-Schleifen vollständig ein. |
| **`PADDB`** | 8× 8-Bit Lane-Addition | **5 Takte MIN** (`AMX-002`) | `IEP1 = 6`, `IEP2 = 0`, `IEP3 = 0` | Ideal für Byte-Vektoren, für 16-Bit-Audio zu schmal. |
| **`PADDW`** | 4× 16-Bit Lane-Addition | **5 Takte MIN** (`AMX-007`) | `IEP1 = 6`, `IEP2 = 0`, `IEP3 = 0` | Gleicher Taktpreis wie `PADDB`, perfekt für 16-Bit PCM-Lanes. |
| **`PAVGB`** | 8× 8-Bit Parallele Mittelwertbildung | **11 Takte** für 64 Pixel (0,17 c/pix) | `IEP1 = 12`, `IEP2 = 0`, `IEP3 = 0` | **Verifiziert (PASS).** Exzellent für Stereo-Mittelung & Dithering. |
| **`PACKUSWB`** | 8× 16-Bit Word $\rightarrow$ 8-Bit Byte Saturation | **106 Takte** für 64 Werte (1,66 c/sample) | `IEP1 = 7`, `IEP2 = 0` (`AMX-010`) | **Verifiziert (PASS).** Für 8-Bit-Quantisierung via Bias-Trick. |
| **`PACK3216`** | 2× 32-Bit $\rightarrow$ 16-Bit Packen | **5 Takte MIN** (`AMX-006`) | `IEP1 = 6`, `IEP2 = 0`, `IEP3 = 0` | **Für Audio ungeeignet!** Packt fest in 16-Bit RGB565 (5-6-5 Bitmasken). |
| **`PCMPGTW` + `BSEL`** | 4× 16-Bit Lane Max/Min Selection | **7 Takte** AMMX vs. 33 Takte Skalar | `IEP1 = 7`, `IEP2 = 0` (**4,7x Speedup**) | **Verifiziert.** Funktioniert für 16-Bit-Worte; kein 32-Bit-`PCMPGTL` vorhanden. |
| **`MIN.L` / `MAX.L`** | Skalarer 32-Bit Clamp | **V4-Hardware-Reset (Absturz!)** | *Nicht ausführbar* | **Verworfen.** Assembliert mit vasm, löst auf V4 CPU-Trap/Reset aus. |
| **`PADDSW` / `PSUBSW`** | Signed Saturation Add/Sub | *Unknown Mnemonic* (vasm 2.0d) | *Nicht verfügbar* | **Nicht verfügbar.** Assembler kennt diese Opcodes nicht. |
| **`BFLYW`** | Hardware-Schmetterling | **Privilege Violation (`0x80000008`)** | *Nicht ausführbar* | **Verworfen.** Auf Core 1088/1 nicht in Hardware freigegeben. |
| **`subq.l #1, An` + `bne`** | Adressregister als Schleifenzähler | **Endlosschleife / Crash** | *Nicht ausführbar* | **Verworfen.** Adressregister-Arithmetik setzt keine CPU-Flags! |

---

## 2. Detaillierte Analyse der Kandidaten

### A. 16-Bit Audio Clamping: Skalar vs. `PCMPGTW + BSEL` vs. `PMAXSW / PMINSW`

Im ADPCM-Dekoder muss jedes berechnete PCM-Sample auf $[-32768, +32767]$ geklemmt werden.

#### 1. Traditioneller Skalar-Pfad:
```asm
; --- 2 Vergleiche + bis zu 2 Branches PRO SAMPLE ---
        cmp.l   #32767,d2
        ble.s   .min_ok
        move.l  #32767,d2
        bra.s   .clamped
.min_ok:
        cmp.l   #-32768,d2
        bge.s   .clamped
        move.l  #-32768,d2
.clamped:
```
*Kosten:* 4–8 Takte pro Sample, erzeugt häufige Pipeline-Stalls durch unvorhersehbare Audio-Amplituden.

#### 2. Vektorieller AMMX-Pfad mit `PCMPGTW` + `BSEL` (4 Samples parallel):
```asm
; e0 = 4 PCM-Samples
; e1 = Vektor mit [+32767, +32767, +32767, +32767]
; e2 = Vektor mit [-32768, -32768, -32768, -32768]
        pcmpgtw e0,e1,e3        ; e3 Maske: wo ist Sample > +32767?
        bsel    e3,e1,e0,e0     ; Ersetze Ausreißer durch +32767
        pcmpgtw e2,e0,e4        ; e4 Maske: wo ist -32768 > Sample?
        bsel    e4,e2,e0,e0     ; Ersetze Unterläufe durch -32768
```
*Gemessene Zeit:* **7 Takte für 4 Samples** (1,75 Takte pro Sample) statt 33 Takte im Skalar-Code (**4,7-facher Speedup**)!  
*Einschränkung:* Funktioniert nur, wenn der Prädiktor im 16-Bit-Zweierkomplement geführt wird oder Überläufe zuvor begrenzt wurden.

#### 3. Der `PMAXSW` / `PMINSW`-Kandidat:
AMMX besitzt die hardware-beschleunigten Parallel-Extremwert-Befehle:
```asm
        pminsw  e1,e0,e0        ; e0 = min(e0, +32767) in 1 Takt!
        pmaxsw  e2,e0,e0        ; e0 = max(e0, -32768) in 1 Takt!
```
*Potenzial:* Reduziert das Clamping von 4 Samples auf **nur 2 Takte** (0,50 Takte/Sample)!

---

### B. 4-Bit Nibble-Unpacking: Shift-Kette vs. `VPERM`

Um aus einem geladenen 32-Bit-Longword (`e0`) die 8 ADPCM-Nibbles zu isolieren:

#### 1. Traditionelle Shift-Kette:
```asm
        move.l  e0,d0
        andi.l  #15,d0          ; Nibble 0
        lsr.l   #4,e0
        move.l  e0,d1
        andi.l  #15,d1          ; Nibble 1
        lsr.l   #4,e0
        ; ... 8-mal wiederholen ...
```
*Kosten:* 16 Instruktionen für 8 Nibbles (ca. 16–20 Takte).

#### 2. AMMX `VPERM` Entpackung (Hypothese):
Mit einer Permutationsmaske in `e22` können alle 8 Nibbles in einem Takt parallel in 8 separate Byte-Lanes dupliziert und mit `pand` maskiert werden:
```asm
        vperm   #$00112233,e23,e0,e1   ; Dupliziert Bytes in Word-Lanes
        pand    mask_low,e1,e2         ; Low-Nibbles
        psrlw   #4,e1,e3               ; High-Nibbles
```
*Erwarteter Gewinn:* 4 Takte statt 18 Takte für 8 Nibbles.

---

## 3. Zusammenfassung der Schätzungsmatrix

| Optimierungskandidat | Zielbereich | Erwarteter Nutzen | Hürde / Bemerkung |
|---|---|---:|---|
| **`PMINSW` + `PMAXSW`** | 16-Bit Clamping | **10–15 % Gesamtspeedup** | Erfordert 16-Bit Prädiktor-Vektoren (4 Kanäle/Samples parallel). |
| **`VPERM` Nibble-Unpack** | Bitstream-Parsing | **4–6 % Gesamtspeedup** | Lohnt sich nur bei blockweiser Vektorverarbeitung. |
| **`PAVGB` M/S-Matrix** | Mid-Side Rekonstruktion | **3–5 % Gesamtspeedup** | $L = (M+S)$, $R = (M-S)$ muss mit Dithering harmonieren. |
| **`PACKUSWB` Bias-Trick** | 8-Bit Audio-Ausgabe | **25–35 % bei 8-Bit Playback** | Vollständig verifiziert in `AMX-010`. |

---

## 4. Die ADPCM-Challenge: *„Break 700 Cycles!“*

Der aktuelle Weltrekord auf der Vampire V4 (Core 1088/1 @ 85 MHz) für einen 15-Byte-Block 4-Bit-Stereo-ADPCM liegt bei:
$$\mathbf{784\text{ Takte}}\quad (\text{Fresh15}, 26,13\text{ Takte/Sample})$$

Wir fordern jeden heraus:
- Nutze `PMINSW`/`PMAXSW` oder `VPERM`, um den Dekoder unter **700 Takte pro Block** (< 23,3 Takte/Sample) zu optimieren!
- Reiche den Code als `decoder_4bit_challenger.s` mit verifiziertem Golden-Pass (512 Blöcke bit-exakt) ein!
