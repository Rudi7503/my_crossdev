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
