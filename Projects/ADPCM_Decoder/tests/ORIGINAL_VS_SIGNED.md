# Original vs. Signed – echte Vergleichsmessungen (2026-10-30)

Alle Läufe auf **V4, Core 11350, VCoreRev 0**, 100× Repeat.
Verglichen wurden die **echten Original-Backups** (`experiments/decoder_*_orig.s`)
gegen die signierten 32-Bit-Implementierungen. Vorher verglichen die Skripte
versehentlich die bereits signierte Produktion als „orig“.

| Format | Original MIN | Signed MIN | Reduktion |
|---|---:|---:|---:|
| 2-Bit Stereo | 2443 | 1337 | **45,3 %** |
| 2-Bit MS | 2869 | 1764 | **38,5 %** |
| 2-Bit Mono | 2335 | 1254 | **46,3 %** |
| 3-Bit Stereo | 1941 | 1092 | **43,7 %** |
| 3-Bit MS | 2229 | 1380 | **38,1 %** |
| 3-Bit Mono | 1805 | 1007 | **44,2 %** |
| 5-Bit Stereo | 1389 | 538 | **61,3 %** |
| 5-Bit MS | 1511 | 700 | **53,7 %** |
| 5-Bit Mono | 1236 | 503 | **59,3 %** |
| 6-Bit Stereo | 1331 | 609 | **54,2 %** |
| 6-Bit MS | 1457 | 735 | **49,6 %** |
| 6-Bit Mono | 1373 | 590 | **57,0 %** |
| 3/2-Bit Hybrid MS | 2411 | 1249 | **48,2 %** |
| 4/2-Bit Hybrid MS | 2254 | 1358 | **39,7 %** |
| 5/3-Bit Hybrid MS | 1755 | 958 | **45,4 %** |

`golden_all_orig` und `golden_all_signed`: **15/15 Modi PASS**.  
`repeat2/3/5/6` und `repeat_hybrid`: PASS.

## Testskripte

- `build_golden_all.sh` – 2/3-Bit, echte `_orig.s`
- `run_5_6.sh` – 5/6-Bit, echte `_orig.s`
- `run_hybrid.sh` – alle 15 inkl. Hybrid, echte `_orig.s`
- `gen_hybrid.py` – jetzt repo-lokal, kein `.gemini`-Pfad mehr

## 5-Bit-Kontrolle (2026-10-30)

Zehn separate Starts desselben `repeat5_signed`-Binaries lieferten stabil:

```text
STEREO 538/538 ; MS 700/700 ; MONO 503/504  (MIN/Median)
```

Die früheren 650/740/504 stammten aus einem anderen/alten Build. Die
Original-vs-Signed-Aussage bleibt: **5-Bit 53,7–61,3 % weniger Zyklen**.
