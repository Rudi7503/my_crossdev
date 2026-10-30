# 19 – 2D Sprite Blitting & 256-Bit Aligned Rectangle / Tiling Benchmark

**Dateien:**
- Single Blit Suite: [`Projects/Sprite_Blit_256bit/`](../Projects/Sprite_Blit_256bit/) (`main.c`, `blit.h`, `blit_c.c`, `blit_asm.s`, `Makefile`, `SpriteBlit`)
- Full-Screen Tiled Suite: [`Projects/Sprite_Tiled_Blit/`](../Projects/Sprite_Tiled_Blit/) (`main.c`, `tiled.h`, `tiled_c.c`, `tiled_asm.s`, `Makefile`, `TiledBlit`, `tiled_results.txt`)

**Ziel:** Benchmark und Verifikation von 2D-Sprite- und Rechteck-Kopierroutinen (Opaque Blits) auf der Vampire V4 (Apollo 68080).
Getestet werden alle 7 Standard-Sprite-Größen (8×8 bis 512×512) über alle 4 Farbtiefen (8-Bit Chunky, 16-Bit RGB565, 24-Bit RGB24, 32-Bit ARGB) auf einem Full-HD-Hintergrund (1920×1080 Schwarz, Ziel-Pixel Grau 127).

Getestet wurden zwei Szenarien:
1. **Szenario 1 (Single Blit):** Einzelnes Sprite an 256-Bit ausgerichteter Zielkoordinate `(128, 128)` auf den 1920×1080 Hintergrund kopieren.
2. **Szenario 2 (Full-Screen Tiled Blit):** Sprites gekachelt (Tiled) über den gesamten 1920×1080 Hintergrund kopieren:
   - **Teil A (Aligned Tiling):** Start bei `(0, 0)` (100% 256-Bit Ziel-Alignment).
   - **Teil B (1-Pixel Offset Unaligned Tiling):** Start bei `(1, 1)` (Zieladressen nicht an 32-Byte Cachelines ausgerichtet).

> **Standard-Instruktion:** `move16` im 256-Bit Burst (2× `move16` pro 32-Byte Cacheline).
> **Hardware-Messstatus (Vampire V4 Standalone, PCR 1088/1 / Apollo Core 11350, 92 MHz, ApolloOS 9.5):**
> - **Single Blit:** **28 von 28 Tests (100%) bit-exakt verifiziert (`diff=0, PASS`)!**
> - **Full-Screen Tiled Blit:** **56 von 56 Tests (100%) bit-exakt verifiziert (`diff=0, PASS`)!**
> - **Spitzen-Bandbreite:** **219,3 MB/s** (`move16` Tiled 128×128 @ 32-Bit).
> - **Maximaler Speedup vs. C `memcpy`:**
>   - Single Blit: **3,88x schneller** (8×8 @ 8-Bit: 181 cyc vs. 702 cyc).
>   - Tiled Blit: **5,78x schneller** (16×16 @ 8-Bit Offset: 3.029.863 cyc vs. 17.504.291 cyc).
> - **Optimierungsdurchbruch (Alignment Peeling):** Bei unaligned Zielkoordinaten `(1, 1)` beschleunigt 256-Bit Alignment Peeling (`copy_line_peeled_m16`) große Kacheln um bis zu **35,0%** gegenüber unaligned `move16` (3,77M cyc vs. 5,80M cyc @ 512×512 32-Bit).

---

## 1. Szenario 1: Single Blit (Rechteck-Kopie an Koordinate 128, 128)

Gemessen auf Vampire V4 Hardware (92 MHz, PCR 1088/1 / Apollo Core 11350). Alle Werte stellen CCC-Zyklen für einen einzelnen Blit dar.

