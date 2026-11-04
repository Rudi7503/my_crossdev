# TODO-Liste: Künftige Komprimierungs-Algorithmen für 68080 & AMMX

Diese Liste sammelt geplante Komprimierungs- und Dekomprimierungs-Algorithmen, die für die Apollo 68080 CPU und den AMMX-Befehlssatz optimiert und vermessen werden sollen.

---

## 1. High-Speed Streaming-Dekompressoren (Maximaler Durchsatz)
- [ ] **LZ4 High-Speed Dekompressor**:
  - *Ziel:* > 400 MB/s Dekompressions-Durchsatz aus FastRAM.
  - *AMMX-Fokus:* 64-Bit Literal-Transfers (`move.q`), unaligned Match-Copy, `storeilm` für Tail-Bytes.
  - *Einsatz:* ROM-, OS- und Spiele-Asset-Streaming.
- [ ] **Google Snappy / FastLZ**:
  - *Ziel:* Byte-orientierte Vektorisierung von Match-Suche und Entpacken.

---

## 2. Amiga-Klassiker & Demoszene-Archivierung
- [ ] **LHA (LH5 / LH6) & LZX Depacker**:
  - *Ziel:* Beschleunigung des Amiga-Standards beim Entpacken von Archiven.
  - *68080-Fokus:* 32-Bit Bitfield-Befehle (`bfextu`, `bfffo`) für verzögerungsfreies Huffman-Parsing; AMMX Match-Copy.
- [ ] **Shrinkler Arithmetic Range Decoder**:
  - *Ziel:* Reduzierung der extrem langen Entpackzeiten von Demoszene-Crunches auf 68k.
  - *68080-Fokus:* Branchless Wahrscheinlichkeits-Aktualisierung und 32-Bit Bereichsteilung.

---

## 3. Verlustfreie Audio-Codecs (Lossless Audio)
- [ ] **FLAC LPC (Linear Predictive Coding) Resynthese-Filter**:
  - *Ziel:* Echtzeit-Dekomprimierung von hochauflösendem FLAC-Audio mit minimaler CPU-Last.
  - *AMMX-Fokus:* FIR-Filter / Polynomauswertung mit parallelen 16-Bit Multiplikationen (`pmullw`, `pmulh`, `paddw`).
- [ ] **QOA (Quite OK Audio)**:
  - *Ziel:* Moderner, offener Audio-Codec für Retro-Games.
  - *AMMX-Fokus:* LMS-Filterung (Least Mean Squares) mit 4-Way SIMD.

---

## 4. 3D-Grafik- & Texturkompression
- [ ] **BC1 / DXT1 (S3TC) Hardware-Texturdecoder**:
  - *Ziel:* Direktes Entpacken von $4 \times 4$ 3D-Texturkacheln für Warp3D / OpenGL / SAGA.
  - *AMMX-Fokus:* 64-Bit Kachel passt exakt in ein `e`-Register; parallele RGB565-Farbinterpolation via `pavgb` und `pmula`.
- [ ] **QOI (Quite OK Image Format)**:
  - *Ziel:* Schnellerer Bildlade-Ersatz für PNG.
  - *AMMX-Fokus:* Paralleles Byte-Tag-Scanning und 64-Eintrags-Farbcache.
