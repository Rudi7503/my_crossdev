# 68080 Multi-Core Benchmark-Verzeichnis & Community Challenge

Dieses Verzeichnis enthält die Benchmark-Messungen, Performance-Zähler (IEP-Pipes) und Verifikationsdaten für jeden getesteten Apollo-Core der Vampire-Serie.

---

## 1. Verfügbare Core-Verzeichnisse

| Verzeichnis | Core-Kennung | Plattform | Status |
|---|---|---|---|
| [`core-11350/`](core-11350/) | Apollo Core 11350; PCR `CORE;1088;1;0` | Vampire V4 Standalone (92 MHz) | **100% vollständig gemessen & validiert (Harness v5)** |

Jedes Core-Verzeichnis enthält:
- `logs/`: Sämtliche Rohdaten-Logs (10 Läufe je Eintrag, je 100 Messungen) mit `FILESTAT`, `BASELINE`, `RES` und `PERF`-Zählern.
- `CORE-INFO.md`: Exakte Hardware- und Core-Spezifikation (Frequenz, Revision, unterstützte AMMX-Instruktionen).
- `PERF-SUMMARY.md`: Vollständige Zyklen- und Pipeline-Auslastungstabelle (IEP1, IEP2, IEP3).
- `CYCLE-SUMMARY.md`: Statistische Auswertung (MIN, Median, MAX, AVG, Stdabw., p95).

---

## 2. Die Community & KI-Challenge: *„Beat Our Cycle Counts!“*

Wir fordern jeden Entwickler, Apollo-Experten, Hardware-Besitzer und jede Programmier-KI heraus:
> **Kannst du unsere Implementierungen auf der Apollo 68080 schlagen?**  
> Zeige uns eine schnellere Sequenz oder teste unsere Optimierungen auf deinem eigenen Core!

### Die Regeln der Challenge:
1. **Bit-Exakte Parität (diff = 0):**  
   Optimierungen dürfen niemals die mathematische Genauigkeit oder das Ergebnis korrumpieren. Jeder Testfall wird gegen eine verifizierte C-Referenz auf Bit-Ebene geprüft.
2. **Echte Hardware-Messung auf der Vampire:**  
   Keine Emulatoren (wie UAE/WinUAE), sondern reale Hardware-Messung mit dem kanonischen `harness-v5.asm` unter `Forbid()` mit warmem Cache (100 Warmup-Läufe, 100 Messläufe).
3. **Reproduzierbarkeit:**  
   Die Sequenz muss als `seq/<ID>.i` vorliegen, fehlerfrei mit `vasmm68k_mot` assemblieren und im Log jitterfrei sein.
4. **CCC- und PERF-Zähler-Nachweis:**  
   Das generierte Log muss vollständige `BASELINE`, `RES;1..100` und `PERF;100;CCC;...` Einträge aufweisen und durch `tools/check_log.py` validiert werden.

### Hall of Fame:
Wenn du eine Sequenz um mindestens 1 Takt verbesserst und das verifizierte Log einreichst, wird dein Code als neuer kanonischer Rekord in die Datenbank aufgenommen!

---

## 3. Aufruf: Teste deinen eigenen Core!

Hast du eine andere Vampire (z. B. Vampire V2 für A500/A600/A1200, Vampire V4+, oder einen anderen Core-Release wie Core 11900 oder Core x89)?

**Wir fordern dich auf, die Testsuite auf deinem Core laufen zu lassen und die Ergebnisse zu vergleichen!**

### Schritt-für-Schritt-Anleitung:
1. **Core-Verzeichnis anlegen** (ApolloCore aus `ApolloControl CORE`, nicht nur PCR):
   ```bash
   mkdir -p 68080-db/cores/core-<ApolloCore>/logs
   ```
2. **Core-Identität erfassen** – Pflicht:
   ```text
   C:ApolloControl CORE
   C:ApolloControl HZ
   C:Version
   C:Type ENV:VCoreRev
   ```
   `tools/CAPTURE-CORE` auf die V4 kopieren, ausführen und
   `RAM:CORE-IDENTITY.txt` herunterladen. Danach
   `templates/CORE-INFO.md` nach `cores/core-<ApolloCore>/CORE-INFO.md`
   kopieren und ausfüllen.
3. **Alle Sequenzen mit Harness v5 bauen:**
   ```bash
   cd 68080-db
   HARNESS=harness-v5.asm BUILD_DIR=build-v5 BIN_DIR=bin-v5 ./tools/build_all.sh
   ```
4. **10× auf der eigenen V4 ausführen:**
   Übertrage `bin-v5/*` und `RUN-ALL` auf die Vampire und führe
   `Execute "Ram Disk:RUN-ALL"` aus. Ergebnis:
   `LOGS/<ID>.run01.log` … `.run10.log`.
5. **Logs installieren und validieren:**
   ```bash
   python3 tools/check_log.py cores/core-<ApolloCore>/logs/*.log
   ```
   Alle 10 Logs je Eintrag müssen `BEGIN`, `FILESTAT`, `CORE`, `BASELINE`,
   100×`RES`, `SUMMARY`, `PERF` und `END` enthalten.
6. **Ergebnisse vergleichen:**  
   Fremde Hardware/Core-Logs bleiben in `cores/core-<ApolloCore>/` und werden
   **nicht** in die kanonischen 770 Logs von `cores/core-11350/` gemischt.
   Reiche einen Pull-Request mit `CORE-INFO.md`, `CORE-IDENTITY.txt` und den
   Rohlogs ein.