### 8-Bit Chunky (1 Bpp, Stride = 1920 Bytes)
| Sprite-Größe | Zeile (Bytes) | C-Ref (cyc) | m68k ASM (cyc) | move16 (cyc) | AMMX SIMD (cyc) | Speedup vs C | Durchsatz (move16) | Parität |
|---|---|---|---|---|---|---|---|---|
| **8 × 8** | 8 Byte | 702 | 181 | **181** | 173 | **3,88x** | 30,1 MB/s | **PASS** |
| **16 × 16** | 16 Byte | 1.013 | 350 | **350** | 335 | **2,89x** | 62,2 MB/s | **PASS** |
| **32 × 32** | 32 Byte | 2.177 | 1.071 | **1.069** | 1.042 | **2,04x** | 81,4 MB/s | **PASS** |
| **64 × 64** | 64 Byte | 4.996 | 2.771 | **2.774** | 2.640 | **1,80x** | 125,5 MB/s | **PASS** |
| **128 × 128** | 128 Byte | 13.871 | 8.268 | **8.269** | 8.209 | **1,68x** | 168,4 MB/s | **PASS** |
| **256 × 256** | 256 Byte | 38.481 | 29.164 | **28.141** | 27.254 | **1,37x** | 198,0 MB/s | **PASS** |
| **512 × 512** | 512 Byte | 162.425 | 135.979 | **138.722** | 140.281 | **1,17x** | 160,6 MB/s | **PASS** |

### 16-Bit RGB565 (2 Bpp, Stride = 3840 Bytes)
| Sprite-Größe | Zeile (Bytes) | C-Ref (cyc) | m68k ASM (cyc) | move16 (cyc) | AMMX SIMD (cyc) | Speedup vs C | Durchsatz (move16) | Parität |
|---|---|---|---|---|---|---|---|---|
| **8 × 8** | 16 Byte | 533 | 199 | **200** | 191 | **2,66x** | 54,4 MB/s | **PASS** |
| **16 × 16** | 32 Byte | 1.113 | 558 | **562** | 542 | **1,98x** | 77,4 MB/s | **PASS** |
| **32 × 32** | 64 Byte | 2.521 | 1.411 | **1.411** | 1.355 | **1,79x** | 123,4 MB/s | **PASS** |
| **64 × 64** | 128 Byte | 6.889 | 4.186 | **4.596** | 4.560 | **1,50x** | 151,5 MB/s | **PASS** |
| **128 × 128** | 256 Byte | 19.361 | 15.071 | **14.432** | 13.443 | **1,34x** | 193,0 MB/s | **PASS** |
| **256 × 256** | 512 Byte | 81.566 | 66.730 | **67.033** | 71.215 | **1,22x** | 166,2 MB/s | **PASS** |
| **512 × 512** | 1024 Byte | 289.191 | 257.877 | **265.738** | 261.166 | **1,09x** | 167,7 MB/s | **PASS** |

### 24-Bit RGB24 (3 Bpp, Stride = 5760 Bytes)
| Sprite-Größe | Zeile (Bytes) | C-Ref (cyc) | m68k ASM (cyc) | move16 (cyc) | AMMX SIMD (cyc) | Speedup vs C | Durchsatz (move16) | Parität |
|---|---|---|---|---|---|---|---|---|
| **8 × 8** | 24 Byte | 794 | 214 | **216** | 203 | **3,68x** | 75,6 MB/s | **PASS** |
| **16 × 16** | 48 Byte | 1.396 | 625 | **625** | 598 | **2,23x** | 104,4 MB/s | **PASS** |
| **32 × 32** | 96 Byte | 2.883 | 1.752 | **1.734** | 1.627 | **1,66x** | 150,6 MB/s | **PASS** |
| **64 × 64** | 192 Byte | 8.048 | 5.499 | **5.904** | 5.619 | **1,36x** | 176,9 MB/s | **PASS** |
| **128 × 128** | 384 Byte | 25.016 | 19.319 | **19.576** | 18.347 | **1,28x** | 213,4 MB/s | **PASS** |
| **256 × 256** | 768 Byte | 111.908 | 99.976 | **99.093** | 99.688 | **1,13x** | 168,6 MB/s | **PASS** |
| **512 × 512** | 1536 Byte | 414.063 | 386.552 | **380.779** | 384.374 | **1,09x** | 175,6 MB/s | **PASS** |

