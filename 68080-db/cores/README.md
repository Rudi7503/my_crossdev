# 68080 Multi-Core Benchmark-Verzeichnis & Community Challenge

Dieses Verzeichnis enthält die Benchmark-Messungen, Performance-Zähler (IEP-Pipes) und Verifikationsdaten für jeden getesteten Apollo-Core der Vampire-Serie.

---

## 1. Verfügbare Core-Verzeichnisse

| Verzeichnis | Core-Kennung | Plattform | Status |
|---|---|---|---|
| [`core-1088-1/`](core-1088-1/) | `CORE;1088;1;0` | Vampire V4 Standalone (85 MHz) | **100% vollständig gemessen & validiert (Harness v5)** |

Jedes Core-Verzeichnis enthält:
- `logs/`: Sämtliche Rohdaten-Logs (100 Durchläufe pro Test) mit `FILESTAT`, `BASELINE`, `RES` und `PERF`-Zählern.
- `CORE-INFO.md`: Exakte Hardware- und Core-Spezifikation (Frequenz, Revision, unterstützte AMMX-Instruktionen).
- `PERF-SUMMARY.md`: Vollständige Zyklen- und Pipeline-Auslastungstabelle (IEP0, IEP1, IEP2).
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
1. **Core-Verzeichnis anlegen:**
   ```bash
   mkdir -p 68080-db/cores/core-<ID>-<REVISION>/logs
   ```
2. **Alle Sequenzen mit Harness v5 bauen:**
   ```bash
   cd 68080-db
   bash tools/build_all_v5.sh
   ```
3. **Auf der Vampire ausführen:**
   Übertrage die Binaries aus `bin-v5/` auf deine Vampire und führe die Messungen aus.
4. **Logs validieren:**
   ```bash
   python3 tools/check_log.py cores/core-<ID>-<REVISION>/logs/*.log
   ```
5. **Ergebnisse vergleichen:**  
   Vergleiche, ob neuere Cores andere Fusing-/Bonding-Eigenschaften haben, ob bisher deaktivierte Instruktionen (wie `bflyw`) funktionieren oder ob sich die Pipeline-Auslastung verbessert hat.
   Reiche einen Pull-Request ein, um deine Core-Ergebnisse mit der Community zu teilen!
