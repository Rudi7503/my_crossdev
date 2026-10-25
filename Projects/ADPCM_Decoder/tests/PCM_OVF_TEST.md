# PCM-Overflow-Test (isoliert)

Ziel: `add.w`/`sub.w` + `bvc`-Clamp gegen C-Referenz prüfen, ohne den Decoder.

## Erkenntnisse

1. **Amiga-GCC `int` ist 16 Bit.** Eine Schleife
   `for (int pcm=-32768; pcm<=32767; pcm++)` läuft bei 32767 über und wird
   endlos. Test verwendet jetzt `int32_t`.
2. **C-Argumente sind 32-Bit-Longs (Big-Endian).** `move.w 8(a7)` liest das
   High-Word; Argumente mit `move.l` lesen, nicht `move.w`.
3. Der externe Test braucht vollständiges Callee-Save
   (`movem.l d2-d7/a2-a6`), sonst korrumpiert er die C-Loop.

## Dateien

- `pcm_ovf.s` – ASM-Funktion, Args `move.l 48/52/56(a7)`
- `pcm_ovf_test.c` – int32-Referenztest, alle PCM-Werte × 30 Deltas + 1M random
- `build_pcm_ovf.sh`

## Status

Test gebaut als `pcm_ovf_v5`; V4 war beim Lauf nicht stabil erreichbar
(Ping 50 %, ASH-Timeout). Nach Power-Cycle:

```bash
ram:pcm_ovf_v5
```

Erwartung: `PCM_OVF;checked=...;PASS`.