### 32-Bit ARGB (4 Bpp, Stride = 7680 Bytes)
| Sprite-Größe | Zeile (Bytes) | C-Ref (cyc) | m68k ASM (cyc) | move16 (cyc) | AMMX SIMD (cyc) | Speedup vs C | Durchsatz (move16) | Parität |
|---|---|---|---|---|---|---|---|---|
| **8 × 8** | 32 Byte | 583 | 301 | **303** | 295 | **1,92x** | 71,8 MB/s | **PASS** |
| **16 × 16** | 64 Byte | 1.281 | 728 | **732** | 700 | **1,75x** | 118,9 MB/s | **PASS** |
| **32 × 32** | 128 Byte | 3.563 | 2.060 | **2.307** | 2.009 | **1,54x** | 150,9 MB/s | **PASS** |
| **64 × 64** | 256 Byte | 9.447 | 7.017 | **7.148** | 7.331 | **1,32x** | 194,8 MB/s | **PASS** |
| **128 × 128** | 512 Byte | 31.658 | 25.502 | **25.848** | 22.845 | **1,22x** | 215,5 MB/s | **PASS** |
| **256 × 256** | 1024 Byte | 145.987 | 130.361 | **129.871** | 129.262 | **1,12x** | 171,6 MB/s | **PASS** |
| **512 × 512** | 2048 Byte | 540.099 | 502.845 | **502.462** | 504.987 | **1,07x** | 177,4 MB/s | **PASS** |

---

## 2. Szenario 2: Full-Screen Tiled Blitting (1920×1080 Fläche)

Hier wird der gesamte Bildschirm durch Wiederholung (Tiling) des Sprites gefüllt (z. B. 32.400 Kacheln bei 8×8).
Verglichen werden vier Implementierungsstrategien:
1. **C-Ref:** C-Referenz-Schleife mit `memcpy`.
2. **Discrete move16:** 2D-Kachel-Gitter ruft für jede Kachel direkt die Assembler-Funktion `blit_rect_m16` auf.
3. **Scanline move16:** Zeilenbasierter Wiederholungs-Blitter mit `copy_line_m16`.
4. **Alignment-Peeled move16:** Scanline-Blitter mit `copy_line_peeled_m16` (schält unaligned Kopf-Bytes ab, sodass der Haupttransfer exakt auf 256-Bit / 32-Byte Cachelines trifft).

### 8-Bit Chunky (1 Bpp, 2.073.600 Bytes)
#### [Teil A] Aligned Tiling (Start bei x=0, y=0)
| Sprite-Größe | C-Ref (cyc) | Discrete m16 | Scanline m16 | Peeled m16 | Bester Speedup | MB/s (Best) | Parität |
|---|---|---|---|---|---|---|---|
| **8 × 8** | 31.615.525 | **6.175.217** | 21.396.342 | 27.664.602 | **5,12x** | 28,5 MB/s | **PASS** |
| **16 × 16** | 13.156.875 | **2.926.022** | 11.055.966 | 17.101.520 | **4,50x** | 60,2 MB/s | **PASS** |
| **32 × 32** | 6.993.683 | **2.218.593** | 6.237.614 | 6.894.671 | **3,15x** | 79,4 MB/s | **PASS** |
| **64 × 64** | 3.831.727 | **1.451.545** | 3.461.391 | 3.766.698 | **2,64x** | 121,4 MB/s | **PASS** |
| **128 × 128** | 2.296.983 | **1.071.586** | 2.116.233 | 2.297.103 | **2,14x** | 164,5 MB/s | **PASS** |
| **256 × 256** | 1.629.892 | **904.971** | 1.423.531 | 1.495.585 | **1,80x** | 194,8 MB/s | **PASS** |
| **512 × 512** | 1.343.441 | **1.095.409** | 1.212.538 | 1.217.706 | **1,23x** | 160,9 MB/s | **PASS** |

