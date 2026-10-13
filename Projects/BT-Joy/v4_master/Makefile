# v4_master -- Makefile fuer den V4-I2C-Master des Bluetooth-Joystick-Adapters
#
# Gegenstueck zum Audio-Projekt (Projects/I2C-test/v4_master). Die Ziele sind
# dieselben, der Inhalt ist der Befehlssatz des Joystick-Adapters.
#
# Ziele:
#   make            Lint + native Testsuite
#   make test       dasselbe
#   make test-san   Testsuite unter AddressSanitizer + UBSan
#   make test-m68k  Testsuite unter qemu-m68k (Big Endian) -- SKIP, wenn die
#                   Cross-Werkzeuge fehlen
#   make demo       Konsolenprogramm gegen den MOCK bauen UND nichtinteraktiv
#                   durchlaufen (keine Hardware noetig)
#   make demo-i2c   dasselbe Programm fuer einen echten i2c-dev-Bus
#   make amiga      Konsole fuer m68k-amigaos (Vampire V4)
#   make amiga-gui  MUI-Oberflaeche fuer m68k-amigaos (braucht -lmui)
#   make asm        Nachweis im erzeugten m68k-Code: Frame-Bytes nur byteweise
#   make verify     alles, was ohne Hardware pruefbar ist
#   make upload     Konsole + GUI per ApolloExplorer/acp auf die V4
#   make log        Logdatei von der V4 holen
#   make clean
#
# Fuer die Upload-Ziele muss AMIGAHOST gesetzt sein (IP der V4) und auf der V4
# der ApolloExplorer-Server laufen:
#     export AMIGAHOST=192.168.178.50
#     make upload
#
# Relativer Bezug: dieses Makefile liegt DREI Ebenen tief
# (ApolloCrossDev/Projects/BT-Joy/v4_master), also ist ../../ = Projects/ und
# ../../../ = ApolloCrossDev/. Die Vorlagen unter Projects/ liegen eine Ebene
# hoeher und schreiben dort ../../Projects/... -- das waere hier falsch.
# ACHTUNG: die I2C-Header liegen im Nachbarprojekt I2C-test, nicht in diesem
# Verzeichnis. Sie werden NICHT kopiert: eine zweite Kopie der Header waere
# genau die Art von Doppelpflege, die irgendwann auseinanderlaeuft.
#
# I2C DIREKT IM HAUPTFADEN: Konsole und MUI rufen v4_master (und damit
# v4_plat_*) selbst auf -- kein CreateNewProc, kein zweiter Faden, kein
# Nachrichtenport, kein Forbid(). Beide Frontends legen die zuletzt gueltigen
# Antworten in v4_state.c ab; die gesamte Ausgabe (stdout + Debug-UART der V4 +
# Logdatei) laeuft ueber v4_debug.c.

CC      ?= cc
OPT     ?= -O2
CSTD    ?= -std=gnu17
WARN     = -Wall -Wextra -Werror
CFLAGS   = $(CSTD) $(WARN) $(OPT) -I.

BUILD    = build

# Header mit angeben: ohne sie wuerde eine Aenderung an v4_proto.h keinen
# Neubau ausloesen und `make test` still das alte Binary laufen lassen.
HDRS      = v4_proto.h v4_master.h v4_state.h v4_joyport.h v4_debug.h v4_mock.h \
            tests/v4_test.h

LIB_SRC   = v4_proto.c v4_master.c
# Zustandshilfe der Frontends (v4_state.c), die gemeinsame Ausgabe
# (v4_debug.c) und die direkte Abfrage der Joystick-Anschluesse (v4_joyport.c).
# Alle drei enthalten weder Betriebssystem- noch Busaufrufe bzw. ausschliesslich
# Ausgabe; v4_joyport.c greift nur ueber EINEN Haken auf vier Register zu
# (ohne Haken: Nullen). Deshalb laufen sie unveraendert auf dem Wirt (Demo,
# Tests) und auf der V4 mit.
BASE_SRC  = v4_state.c v4_joyport.c v4_debug.c
TEST_SRC  = $(LIB_SRC) $(BASE_SRC) v4_mock.c tests/test_main.c \
            tests/test_proto.c tests/test_master.c tests/test_state.c \
            tests/test_joyport.c tests/test_debug.c
# Der Demo-Lauf benutzt den Mock (keine Hardware) und den echten Konsolencode:
# -DV4_CONSOLE_NO_MAIN, weil tests/demo_main.c den Mock vorher bestueckt.
DEMO_SRC  = $(LIB_SRC) $(BASE_SRC) v4_mock.c v4_console.c tests/demo_main.c
DEMO_I2C_SRC = $(LIB_SRC) $(BASE_SRC) v4_linux_i2c.c v4_console.c

# Apollo-Toolchain.
#
# CPU-Ziel: -m68020, absichtlich NICHT -m68080.
# GCC laedt mit -m68080 16-Bit-Konstanten in Long-Register als "moviw.l #imm,dn".
# Auf der V4 gemessen ist das keine Konstante: die Bytes werden als PC-relativer
# Wort-Load ausgefuehrt, das obere Wort bleibt stehen, das untere wird Muell
# (Befund aus dem Audio-Projekt). Mit -m68020 bildet GCC Long-Konstanten als
# volles move.l -- das Gate `check-no-moviw` des Audio-Projekts wacht dort
# darueber. Die V4 ist 68020-kompatibel.
APOLLO_PREFIX ?= ../../../Compilers/GCC-6.50-Latest
APOLLO_CC      = $(APOLLO_PREFIX)/bin/m68k-amigaos-gcc
I2C_INC        ?= ../../I2C-test/i2clib-master/include
AMIGA_CPU      ?= -m68020 -m68881
# ApolloLib (Debug-UART der V4, siehe Projects/ApolloLib). Beide
# Amiga-Programme schreiben auf den Debug-UART, deshalb wird die Hilfe
# ApolloCrossDev_Debug.c in beide gelinkt.
APOLLOLIB      ?= ../../ApolloLib
AMIGA_SRC       = $(LIB_SRC) $(BASE_SRC) v4_amiga_i2c.c v4_console.c

