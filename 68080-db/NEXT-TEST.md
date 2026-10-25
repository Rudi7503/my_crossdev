# Nächster Testlauf: Pipeline- und Cache-Zähler

**Status:** gemerkt für den **nächsten** Testlauf.  
**In Harness v5 implementiert** (separater Zählerdurchgang nach der Taktmessung);
noch nicht auf der V4 gemessen. v4 bleibt kanonisch.

## Register (Apollo 68080, MOVEC; PRM S. 29)

| Struct-Feld | MOVEC-Name | Adresse | Bedeutung |
|---|---|---:|---|
| `SPRCLK` | `CCC` | `$809` | CPU-Clock-Cycles |
| `MIPS1` | `IEP1` | `$80A` | Instructions Executed Pipe 1 |
| `MIPS2` | `IEP2` | `$80B` | Instructions Executed Pipe 2 |
| `MIPS3` | `IEP3` | `$00C` | Instructions Executed Pipe 3 (früher STH) |
| `BPC` | `BPC` | `$80C` | Branches Predicted Correct |
| `BPW` | `BPW` | `$80D` | Branches Predicted Wrong |
| `HITS` | `DCH` | `$80E` | Data Cache Hits |
| `MISS` | `DCM` | `$80F` | Data Cache Miss |

Achtung: `IEP3` liegt auf **`$00C`**, nicht auf `$80C`. `BPC`/`BPW`/`DCH`/`DCM`
liegen auf `$80C`–`$80F`.

## Referenzcode (vom Nutzer, für den nächsten Harness)

```c
struct SpeedSpecialRegisters {
    ULONG SPRCLK;   // $809 CPU CLOCKS
    ULONG MIPS1;    // $80A MIPS Pipe 1
    ULONG MIPS2;    // $80B MIPS Pipe 2
    ULONG MIPS3;    // $00C MIPS Pipe 3
    ULONG BPC;      // $80C Branches Predicted Correct
    ULONG BPW;      // $80D Branches Predicted Wrong
    ULONG HITS;     // $80E Data Cache Hits
    ULONG MISS;     // $80F Data Cache Miss
    ULONG pad;
};
```

```asm
_FunctionAllSpeedRegistersASM:
        movec   ccc,d2
        move.l  d2,(a2)+

        movec   iep1,d2
        move.l  d2,(a2)+

        movec   iep2,d2
        move.l  d2,(a2)+

        dc.w    $4e7a,$200c        ; IEP3 = $00C; movec iep3 ist nicht überall akzeptiert
        move.l  d2,(a2)+

        movec   bpc,d2
        move.l  d2,(a2)+

        movec   bpw,d2
        move.l  d2,(a2)+

        movec   dch,d2
        move.l  d2,(a2)+

        movec   dcm,d2
        move.l  d2,(a2)+

        rts
```

## Wichtige Punkte für die Umsetzung

1. **Messstörung:** Jeder zusätzliche `MOVEC`-Read kostet Takte. Die Zähler
   dürfen nicht innerhalb des gemessenen `MOVEC CCC,d5 … MOVEC CCC,d6`-Blocks
   gelesen werden, sonst verfälschen sie genau die Werte, die sie erklären
   sollen.
2. **Sinnvolle Varianten:**
   - Zähler **vor und nach** der 100er-Messschleife lesen und die Differenz
     loggen (ein Read pro Lauf, nicht pro Einzelmessung).
   - Oder einen separaten „Counter-Run“ ohne `CCC`-Zeitmessung fahren.
3. **Log-Format (Vorschlag):**
   ```text
   PIPE;<IEP1>;<IEP2>;<IEP3>;<BPC>;<BPW>;<DCH>;<DCM>
   ```
   jeweils als Delta über den Messblock, nicht als Absolutwert.
4. **32-Bit-/Wrap-Verhalten** der Zähler prüfen; bei 100 kurzen Läufen
   unwahrscheinlich, aber nicht dokumentiert.
5. **vasm-Test (2.0d):** `IEP1`, `IEP2`, `BPC`, `BPW`, `DCH`, `DCM` werden
   symbolisch akzeptiert; `IEP3` nicht („illegal operand types“). Für
   `IEP3` bleibt `dc.w $4e7a,$200c` (so wie oben).
6. **Reihenfolge:** erst v5-Lauf (Einzeltest, dann 10×) abschließen, Logs auswerten,
   dann die Zähler als eigene Harness-Version ergänzen.

## Quellen

- Apollo 68080 PRM, S. 29 (MOVEC-Registerliste).
- Nutzerangabe/Referenzcode vom 2026-10-23.