#### [Teil B] 1-Pixel Offset Tiling (Start bei x=1, y=1: Unaligned Destination)
| Sprite-Größe | C-Ref (cyc) | Discrete m16 | Scanline m16 | Peeled m16 | Bester Speedup | MB/s (Best) | Parität |
|---|---|---|---|---|---|---|---|
| **8 × 8** | 26.476.431 | **6.161.469** | 21.429.479 | 27.371.926 | **4,30x** | 28,6 MB/s | **PASS** |
| **16 × 16** | 17.504.291 | **3.029.863** | 11.113.367 | 17.153.137 | **5,78x** | 58,2 MB/s | **PASS** |
| **32 × 32** | 9.623.853 | **2.274.763** | 6.306.599 | 7.562.939 | **4,23x** | 77,5 MB/s | **PASS** |
| **64 × 64** | 5.180.366 | **1.596.507** | 3.586.971 | 4.262.027 | **3,24x** | 110,4 MB/s | **PASS** |
| **128 × 128** | 2.996.924 | **1.233.314** | 2.288.969 | 2.484.073 | **2,43x** | 142,9 MB/s | **PASS** |
| **256 × 256** | 1.983.732 | **1.013.499** | 1.572.096 | 1.675.345 | **1,96x** | 173,9 MB/s | **PASS** |
| **512 × 512** | 1.480.536 | 1.551.773 | 1.423.762 | **1.282.097** | **1,15x** | 137,5 MB/s | **PASS** |

---

### 16-Bit RGB565 (2 Bpp, 4.147.200 Bytes)
#### [Teil A] Aligned Tiling (Start bei x=0, y=0)
| Sprite-Größe | C-Ref (cyc) | Discrete m16 | Scanline m16 | Peeled m16 | Bester Speedup | MB/s (Best) | Parität |
|---|---|---|---|---|---|---|---|
| **8 × 8** | 26.225.927 | **6.756.559** | 21.973.162 | 34.144.430 | **3,88x** | 52,2 MB/s | **PASS** |
| **16 × 16** | 13.912.118 | **4.711.579** | 12.449.903 | 13.667.809 | **2,95x** | 74,8 MB/s | **PASS** |
| **32 × 32** | 7.631.961 | **2.976.883** | 6.887.082 | 7.444.310 | **2,56x** | 118,4 MB/s | **PASS** |
| **64 × 64** | 4.566.965 | **2.200.571** | 4.174.137 | 4.467.800 | **2,08x** | 160,2 MB/s | **PASS** |
| **128 × 128** | 3.066.487 | **1.809.122** | 2.689.303 | 2.884.302 | **1,70x** | 194,9 MB/s | **PASS** |
| **256 × 256** | 2.437.558 | **2.230.894** | 2.295.025 | 2.439.698 | **1,09x** | 158,0 MB/s | **PASS** |
| **512 × 512** | 2.116.851 | 2.080.755 | **2.002.527** | 2.026.342 | **1,06x** | 176,0 MB/s | **PASS** |

#### [Teil B] 1-Pixel Offset Tiling (Start bei x=1, y=1: Unaligned Destination)
| Sprite-Größe | C-Ref (cyc) | Discrete m16 | Scanline m16 | Peeled m16 | Bester Speedup | MB/s (Best) | Parität |
|---|---|---|---|---|---|---|---|
| **8 × 8** | 35.325.331 | **6.812.404** | 22.099.362 | 34.130.429 | **5,19x** | 51,7 MB/s | **PASS** |
| **16 × 16** | 19.256.454 | **4.863.806** | 12.497.892 | 14.677.717 | **3,96x** | 72,5 MB/s | **PASS** |
| **32 × 32** | 10.474.041 | **3.205.560** | 7.088.256 | 8.469.113 | **3,27x** | 110,0 MB/s | **PASS** |
| **64 × 64** | 5.950.884 | **2.432.082** | 4.517.209 | 4.854.316 | **2,45x** | 144,9 MB/s | **PASS** |
| **128 × 128** | 3.793.833 | **2.197.800** | 2.917.026 | 3.107.448 | **1,73x** | 160,4 MB/s | **PASS** |
| **256 × 256** | 2.846.181 | 3.056.080 | 2.609.647 | **2.441.100** | **1,17x** | 144,4 MB/s | **PASS** |
| **512 × 512** | 2.374.597 | 3.049.438 | 2.369.834 | **2.054.441** | **1,16x** | 171,6 MB/s | **PASS** |

