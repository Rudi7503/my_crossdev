# Reproduzierbarkeit: vasm 2.0d vs. 2.0f

**Datum:** 2026-11-01  
**Hardware:** Vampire V4, Apollo Core **11350**, PCR 1088/1, VCoreRev 0, 92 MHz  
**Harness:** Harness v5, 77 Einträge × 10 Läufe  
**Serielle Verbindung:** nicht verwendet (Dateilogs)

## Build

Beide Toolchains erzeugen **byte-identische Binaries**:

```text
vasm 2.0d: 77/77 Binaries
vasm 2.0f: 77/77 Binaries
Vergleich: 77/77 identisch, 0 Unterschiede
```

Damit ist ausgeschlossen, dass die 2.0f-Regeländerung (B-Register) den
kanonischen Code verändert.

## Messlauf mit 2.0f

```text
Upload:       77 Binaries + RUN-ALL, 0 Fehler
RUN-ALL:      125 s
Logs:         770 (77 × 10)
check_log:    770 OK, 0 INVALID
```

## Vergleich mit dem kanonischen 2.0d-Lauf

```text
SUMMARY exakt identisch:       419 / 770
SUMMARY unterschiedlich:       351 / 770
davon MIN identisch:           351 / 351
davon MIN unterschiedlich:       0 / 351
IDs mit gleichem 10-Lauf-MIN:   77 / 77
IDs mit abweichendem MIN:        0 / 77
```

Die 351 Abweichungen betreffen **ausschließlich MAX/AVG** (Interrupts,
Scheduling-Jitter, einzelne Ausreißer). Der Referenzwert **MIN** ist in
allen 770 Läufen und in allen 77 10-Lauf-Aggregaten identisch.

## Konsequenz

- **Keine inhaltliche Änderung** der Datenbank.
- Der kanonische 2.0d-Logbestand bleibt gültig.
- Die 2.0f-Logs liegen nur als Reproduktionsnachweis unter
  `/tmp/rerun-2.0f/` und werden nicht in `cores/core-11350/` gemischt.
- Ein separater 2.0f-Korpus ist nicht nötig, solange die Binaries identisch
  sind und MIN identisch bleibt.
