# Core-Identität: <ApolloControl CORE>

Diese Datei ist für **Messungen auf einer fremden/anderen V4** verpflichtend.
Sie gehört in `cores/core-<ApolloCore>/` und wird zusammen mit den Logs
versioniert.

## Identität

- **ApolloControl CORE:** `<11350>`
- **PCR:** `CORE;<PCR-ID>;<PCR-Rev>;<VCoreRev>` aus dem Log
- **VCoreRev:** `<0>`
- **Takt:** `<92 MHz>`
- **ApolloOS/Kickstart:** `<...>`
- **Host/Board:** `<V4 Standalone, ...>`
- **Messdatum:** `<YYYY-MM-DD>`
- **Harness:** v5
- **Anzahl Läufe:** 10 je Eintrag

## Rohaufnahme

Hilfsskript: [`tools/CAPTURE-CORE`](../tools/CAPTURE-CORE).

```text
C:ApolloControl CORE
C:ApolloControl HZ
C:Version
C:Type ENV:VCoreRev
```

Rohausgabe als `CORE-IDENTITY.txt` daneben ablegen.

## Wichtig

`CORE;1088;1;0` im Log ist die **PCR-Identität**, nicht automatisch die
ApolloControl-Core-Buildnummer. Beides muss übereinstimmen und dokumentiert
werden.

## Logs

Alle Logs unter `cores/core-<ApolloCore>/logs/<ID>.runNN.log`.
Nicht in die kanonischen 770 Logs von `cores/core-11350/` mischen, wenn die
Hardware/Core-Identität abweicht.