---

### 24-Bit RGB24 (3 Bpp, 6.220.800 Bytes)
#### [Teil A] Aligned Tiling (Start bei x=0, y=0)
| Sprite-Größe | C-Ref (cyc) | Discrete m16 | Scanline m16 | Peeled m16 | Bester Speedup | MB/s (Best) | Parität |
|---|---|---|---|---|---|---|---|
| **8 × 8** | 34.773.940 | **7.283.467** | 22.460.400 | 40.810.279 | **4,77x** | 72,6 MB/s | **PASS** |
| **16 × 16** | 14.683.336 | **5.264.966** | 13.010.380 | 15.615.048 | **2,79x** | 100,4 MB/s | **PASS** |
| **32 × 32** | 8.383.102 | **3.612.565** | 7.623.241 | 8.218.877 | **2,32x** | 146,4 MB/s | **PASS** |
| **64 × 64** | 5.294.998 | **2.855.437** | 4.815.169 | 4.996.609 | **1,85x** | 185,2 MB/s | **PASS** |
| **128 × 128** | 3.898.997 | **2.512.993** | 3.484.920 | 3.748.696 | **1,55x** | 210,4 MB/s | **PASS** |
| **256 × 256** | 3.274.205 | 3.168.920 | **2.951.564** | 3.074.200 | **1,11x** | 179,1 MB/s | **PASS** |
| **512 × 512** | 2.956.537 | 2.998.049 | **2.918.845** | 2.949.024 | **1,01x** | 181,2 MB/s | **PASS** |

#### [Teil B] 1-Pixel Offset Tiling (Start bei x=1, y=1: Unaligned Destination)
| Sprite-Größe | C-Ref (cyc) | Discrete m16 | Scanline m16 | Peeled m16 | Bester Speedup | MB/s (Best) | Parität |
|---|---|---|---|---|---|---|---|
| **8 × 8** | 31.141.991 | **7.560.466** | 22.528.610 | 40.666.811 | **4,12x** | 69,9 MB/s | **PASS** |
| **16 × 16** | 20.697.671 | **5.589.606** | 13.212.611 | 17.087.508 | **3,70x** | 94,6 MB/s | **PASS** |
| **32 × 32** | 11.446.380 | **4.115.908** | 8.098.156 | 9.088.936 | **2,78x** | 128,5 MB/s | **PASS** |
| **64 × 64** | 6.912.302 | **3.243.916** | 4.937.029 | 5.590.269 | **2,13x** | 163,0 MB/s | **PASS** |
| **128 × 128** | 4.648.608 | **2.859.774** | 3.825.752 | 3.942.150 | **1,63x** | 184,9 MB/s | **PASS** |
| **256 × 256** | 3.715.686 | 4.333.858 | 3.444.951 | **3.209.791** | **1,16x** | 164,7 MB/s | **PASS** |
| **512 × 512** | 3.241.216 | 4.350.668 | 3.363.818 | **2.933.054** | **1,11x** | 180,3 MB/s | **PASS** |

---

