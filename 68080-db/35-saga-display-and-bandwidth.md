# 35. SAGA-Bildausgabe, Takt & Speicherbandbreite unter Display-DMA (Vampire V4)

**Hardware:** Vampire V4 Standalone, Apollo Core **11350** (realer Takt **`92,09 MHz`**, kalibriert über 60× PAL-`WaitTOF()`).  
**Rohlog:** [`log-v6/SAGA-BW-001.log`](log-v6/SAGA-BW-001.log) (gemessen unter `Forbid()`, 10 Läufe je Kernel, 64-Byte-aligned DDR3-FastRAM).

---

## 1. Bildausgabe, VBL-Takt, Bildzeiger (`$DFF1EC`) & Strahlposition (`$DFF004/$DFF006`)

### Gemessene Hardware-Werte

| Messung | Messwert (Rohlog) | Umgerechnet (`92,09 MHz`) | Bedeutung |
|:---|:---|:---|:---|
| `VPOSR/VHPOSR` (`$DFF004/$DFF006`) | **`0 .. 312`** (313 Zeilen) | `50,00 Hz` PAL | Klassischer Amiga-Chipset-Strahlzähler |
| `WaitTOF()` (60 Intervalle, `CCC`) | Min `1.808.560` / Avg **`1.841.848`** / Max `1.876.020` | **`20,00 ms`** (`±0,37 ms` Jitter) | `WaitTOF()` / `INTB_VERTB` taktet auf PAL `50 Hz`, **nicht** auf HDMI `60 Hz`! |
| `BPLHPT` (`$DFF1EC`) schreiben | `7.523`–`7.531` Takte / 100× | **`~75` Takte (`0,81 µs`)** | Bildzeiger-Tausch ist praktisch kostenlos |
| `_LVOCacheClearE(614400, CACRF_ClearD)` | **`100` Takte** | **`1,09 µs`** | D-Cache-Flush vor Zeigertausch kostet ~1 µs |

### Kritische Hardware-Regeln für 60 FPS auf SAGA

1. **Bildzeiger-Latch (`BPLHPT = $DFF1EC`):**
   - `BPLHPT` ist ein 32-Bit-Register (Zeiger muss **64-Byte-aligned** sein: `(ptr + 63) & ~63`, untere 5–6 Bits = 0).
   - **SAGA liest `BPLHPT` erst beim Start des nächsten Bildaufbaus (HDMI-VBlank) ein.** Ein Schreibzugriff mitten im laufenden Frame zerreißt das aktuelle Bild also *nicht*, sondern aktiviert den neuen Puffer atomar zum nächsten Bildaufbau.
   - Vor dem Schreiben von `BPLHPT` sollte `_LVOCacheClearE(buf, size, CACRF_ClearD)` aufgerufen werden (`~1 µs`), damit alle noch im 32-KB-D-Cache liegenden Zeilen im DDR3 stehen.
2. **Achtung `WaitTOF()` / `INTB_VERTB` vs. 60 Hz HDMI:**
   - Auf einer PAL-AmigaOS-Installation feuern `WaitTOF()` und `INTB_VERTB` alle `20,00 ms` (`50 Hz`, `VPOSR` `0..312`), während SAGA über HDMI mit `60 Hz` (`16,67 ms`) ausgibt.
   - Wer echte **60 FPS** erreichen will, darf *nicht* pro Frame auf PAL-`WaitTOF()` warten (sonst hartes 50-FPS-Limit!), sondern nutzt **Triple-Buffering** mit direktem `BPLHPT`-Tausch (und ggf. `CCC`- oder `timer.device`-Pacing auf `16,67 ms`) oder schaltet den Chipset-Takt auf NTSC (`60 Hz`, 262 Zeilen).
3. **Double-Buffering vs. Triple-Buffering:**
   - **Double-Buffering (`2 × 614 KB = 1,23 MB`):** Nach `BPLHPT = Back` bleibt der vorherige Frontbuffer noch bis zum *nächsten* Bildaufbau sichtbar. Die CPU muss also zwingend bis zum VBlank warten, bevor sie wieder zeichnen darf. Dauert ein Frame `17,0 ms` (0,33 ms über `16,67 ms`), fällt die Framerate sofort hart auf **`30 FPS` (`33,33 ms`)** ab.
   - **Triple-Buffering (`3 × 614 KB = 1,84 MB`):** Kostet nur `+614 KB` FastRAM und **0 CPU-Overhead** (gleiche `~75` Takte für `BPLHPT`). Nach `BPLHPT = Buf[i]` ist `Buf[(i+1) mod 3]` garantiert frei und die CPU kann **sofort** das nächste Bild rendern — bei `17,0 ms` Framezeit laufen flüssige **`~58,8 FPS`** ohne Tearing.
4. **Reales Rechenzeit-Budget bei 60 Hz (`640×480×16`):**
   - Gesamtbudget pro Frame bei `60,0 Hz` (`16,667 ms`) auf Core 11350 (`92,09 MHz`): **`1.534.873` CPU-Takte**.
   - Abzüglich `BPLHPT`-Swap (`75` Takte) + `CacheClearE` (`100` Takte) + minimaler OS/ISR-Last bleiben **netto `~1,53 Mio.` Takte (`~16,6 ms`)** für Logik + Rendering.

---

## 2. Speicherbandbreite neben SAGA-Bildausgabe (`640×480×16` vs. `960×540×16`)

