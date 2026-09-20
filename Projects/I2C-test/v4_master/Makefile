# v4_master -- Makefile fuer den V4-I2C-Master aus PROTOCOL_V4_MASTER.md
#
# Ziele:
#   make              Lint + native Testsuite (Little Endian)
#   make test         dasselbe
#   make test-m68k    Testsuite unter qemu-m68k (Big Endian) -- siehe unten
#   make amiga        Bibliothek + Amiga-Plattform + Konsole fuer m68k-amigaos
#   make demo         Konsolenprogramm fuer den Linux-Harness (i2c-dev)
#   make asm          m68k-Disassembly der Byte-Order-Helfer (Nachweis)
#   make lint         nur der Portabilitaets-Lint
#   make upload       Programm + Ikone direkt auf die V4 (ApolloExplorer/acp)
#   make upload-debug zusaetzlich bgdbserver und Debug-Startskript
#   make acp-ls       Verzeichnis auf der V4 auflisten (nur lesend)
#   make clean
#
# Fuer die Upload-Ziele muss AMIGAHOST gesetzt sein (IP der V4) und auf der
# V4 der ApolloExplorer-Server laufen:
#     export AMIGAHOST=192.168.178.50
#     make upload
#
# Der Endianness-Lauf auf einem echten Big-Endian-Ziel (§0) braucht:
#     sudo apt install gcc-m68k-linux-gnu qemu-user-static
# Fehlen die Werkzeuge, meldet `make test-m68k` ausdruecklich SKIP und
# fuehrt den Lauf NICHT aus. Mit REQUIRE_M68K=1 wird der Lauf erzwungen:
#     make test-m68k REQUIRE_M68K=1

CC      ?= cc
OPT     ?= -O2
CSTD    ?= -std=gnu17
WARN     = -Wall -Wextra -Werror
CFLAGS   = $(CSTD) $(WARN) $(OPT) -I.

BUILD    = build

# Header mit angeben: ohne sie wuerde eine Aenderung an v4_proto.h keinen
# Neubau ausloesen und `make test` still das alte Binary laufen lassen.
HDRS      = v4_proto.h v4_master.h v4_mock.h tests/v4_test.h

LIB_SRC   = v4_proto.c v4_master.c
TEST_SRC  = $(LIB_SRC) v4_mock.c tests/test_main.c tests/test_proto.c \
            tests/test_master.c tests/test_session.c tests/test_fuzz.c
DEMO_SRC  = $(LIB_SRC) v4_linux_i2c.c v4_console.c

# Apollo-Toolchain (siehe ../../../GCC-6.50-Latest.sh)
APOLLO_PREFIX ?= ../../../Compilers/GCC-6.50-Latest
APOLLO_CC      = $(APOLLO_PREFIX)/bin/m68k-amigaos-gcc
I2C_INC        = ../i2clib-master/include
AMIGA_SRC      = $(LIB_SRC) v4_amiga_i2c.c v4_console.c

# Cross-Werkzeuge fuer den Endianness-Lauf
M68K_CC   ?= m68k-linux-gnu-gcc
QEMU_M68K ?= qemu-m68k

# Werkzeuge und Ziel fuer die Uebertragung auf die V4 -- gleicher Weg und
# gleiche Namen wie im Hausstil (Projects/_makefiles/Makefile, z.B.
# Projects/I2C-test/Makefile): ApolloExplorer/acp ueber das Netz.
# Achtung: dieses Makefile liegt drei Ebenen tief (Projects/I2C-test/v4_master),
# also ist ../../ bereits Projects/. Die Vorlagen unter Projects/ liegen eine
# Ebene hoeher und schreiben dort ../../Projects/... -- das waere hier falsch.
REPO_ROOT ?= $(PWD)/../..
# Zielverzeichnis auf der V4. `ram/` ist der Geraetename des RAM-Disk und damit
# ohne Leerzeichen; `Ram Disk/` ist derselbe Ort unter seinem Volume-Namen
# (so schreiben es die Vorlagen unter Projects/). Beides funktioniert -- bei
# Leerzeichen im Pfad muss der Aufrufer den Pfad quoten.
V4_DIR    ?= ram/
APOLLO_V4_HOST ?= $(AMIGAHOST):$(V4_DIR)
ACP    ?= $(REPO_ROOT)/ApolloExplorer/acp/acp
GDB    ?= $(REPO_ROOT)/bgdbserver/bgdbserver
ICONS  ?= $(REPO_ROOT)/_icons
# Name, unter dem das Programm auf der V4 liegt (ohne Endung).
AMIGA_NAME ?= v4_console