### 32-Bit ARGB (4 Bpp, 8.294.400 Bytes)
#### [Teil A] Aligned Tiling (Start bei x=0, y=0)
| Sprite-Größe | C-Ref (cyc) | Discrete m16 | Scanline m16 | Peeled m16 | Bester Speedup | MB/s (Best) | Parität |
|---|---|---|---|---|---|---|---|
| **8 × 8** | 27.818.376 | **10.213.630** | 24.901.409 | 27.238.312 | **2,72x** | 69,0 MB/s | **PASS** |
| **16 × 16** | 15.359.846 | **6.097.649** | 13.757.788 | 14.853.102 | **2,52x** | 115,6 MB/s | **PASS** |
| **32 × 32** | 9.127.485 | **4.383.861** | 8.309.426 | 8.818.029 | **2,08x** | 160,8 MB/s | **PASS** |
| **64 × 64** | 6.098.530 | **3.663.593** | 5.414.037 | 5.703.058 | **1,66x** | 192,4 MB/s | **PASS** |
| **128 × 128** | 4.617.479 | **3.215.397** | 4.196.528 | 4.364.736 | **1,44x** | 219,3 MB/s | **PASS** |
| **256 × 256** | 4.062.574 | 4.084.713 | **3.897.898** | 3.913.345 | **1,04x** | 180,9 MB/s | **PASS** |
| **512 × 512** | 3.943.753 | 4.043.041 | **3.669.207** | 3.715.863 | **1,07x** | 192,1 MB/s | **PASS** |

#### [Teil B] 1-Pixel Offset Tiling (Start bei x=1, y=1: Unaligned Destination)
| Sprite-Größe | C-Ref (cyc) | Discrete m16 | Scanline m16 | Peeled m16 | Bester Speedup | MB/s (Best) | Parität |
|---|---|---|---|---|---|---|---|
| **8 × 8** | 39.916.046 | **10.457.453** | 24.917.849 | 28.639.222 | **3,82x** | 67,4 MB/s | **PASS** |
| **16 × 16** | 21.533.292 | **6.631.917** | 14.094.124 | 16.448.321 | **3,25x** | 106,3 MB/s | **PASS** |
| **32 × 32** | 12.281.725 | **4.935.784** | 8.958.501 | 9.470.312 | **2,49x** | 142,8 MB/s | **PASS** |
| **64 × 64** | 7.811.739 | **4.203.514** | 5.697.762 | 6.166.253 | **1,86x** | 167,7 MB/s | **PASS** |
| **128 × 128** | 5.549.211 | **3.778.459** | 4.761.415 | 4.590.118 | **1,47x** | 186,6 MB/s | **PASS** |
| **256 × 256** | 4.533.394 | 5.818.537 | 4.346.389 | **3.905.645** | **1,16x** | 180,5 MB/s | **PASS** |
| **512 × 512** | 4.084.308 | 5.802.226 | 4.370.886 | **3.772.455** | **1,08x** | 186,9 MB/s | **PASS** |

---

## 3. Technische Erkenntnisse & Optimierungsanalyse

### A. Discrete move16 vs. Scanline-Loop bei kleinen Kacheln
- Für kleine bis mittlere Kacheln (8×8 bis 128×128) ist **Discrete `move16` um Faktor 2x bis 3,5x schneller als die Scanline-Schleife**.
- **Grund:** In einem Scanline-Repeater muss pro Scanline (1080 Zeilen) und pro horizontalem Kachelsegment die X-Grenze, der Modulo-Versatz und Zeilenpointer in C neu berechnet werden. `Discrete move16` ruft dagegen für jede Kachel direkt die 2D-Assembler-Funktion `blit_rect_m16` auf. Die innere Schleife verbleibt für alle Zeilen der Kachel fest im Befehlscache der 68080.

### B. Das Alignment-Peeling Phänomen bei großen unaligned Kacheln
- Auf der Apollo 68080 führen unaligned Speicherzugriffe (z. B. Start bei Koordinate `x=1, y=1`) zwar hardwareseitig zu **keinem Absturz oder Trap**, aber beim Schreiben über 32-Byte Cacheline-Grenzen muss der Speicher-Controller Bus-Zyklen aufsplitten.
- Bei kleinen Kacheln dominiert der Schleifen-Overhead. Bei großen Kacheln (256×256 und 512×512) dominiert jedoch die Speicher-Bandbreite.
- **Messbeweis:**
  - Bei 512×512 @ 32-Bit mit 1-Pixel Versatz benötigt `Discrete move16` unaligned **5.802.226 Zyklen**.
  - `Alignment-Peeled move16` schält die ersten unaligned Bytes separat ab und schreibt die restliche Zeile mit **100% 256-Bit ausgerichteten Cacheline-Bursts**.
  - Ergebnis: **3.772.455 Zyklen** – ein Performance-Vorteil von **35,0% (bzw. 54% mehr Bandbreite: 186,9 MB/s vs. 121,3 MB/s)**!