# Cross-Werkzeuge fuer den Endianness-Lauf
M68K_CC   ?= m68k-linux-gnu-gcc
QEMU_M68K ?= qemu-m68k

# Werkzeuge und Ziel fuer die Uebertragung auf die V4 -- gleicher Weg und
# gleiche Namen wie im Hausstil (Projects/_makefiles/Makefile).
REPO_ROOT ?= $(PWD)/../..
V4_DIR    ?= Programs/test/
APOLLO_V4_HOST ?= $(AMIGAHOST):$(V4_DIR)
ACP    ?= $(REPO_ROOT)/ApolloExplorer/acp/acp
ICONS  ?= $(REPO_ROOT)/_icons
# Namen, unter denen die Programme auf der V4 liegen (ohne Endung).
AMIGA_NAME ?= v4_joy
# Name auf der V4. Die Workbench zeigt den DATEINAMEN der Ikone an -
# deshalb traegt die Datei den Namen mit Leerzeichen, und alle
# Verwendungen muessen in Anfuehrungszeichen stehen.
GUI_NAME   ?= BT Gamecontroller GUI
LOG_NAME   ?= v4_joy.log

.PHONY: all test lint selftest-lint mui-lint gui-lint test-san test-m68k demo demo-i2c \
        amiga amiga-gui asm verify clean upload upload-gui log check-host \
        check-upload acp-ls

all: test

$(BUILD):
	@mkdir -p $(BUILD)

# ------------------------------------------------------------------ #
# Wirt: Little Endian                                                 #
# ------------------------------------------------------------------ #

$(BUILD)/v4_tests: $(TEST_SRC) $(HDRS) | $(BUILD)
	$(CC) $(CFLAGS) -o $@ $(TEST_SRC)

test: lint $(BUILD)/v4_tests
	@echo
	@./$(BUILD)/v4_tests

# Derselbe Testlauf unter AddressSanitizer und UndefinedBehaviorSanitizer.
# Findet Pufferueberlaeufe, Lecks, vorzeichenbehaftete Ueberlaeufe und
# fehlausgerichtete Zugriffe -- Klassen von Fehlern, die bei Byte-Order-Arbeit
# typisch sind.
$(BUILD)/v4_tests_san: $(TEST_SRC) $(HDRS) | $(BUILD)
	$(CC) $(CSTD) $(WARN) -O1 -g -fsanitize=address,undefined \
	    -fno-omit-frame-pointer -I. -o $@ $(TEST_SRC)

test-san: $(BUILD)/v4_tests_san
	@echo "== Testlauf mit ASan + UBSan =="
	@ASAN_OPTIONS=detect_leaks=1 UBSAN_OPTIONS=print_stacktrace=1 \
	    ./$(BUILD)/v4_tests_san > $(BUILD)/san.log 2>&1 || { \
		echo "SANITIZER-BEFUND:"; tail -40 $(BUILD)/san.log; exit 1; }
	@tail -6 $(BUILD)/san.log
	@grep -q "ERGEBNIS: OK" $(BUILD)/san.log \
	    || { echo "FEHLER: Suite unter Sanitizern nicht OK"; exit 1; }
	@echo "Sanitizer: keine Befunde"

lint:
	@sh tools/check_portability.sh

selftest-lint:
	@sh tools/check_portability.sh --selftest

# Aufbaupruefer fuer den MUI-Objektbaum in v4_gui.c (tools/check_mui_tree.py).
#
# WARUM DAS EIGENE ZIEL: ein falsch gesetztes `End`, ein Tag ohne Wert oder ein
# Tag einer fremden Klasse uebersetzt der C-Uebersetzer fehlerfrei -- MUI
# stuerzt damit aber BEIM AUFBAU ab, und auf der V4 sieht man dann nur "kein
# Fenster". Der Pruefer liest den Aufbau aus dem Quelltext (Zerlegung an den
# Kommas der Tiefe 0, also genau die Tag-Liste), baut den Baum nach und prueft
# Klammerbalance, `Child`/Objekt, Tag/Wert, Klassenzugehoerigkeit und dass
# jede Gruppe mindestens ein Kind hat. `--selftest` belegt an einem
# kuenstlichen Baum, dass der Pruefer diese Fehler wirklich findet.
mui-lint:
	@command -v python3 >/dev/null 2>&1 || { \
	    echo "FEHLER: python3 fehlt -- der MUI-Baumpruefer laeuft nicht."; \
	    exit 1; }
	@python3 tools/check_mui_tree.py v4_gui.c
	@python3 tools/check_mui_tree.py --selftest

# Wache ueber die EINGANGSQUELLE der Oberflaeche.
#
# Der Benutzer will im Fenster nur sehen, was ueber den Joystick-Port ankommt.
# Quelle dafuer sind die Register der V4 (v4_joyport.c), NICHT GET_INPUT. Diese
# Wache liest den Quelltext von v4_gui.c und schlaegt fehl, sobald dort wieder
# ein I2C-Eingang oder ein I2C-Vergleich auftaucht. Sie ergaenzt den Mock-Test
# "Oberflaechen-Takt: die I2C-Folge enthaelt KEIN GET_INPUT" (tests/test_master.c):
# der prueft die Aufruffolge, diese Wache den Quelltext.
#
# Die Konsole (v4_console.c) ist AUSDRUECKLICH nicht betroffen: sie ist der
# Notfallweg und das Diagnosewerkzeug und behaelt GET_INPUT samt Zug/Pegel.
gui-lint:
	@fail=0; \
	for wort in v4_get_input v4_joy_debug_text v4_joy_herz_text \
	            v4_joy_vergleich_text v4_joy_port_text v4_joy_abweichung \
	            s_input_frisch ABWEICHUNG; do \
	    if grep -q "$$wort" v4_gui.c; then \
	        echo "FEHLER: v4_gui.c nennt $$wort -- die Oberflaeche darf keine"; \
	        echo "        Controller-Eingabewerte ueber I2C holen oder vergleichen."; \
	        echo "        Quelle ist v4_joyport.c (JOY0DAT/JOY1DAT/CIAAPRA/POTINP)."; \
	        fail=1; \
	    fi; \
	done; \
	test $$fail -eq 0 || exit 1; \
	echo "gui-lint: v4_gui.c ohne GET_INPUT und ohne I2C-Vergleich -- sauber"

