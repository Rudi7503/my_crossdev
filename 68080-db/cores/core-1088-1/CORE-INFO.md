# Core-Spezifikation: Apollo 68080 (Core 1088 / Release 1)

**Hardware-Plattform:** Vampire V4 Standalone (V4SA)  
**Core-Kennung:** `CORE;1088;1;0` (PCR: 1088, Revision: 1)  
**Taktfrequenz:** 85.0 MHz effektiv  
**Betriebssystem:** ApolloOS Release 9.5 (AROS-basiert / AmigaOS 3.x kompatibel)  
**Mess-Harness:** Harness v5 (kanonisch) unter `Forbid()`, Warmup: 100 Durchläufe, Messung: 100 Durchläufe, PERF-Zähleraktivierung über `MOVEC $809, CCC`.

---

## 1. Architekturmerkmale dieses Cores

- **Befehlssatz:** Motorola 68000, 68020, 68030, 68040, 68060 voll kompatibel + Apollo 68080 Erweiterungen.
- **AMMX 64-Bit SIMD:**
  - 32 64-Bit-Register (`d0..d7` als E-Register-Unterhälfte, `e0..e23` voll adressierbar).
  - Vektor-Arithmetik: `paddb`, `paddw`, `paddusb`, `paddusw`, `psubb`, `psubw`, `psubusb`, `psubusw`.
  - Vektor-Multiplikation: `pmulh` (Signed 16x16 $\rightarrow$ High 16-Bit), `pmull`, `pmula`, `pmul88`.
  - Vektor-Permutation & Mischen: `vperm`, `packuswb`, `pack3216`, `c2p`, `bsel`.
  - Spezielle Instruktionen:
    - `pavgb`: **Unterstützt (1 Takt für 8 Bytes parallel)** – exzellent für Subpixel/Half-Pel Motion Comp!
    - `bflyw`: **NICHT in Hardware freigegeben** in diesem Core-Release (erzeugt *Privilege Violation* / Exception). Schmetterlinge müssen über `paddw` + `psubw` berechnet werden.
- **Superskalarität & Pipelines:**
  - Zwei parallele Integer-Pipelines (Pipe 0 & Pipe 1), plus FPU/AMMX-Koprozessoreinheit.
  - Hardware Instruction Fusing: 2 aufeinanderfolgende Instruktionen werden in 1 interne Mikro-Op verschmolzen (z.B. `move.l` + `add.l` $\rightarrow$ 3-Operanden-Add in 1 Takt).
  - Instruction Bonding: 2 unabhängige Instruktionen werden im selben Takt parallel abgesetzt.
  - Hardware Performance Monitoring: IEP0, IEP1, IEP2 Pipe-Slots via MOVEC-Schnittstelle (`$809`).
- **SAGA Grafik & Video:**
  - Native YUV-Modi: `APOLLO_SAGA_YUV_888` (`0x0006`) und `APOLLO_SAGA_YUV_444` (`0x000E`).
  - SAGA PiP (Picture-in-Picture) Hardware-Overlay mit Skalierung und Chromakeying.

---

## 2. Zusammenfassung der Messungen auf Core 1088/1

Alle 42 kanonischen Sequenzen sowie die erweiterten Signal- und Transformations-Bibliotheken (Matrix, FFT, DCT, MDCT, JPEG, MPEG, H.264) wurden auf diesem Core gemessen und verifiziert:
- **Jitterfreiheit:** 10/10 v5-Messdurchgänge mit 100/100 Läufen absolut jitterfrei (`MIN == MAX == AVG` bei sauberem Cache-Warmup).
- **Baseline:** 5 Takte für Harness-Overhead (nicht abgezogen, Rohdaten exakt protokolliert).
- **Validierung:** 100% aller Logs besitzen `FILESTAT;1;0` und gültige `PERF`-Zählerblöcke.
