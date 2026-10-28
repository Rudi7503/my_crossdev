# Index aller Einträge

Alphabetisch nach ID. **Alle 42 Einträge wurden mit Harness v5 auf einer
Vampire V4 gemessen** (10 × 100 Läufe). Referenz ist MIN; MAX/AVG sind
Mediane der 10 Läufe; zusätzlich sind die **IEP-Pipe-Slots** angegeben.
Die v5-Logs liegen unter `measurements/v5-files/<ID>.runNN.log`; die
v4- und v1-Logs bleiben Archiv. Eine Tabelle aller Werte steht in
[`measurements/SUMMARY.md`](measurements/SUMMARY.md). Jeder Eintrag enthält
am Ende den **verbatim gemessenen Code** aus `seq/<ID>.i`. Der Übergabestand
steht in [`HANDOFF.md`](HANDOFF.md).

| ID | Titel | Datei | Status |
|---|---|---|---|
| [`AMX-001`](03-ammx.md) | 64-Bit-LOAD/STORE statt zwei 32-Bit-Moves | `03-ammx.md` | gemessen |
| [`AMX-002`](03-ammx.md) | Acht Byte-Additionen mit PADDB | `03-ammx.md` | gemessen |
| [`AMX-003`](03-ammx.md) | Sättigende unsigned Byte-Adds mit PADDUSB | `03-ammx.md` | gemessen |
| [`AMX-004`](03-ammx.md) | Branchless Byte-Auswahl mit PCMPGTB + BSEL | `03-ammx.md` | gemessen |
| [`AMX-005`](03-ammx.md) | Chunky-to-Planar mit C2P | `03-ammx.md` | gemessen |
| [`AMX-006`](03-ammx.md) | 32-Bit-ARGB zu RGB565 mit PACK3216 | `03-ammx.md` | gemessen |
| [`AMX-007`](03-ammx.md) | Vier 16-Bit-Lane-Additionen mit PADDW | `03-ammx.md` | gemessen |
| [`AMX-008`](03-ammx.md) | 16-Bit-Lane-Max mit PCMPGTW + BSEL | `03-ammx.md` | gemessen |
| [`AMX-010`](03-ammx.md) | PACKSSWB-Emulation (Signed Saturation Word zu Byte mit Bias) | `03-ammx.md` | vorbereitet |
| [`ANTI-001`](08-antipatterns.md) | NOP zwischen MOVE und abhängigem ADD | `08-antipatterns.md` | gemessen |
| [`ANTI-002`](08-antipatterns.md) | 68060-Scheduling durch Einschub einer unabhängigen Instruktion | `08-antipatterns.md` | gemessen |
| [`ANTI-003`](08-antipatterns.md) | Word-Updates statt Long-Updates | `08-antipatterns.md` | gemessen |
| [`ANTI-004`](08-antipatterns.md) | Zwei mehrzyklische Instruktionen auf Pipe 1 erwarten | `08-antipatterns.md` | gemessen |
| [`ANTI-005`](08-antipatterns.md) | Unausgerichtete Long-Writes | `08-antipatterns.md` | gemessen |
| [`ANTI-006`](08-antipatterns.md) | TOUCH unmittelbar vor dem Load | `08-antipatterns.md` | gemessen |
| [`BND-001`](02-bonding.md) | Abhängiges MOVEQ + ADD mit zwei Zielen bonden | `02-bonding.md` | gemessen |
| [`BND-002`](02-bonding.md) | Gleiches Ziel ist Fusing, nicht Bonding | `02-bonding.md` | gemessen |
| [`BND-003`](02-bonding.md) | Zwei unabhängige ALU-Instruktionen sind normales Superscalar | `02-bonding.md` | gemessen |
| [`BND-004`](02-bonding.md) | MOVEM belegt Pipe 1 – unabhängige ALU daneben legen | `02-bonding.md` | gemessen |
| [`BND-005`](02-bonding.md) | Drei unabhängige ALU-Instruktionen auf Drei-Pipe-Cores | `02-bonding.md` | gemessen |
| [`FUS-001`](01-fusing.md) | MOVE.L + ADD.L zur 3-Operanden-Fusion | `01-fusing.md` | gemessen |
| [`FUS-002`](01-fusing.md) | MOVE.L + NEG.L | `01-fusing.md` | gemessen |
| [`FUS-003`](01-fusing.md) | MOVEQ + OR.L | `01-fusing.md` | gemessen |
| [`FUS-004`](01-fusing.md) | Zwei MOVE.L (An)+,(Am)+ werden zu einem 64-Bit-Move | `01-fusing.md` | gemessen |
| [`FUS-005`](01-fusing.md) | Zwei CLR.L (An)+ werden zu einem Quad-Clear | `01-fusing.md` | gemessen |
| [`MEM-001`](06-memory.md) | Zwei Long-Writes an einer Quad-Grenze ausrichten | `06-memory.md` | gemessen |
| [`MEM-002`](06-memory.md) | TOUCH mit 12–15 Takten Vorlauf | `06-memory.md` | gemessen |
| [`MEM-003`](06-memory.md) | Sequentielle Loads lösen den Auto-Prefetch aus | `06-memory.md` | gemessen |
| [`MEM-004`](06-memory.md) | 32-Byte-Burst mit acht Long-Loads füllen | `06-memory.md` | gemessen |
| [`MEM-005`](06-memory.md) | 64-Bit-Load/Store statt zweier 32-Bit-Zugriffe | `06-memory.md` | gemessen |
| [`MAT-001`](12-matrix.md) | 2x2 Matrix-Multiplikation (FPU Single Precision) | `12-matrix.md` | gemessen |
| [`MAT-002`](12-matrix.md) | 3x3 Matrix-Multiplikation (FPU Single Precision) | `12-matrix.md` | gemessen |
| [`MAT-003`](12-matrix.md) | 4x4 Matrix-Multiplikation (FPU Single Precision) | `12-matrix.md` | gemessen |
| [`PAT-001`](07-patterns.md) | Kopierschleife mit 16 Byte pro Iteration | `07-patterns.md` | gemessen |
| [`PAT-002`](07-patterns.md) | Clear-Schleife mit 8 Byte pro Iteration | `07-patterns.md` | gemessen |
| [`PAT-003`](07-patterns.md) | Zeiger in der EA-Einheit fortschreiben | `07-patterns.md` | gemessen |
| [`PAT-004`](07-patterns.md) | Branchless Byte-Blend mit AMMX | `07-patterns.md` | gemessen |
| [`PAT-005`](07-patterns.md) | 32-Byte-Kopierburst pro Iteration | `07-patterns.md` | gemessen |
| [`REG-001`](04-register.md) | E-Register als 64-Bit-Speicher – aber nicht als Allzweck-ALU | `04-register.md` | gemessen |
| [`REG-002`](04-register.md) | B-Register nur mit den erlaubten Legacy-Formen | `04-register.md` | gemessen |
| [`REG-003`](04-register.md) | Long/Quad-Updates statt Byte/Word-Merges | `04-register.md` | gemessen |
| [`REG-004`](04-register.md) | Adressarithmetik in der EA-Einheit mit LEA | `04-register.md` | gemessen |
| [`REG-005`](04-register.md) | Keine 3-Operanden-Syntax erzwingen | `04-register.md` | gemessen |
| [`TIM-001`](05-timing.md) | Division durch eine Zweierpotenz durch Shift ersetzen | `05-timing.md` | gemessen |
| [`TIM-002`](05-timing.md) | Adresse mit LEA erzeugen, nicht über ALU->EA | `05-timing.md` | gemessen |
| [`TIM-003`](05-timing.md) | EA->EA-Ketten haben keine Latenz | `05-timing.md` | gemessen |
| [`TIM-004`](05-timing.md) | Fünf Register mit MOVEM statt fünf MOVE.L | `05-timing.md` | gemessen |
| [`TIM-005`](05-timing.md) | MOVE16 ist vier Takte – nicht automatisch schneller | `05-timing.md` | gemessen |

