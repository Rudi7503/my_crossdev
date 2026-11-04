# 28. Umfassende Optimierung des 68080-Testkorpus

Dieses Kapitel dokumentiert das systematische Audit aller **77 kanonischen Testfälle** der 68080-Datenbank sowie die auf der physikalischen **Vampire V4 Standalone** (Apollo 68080 Core 11350 @ 92 MHz, PCR 1088/1) gemessenen Optimierungen unter `Forbid()` mit **Harness v5** (jeweils 10 Durchgänge à 100 Iterationen).

---

## 1. Architektur-Audit: Die zwei Test-Kategorien

Das Audit aller 77 Fälle zeigt eine fundamentale Trennung:

### A. 32 Synthetische Kalibrierungs- & Anti-Pattern-Benchmarks (Unverändert)
Diese Tests dürfen **nicht** optimiert werden, da sie als Referenzmaßstab für Hardware-Gefahren und Überhänge dienen:
* **`ANTI-001` bis `ANTI-007` (7 Tests):** Isolieren gezielt Programmierfehler (z. B. ALU-zu-EA-Stallblasen von 2 Takten, Double-Memory-Indirect-Stalls).
* **`PAT-001` bis `PAT-005` (5 Tests):** Messen Pipeline-Hazards (RAW, WAW, WAR).
* **`BND-001` bis `BND-005` (5 Tests):** Ermitteln die Dekodiergrenzen von Dual- und Triple-Issue.
* **`FUS-001` bis `FUS-005` (5 Tests):** Prüfen Instruktionsfusions-Bedingungen.
* **`MEM-001` bis `MEM-005` (5 Tests):** Kalibrieren Adressierungsmodi.
* **`REG-` & `TIM-001..005` (10 Tests):** Grundrauschen und Messoverhead von `MOVEC CCC, d0`.

### B. 45 Algorithmische Rechenkerne (Enormes Optimierungspotenzial)
In den echten SIMD-, Matrix-, Signal- und Blit-Kernen führten **Memory-VEA**, **2× Unrolling** und **Interleaved Scheduling** zu dramatischen Geschwindigkeitssteigerungen.

---

## 2. Gemessene Ergebnisse auf Vampire V4 Hardware

### Gruppe A: AMMX-Grundoperationen (Memory-VEA & 2× Unrolling)

| Basis-Test | Opt-ID | Operation / Strategie | Basis (Netto) | Opt (Netto) | Durchsatz-Gewinn |
|---|---|---|:---:|:---:|:---:|
| `AMX-002` | **`AMX-102`** | `PADDB` (Memory-VEA + 2× Unroll, 16 Bytes) | 5 Takte / 8B | **2 Takte / 16B** | **5.0× höherer Durchsatz** |
| `AMX-003` | **`AMX-103`** | `PADDUSB` (Memory-VEA + 2× Unroll, 16 Bytes) | 5 Takte / 8B | **2 Takte / 16B** | **5.0× höherer Durchsatz** |
| `AMX-005` | **`AMX-105`** | `C2P` Chunky-to-Planar (2× Unroll, 16 Bytes) | 4 Takte / 8B | **2 Takte / 16B** | **4.0× höherer Durchsatz** |
| `AMX-006` | **`AMX-106`** | `PACK3216` ARGB$\rightarrow$RGB565 (2× Unroll + Store Overlap) | 5 Takte / 4px | **4 Takte / 8px** | **2.5× höherer Durchsatz** |
| `AMX-007` | **`AMX-107`** | `PADDW` (Memory-VEA + 2× Unroll, 8 Words) | 5 Takte / 4w | **2 Takte / 8w** | **5.0× höherer Durchsatz** |
| `AMX-008` | **`AMX-108`** | `PCMPGTW` + `BSEL` (2× Unroll + Store Overlap) | 7 Takte / 4w | **8 Takte / 8w** | **1.75× höherer Durchsatz** |

### Gruppe D: 3D-Matrix- & Fixed-Point-Kerne (Interleaved Pipes)

| Basis-Test | Opt-ID | Operation / Strategie | Basis (Netto) | Opt (Netto) | Speedup |
|---|---|---|:---:|:---:|:---:|
| `FIX-001` | **`FIX-101`** | $2\times 2$ 16-Bit Fixed-Point Matrix Multiply (Interleaved Pipes) | 17 Takte | **12 Takte** | **+29.4% schneller** |
| `FIX-002` | **`FIX-102`** | $3\times 3$ 16-Bit Fixed-Point Matrix Multiply (Interleaved Rows + Early Store) | 34 Takte | **30 Takte** | **+11.8% schneller** |

### Gruppe B/C: Signalverarbeitung (DCT & FFT)

| Basis-Test | Opt-ID | Operation / Strategie | Basis (Netto) | Opt (Netto) | Speedup |
|---|---|---|:---:|:---:|:---:|
| `DCT-001` | **`DCT-101`** | Chen 1D 8-Punkt Float DCT (Dual-Accumulator Tree) | 53 Takte | **49 Takte** | **-4 Takte Latenz** |
| `DCT-009` | **`DCT-109`** | MP3 18-Sample Overlap-Add (Interleaved Memory-VEA) | 23 Takte | **13 Takte** | **+43.5% schneller** |
| `FFT-001` | `FFT-101` | Radix-2 Float Butterfly (Parallel Loads) | 28 Takte | 30 Takte | Skalar-FPU bevorzugt Block-Arithmetic |

---

## 3. Zentrale architektonische Erkenntnisse für 68080-Entwickler

1. **Memory-VEA schlägt Register-Loads dramatisch:**
   * Bei vektoriellen Additionen (`PADDB`, `PADDUSB`, `PADDW`) spart der Direktzugriff `(a0)+` das Laden in ein E-Register vollständig ein. Die EA-Unit holt den Operanden parallel, wodurch 16 Bytes in nur **2 Takten** abgearbeitet werden (Verfünffachung des Durchsatzes).
2. **Interleaving bricht Matrix-Multiplikationslatenzen:**
   * In `FIX-101` werden die Multiplikationen von Zeile 0 und Zeile 1 verschränkt. Während Zeile 0 auf die Multiplikationsergebnisse wartet, berechnet der Multiplikator bereits Zeile 1. Das senkt die Ausführungszeit von 17 auf **12 Takte**.
3. **Dual-Accumulator Trees bei Skalar-FPU (`DCT-101`):**
   * Statt alle 8 Multiplikationen seriell in ein einziges Register `fp0` aufzuaddieren, teilt ein 2-Wege-Akkumulator-Baum (`fp0` für gerade, `fp2` für ungerade Indizes) die Latenz auf.
4. **Grenzen bei der Skalar-FPU (`FFT-101`):**
   * Im Gegensatz zum AMMX-SIMD-Registerfile mit 24 Registern führt das Zerlegen von FPU-Operationen bei Skalar-Float schnell zu FPU-Registerdruck. Hier bleibt die kompakte Block-Arithmetik am schnellsten.

---

## 4. Speicherorte der Messprotokolle

* **Gruppe A Logs:** `cores/core-11350/suites/ammx_opt_logs/` (60 Rohlogs)
* **Gruppe D Logs:** `cores/core-11350/suites/fix_opt_logs/` (20 Rohlogs)
* **Gruppe B/C Logs:** `cores/core-11350/suites/dsp_opt_logs/` (30 Rohlogs)
* **ASM-Sequenzen:** `Projects/Optimization_Suite/*.i`