.PHONY: all test lint test-m68k amiga demo asm clean selftest-lint verify \
        smoke test-san upload upload-debug check-host check-upload acp-hosts acp-ls verify

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
# Findet Pufferueberlaeufe, Use-after-free, Lecks, vorzeichenbehaftete
# Ueberlaeufe und fehlausgerichtete Zugriffe -- Klassen von Fehlern, die
# bei Byte-Order-Arbeit typisch sind.
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

demo: $(DEMO_SRC) $(HDRS) | $(BUILD)
	$(CC) $(CFLAGS) -o $(BUILD)/v4_console_linux $(DEMO_SRC)
	@echo "Aufruf: ./$(BUILD)/v4_console_linux [/dev/i2c-N]"

# ------------------------------------------------------------------ #
# Big Endian: derselbe Testlauf unter qemu-m68k (§0)                   #
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
	$(APOLLO_CC) -std=gnu99 $(WARN) $(OPT) -m68080 -m68881 -noixemul \
	    -DREGPARM=4 -DV4_PLAT_AMIGA=1 -I. -I$(I2C_INC) \
	    -o $(BUILD)/v4_console_amiga $(AMIGA_SRC)
	@echo "Fertig: $(BUILD)/v4_console_amiga (m68k-amigaos)"

# Nachweis der Byte-Order-Behandlung im erzeugten m68k-Code: die Helfer und
# alle Frame-Funktionen duerfen nur move.b auf Puffer erzeugen (§0).
# Ersatz fuer den qemu-Lauf, der ohne gcc-m68k-linux-gnu nicht moeglich ist.
asm:
	@sh tools/check_m68k_byteaccess.sh \
	    $(APOLLO_PREFIX)/bin/m68k-amigaos-objdump \
	    $(APOLLO_PREFIX)/bin/m68k-amigaos-gcc

# ------------------------------------------------------------------ #
# Smoketest: das ECHTE Konsolenprogramm gegen den Mock durchlaufen     #
# ------------------------------------------------------------------ #

$(BUILD)/v4_console_smoke: $(LIB_SRC) v4_mock.c v4_console.c \
        tests/smoke_console.c $(HDRS) | $(BUILD)
	$(CC) $(CFLAGS) -DV4_CONSOLE_NO_MAIN -o $@ \
	    $(LIB_SRC) v4_mock.c v4_console.c tests/smoke_console.c

smoke: $(BUILD)/v4_console_smoke
	@fail=0; \
	echo "== Smoke 1: nur Status und Liste, dann Abbruch =="; \
	printf 'q\n' | ./$(BUILD)/v4_console_smoke > $(BUILD)/smoke1.txt 2>&1 || fail=1; \
	grep -q "PING ok: proto=3" $(BUILD)/smoke1.txt || { echo "  FEHLER: PING-Zeile fehlt"; fail=1; }; \
	grep -q "Geraete: 3" $(BUILD)/smoke1.txt || { echo "  FEHLER: Geraeteliste fehlt"; fail=1; }; \
	grep -q "JBL Flip 5" $(BUILD)/smoke1.txt || { echo "  FEHLER: Geraetename fehlt"; fail=1; }; \
	echo "== Smoke 2: verbinden, kein Dateileseauftrag =="; \
	printf '0\n\n' | ./$(BUILD)/v4_console_smoke > $(BUILD)/smoke2.txt 2>&1 || fail=1; \
	grep -q "Verbunden mit Index 0" $(BUILD)/smoke2.txt || { echo "  FEHLER: CONNECT fehlt"; fail=1; }; \
	grep -q "SD_MOUNT: OK" $(BUILD)/smoke2.txt || { echo "  FEHLER: SD_MOUNT fehlt"; fail=1; }; \
	grep -q "total=8000000 kB free=491520 kB" $(BUILD)/smoke2.txt || { echo "  FEHLER: SD_INFO fehlt"; fail=1; }; \
	grep -q "TXT.TXT" $(BUILD)/smoke2.txt || { echo "  FEHLER: Verzeichnisliste fehlt"; fail=1; }; \
	grep -q "DISCONNECT: OK" $(BUILD)/smoke2.txt || { echo "  FEHLER: DISCONNECT fehlt"; fail=1; }; \
	echo "== Smoke 3: Datei wirklich lesen =="; \
	printf '0\nMUSIC/B.MP3\n' | ./$(BUILD)/v4_console_smoke > $(BUILD)/smoke3.txt 2>&1 || fail=1; \
	grep -q "Datei: 37 Byte" $(BUILD)/smoke3.txt || { echo "  FEHLER: Dateigroesse fehlt"; fail=1; }; \
	grep -q "FILE_READ: OK (37 Byte in 1 Bloecken)" $(BUILD)/smoke3.txt || { echo "  FEHLER: Leseergebnis fehlt"; fail=1; }; \
	echo "== Smoke 4: PLAY_FILE / STOP_PLAY =="; \
	printf '0\n\nMUSIC/B.MP3\n\n' | ./$(BUILD)/v4_console_smoke > $(BUILD)/smoke4.txt 2>&1 || fail=1; \
	grep -q "PLAY_FILE: OK" $(BUILD)/smoke4.txt || { echo "  FEHLER: PLAY_FILE fehlt"; fail=1; }; \
	grep -q "STOP_PLAY: OK" $(BUILD)/smoke4.txt || { echo "  FEHLER: STOP_PLAY fehlt"; fail=1; }; \
	grep -q "SD-Wiedergabe" $(BUILD)/smoke4.txt || { echo "  FEHLER: audio_flags SD-Wiedergabe fehlt"; fail=1; }; \
	if [ "$$fail" -eq 0 ]; then echo "Smoke: OK (4 Durchlaeufe)"; else echo "Smoke: FEHLGESCHLAGEN"; exit 1; fi

