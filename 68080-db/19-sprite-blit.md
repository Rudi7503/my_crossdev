# 19 – 2D Sprite Blitting & 256-Bit Aligned Rectangle Copy Benchmark

**Dateien:**
- Vollständige Suite: [`Projects/Sprite_Blit_256bit/`](../Projects/Sprite_Blit_256bit/) (`main.c`, `blit.h`, `blit_c.c`, `blit_asm.s`, `Makefile`, `SpriteBlit`)

**Ziel:** Benchmark und Verifikation von 2D-Sprite- und Rechteck-Kopierroutinen (Opaque Blits) auf der Vampire V4 (Apollo 68080).
Getestet werden alle 7 Standard-Sprite-Größen (8×8 bis 512×512) über alle 4 Farbtiefen (8-Bit Chunky, 16-Bit RGB565, 24-Bit RGB24, 32-Bit ARGB) auf einem Full-HD-Hintergrund (1920×1080 Schwarz, Ziel-Pixel Grau 127).

> **Standard-Instruktion:** `move16` im 256-Bit Burst (2× `move16` pro 32-Byte Cacheline).
> **Ziel-Alignment:** 256-Bit (32-Byte) Aligned Puffer & Zeilenanfänge.

> **Hardware-Messstatus (Vampire V4 Standalone, Core 1088/1, 85 MHz, ApolloOS 9.5):**
> - **28 von 28 Tests (100%) bit-exakt verifiziert (`diff=0, PASS`)!**
> - **Spitzen-Bandbreite:** **215,5 MB/s** mit `move16` (128×128 @ 32-Bit).
> - **Speedup bei kleinen Sprites:** Bis zu **3,88x schneller als C `memcpy`** (181 Takte vs. 702 Takte für 8×8).

---

## 1. Übersicht der Messwerte auf Vampire V4 (85 MHz Hardware)

### Farbtiefe: 8-Bit Chunky (1 Byte/Pixel, Framebuffer 1920×1080 = 2.073.600 Bytes)
| Sprite-Größe | Zeile (Bytes) | C-Ref (cyc) | m68k ASM (cyc) | move16 (cyc) | AMMX SIMD (cyc) | Speedup vs C | Durchsatz (move16) | Bit-Exakt |
|---|---|---|---|---|---|---|---|---|
| **8 × 8** | 8 Byte | 702 | 181 | **181** | 173 | **3,88x** | 30,1 MB/s | **PASS** |
| **16 × 16** | 16 Byte | 1.013 | 350 | **350** | 335 | **2,89x** | 62,2 MB/s | **PASS** |
| **32 × 32** | 32 Byte | 2.177 | 1.071 | **1.069** | 1.042 | **2,04x** | 81,4 MB/s | **PASS** |
| **64 × 64** | 64 Byte | 4.996 | 2.771 | **2.774** | 2.640 | **1,80x** | 125,5 MB/s | **PASS** |
| **128 × 128** | 128 Byte | 13.871 | 8.268 | **8.269** | 8.209 | **1,68x** | 168,4 MB/s | **PASS** |
| **256 × 256** | 256 Byte | 38.481 | 29.164 | **28.141** | 27.254 | **1,37x** | 198,0 MB/s | **PASS** |
| **512 × 512** | 512 Byte | 162.425 | 135.979 | **138.722** | 140.281 | **1,17x** | 160,6 MB/s | **PASS** |

---

### Farbtiefe: 16-Bit RGB565 (2 Bytes/Pixel, Framebuffer 1920×1080 = 4.147.200 Bytes)
| Sprite-Größe | Zeile (Bytes) | C-Ref (cyc) | m68k ASM (cyc) | move16 (cyc) | AMMX SIMD (cyc) | Speedup vs C | Durchsatz (move16) | Bit-Exakt |
|---|---|---|---|---|---|---|---|---|
| **8 × 8** | 16 Byte | 533 | 199 | **200** | 191 | **2,66x** | 54,4 MB/s | **PASS** |
| **16 × 16** | 32 Byte | 1.113 | 558 | **562** | 542 | **1,98x** | 77,4 MB/s | **PASS** |
| **32 × 32** | 64 Byte | 2.521 | 1.411 | **1.411** | 1.355 | **1,79x** | 123,4 MB/s | **PASS** |
| **64 × 64** | 128 Byte | 6.889 | 4.186 | **4.596** | 4.560 | **1,50x** | 151,5 MB/s | **PASS** |
| **128 × 128** | 256 Byte | 19.361 | 15.071 | **14.432** | 13.443 | **1,34x** | 193,0 MB/s | **PASS** |
| **256 × 256** | 512 Byte | 81.566 | 66.730 | **67.033** | 71.215 | **1,22x** | 166,2 MB/s | **PASS** |
| **512 × 512** | 1024 Byte | 289.191 | 257.877 | **265.738** | 261.166 | **1,09x** | 167,7 MB/s | **PASS** |

---

### Farbtiefe: 24-Bit RGB24 (3 Bytes/Pixel, Framebuffer 1920×1080 = 6.220.800 Bytes)
| Sprite-Größe | Zeile (Bytes) | C-Ref (cyc) | m68k ASM (cyc) | move16 (cyc) | AMMX SIMD (cyc) | Speedup vs C | Durchsatz (move16) | Bit-Exakt |
|---|---|---|---|---|---|---|---|---|
| **8 × 8** | 24 Byte | 794 | 214 | **216** | 203 | **3,68x** | 75,6 MB/s | **PASS** |
| **16 × 16** | 48 Byte | 1.396 | 625 | **625** | 598 | **2,23x** | 104,4 MB/s | **PASS** |
| **32 × 32** | 96 Byte | 2.883 | 1.752 | **1.734** | 1.627 | **1,66x** | 150,6 MB/s | **PASS** |
| **64 × 64** | 192 Byte | 8.048 | 5.499 | **5.904** | 5.619 | **1,36x** | 176,9 MB/s | **PASS** |
| **128 × 128** | 384 Byte | 25.016 | 19.319 | **19.576** | 18.347 | **1,28x** | 213,4 MB/s | **PASS** |
| **256 × 256** | 768 Byte | 111.908 | 99.976 | **99.093** | 99.688 | **1,13x** | 168,6 MB/s | **PASS** |
| **512 × 512** | 1536 Byte | 414.063 | 386.552 | **380.779** | 384.374 | **1,09x** | 175,6 MB/s | **PASS** |