### C. 256-Bit Burst mit `move16`:
- Auf der 68080 überträgt `move16 (a0)+,(a1)+` genau 16 Bytes (128 Bit) in einem optimierten Speicherzyklus.
- Ein Paar von zwei `move16`-Befehlen füllt bzw. leert exakt eine 32-Byte-Cacheline (256 Bit) der Apollo 68080:
  ```asm
  .loop32:
          move16  (a0)+,(a1)+     ; 16 Bytes (128 Bit)
          move16  (a0)+,(a1)+     ; 16 Bytes (128 Bit) -> 32 Bytes (256 Bit Burst)
          subq.l  #1,d0
          bne.s   .loop32
  ```

---

## 4. Pipeline-Auslastung & Superskalarität (Harness-v5 Methodik: IEP1, IEP2, IEP3)

Gemessen auf **Vampire V4 Hardware** mit den hardwarenahen Performance-Zählern der 68080 (`IEP1` = Decoder Anton, `IEP2` = Decoder Bruno, `IEP3` = Decoder Cesar, `DCH`/`DCM` = Daten-Cache):

### Farbtiefe: 32-Bit ARGB (Stride 7680 Bytes)
| Größe | Routine | Zyklen (CCC) | Befehle gesamt | IPC | Anton (IEP1) | Bruno (IEP2) | Cesar (IEP3) | Cache-Verhalten |
|---|---|---|---|---|---|---|---|---|
| **8×8** | `move16` | 303 | 218 | 0,72 | 90,8 % | 9,2 % | 0,0 % | L1-Cache |
| | **AMMX** | **296** | 275 | **0,93** | 86,5 % | **13,5 %** | 0,0 % | L1-Cache |
| | `m68k` | 301 | 275 | 0,91 | 92,4 % | 7,6 % | 0,0 % | L1-Cache |
| **32×32** | `move16` | 2.103 | 1.179 | 0,56 | 94,1 % | 5,9 % | 0,0 % | L1-Cache |
| | **AMMX** | **2.014** | 2.011 | **1,00** | 83,8 % | **16,2 %** | 0,0 % | L1-Cache |
| | `m68k` | 2.318 | 2.079 | 0,90 | 96,5 % | 3,5 % | 0,0 % | L1-Cache |
| **128×128** | `move16` | 24.052 | 10.811 | 0,45 | 97,6 % | 2,4 % | 0,0 % | L2-Cache |
| | **AMMX** | **22.727** | 23.325 | **1,03** | 81,3 % | **18,7 %** | 0,0 % | L2-Cache |
| | `m68k` | 25.760 | 23.636 | 0,92 | 98,8 % | 1,2 % | 0,0 % | L2-Cache |
| **512×512** | `move16` | 504.123 | 144.288 | 0,29 | 99,1 % | 0,9 % | 0,0 % | DDR-Buslimit |
| | `AMMX` | 504.365 | 341.294 | 0,68 | 80,4 % | **19,6 %** | 0,0 % | DDR-Buslimit |
| | **m68k** | **491.640** | 341.302 | 0,69 | 99,6 % | 0,4 % | 0,0 % | DDR-Buslimit |

### Erkenntnisse zur Pipeline-Optimierung:
1. **Warum `move16` fast nur Anton (IEP1) nutzt:**
   `move16` bewegt mit einem einzelnen Befehl 16 Bytes. Es spart dadurch über 50 % der Befehlsanzahl ein (z. B. 10.811 Befehle vs. 23.325 bei AMMX). Da `move16` jedoch ein mehrzyklischer Speicherburst-Befehl ist, belegt er das Speicherinterface exklusiv und läuft zu > 90–99 % ausschließlich auf Decoder Anton (IEP1).