# ------------------------------------------------------------------ #
# Demo: das echte Konsolenprogramm gegen den Mock                     #
#                                                                     #
# Der Lauf ist NICHT interaktiv: die Befehle kommen ueber eine Pipe,   #
# die letzte Zeile ist 'q'. Ohne das bliebe ein Vorfuehrlauf an einer  #
# Konsole ohne Eingabe stehen.                                        #
# ------------------------------------------------------------------ #

$(BUILD)/v4_console_demo: $(DEMO_SRC) $(HDRS) | $(BUILD)
	$(CC) $(CFLAGS) -DV4_CONSOLE_NO_MAIN -o $@ $(DEMO_SRC)

demo: $(BUILD)/v4_console_demo
	@fail=0; \
	echo "== Demo 1: Zustand, Controller, Ports, dann Ende =="; \
	printf 's\nh\ni\nq\n' | ./$(BUILD)/v4_console_demo -n > $(BUILD)/demo1.txt 2>&1 || fail=1; \
	grep -q "Adresse 0x5E" $(BUILD)/demo1.txt \
	    || { echo "  FEHLER: Adresse 0x5E steht nicht im Kopf"; fail=1; }; \
	grep -q "PING ok: proto=3 fw=5 write=32 read=128" $(BUILD)/demo1.txt \
	    || { echo "  FEHLER: PING-Zeile fehlt oder nennt andere Rahmengroessen"; fail=1; }; \
	grep -q "Faehigkeiten: 0x07FF" $(BUILD)/demo1.txt \
	    || { echo "  FEHLER: Faehigkeiten fehlen"; fail=1; }; \
	grep -q "Verteilung,Geraetezuordnung" $(BUILD)/demo1.txt \
	    || { echo "  FEHLER: die Faehigkeitsbits der Eingangsverteilung fehlen"; fail=1; }; \
	grep -q "Maus-Einstellungen" $(BUILD)/demo1.txt \
	    || { echo "  FEHLER: das Faehigkeitsbit der Maus-Einstellungen fehlt"; fail=1; }; \
	grep -q "Buchse 0: Geraet (keines)" $(BUILD)/demo1.txt \
	    || { echo "  FEHLER: die Eingangsverteilung steht nicht im Anfangszustand"; fail=1; }; \
	grep -q "Vorgabe auto" $(BUILD)/demo1.txt \
	    || { echo "  FEHLER: die VORGABE (mode_soll) fehlt -- sie ist nicht die wirksame Betriebsart"; fail=1; }; \
	grep -q "Bulk-Weg: nein" $(BUILD)/demo1.txt \
	    || { echo "  FEHLER: der fehlende Bulk-Weg wird nicht gemeldet"; fail=1; }; \
	grep -q "Controller: 8BitDo Pro 2" $(BUILD)/demo1.txt \
	    || { echo "  FEHLER: Controller-Zeile fehlt"; fail=1; }; \
	grep -q "VID:PID 2DC8:6100" $(BUILD)/demo1.txt \
	    || { echo "  FEHLER: VID/PID fehlen"; fail=1; }; \
	grep -q "Port 0 (Joystick)" $(BUILD)/demo1.txt \
	    || { echo "  FEHLER: Betriebsart von Buchse 0 fehlt"; fail=1; }; \
	grep -q "Pegel=0x7F" $(BUILD)/demo1.txt \
	    || { echo "  FEHLER: die gemessenen Pegel (Ruhe 0x7F) fehlen"; fail=1; }; \
	grep -q "Leitungspruefung: Pull-up wirkt" $(BUILD)/demo1.txt \
	    || { echo "  FEHLER: die Leitungspruefung fehlt"; fail=1; }; \
	grep -q "BTN3    Pin 5" $(BUILD)/demo1.txt \
	    || { echo "  FEHLER: die sieben Leitungen sind nicht vollstaendig"; fail=1; }; \
	grep -c "Leitung DB9" $(BUILD)/demo1.txt > /dev/null || { echo "  FEHLER: keine Portgruppe gefunden"; fail=1; }; \
	test $$(grep -c "Leitung DB9" $(BUILD)/demo1.txt) -eq 4 \
	    || { echo "  FEHLER: es sind nicht zwei Portgruppen je Ausgabe (Start + 'i')"; fail=1; }; \
	echo "== Demo 2: gezogene Leitungen an beiden Buchsen =="; \
	V4_DEMO_DRIVE0=0x11 V4_DEMO_DRIVE1=0x08 \
	    sh -c "printf 'i\nq\n' | ./$(BUILD)/v4_console_demo -n" > $(BUILD)/demo2.txt 2>&1 || fail=1; \
	grep -q "Zug=0x11  Pegel=0x6E" $(BUILD)/demo2.txt \
	    || { echo "  FEHLER: Zug und gemessener Pegel von Buchse 0 fehlen"; fail=1; }; \
	grep -qF "UP      Pin 1   GND       0 (Low)      [aktiv] ok (gezogen -> Low)" $(BUILD)/demo2.txt \
	    || { echo "  FEHLER: gezogene Leitung wird nicht als GND/Low gezeigt"; fail=1; }; \
	grep -qF "RIGHT   Pin 4   GND       0 (Low)      [aktiv] ok (gezogen -> Low)" $(BUILD)/demo2.txt \
	    || { echo "  FEHLER: Buchse 1 zeigt RIGHT nicht gezogen"; fail=1; }; \
	grep -q "Zug=0x08  Pegel=0x77" $(BUILD)/demo2.txt \
	    || { echo "  FEHLER: Zug und Pegel von Buchse 1 stimmen nicht"; fail=1; }; \
	echo "== Demo 3: Scan, Verbinden, Portmodus, Tauschen =="; \
	printf 'a\nc\n1\np\n0\nt\nd\nf\nr\nq\n' | ./$(BUILD)/v4_console_demo -n > $(BUILD)/demo3.txt 2>&1 || fail=1; \
	grep -q "Geraete: 3" $(BUILD)/demo3.txt \
	    || { echo "  FEHLER: Geraeteliste fehlt"; fail=1; }; \
	grep -q "Logitech MX Anywhere" $(BUILD)/demo3.txt \
	    || { echo "  FEHLER: Geraetename fehlt"; fail=1; }; \
	grep -q "Verbunden mit Index 1 (CONNECTED)" $(BUILD)/demo3.txt \
	    || { echo "  FEHLER: CONNECT wird nicht bis CONNECTED verfolgt"; fail=1; }; \
	grep -q "Port 0: Joystick -> Maus (bestaetigt: Maus)" $(BUILD)/demo3.txt \
	    || { echo "  FEHLER: Modusweiterschaltung fehlt"; fail=1; }; \
	grep -q "Buchsen getauscht: Port 0 = aus, Port 1 = Maus" $(BUILD)/demo3.txt \
	    || { echo "  FEHLER: Buchsentausch fehlt"; fail=1; }; \
	grep -q "DISCONNECT: OK" $(BUILD)/demo3.txt \
	    || { echo "  FEHLER: DISCONNECT fehlt"; fail=1; }; \
	grep -q "FORGET: OK" $(BUILD)/demo3.txt \
	    || { echo "  FEHLER: FORGET fehlt"; fail=1; }; \
	grep -q "RESET: OK (beide Buchsen aus, Verbindung bleibt)" $(BUILD)/demo3.txt \
	    || { echo "  FEHLER: RESET fehlt"; fail=1; }; \
	echo "== Demo 4: laufende Verbindung und Maus-Controller =="; \
	V4_DEMO_ALREADY=1 sh -c "printf 's\nq\n' | ./$(BUILD)/v4_console_demo -n" \
	    > $(BUILD)/demo4.txt 2>&1 || fail=1; \
	grep -q "Zustand 3 (CONNECTED)" $(BUILD)/demo4.txt \
	    || { echo "  FEHLER: bestehende Verbindung wird nicht gezeigt"; fail=1; }; \
	V4_DEMO_MOUSE=1 sh -c "printf 'h\ni\nq\n' | ./$(BUILD)/v4_console_demo -n" \
	    > $(BUILD)/demo5.txt 2>&1 || fail=1; \
	grep -q "Art Maus, Transport BLE" $(BUILD)/demo5.txt \
	    || { echo "  FEHLER: Maus/Transport wird nicht gezeigt"; fail=1; }; \
	grep -q "Port 1 (Maus)" $(BUILD)/demo5.txt \
	    || { echo "  FEHLER: Buchse 1 steht nicht auf Maus"; fail=1; }; \
	echo "== Demo 5: Pegel -- Widerspruch und alte Firmware =="; \
	V4_DEMO_DRIVE1=0x20 V4_DEMO_LEVEL1=0x20 \
	    sh -c "printf 'i\nq\n' | ./$(BUILD)/v4_console_demo -n" \
	    > $(BUILD)/demo9.txt 2>&1 || fail=1; \
	grep -qF "WIDERSPRUCH: gezogen, liest High -> elektrisch unmoeglich" $(BUILD)/demo9.txt \
	    || { echo "  FEHLER: 'gezogen, liest High' wird nicht als Widerspruch gezeigt"; fail=1; }; \
	grep -q "WARNUNG: Port 1 zieht BTN2 nach GND, liest dort aber High" $(BUILD)/demo9.txt \
	    || { echo "  FEHLER: die WARNUNG zum unmoeglichen Pegel fehlt"; fail=1; }; \
	V4_DEMO_LEVEL0=0x00 sh -c "printf 'i\nq\n' | ./$(BUILD)/v4_console_demo -n" \
	    > $(BUILD)/demo10.txt 2>&1 || fail=1; \
	grep -qF "WIDERSPRUCH: losgelassen, liest 0 -> kein Pull-up" $(BUILD)/demo10.txt \
	    || { echo "  FEHLER: der fehlende Pull-up wird nicht erkannt"; fail=1; }; \
	grep -q "WARNUNG: Port 0 hat UP DOWN LEFT RIGHT BTN1 BTN2 BTN3 losgelassen, liest dort aber 0" $(BUILD)/demo10.txt \
	    || { echo "  FEHLER: die WARNUNG zum fehlenden Pull-up fehlt"; fail=1; }; \
	V4_DEMO_LEGACY_INPUT=1 sh -c "printf 'i\nq\n' | ./$(BUILD)/v4_console_demo -n" \
	    > $(BUILD)/demo11.txt 2>&1 || fail=1; \
	grep -q "Pegel: Firmware ohne Pegel -- neu flashen" $(BUILD)/demo11.txt \
	    || { echo "  FEHLER: bei der 10-Byte-Fassung fehlt der Hinweis"; fail=1; }; \
	grep -q "Hinweis: die Gegenstelle antwortet mit der 10-Byte-Fassung" $(BUILD)/demo11.txt \
	    || { echo "  FEHLER: der Versionshinweis fehlt"; fail=1; }; \
	grep -qF "(Pegel unbekannt)" $(BUILD)/demo11.txt \
	    || { echo "  FEHLER: die Zeilen ohne Pegel sind nicht gekennzeichnet"; fail=1; }; \
	grep -q "Pegel=0x" $(BUILD)/demo11.txt \
	    && { echo "  FEHLER: ohne Pegel darf KEINE Zahl erscheinen"; fail=1; }; \
	echo "== Demo 6: Fehlerwege und Schalter =="; \
	V4_DEMO_BAD_ECHO=1 sh -c "printf 'p\n0\nq\n' | ./$(BUILD)/v4_console_demo -n" \
	    > $(BUILD)/demo6.txt 2>&1 || fail=1; \
	grep -q "SET_PORT_MODE: Antwort gueltig, Nutzlast unbrauchbar" $(BUILD)/demo6.txt \
	    || { echo "  FEHLER: falsche Bestaetigung wird nicht gemeldet"; fail=1; }; \
	if ./$(BUILD)/v4_console_demo -Z > $(BUILD)/demo7.txt 2>&1; then \
	    echo "  FEHLER: unbekannter Schalter wurde nicht abgelehnt"; fail=1; fi; \
	grep -q "Unbekannter Schalter" $(BUILD)/demo7.txt \
	    || { echo "  FEHLER: Meldung zum unbekannten Schalter fehlt"; fail=1; }; \
	rm -f $(BUILD)/demo_log.txt; \
	sh -c "printf 's\nq\n' | ./$(BUILD)/v4_console_demo -o $(BUILD)/demo_log.txt" \
	    > $(BUILD)/demo8.txt 2>&1 || fail=1; \
	test -s $(BUILD)/demo_log.txt \
	    || { echo "  FEHLER: Logdatei (-o) bleibt leer"; fail=1; }; \
	grep -q "PING ok: proto=3" $(BUILD)/demo_log.txt \
	    || { echo "  FEHLER: Logdatei spiegelt die Ausgabe nicht"; fail=1; }; \
	echo "== Demo 7: toter Link -- die Konsole meldet das und wartet nicht =="; \
	V4_DEMO_LINK_TOT=1 sh -c "printf 's\nq\n' | ./$(BUILD)/v4_console_demo -n" \
	    > $(BUILD)/demo12.txt 2>&1 || true; \
	grep -q "PING: LINK-Fehler" $(BUILD)/demo12.txt \
	    || { echo "  FEHLER: der tote Link wird nicht als LINK-Fehler gemeldet"; fail=1; }; \
	grep -q "Erwartet wird ein 128-Byte-READ-Rahmen" $(BUILD)/demo12.txt \
	    || { echo "  FEHLER: der Abbruch nach dem PING fehlt"; fail=1; }; \
	grep -q "Versuche=" $(BUILD)/demo12.txt \
	    || { echo "  FEHLER: die Diagnosezeile (Versuche/Wiederholungen) fehlt"; fail=1; }; \
	grep -q "geprueft=" $(BUILD)/demo12.txt \
	    || { echo "  FEHLER: die Rahmenzaehler fehlen in der Fehlerzeile"; fail=1; }; \
	if grep -q "letzte_Rahmenpruefung" $(BUILD)/demo12.txt; then \
	    echo "  FEHLER: der haftenbleibende letzte Zustand steht noch da"; \
	    fail=1; fi; \
	echo "== Demo 8: Debugausgabe -- Transaktionszeilen und -quiet =="; \
	sh -c "printf 's\nq\n' | ./$(BUILD)/v4_console_demo -n" \
	    > $(BUILD)/demo13.txt 2>&1 || fail=1; \
	grep -q "^\[i2c\] cmd=GET_STATUS" $(BUILD)/demo13.txt \
	    || { echo "  FEHLER: keine Zeile je Transaktion (GET_STATUS)"; fail=1; }; \
	grep -q "^\[i2c\] cmd=GET_INPUT .*state0=0x00 state1=0x00 level0=0x7F level1=0x7F" \
	    $(BUILD)/demo13.txt \
	    || { echo "  FEHLER: Kurzform der GET_INPUT-Antwort fehlt"; fail=1; }; \
	grep -q "^\[plattform\] v4_plat_open -> 0 (OK)" $(BUILD)/demo13.txt \
	    || { echo "  FEHLER: Plattformzeile mit Rueckgabewert fehlt"; fail=1; }; \
	grep -q "^\[i2c\].*dauer=[0-9]*us" $(BUILD)/demo13.txt \
	    || { echo "  FEHLER: die Dauer der Transaktion fehlt"; fail=1; }; \
	grep -q "^\[modus\] Buchse 0" $(BUILD)/demo13.txt \
	    || { echo "  FEHLER: die Buchsenmodi werden nicht gemeldet"; fail=1; }; \
	grep -q "^\[ende\]" $(BUILD)/demo13.txt \
	    || { echo "  FEHLER: die Schlusszeile fehlt"; fail=1; }; \
	grep -q "^\[ende\].*geprueft=[0-9]* fehlerhaft=[0-9]* fehlerserie=[0-9]*" \
	    $(BUILD)/demo13.txt \
	    || { echo "  FEHLER: die Schlusszeile traegt die Rahmenzaehler nicht"; fail=1; }; \
	sh -c "printf 's\nq\n' | ./$(BUILD)/v4_console_demo -n -quiet" \
	    > $(BUILD)/demo14.txt 2>&1 || fail=1; \
	if grep -q "^\[i2c\]" $(BUILD)/demo14.txt; then \
	    echo "  FEHLER: -quiet unterdrueckt die Transaktionszeilen nicht"; fail=1; fi; \
	grep -q "^\[plattform\]" $(BUILD)/demo14.txt \
	    || { echo "  FEHLER: -quiet verschluckt auch die Startzeilen"; fail=1; }; \
	grep -q "^\[ende\]" $(BUILD)/demo14.txt \
	    || { echo "  FEHLER: -quiet verschluckt die Schlusszeile"; fail=1; }; \
	echo "== Demo 9: Eingangsverteilung (fw_ver 4) -- das Abnahmebeispiel =="; \
	printf 'a\ncfg 0 mode maus\ncfg 1 mode joystick\ncfg 0 dir stick_l\ncfg 0 btn1 l1\ncfg 0 btn2 r1\ncfg 0 btn3 keine\ncfg 1 dir dpad\ncfg 1 btn1 a\ncfg 1 btn2 b\ncfg 1 btn3 start\ncfg\ncfg 0 btn2 l1\ncfg assign 1 0\ncfg assign F4:17:D8:11:22:33 1\ncfg\nq\n' \
	    | ./$(BUILD)/v4_console_demo -n > $(BUILD)/demo15.txt 2>&1 || fail=1; \
	grep -q "cmd=GET_PORT_CONFIG" $(BUILD)/demo15.txt \
	    || { echo "  FEHLER: GET_PORT_CONFIG wird nicht abgesetzt"; fail=1; }; \
	grep -q "cmd=SET_PORT_ROUTING" $(BUILD)/demo15.txt \
	    || { echo "  FEHLER: SET_PORT_ROUTING wird nicht abgesetzt"; fail=1; }; \
	grep -q "cmd=SET_DEVICE_PORT" $(BUILD)/demo15.txt \
	    || { echo "  FEHLER: SET_DEVICE_PORT wird nicht abgesetzt"; fail=1; }; \
	grep -q "bestaetigt port=0 dir=0x01 btn=L1/R1/keine" $(BUILD)/demo15.txt \
	    || { echo "  FEHLER: die Echo-Zeile der Verteilung fehlt"; fail=1; }; \
	grep -qE "Buchse 0: Geraet Logitech MX Anywhere +Vorgabe Maus +wirkt Maus +Richtungen Stick links +Pin6<-L1 Pin9<-R1 Pin5<-keine" $(BUILD)/demo15.txt \
	    || { echo "  FEHLER: Buchse 0 zeigt nicht das Abnahmebeispiel"; fail=1; }; \
	grep -qE "Buchse 1: Geraet F4:17:D8:11:22:33 +Vorgabe Joystick +wirkt Joystick +Richtungen D-Pad +Pin6<-A Pin9<-B Pin5<-Start" $(BUILD)/demo15.txt \
	    || { echo "  FEHLER: Buchse 1 zeigt nicht das Abnahmebeispiel"; fail=1; }; \
	grep -q "SET_PORT_ROUTING: BAD_ARG -- \"L1\" ist auf dieser Buchse schon einer anderen Leitung zugeordnet" $(BUILD)/demo15.txt \
	    || { echo "  FEHLER: die doppelte Knopfquelle wird nicht im Klartext gemeldet"; fail=1; }; \
	grep -q 'Geraet \[1\] "Logitech MX Anywhere" -> Buchse 0 (bestaetigt).' $(BUILD)/demo15.txt \
	    || { echo "  FEHLER: SET_DEVICE_PORT ueber den Listenindex fehlt"; fail=1; }; \
	grep -q "Adresse F4:17:D8:11:22:33 -> Buchse 1 (bestaetigt)." $(BUILD)/demo15.txt \
	    || { echo "  FEHLER: SET_DEVICE_PORT ueber die Adresse fehlt"; fail=1; }; \
	echo "== Demo 10: fw_ver 3 -- die V4 nimmt sie an, die neuen Felder bleiben aus =="; \
	V4_DEMO_LEGACY_FW3=1 sh -c "printf 'cfg\ncfg 0 mode auto\nq\n' | ./$(BUILD)/v4_console_demo -n -quiet" \
	    > $(BUILD)/demo16.txt 2>&1 || fail=1; \
	grep -q "PING ok: proto=3 fw=3 write=32 read=128" $(BUILD)/demo16.txt \
	    || { echo "  FEHLER: fw_ver 3 wird nicht angenommen (gleicher Rahmen!)"; fail=1; }; \
	grep -q "Faehigkeiten: 0x00FF" $(BUILD)/demo16.txt \
	    || { echo "  FEHLER: die alten Faehigkeiten fehlen"; fail=1; }; \
	grep -q "Die Gegenstelle kann die Eingangsverteilung nicht" $(BUILD)/demo16.txt \
	    || { echo "  FEHLER: der fehlende Befehl wird nicht gemeldet"; fail=1; }; \
	if grep -q "^\[i2c\] cmd=GET_PORT_CONFIG" $(BUILD)/demo16.txt; then \
	    echo "  FEHLER: GET_PORT_CONFIG wurde trotz fehlendem Faehigkeitsbit gesendet"; fail=1; fi; \
	echo "== Demo 11: Beitrag (analog/digital/beide) und Maus-Einstellungen =="; \
	printf 'cfg 0 beitrag analog\ncfg 1 beitrag digital\ncfg 0 beitrag beide\ncfg 0 mouse speed 3\ncfg 0 mouse totzone 40\ncfg 1 mouse speed 9\ncfg 1 mouse totzone 100\ncfg\nq\n' \
	    | ./$(BUILD)/v4_console_demo -n > $(BUILD)/demo17.txt 2>&1 || fail=1; \
	grep -q "cmd=GET_MOUSE_CFG" $(BUILD)/demo17.txt \
	    || { echo "  FEHLER: GET_MOUSE_CFG wird nicht abgesetzt"; fail=1; }; \
	grep -q "cmd=SET_MOUSE_CFG" $(BUILD)/demo17.txt \
	    || { echo "  FEHLER: SET_MOUSE_CFG wird nicht abgesetzt"; fail=1; }; \
	grep -q "bestaetigt port=0 speed=3 totzone=12%" $(BUILD)/demo17.txt \
	    || { echo "  FEHLER: die Echo-Zeile der Maus-Einstellung fehlt"; fail=1; }; \
	grep -q "Buchse 0: Beitrag analog gesetzt -- Richtungen Stick links, Pin6<-L1 Pin9<-R1 Pin5<-keine." $(BUILD)/demo17.txt \
	    || { echo "  FEHLER: Beitrag analog setzt nicht Stick links + L1/R1"; fail=1; }; \
	grep -q "Buchse 1: Beitrag digital gesetzt -- Richtungen D-Pad, Pin6<-A Pin9<-B Pin5<-Start." $(BUILD)/demo17.txt \
	    || { echo "  FEHLER: Beitrag digital setzt nicht D-Pad + A/B/Start"; fail=1; }; \
	grep -q "Buchse 0: Beitrag beide gesetzt -- Richtungen Stick links+Stick rechts+D-Pad, Pin6<-Gruppe 1 (A,L1,L2,L3) Pin9<-Gruppe 2 (B,R1,R2,R3) Pin5<-Gruppe 3 (X,Y,Start,Select)." $(BUILD)/demo17.txt \
	    || { echo "  FEHLER: Beitrag beide setzt nicht alle Richtungen + die drei Gruppen"; fail=1; }; \
	grep -q "Buchse 0: Maus-Einstellung bestaetigt -- Tempo 3/10, Totzone 40%." $(BUILD)/demo17.txt \
	    || { echo "  FEHLER: die Maus-Einstellung von Buchse 0 wird nicht bestaetigt"; fail=1; }; \
	grep -q "Buchse 1: Maus-Einstellung bestaetigt -- Tempo 9/10, Totzone 100%." $(BUILD)/demo17.txt \
	    || { echo "  FEHLER: die Maus-Einstellung von Buchse 1 wird nicht bestaetigt"; fail=1; }; \
	grep -qE "Buchse 0: Geraet .*Beitrag beide +Tempo 3/10, Totzone 40% \(Einzelschalter gegenstandslos\)" $(BUILD)/demo17.txt \
	    || { echo "  FEHLER: die Beitrags-Zeile mit dem Gegenstandslos-Hinweis fehlt"; fail=1; }; \
	grep -qE "Buchse 1: Geraet .*Beitrag digital +Tempo 9/10, Totzone 100%" $(BUILD)/demo17.txt \
	    || { echo "  FEHLER: Beitrag/Maus von Buchse 1 stehen nicht in der Zeile"; fail=1; }; \
	grep -q "Hinweis: bei 'beide' sind die EINZELNEN Richtungs- und Knopfschalter gegenstandslos" $(BUILD)/demo17.txt \
	    || { echo "  FEHLER: der Gegenstandslos-Hinweis zu 'beide' fehlt"; fail=1; }; \
	echo "== Demo 12: fw_ver 4 -- Maus-Regler nicht verfuegbar, nichts gesendet =="; \
	V4_DEMO_LEGACY_FW4=1 sh -c "printf 'cfg\ncfg 0 mouse speed 3\nq\n' | ./$(BUILD)/v4_console_demo -n" \
	    > $(BUILD)/demo18.txt 2>&1 || fail=1; \
	grep -q "PING ok: proto=3 fw=4 write=32 read=128" $(BUILD)/demo18.txt \
	    || { echo "  FEHLER: fw_ver 4 wird nicht angenommen (gleicher Rahmen!)"; fail=1; }; \
	grep -q "Faehigkeiten: 0x03FF" $(BUILD)/demo18.txt \
	    || { echo "  FEHLER: fw_ver 4 meldet nicht genau die alten Faehigkeiten"; fail=1; }; \
	grep -q "Die Gegenstelle kann die Maus-Einstellungen nicht" $(BUILD)/demo18.txt \
	    || { echo "  FEHLER: die fehlende Maus-Faehigkeit wird nicht gemeldet"; fail=1; }; \
	grep -q "Maus nicht verfuegbar (fw_ver 5 fehlt)" $(BUILD)/demo18.txt \
	    || { echo "  FEHLER: die Zeile kennzeichnet den Maus-Regler nicht als nicht verfuegbar"; fail=1; }; \
	grep -q "cmd=GET_PORT_CONFIG" $(BUILD)/demo18.txt \
	    || { echo "  FEHLER: die Eingangsverteilung von fw_ver 4 wird nicht mehr gelesen"; fail=1; }; \
	if grep -q "cmd=GET_MOUSE_CFG" $(BUILD)/demo18.txt; then \
	    echo "  FEHLER: GET_MOUSE_CFG wurde trotz fehlendem Faehigkeitsbit gesendet"; fail=1; fi; \
	if grep -q "cmd=SET_MOUSE_CFG" $(BUILD)/demo18.txt; then \
	    echo "  FEHLER: SET_MOUSE_CFG wurde trotz fehlendem Faehigkeitsbit gesendet"; fail=1; fi; \
	echo "== Demo 13: falsches Echo von SET_MOUSE_CFG wird nicht geglaubt =="; \
	V4_DEMO_BAD_MOUSE_ECHO=1 sh -c "printf 'cfg 0 mouse speed 3\ncfg\nq\n' | ./$(BUILD)/v4_console_demo -n" \
	    > $(BUILD)/demo19.txt 2>&1 || fail=1; \
	grep -q "SET_MOUSE_CFG: Antwort gueltig, Nutzlast unbrauchbar -- nichts geaendert." $(BUILD)/demo19.txt \
	    || { echo "  FEHLER: das falsche Echo wird nicht als Fehler gemeldet"; fail=1; }; \
	grep -qE "Buchse 0: Geraet .*Tempo 7/10, Totzone 12%" $(BUILD)/demo19.txt \
	    || { echo "  FEHLER: nach dem falschen Echo steht nicht mehr der echte Stand"; fail=1; }; \
	if [ "$$fail" -eq 0 ]; then \
	    echo "Demo: OK (13 Durchlaeufe, Mock, ohne Hardware)"; \
	else echo "Demo: FEHLGESCHLAGEN"; exit 1; fi