---

### Farbtiefe: 32-Bit ARGB (4 Bytes/Pixel, Framebuffer 1920×1080 = 8.294.400 Bytes)
| Sprite-Größe | Zeile (Bytes) | C-Ref (cyc) | m68k ASM (cyc) | move16 (cyc) | AMMX SIMD (cyc) | Speedup vs C | Durchsatz (move16) | Bit-Exakt |
|---|---|---|---|---|---|---|---|---|
| **8 × 8** | 32 Byte | 583 | 301 | **303** | 295 | **1,92x** | 71,8 MB/s | **PASS** |
| **16 × 16** | 64 Byte | 1.281 | 728 | **732** | 700 | **1,75x** | 118,9 MB/s | **PASS** |
| **32 × 32** | 128 Byte | 3.563 | 2.060 | **2.307** | 2.009 | **1,54x** | 150,9 MB/s | **PASS** |
| **64 × 64** | 256 Byte | 9.447 | 7.017 | **7.148** | 7.331 | **1,32x** | 194,8 MB/s | **PASS** |
| **128 × 128** | 512 Byte | 31.658 | 25.502 | **25.848** | 22.845 | **1,22x** | 215,5 MB/s | **PASS** |
| **256 × 256** | 1024 Byte | 145.987 | 130.361 | **129.871** | 129.262 | **1,12x** | 171,6 MB/s | **PASS** |
| **512 × 512** | 2048 Byte | 540.099 | 502.845 | **502.462** | 504.987 | **1,07x** | 177,4 MB/s | **PASS** |

---

## 2. Technische Erkenntnisse & 256-Bit Alignment

1. **256-Bit Burst mit `move16`:**
   - Auf der 68080 überträgt `move16 (a0)+,(a1)+` genau 16 Bytes (128 Bit) in einem optimierten Speicherzyklus.
   - Ein Paar von zwei `move16`-Befehlen füllt bzw. leert exakt eine 32-Byte-Cacheline (256 Bit) der Apollo 68080:
     ```asm
     .loop32:
             move16  (a0)+,(a1)+     ; 16 Bytes (128 Bit)
             move16  (a0)+,(a1)+     ; 16 Bytes (128 Bit) -> 32 Bytes (256 Bit Burst)
             subq.l  #1,d0
             bne.s   .loop32
     ```
2. **Behandlung ungerader Breiten (24-Bit Farbtiefe & Reste < 32 Bytes):**
   - Bei 24-Bit Farben (3 Bytes/Pixel) betragen die Zeilenbreiten z. B. 24, 48, 96, 192 Bytes.
   - Für die vollen 32-Byte-Blöcke wird der 256-Bit Burst genutzt. Der Rest wird per Bitmaske `andi.l #31, d3` ohne Schleifen-Overhead über saubere Einzel-Instruktionen übertragen:
     - 16-Byte Rest (`btst #4`): `move16 (a0)+,(a1)+`
     - 8-Byte Rest (`btst #3`): 2x `move.l (a0)+,(a1)+`
     - 4-Byte Rest (`btst #2`): 1x `move.l (a0)+,(a1)+`
     - 2-Byte Rest (`btst #1`): 1x `move.w (a0)+,(a1)+`
     - 1-Byte Rest (`btst #0`): 1x `move.b (a0)+,(a1)+`
3. **Durchsatz-Charakteristik:**
   - **Kleine Sprites (8×8 .. 32×32):** Hier dominiert der Funktionsaufruf- und Schleifen-Overhead in Standard-C `memcpy`. Die Assembler-Routinen sind um den Faktor **2,0x bis 3,88x schneller**!
   - **Mittlere Sprites (64×64 .. 128×128):** Erreichen den absoluten Spitzen-Durchsatz von **> 215 MB/s**, da sie vollständig in den L1/L2-Caches der 68080 operieren.
   - **Große Sprites (256×256 .. 512×512):** Bei 512×512 @ 32-Bit (1 MB Daten pro Blit) wird der L2-Cache überschritten; die Geschwindigkeit wird durch die maximale Bandbreite des DDR3-Hauptspeichers der Vampire V4 (ca. 175 MB/s) begrenzt.

---

## 3. Challenge: Beat Our 2D Blit Cycles!

Können Sie unsere Bestmarken für 256-Bit ausgerichtetes Sprite-Blitting auf einer Vampire V4 Standalone (Core 1088/1, 85 MHz) unterbieten?

* **8×8 Sprite @ 8-Bit:** `173 Takte` (AMMX) / `181 Takte` (`move16`)
* **16×16 Sprite @ 16-Bit:** `542 Takte` (AMMX) / `562 Takte` (`move16`)
* **32×32 Sprite @ 32-Bit:** `2.009 Takte` (AMMX) / `2.307 Takte` (`move16`)
* **Spitzen-Bandbreite (128×128 @ 32-Bit):** `215,5 MB/s`

> **Bedingungen:** Reale V4-Hardware-Messung, 1920×1080 Framebuffer, bit-exakt (`diff=0`), vollständige CCC-Zyklen.