# Alles, was ohne Hardware pruefbar ist.
verify: lint selftest-lint test test-san smoke asm amiga
	@echo
	@echo "verify: Lint, Tests, Sanitizer, Smoke, m68k-Nachweis und Amiga-Build sind durch."

# ------------------------------------------------------------------ #
# Direkt auf die V4 uebertragen                                        #
#                                                                      #
# Der Weg ist derselbe wie in allen anderen Projekten dieses Repos:    #
# `acp <datei> "<host>:Ram Disk/"` schickt die Datei ueber das Netz     #
# an den ApolloExplorer-Server auf der V4 (kein USB, kein Kabel). Zum   #
# Programm gehoert eine .info-Datei gleichen Namens -- das ist die      #
# Amiga-Ikone.                                                         #
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

# Programm + Ikone auf die V4 legen.
upload: amiga check-upload
	@cp -f $(BUILD)/v4_console_amiga $(BUILD)/$(AMIGA_NAME)
	@cp -f $(ICONS)/ApolloIcon.info $(BUILD)/$(AMIGA_NAME).info
	$(ACP) "$(BUILD)/$(AMIGA_NAME)" "$(APOLLO_V4_HOST)"
	$(ACP) "$(BUILD)/$(AMIGA_NAME).info" "$(APOLLO_V4_HOST)"
	@echo "Auf der V4: $(APOLLO_V4_HOST)$(AMIGA_NAME)"

# Wie im Hausstil zusaetzlich der Debug-Stub und ein Startskript, das den
# Debugger startet (dazu passt launch.json: ${AMIGAHOST}:2345).
upload-debug: amiga check-upload
	@cp -f $(BUILD)/v4_console_amiga $(BUILD)/$(AMIGA_NAME)
	@cp -f $(ICONS)/ApolloIcon.info $(BUILD)/$(AMIGA_NAME).info
	@cp -f $(ICONS)/ApolloIcon_Debug.info $(BUILD)/$(AMIGA_NAME)_Debug.info
	@printf 'bgdbserver %s\n' "$(AMIGA_NAME)" > $(BUILD)/$(AMIGA_NAME)_Debug
	$(ACP) "$(BUILD)/$(AMIGA_NAME)" "$(APOLLO_V4_HOST)"
	$(ACP) "$(BUILD)/$(AMIGA_NAME).info" "$(APOLLO_V4_HOST)"
	$(ACP) "$(BUILD)/$(AMIGA_NAME)_Debug.info" "$(APOLLO_V4_HOST)"
	$(ACP) "$(BUILD)/$(AMIGA_NAME)_Debug" "$(APOLLO_V4_HOST)"
	$(ACP) "$(GDB)" "$(APOLLO_V4_HOST)"

# Nur lesende Hilfen, um die Verbindung zu pruefen.
acp-hosts:
	@$(ACP) --list-hosts

acp-ls: check-host
	@test -x "$(ACP)" || { echo "FEHLER: acp nicht gefunden: $(ACP)"; exit 1; }
	@$(ACP) --list-directory "$(APOLLO_V4_HOST)"

clean:
	@rm -rf $(BUILD)
	@rm -f *.o tests/*.o
	@echo "sauber"