# Dasselbe Programm fuer einen echten i2c-dev-Bus. Laeuft nur, wenn dort auch
# ein Adapter haengt -- deshalb kein Automatiklauf in `verify`.
demo-i2c: $(DEMO_I2C_SRC) $(HDRS) | $(BUILD)
	$(CC) $(CFLAGS) -o $(BUILD)/v4_console_i2c $(DEMO_I2C_SRC)
	@echo "Aufruf: ./$(BUILD)/v4_console_i2c -d /dev/i2c-1"

# ------------------------------------------------------------------ #
# Big Endian: derselbe Testlauf unter qemu-m68k                       #
# ------------------------------------------------------------------ #

test-m68k: | $(BUILD)
	@if command -v $(M68K_CC) >/dev/null 2>&1 \
	    && command -v $(QEMU_M68K) >/dev/null 2>&1; then \
		echo "Baue Testbinary fuer m68k-linux-gnu und starte unter $(QEMU_M68K)..."; \
		$(M68K_CC) $(CFLAGS) -static -o $(BUILD)/v4_tests_m68k $(TEST_SRC) \
		    && $(QEMU_M68K) $(BUILD)/v4_tests_m68k; \
	else \
		echo "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!"; \
		echo "!!! SKIP: $(M68K_CC) und/oder $(QEMU_M68K) fehlen."; \
		echo "!!! Der Endianness-Lauf auf Big Endian wurde NICHT ausgefuehrt."; \
		echo "!!! Nachinstallieren: sudo apt install gcc-m68k-linux-gnu qemu-user-static"; \
		echo "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!"; \
		test -z "$(REQUIRE_M68K)" || exit 1; \
	fi