## Nach Kategorie

- **Fusing** (5): [`FUS-001`](01-fusing.md), [`FUS-002`](01-fusing.md), [`FUS-003`](01-fusing.md), [`FUS-004`](01-fusing.md), [`FUS-005`](01-fusing.md)
- **Bonding/Superscalar** (5): [`BND-001`](02-bonding.md), [`BND-002`](02-bonding.md), [`BND-003`](02-bonding.md), [`BND-004`](02-bonding.md), [`BND-005`](02-bonding.md)
- **AMMX** (6): [`AMX-001`](03-ammx.md), [`AMX-002`](03-ammx.md), [`AMX-003`](03-ammx.md), [`AMX-004`](03-ammx.md), [`AMX-005`](03-ammx.md), [`AMX-006`](03-ammx.md)
- **Register** (5): [`REG-001`](04-register.md), [`REG-002`](04-register.md), [`REG-003`](04-register.md), [`REG-004`](04-register.md), [`REG-005`](04-register.md)
- **Timing** (5): [`TIM-001`](05-timing.md), [`TIM-002`](05-timing.md), [`TIM-003`](05-timing.md), [`TIM-004`](05-timing.md), [`TIM-005`](05-timing.md)
- **Speicher/Cache** (5): [`MEM-001`](06-memory.md), [`MEM-002`](06-memory.md), [`MEM-003`](06-memory.md), [`MEM-004`](06-memory.md), [`MEM-005`](06-memory.md)
- **Patterns** (5): [`PAT-001`](07-patterns.md), [`PAT-002`](07-patterns.md), [`PAT-003`](07-patterns.md), [`PAT-004`](07-patterns.md), [`PAT-005`](07-patterns.md)
- **Matrix (FPU)** (3): [`MAT-001`](12-matrix.md), [`MAT-002`](12-matrix.md), [`MAT-003`](12-matrix.md)
- **Antipatterns** (6): [`ANTI-001`](08-antipatterns.md), [`ANTI-002`](08-antipatterns.md), [`ANTI-003`](08-antipatterns.md), [`ANTI-004`](08-antipatterns.md), [`ANTI-005`](08-antipatterns.md), [`ANTI-006`](08-antipatterns.md)

## Sequenzen und Logs

- Sequenzen: `seq/<ID>.i`
- v5-Dateilogs: `measurements/v5-files/<ID>.runNN.log` (420, mit `PERF`)
- v4-Archiv: `measurements/v4/`, `v4-files/`
- v1-Archiv: `measurements/<ID>.log` (42, plus `FUS-001.run1.log`)
- Build-Artefakte: `build-v5/<ID>.asm`, `bin-v5/<ID>`; v1: `build/`, `bin/`