2. **Warum AMMX bei 8×8 bis 128×128 noch schneller ist als `move16`:**
   AMMX (`load`/`store` mit `e0–e3`) nutzt die zweite Pipeline **Bruno (IEP2) zu bis zu 19,6 %** aus! Durch die echte 2-Wege-Superskalarität erreicht AMMX einen **IPC von 1,00 bis 1,03** (mehr als 1 Befehl pro Takt) und schlägt `move16` bei 128×128 (22.727 Takte vs. 24.052 Takte).
3. **Warum bei 512×512 der DDR-Bus limitiert:**
   Bei 512×512 @ 32-Bit (1 MB Daten) bricht die IPC bei `move16` auf **0,29** ein. Der Speicherbus ist zu 100 % ausgelastet (~177 MB/s physikalisches DDR3-Limit der V4); die CPU-Pipelines verbringen die meiste Zeit wartend auf den DDR-Controller.

---

## 5. Challenge: Beat Our 2D Blit & Tiling Cycles!

Können Sie unsere Bestmarken für 256-Bit ausgerichtetes Sprite-Blitting und Tiling auf einer Vampire V4 Standalone (PCR 1088/1 / Apollo Core 11350, 92 MHz) unterbieten?

* **Single 8×8 Sprite @ 8-Bit:** `173 Takte` (AMMX) / `181 Takte` (`move16`)
* **Single 16×16 Sprite @ 16-Bit:** `542 Takte` (AMMX) / `562 Takte` (`move16`)
* **Single 32×32 Sprite @ 32-Bit:** `2.009 Takte` (AMMX) / `2.307 Takte` (`move16`)
* **Full-Screen 1920×1080 Tiling (128×128 @ 32-Bit):** `3.215.397 Takte` (**219,3 MB/s**)
* **Full-Screen 1920×1080 Unaligned Tiling (512×512 @ 32-Bit Offset 1,1):** `3.772.455 Takte` (**186,9 MB/s**)

> **Bedingungen:** Reale V4-Hardware-Messung, 1920×1080 Framebuffer, bit-exakt (`diff=0`), vollständige CCC-Zyklen.

## Kanonische v5-Sequenz und Rohlogs

**Sequenz:** [`seq/BLT-001.i`](seq/BLT-001.i) – zwei `move16` (32 Byte, 256-Bit-aligned Copy).

```asm
        move16  (a0)+,(a1)+
        move16  (a0)+,(a1)+
```

Gemessen mit Harness v5: **MIN 9 Takte** (100×, 10 Wiederholungen; Ausreißer durch Interrupts).
Die vollständigen 84/84-Suite-Zahlen oben stammen aus der separaten
`Projects/Sprite_Blit_256bit`-/`Sprite_Tiled_Blit`-Suite und sind **nicht**
diese Einzelsequenz.

| ID | run01 … run10 |
|---|---|
| `BLT-001` | [01](cores/core-11350/logs/BLT-001.run01.log) [02](cores/core-11350/logs/BLT-001.run02.log) [03](cores/core-11350/logs/BLT-001.run03.log) [04](cores/core-11350/logs/BLT-001.run04.log) [05](cores/core-11350/logs/BLT-001.run05.log) [06](cores/core-11350/logs/BLT-001.run06.log) [07](cores/core-11350/logs/BLT-001.run07.log) [08](cores/core-11350/logs/BLT-001.run08.log) [09](cores/core-11350/logs/BLT-001.run09.log) [10](cores/core-11350/logs/BLT-001.run10.log) |


## Vollständige Suite-Rohlogs

- [`SpriteBlit.log`](cores/core-11350/suites/SpriteBlit.log) – **28/28 PASS**
- [`TiledBlit.log`](cores/core-11350/suites/TiledBlit.log) – **56/56 PASS**