# ------------------------------------------------------------------ #
# Zielsystem: Vampire V4 (m68k-amigaos)                               #
# ------------------------------------------------------------------ #

amiga: | $(BUILD)
	$(APOLLO_CC) -std=gnu99 $(WARN) $(OPT) $(AMIGA_CPU) -noixemul \
	    -DREGPARM=4 -DV4_PLAT_AMIGA=1 -I. -I$(I2C_INC) -I$(APOLLOLIB) \
	    -o $(BUILD)/v4_console_amiga $(AMIGA_SRC) \
	    $(APOLLOLIB)/ApolloCrossDev_Debug.c
	@echo "Fertig: $(BUILD)/v4_console_amiga (m68k-amigaos)"

# MUI-Oberflaeche -- zweites Frontend, gleiche Protokollschicht.
# Braucht -lmui (libmui.a liegt im Compilerbaum) und dieselbe Debug-UART-Hilfe
# aus Projects/ApolloLib wie die Konsole. -Wno-pointer-sign, weil die MUI-Makros
# IPTR-Casts verlangen; ohne das meldet -Werror bei jedem Tag.
amiga-gui: | $(BUILD)
	$(APOLLO_CC) -std=gnu99 $(WARN) -Wno-pointer-sign $(OPT) $(AMIGA_CPU) -noixemul \
	    -DREGPARM=4 -DV4_PLAT_AMIGA=1 -I. -I$(I2C_INC) -I$(APOLLOLIB) \
	    -o $(BUILD)/v4_gui_amiga $(LIB_SRC) $(BASE_SRC) v4_amiga_i2c.c \
	    v4_gui.c \
	    $(APOLLOLIB)/ApolloCrossDev_Debug.c -lmui
	@echo "Fertig: $(BUILD)/v4_gui_amiga (m68k-amigaos, MUI)"