Gemessen mit identischen 64-Byte-DDR3-Burst-Kernels (AMMX `8× store`, `8× load + 8× store`, `2× (4× load + paddw + 4× store)`) über `614.400` Bytes (`640×480×16` = `9.600 × 64 B`) in Offscreen-FastRAM sowie direkt in den aktiven SAGA-Framebuffer:

### 2.1 Vergleich über 614.400 Bytes (`640×480×16`) bei verschiedenen SAGA-Modi

| Aktiver SAGA-Modus | SAGA-DMA (60 Hz) | **WRITE** `64B` (Avg Takte / ms / MB/s) | **COPY** `64B` (R+W = 1,23 MB) | **RMW** `64B` (R+W = 1,23 MB) | Verlangsamung vs. `320×240×8` |
|:---|:---:|:---:|:---:|:---:|:---|
| **`320×240×8`** *(Min-DMA Baseline)* | ~`4,6 MB/s` | `92.187` (`1,00 ms`, **`614 MB/s`**) | `209.232` (`2,27 ms`, **`541 MB/s`**) | `240.991` (`2,62 ms`, **`470 MB/s`**) | Baseline (`0,0 %`) |
| **`640×480×16`** *(Hurrican)* | ~`36,9 MB/s` | `96.608` (`1,05 ms`, **`586 MB/s`**) | `217.629` (`2,36 ms`, **`520 MB/s`**) | `248.847` (`2,70 ms`, **`455 MB/s`**) | **`+4,8 %`** Write / **`+4,0 %`** Copy / **`+3,3 %`** RMW |
| **`960×540×16`** *(OpenSWOS/HD)* | ~`62,2 MB/s` | `101.035` (`1,10 ms`, **`560 MB/s`**) | `226.096` (`2,46 ms`, **`501 MB/s`**) | `256.934` (`2,79 ms`, **`440 MB/s`**) | **`+9,6 %`** Write / **`+8,1 %`** Copy / **`+6,6 %`** RMW |
| **Workbench** *(HD 32-Bit)* | ~`150–220 MB/s` | `117.812` (`1,28 ms`, **`480 MB/s`**) | `245.523` (`2,67 ms`, **`461 MB/s`**) | `282.448` (`3,07 ms`, **`401 MB/s`**) | **`+27,8 %`** Write / **`+17,3 %`** Copy / **`+17,2 %`** RMW |

### 2.2 Offscreen-Backbuffer vs. direktes Schreiben in den sichtbaren SAGA-Framebuffer

| Modus | Offscreen `WRITE_64B` (Avg) | Live-Framebuffer `WRITE_64B` (Avg) | Differenz |
|:---|:---:|:---:|:---:|
| **`640×480×16`** | `96.608` Takte (`1,05 ms`) | `96.604` Takte (`1,05 ms`) | **`0,00 %`** |
| **`960×540×16`** | `101.035` Takte (`1,10 ms`) | `101.017` Takte (`1,10 ms`) | **`0,00 %`** |

### 2.3 Voller Frame-Durchsatz für `960×540×16` (`1.036.800` Bytes = `1,04 MB`)

| Kernel (`1.036.800` B, 64-B-Burst) | Min / Avg / Max (`CCC`) | Zeit (`92,09 MHz`) | Effektive DDR3-Bandbreite | Anteil an 60-Hz-Frame (`16,67 ms`) |
|:---|:---:|:---:|:---:|:---:|
| **Pure Write** (Screen Clear) | `152.013` / **`167.421`** / `182.404` | **`1,82 ms`** | `570 MB/s` | `10,9 %` |
| **Pure Copy** (1 Full-Screen Blit) | `361.806` / **`381.808`** / `393.958` | **`4,15 ms`** | `500 MB/s` (R+W) | `24,9 %` |
| **RMW** (1 Full-Screen Alpha/Add/Dark) | `416.093` / **`433.909`** / `445.159` | **`4,71 ms`** | `440 MB/s` (R+W) | `28,3 %` |

---

## 3. Fazit für 60-FPS-Design (Hurrican & OpenSWOS)

1. **SAGA-Display-DMA ist moderat, aber messbar:** `640×480×16` kostet nur **`~4–5 %`** DDR3-Bandbreite gegenüber Minimal-DMA, `960×540×16` kostet **`~7–10 %`**. Eine hochauflösende 32-Bit-Workbench im Vordergrund kostet dagegen bis zu **`~28 %`**!
2. **Kein Sonder-Penalty für Live-Framebuffer:** Offscreen-FastRAM und SAGA-Framebuffer liegen im selben Unified DDR3 — die Contention entsteht rein global durch die aktive SAGA-Auflösung/Farbtiefe.
3. **Bandbreiten-Budget bestimmt 60 FPS:** In `640×480×16` kostet 1 voller Copy-Pass `2,36 ms` und 1 voller RMW-Pass `2,70 ms`. Bei 4–5 überlappenden Layern ist die Hälfte des `16,67 ms`-Budgets allein durch Speicherbandbreite belegt — **Overdraw-Reduktion** (Caching statischer Layer, Occlusion-Culling, Dirty-Rects) ist daher neben 64-Byte-AMMX-Bursts der größte Hebel für echte 60 FPS.
4. **Triple-Buffering + `$DFF1EC`** ist auf der V4 der goldene Standard: `~75` Takte Swap, kein Tearing (Hardware-Latch bei neuem Bildaufbau), kein harter Einbruch auf 30 FPS bei leichten Frame-Überschreitungen.
