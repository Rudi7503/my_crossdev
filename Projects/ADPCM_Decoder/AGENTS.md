# ADPCM Decoder – Agent Guide

Produktivstand: **Opt9** in `decoder_4bit.s`.

## Pflicht vor jeder Änderung

```bash
python3 tests/auto_opt.py
```

Das baut, lädt per ACP hoch, führt per ASH auf der V4 aus, lädt die
Ergebnisse und prüft Golden + 100×-Repeat.

## Regeln

- `decoder_4bit.s` nur nach **Golden PASS + Repeat PASS** ändern.
- 100×-MIN/Median ist die Referenz; AVG/Max sind durch Interrupts verrauscht.
- Kandidaten unter `experiments/` ablegen, nie direkt experimentieren.
- Ergebnisse unter `tests/auto-results/` sind generiert (gitignored).
- Doku: `tests/OPTIMIZATION.md`, `tests/AUTOMATION.md`.

## Umgebung

- V4: 192.168.178.50
- ACP/ASH: `C:\Program Files\ApolloExplorer`
- Toolchain: `Compilers/GCC-6.50-Latest`
- Server startet automatisch aus `S:Startup-Sequence` (1.4.0).
