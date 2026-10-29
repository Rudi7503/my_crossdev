# Vampire V4 Hardware-Messergebnisse: Video-Optimierungen

**Plattform:** Vampire V4 Standalone (Apollo 68080 Core 1088 / Release 1 @ 85 MHz)  
**Binary:** `RAM:VideoOpt` (200 Durchläufe pro Test unter Hardware Clock Cycle Counter CCC `$809`)

```
=================================================================================================================================
 Optimization 1: H.264 4x4 Core IDCT (16 Coefficients, 200 runs)
---------------------------------------------------------------------------------------------------------------------------------
 4x4 Core IDCT (C-Ref):                   225 cycles (14.06 cyc/sample)
 4x4 Core IDCT (Baseline ASM):            223 cycles (13.94 cyc/sample, 1.01x vs C)
 4x4 Core IDCT (Unrolled In-Register):    195 cycles (12.19 cyc/sample, 1.14x vs Baseline, 1.15x vs C) [diff=0, PASS]

=================================================================================================================================
 Optimization 2: Sub-Pixel Motion Compensation (8x8 Block = 64 Pixels, 200 runs)
---------------------------------------------------------------------------------------------------------------------------------
 Horizontal Half-Pel (C-Ref):            1021 cycles (15.95 cyc/pixel)
 Horizontal Half-Pel (AMMX PAVGB):          69 cycles ( 1.08 cyc/pixel, 14.80x vs C) [diff=0, PASS]
 Vertical Half-Pel (C-Ref):               998 cycles (15.59 cyc/pixel)
 Vertical Half-Pel (AMMX PAVGB):            69 cycles ( 1.08 cyc/pixel, 14.46x vs C) [diff=0, PASS]
 Diagonal Half-Pel (C-Ref):              1127 cycles (17.61 cyc/pixel)
 Diagonal Half-Pel (AMMX PAVGB):           100 cycles ( 1.56 cyc/pixel, 11.27x vs C) [diff=0, PASS]

=================================================================================================================================
 Optimization 3: Video Color Pipeline (8x8 Block = 64 Pixels, 200 runs)
---------------------------------------------------------------------------------------------------------------------------------
 Software YUV->RGB32 (C-Ref):            3619 cycles (56.55 cyc/pixel)
 Software YUV->RGB32 (AMMX / ASM):       2149 cycles (33.58 cyc/pixel,  1.68x vs C)
 SAGA NATIVE YUV MODE (Hardware Overlay): 594 cycles ( 9.28 cyc/pixel,  6.09x vs C-RGB,  3.62x vs AMMX-RGB!)
   >> SAGA NATIVE YUV ELIMINATES 100% DER FARBRAUM-KONVERTIERUNG AUF DER CPU!
=================================================================================================================================
```