# Nachweis der Byte-Order-Behandlung im erzeugten m68k-Code: die Helfer und die
# Frame-Funktionen duerfen nur move.b auf Puffer erzeugen (§0). Ersatz fuer den
# qemu-Lauf, wenn gcc-m68k-linux-gnu fehlt.
asm:
	@sh tools/check_m68k_byteaccess.sh \
	    $(APOLLO_PREFIX)/bin/m68k-amigaos-objdump \
	    $(APOLLO_PREFIX)/bin/m68k-amigaos-gcc

# ------------------------------------------------------------------ #
# Alles, was ohne Hardware pruefbar ist.                               #
# ------------------------------------------------------------------ #

verify: lint selftest-lint mui-lint gui-lint test test-san demo asm amiga amiga-gui
	@echo
	@echo "verify: Lint, Tests, Sanitizer, Demo gegen den Mock, MUI-Baumpruefer,"
	@echo "        m68k-Nachweis, Amiga-Konsole und MUI-Oberflaeche sind durch."

# ------------------------------------------------------------------ #
# Direkt auf die V4 uebertragen                                        #
#                                                                      #
# Der Weg ist derselbe wie in allen anderen Projekten dieses Repos:    #
# `acp <datei> "<host>:Programs/test/"` schickt die Datei ueber das     #
# Netz an den ApolloExplorer-Server auf der V4 (kein USB, kein Kabel).  #
# Zu jedem Programm gehoert eine .info-Datei gleichen Namens.          #
# ------------------------------------------------------------------ #

