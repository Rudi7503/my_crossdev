# NOP-Kalibrierung & Pipeline-Auslastung (Pipe 1..3)

**Core**: Apollo 68080 Core 11350 @ 92 MHz  
**Target Hardware**: Physical Vampire V4 Standalone  
**Measurement Method**: Harness v6 (10-Iteration Inner Loop)  
**Baseline Loop Overhead**: BASE_10 = 30 Zyklen (3.0 Zyklen/Schleifendurchlauf)

---

## 1. NOP-Kalibrierung (NOP-001 bis NOP-009)

Zur Kalibrierung der Messschleife und zur Bestimmung der Pipeline-Serialisierung wurden 1 bis 9 sequentielle NOP-Befehle gemessen.

### Messergebnisse auf physikalischer V4
| Test-ID | Instruktionsfolge | RAW_10 | BASE_10 | Netto (10x) | Takte / Durchlauf | Takte / NOP | Pipe 1 (Anton) | Pipe 2 (Bruno) | Pipe 3 (Cesar) |
|:---|:---|:---:|:---:|:---:|:---:|:---:|:---:|:---:|:---:|
| **NOP-001** | 1x nop | 86 | 30 | +56 | **5.6 Cyc** | **5.6 Cyc** | 100% | 0% | 0% |
| **NOP-002** | 2x nop | 146 | 30 | +116 | **11.6 Cyc** | **5.8 Cyc** | 100% | 0% | 0% |
| **NOP-003** | 3x nop | 206 | 30 | +176 | **17.6 Cyc** | **5.9 Cyc** | 100% | 0% | 0% |
| **NOP-004** | 4x nop | 266 | 30 | +236 | **23.6 Cyc** | **5.9 Cyc** | 100% | 0% | 0% |
| **NOP-005** | 5x nop | 326 | 30 | +296 | **29.6 Cyc** | **5.9 Cyc** | 100% | 0% | 0% |
| **NOP-006** | 6x nop | 386 | 30 | +356 | **35.6 Cyc** | **5.9 Cyc** | 100% | 0% | 0% |
| **NOP-007** | 7x nop | 446 | 30 | +416 | **41.6 Cyc** | **5.9 Cyc** | 100% | 0% | 0% |
| **NOP-008** | 8x nop | 506 | 30 | +476 | **47.6 Cyc** | **6.0 Cyc** | 100% | 0% | 0% |
| **NOP-009** | 9x nop | 566 | 30 | +536 | **53.6 Cyc** | **6.0 Cyc** | 100% | 0% | 0% |

### Wichtige architektonische Erkenntnisse:
1. **Exakte Linearität:** Jeder zusätzliche NOP fügt der 10-fachen Messschleife **exakt 60 Zyklen** hinzu ($+6.0$ Zyklen pro NOP).
2. **Kein Superskalar-Pairing für NOP:** NOP dient in der 68k-Architektur als Synchronisationsbarriere. Alle NOPs laufen zu 100% über **Decoder Pipe 1 (Anton)**. Pipe 2 (Bruno) und Pipe 3 (Cesar) werden nicht belegt.

---

## 2. Pipeline-Auslastung bei SIMD- und Rechenoperationen

| Test-ID | Operation / Code | Netto-Takte / Iteration | Pipe 1 (Anton) | Pipe 2 (Bruno) | Pipe 3 (Cesar) | Superskalar-Verhalten |
|:---|:---|:---:|:---:|:---:|:---:|:---|
| **BFLY-001** | flyw e2,e8,e0:e1 | **2.6 Cyc** (inc. Loop) | 0% | **100%** | 0% | Einzeltakt-Vektor-Rechenwerk |
| **BFLY-002** | paddw + psubw | **3.6 Cyc** (inc. Loop) | **50%** | **50%** | 0% | **Perfektes Dual-Issue Pairing** |
| **BFLY-003** | flyw (a0),e8,e0:e1 | **2.6 Cyc** (inc. Loop) | 0% | **100%** | 0% | Speicher-VEA gefaltet (0 Zyklen) |
| **BFLY-004** | load + paddw + psubw | **4.6 Cyc** (inc. Loop) | **76%** | **24%** | 0% | Load in Pipe 1, Arithmetik geteilt |
| **SRT-013** | 8-Way Bitonic Sort | **19.6 Cyc** | **92%** | **8%** | 0% | Parallele Compare-Swaps |
