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

Der große Test (`pcm_ovf_v5`, ~4,8 Mio. Calls) lief nach 240 s nicht durch;
V4 verlor dabei Netz/ASH. Zusätzlich gebaut: **`pcm_one32`** (nur drei
Einzelaufrufe) als kleinster ABI/Logik-Test.

V4 ist aktuell nur sporadisch erreichbar (Ping 50 %, ACP/ASH-Fehler).
Nach Power-Cycle:

```bash
ram:pcm_ovf_v5
```

Erwartung: `PCM_OVF;checked=...;PASS`.


## Minimaltest

`pcm_one32` prüft:

- `pcm_ovf(-1, 1, 0)` → 0
- `pcm_ovf(32767, 1, 0)` → 32767
- `pcm_ovf(-32768, 1, 1)` → -32768

Das ist der erste Lauf nach dem Neustart; erst danach den großen Test.


## Ergebnis Minimaltest (V4, Core 11350)

```text
ONE32 0 32767 -32768
PASS
```

Das bestätigt die drei kritischen Fälle:

- `-1 + 1 = 0`
- `32767 + 1` → `32767` (Clamp)
- `-32768 - 1` → `-32768` (Clamp)

Der große C-Test ist noch **nicht belastbar**: Die C-Referenz liefert trotz
`int32_t`/`volatile` Werte außerhalb int16. Das ist ein Test-/Compilerproblem,
kein Freibrief für Fresh8. Fresh7 bleibt Produktivstand.