check-host:
	@test -n "$(AMIGAHOST)" || { \
	    echo "FEHLER: AMIGAHOST ist nicht gesetzt."; \
	    echo "        export AMIGAHOST=<ip-der-v4>    z.B. 192.168.178.50"; \
	    exit 1; }

check-upload: check-host
	@test -x "$(ACP)" || { echo "FEHLER: acp nicht gefunden: $(ACP)"; exit 1; }
	@test -f "$(ICONS)/ApolloIcon.info" || { \
	    echo "FEHLER: Ikone fehlt: $(ICONS)/ApolloIcon.info"; exit 1; }

upload: amiga amiga-gui check-upload
	@cp -f $(BUILD)/v4_console_amiga $(BUILD)/$(AMIGA_NAME)
	@cp -f $(ICONS)/ApolloIcon.info $(BUILD)/$(AMIGA_NAME).info
	@cp -f $(BUILD)/v4_gui_amiga "$(BUILD)/$(GUI_NAME)"
	@cp -f $(ICONS)/ApolloIcon.info "$(BUILD)/$(GUI_NAME).info"
	$(ACP) "$(BUILD)/$(AMIGA_NAME)" "$(APOLLO_V4_HOST)"
	$(ACP) "$(BUILD)/$(AMIGA_NAME).info" "$(APOLLO_V4_HOST)"
	$(ACP) "$(BUILD)/$(GUI_NAME)" "$(APOLLO_V4_HOST)"
	$(ACP) "$(BUILD)/$(GUI_NAME).info" "$(APOLLO_V4_HOST)"
	@echo "Auf der V4: $(APOLLO_V4_HOST)$(AMIGA_NAME) (Konsole) und $(GUI_NAME) (MUI)"
	@echo "Logdatei des Programms: $(V4_DIR)$(LOG_NAME)  (abholen: make log)"

