# ADPCM Real-Audio-Vergleich (V4, 2026-10-25)

Getestet mit sichtbarem CLI pro Lauf, Screenshot nach Liedende.
Core **11350**, VCoreRev 0. Alle Läufe endeten mit `Wiedergabe beendet`.

## Versionen

- **C**: `ADPCM_Decoder -no_asm` (C-Referenz)
- **Alt**: GitHub-Commit `4b59fec50` (Opt9) → `ADPCM_Decoder_old_gh`
- **Neu**: Fresh15 (`decoder_4bit.s`, HEAD)

## Dateien

| Modus | Datei | Dauer |
|---|---|---:|
| Mono | `Kakao_bei_Lotte_Run17_4-Bit,Mono,LE.adpx` | 30,77 s |
| Stereo | `Kakao_bei_Lotte_Run8_4-Bit,L_R.adpx` | 10,00 s |
| M/S | `Kakao_bei_Lotte_Run9_4-Bit,M_S.adpx` | 10,00 s |

## Messwerte (cycles/sample, CLI)

| Modus | C | Alt (Opt9) | **Neu (Fresh15)** |
|---|---:|---:|---:|
| Mono | ~94 | ~50 | **~39–40** |
| Stereo | ~188–190 | ~137–142 | **~83–86** |
| M/S | ~200–203 | ~147–155 | **~89–93** |

**Neu vs. Alt:** Mono ~21 % weniger, Stereo ~39 %, M/S ~40 %.
**Neu vs. C:** Mono ~58 %, Stereo ~55 %, M/S ~55 % weniger Zyklen.

## Bewertung

- Alle neun Läufe vollständig, keine Crashes, kein Hänger.
- Klang sauber; C/Alt/Neu liefern das erwartete identische Material.
- Der neue Decoder hat deutlich mehr Echtzeit-Reserve als Opt9.
- Screenshots: `/home/papa/V4-audio-final/`, Montage: `tests/audio-compare.png`.

---

# ADPCM Real-Audio-Hörtest aller 17 Modi (V4, 2026-10-26)

Alle 17 vorhandenen Modi (2–6 Bit Stereo L/R, M/S, Mono sowie Hybrid M/S 3/2, 4/2, 5/3) wurden als **10,00-Sekunden-Echtaudio** (`480000` Samples @ 48 kHz) einzeln auf der Vampire V4 (**Core 11350**) mit dem neuen `ADPCM_Decoder` abgespielt und vom Benutzer im Hörtest bestätigt (**Sound OK** bei allen 17 Läufen).

## Messwerte im Echtzeit-Streaming (`ADPCM_Decoder`, 2 KB Disk-Streaming-Buffer)

| Nr. | Modus | Datei (10,00 s @ 48 kHz) | Bitrate | Reine ASM-Decoderzeit (`cycles/sample`) | Gesamtzeit inkl. Streaming (`cycles/sample`) | Hörtest |
|---:|---|---|---:|---:|---:|:---:|
| 1 | **2-Bit Stereo L/R** | `Kakao_bei_Lotte_Run2_2-Bit,L_R.adpx` | 192 kbps | **41–43** | **58–60** | **OK** |
| 2 | **2-Bit M/S** | `Kakao_bei_Lotte_Run3_2-Bit,M_S.adpx` | 192 kbps | **50–52** | **67–69** | **OK** |
| 3 | **2-Bit Mono** | `Kakao_bei_Lotte_Run15_2-Bit,Mono,LE.adpx` (10s) | 96 kbps | **20–21** | **28–29** | **OK** |
| 4 | **3-Bit Stereo L/R** | `Kakao_bei_Lotte_Run5_3-Bit,L_R.adpx` | 288 kbps | **48–50** | **73–76** | **OK** |
| 5 | **3-Bit M/S** | `Kakao_bei_Lotte_Run6_3-Bit,M_S.adpx` | 288 kbps | **58–60** | **82–85** | **OK** |
| 6 | **3-Bit Mono** | `Kakao_bei_Lotte_Run16_3-Bit,Mono,LE.adpx` (10s) | 144 kbps | **24** | **36–38** | **OK** |
| 7 | **4-Bit Stereo L/R** | `Kakao_bei_Lotte_Run8_4-Bit,L_R.adpx` | 384 kbps | **53–54** | **85–89** | **OK** |
| 8 | **4-Bit M/S** | `Kakao_bei_Lotte_Run9_4-Bit,M_S.adpx` | 384 kbps | **59–61** | **92–95** | **OK** |
| 9 | **4-Bit Mono** | `Kakao_bei_Lotte_Run17_4-Bit,Mono,LE.adpx` (10s) | 192 kbps | **24–26** | **40–43** | **OK** |
| 10 | **5-Bit Stereo L/R** | `Kakao_bei_Lotte_Run11_5-Bit,L_R.adpx` | 480 kbps | **45–47** | **88–90** | **OK** |
| 11 | **5-Bit M/S** | `Kakao_bei_Lotte_Run12_5-Bit,M_S.adpx` | 480 kbps | **57–58** | **99–101** | **OK** |
| 12 | **5-Bit Mono** | `Kakao_bei_Lotte_Run18_5-Bit,Mono,LE.adpx` (10s) | 240 kbps | **21–22** | **41–43** | **OK** |
| 13 | **6-Bit Stereo L/R** | `Kakao_bei_Lotte_Run19_6-Bit,L_R.adpx` | 576 kbps | **54–55** | **103–105** | **OK** |
| 14 | **6-Bit Mono** | `Kakao_bei_Lotte_Run14_6-Bit,Mono,LE.adpx` (10s) | 288 kbps | **26–28** | **51–52** | **OK** |
| 15 | **3/2-Bit Hybrid M/S** | `Kakao_bei_Lotte_Run21_3_2-Bit,M_S.adpx` | 240 kbps | **54–56** | **74–77** | **OK** |
| 16 | **4/2-Bit Hybrid M/S** | `Kakao_bei_Lotte_Run20_4_2-Bit,M_S.adpx` | 288 kbps | **56–58** | **80–84** | **OK** |
| 17 | **5/3-Bit Hybrid M/S** | `Kakao_bei_Lotte_Run13_5_3-Bit,M_S.adpx` | 384 kbps | **64–66** | **102–104** | **OK** |

