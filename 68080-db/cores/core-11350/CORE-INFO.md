# Core-Spezifikation: Apollo 68080 (Apollo Core 11350)

**Hardware-Plattform:** Vampire V4 Standalone  
**Apollo-Core (ApolloControl):** `11350`  
**PCR:** `CORE;1088;1;0` (PCR-ID 1088, Revision 1, VCoreRev 0)  
**Takt:** **92 MHz** (`ApolloControl HZ`: AC68080 @ 92 MHz, x13)  
**Betriebssystem:** ApolloOS 9.5 (Kickstart 51.51 / Workbench 40.0)  
**Mess-Harness:** Harness v5, 100 Warmup + 100 Messläufe, 10 Wiederholungen je Eintrag.

> `CORE;1088;1;0` in den Logs ist die **PCR-Identität**, nicht die Apollo-Core-Buildnummer.
> Die V4 wurde nicht auf einen anderen Core geflasht; ApolloControl meldet 11350.

## Messungen

- **74 Einträge**, **740 v5-Dateilogs** (`<ID>.run01..10.log`)
- Alle Logs mit `FILESTAT;1;0`, `BASELINE`, 100×`RES`, `SUMMARY`, `PERF`
- Alle 740 mit `tools/check_log.py` validiert
- Logs: `cores/core-11350/logs/`, Gesamtdaten: `measurements/v5-files/`