upload-gui: amiga-gui check-upload
	@cp -f $(BUILD)/v4_gui_amiga "$(BUILD)/$(GUI_NAME)"
	@cp -f $(ICONS)/ApolloIcon.info "$(BUILD)/$(GUI_NAME).info"
	$(ACP) "$(BUILD)/$(GUI_NAME)" "$(APOLLO_V4_HOST)"
	$(ACP) "$(BUILD)/$(GUI_NAME).info" "$(APOLLO_V4_HOST)"
	@echo "Auf der V4: $(APOLLO_V4_HOST)$(GUI_NAME)  (Ausgabe auch ueber ser:)"

# Die Logdatei von der V4 holen und anzeigen -- der Weg nach einem Absturz.
log: check-host
	@test -x "$(ACP)" || { echo "FEHLER: acp nicht gefunden: $(ACP)"; exit 1; }
	@mkdir -p $(BUILD)
	$(ACP) "$(AMIGAHOST):$(V4_DIR)$(LOG_NAME)" $(BUILD)/
	@echo "---- $(V4_DIR)$(LOG_NAME) ----"
	@cat $(BUILD)/$(LOG_NAME)

# Nur lesende Hilfe, um die Verbindung zu pruefen.
acp-ls: check-host
	@test -x "$(ACP)" || { echo "FEHLER: acp nicht gefunden: $(ACP)"; exit 1; }
	@$(ACP) --list-directory "$(APOLLO_V4_HOST)"

clean:
	@rm -rf $(BUILD)
	@rm -f *.o tests/*.o
	@echo "sauber"
