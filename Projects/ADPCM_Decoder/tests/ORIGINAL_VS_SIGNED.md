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
| 5-Bit Stereo | 1389 | 650 | **53,2 %** |
| 5-Bit MS | 1511 | 740 | **51,0 %** |
| 5-Bit Mono | 1236 | 504 | **59,2 %** |
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

## Auffälligkeit

5-Bit Signed lag in einem früheren Lauf bei 538/699/503; im korrigierten Lauf
650/740/504. Die Ursache ist noch nicht isoliert (Layout/Testbinary/Takt).
Die Original-vs-Signed-Aussage bleibt davon unberührt, die exakten 5-Bit-Werte
sollten aber bei Bedarf erneut gegengeprüft werden.
