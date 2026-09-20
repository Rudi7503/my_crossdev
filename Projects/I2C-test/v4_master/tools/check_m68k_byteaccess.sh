#!/bin/sh
#
# check_m68k_byteaccess.sh -- Nachweis der Byte-Order-Behandlung im ERZEUGTEN
# m68k-Code (PROTOCOL_V4_MASTER.md §0).
#
# Warum das hier und nicht nur der Quelltext-Lint: der Linux-Harness ist
# Little Endian, ein Byte-Order-Fehler waere dort unsichtbar. Ein Lauf auf
# echter Big-Endian-Hardware (qemu-m68k, Makefile-Ziel `test-m68k`) ist in
# einer Umgebung mit `no_new_privs` nicht moeglich. Ersatzweise wird hier
# geprueft, was der Compiler fuer m68k tatsaechlich erzeugt.
#
# WAS STRIKT GEPRUEFT WIRD (Fehler => Exit 1):
#   1. Helfer-Probe: v4p_put_u16le/u32le und v4p_get_u16le/u32le duerfen
#      ausschliesslich move.b auf den Puffer erzeugen, in beide Richtungen.
#   2. Frame-Bauer v4p_build_write/build_read/build_bulk: kein breiter Store
#      (move.w/move.l/movem) in einen Pufferregister-Ausdruck, auch nicht mit
#      Displacement (8(a1)). Nullfuellen per clr.l ist erlaubt -- Nullen sind
#      in jeder Byte-Reihenfolge identisch.
#   3. Gegenproben, damit der Nachweis nicht vakuant ist:
#      (a) die Disassembly muss lang genug sein und move.b-Zugriffe enthalten,
#      (b) zwei absichtlich falsche Probefunktionen (breiter Store bzw. Load
#          mit Displacement) MUESSEN von genau den Mustern aus (1)/(2)
#          erkannt werden.
#
# WAS NUR BERICHTET WIRD (kein Fehlerkriterium):
#   Die Prueffunktionen v4p_check_read/check_bulk lesen den Frame byteweise
#   und schreiben in eine NATIVE Ausgabestruktur (v4p_read_t / v4p_bulk_t).
#   Breite Stores in diese Struktur sind korrekt und kein Byte-Order-Problem.
#   Eine automatische Registeranalyse kann Frame- und Ausgaberegister hier
#   nicht zuverlaessig trennen: der Compiler verwendet dasselbe Register
#   sowohl fuer den Argumentzeiger (breite Zugriffe) als auch fuer einen vom
#   Frame abgeleiteten Zeiger (Byte-Zugriffe). Fuer die Leserichtung ruht der
#   Nachweis deshalb auf (1) den identischen, hier bewiesenen Inline-Helfern
#   und (2) dem Quelltext-Lint, der jeden anderen Frame-Zugriff verbietet.
#
# Aufruf: sh tools/check_m68k_byteaccess.sh <objdump> <cc>
#
set -u

OD="${1:?objdump-Pfad fehlt}"
CC="${2:?Compiler-Pfad fehlt}"

WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT
fail=0

# Stack-Zugriffe (a5 = Frame, a7/sp = Stack) sind keine Pufferzugriffe.
REGS='a[0-46]'
# Breiter Zugriff auf einen Pufferregister-Ausdruck, MIT Displacement:
#   move.w d0,(a1) | move.w d0,8(a1) | move.l (a0),d1 | movem.l d0-d1,(a1)
WIDE_ANY="(move\\.(w|l)|movem\\.(w|l))[[:space:]]+.*\\(${REGS}\\)"
# Nur die STORE-Richtung (Register vor dem Komma).
WIDE_STORE="move\\.(w|l)[[:space:]]+[^,]+,.*\\(${REGS}\\)"

echo "== 1. Helfer-Probe (v4p_put/get_u16le/u32le) =="
cat > "$WORK/probe.c" <<'EOF'
#include "v4_proto.h"
void probe_put(unsigned char *p, unsigned v, unsigned long w)
{
    v4p_put_u16le(p, (uint16_t)v);
    v4p_put_u32le(p, (uint32_t)w);
}
unsigned probe_get(const unsigned char *p)
{
    return (unsigned)v4p_get_u16le(p) + (unsigned)v4p_get_u32le(p + 2);
}
/* Absichtliche Verstoesse -- nur als Gegenprobe fuer die Muster. */
void probe_bad_wide_store(unsigned char *p, unsigned v)
{
    *(uint32_t *)(void *)(p + 8) = v;
}
unsigned probe_bad_wide_load(const unsigned char *p)
{
    return *(const uint32_t *)(const void *)(p + 4);
}
EOF

if ! "$CC" -std=gnu99 -O2 -m68020 -m68881 -noixemul -I. \
        -c "$WORK/probe.c" -o "$WORK/probe.o" 2> "$WORK/cc.log"; then
    echo "  FEHLER: Probe liess sich nicht uebersetzen"
    sed 's/^/    /' "$WORK/cc.log"
    exit 1
fi

# Ermittelt die Register, die in einer Funktion per move.b als Puffer dienen.
buffer_regs() {
    "$OD" -d --disassemble="$1" "$WORK/probe.o" | tail -n +5 \
        | grep -oE 'move\.b[[:space:]]+[^,]*[,(]a[0-7]' \
        | grep -oE 'a[0-7]' | sort -u | tr '\n' '|' | sed 's/|$//'
}

# disasm <objdatei> <symbol>
disasm() {
    "$OD" -d --disassemble="$2" "$1" | tail -n +5 > "$WORK/d.txt"
}

for sym in _probe_put _probe_get; do
    disasm "$WORK/probe.o" "$sym"
    insn=$(grep -cE '^[[:space:]]+[0-9a-f]+:' "$WORK/d.txt")
    byte=$(grep -cE "move\\.b.*\\(${REGS}\\)" "$WORK/d.txt")
    regs=$(buffer_regs "$sym")
    [ -n "$regs" ] || regs="$REGS"
    wide=$(grep -cE "(move\\.(w|l)|movem\\.(w|l))[[:space:]]+.*\\(${regs}\\)" "$WORK/d.txt")
    printf '  %-12s Instruktionen=%-4s move.b auf Puffer=%-3s breite Zugriffe(%s)=%s\n' \
        "$sym" "$insn" "$byte" "$regs" "$wide"
    if [ "$insn" -lt 5 ]; then
        echo "    FEHLER: Disassembly leer -- Test waere wertlos"
        fail=1
    fi
    if [ "$byte" -lt 2 ]; then
        echo "    FEHLER: keine Byte-Zugriffe gefunden"
        fail=1
    fi
    if [ "$wide" -ne 0 ]; then
        echo "    FEHLER: 16/32-Bit-Zugriff auf den Puffer:"
        grep -E "(move\\.(w|l)|movem\\.(w|l))[[:space:]]+.*\\(${regs}\\)" "$WORK/d.txt" \
            | sed 's/^/      /'
        fail=1
    fi
done

echo "== 1b. Gegenprobe: die Muster MUESSEN bekannte Verstoesse finden =="
control() {
    sym="$1"
    pat="$2"
    desc="$3"
    disasm "$WORK/probe.o" "$sym"
    if grep -qE "$pat" "$WORK/d.txt"; then
        echo "  ok: $desc wird erkannt"
    else
        echo "  FEHLER: $desc wurde NICHT erkannt -- das Muster ist zahnlos"
        sed 's/^/      /' "$WORK/d.txt"
        fail=1
    fi
}
control _probe_bad_wide_store "$WIDE_STORE" "breiter Store mit Displacement"
control _probe_bad_wide_load  "$WIDE_ANY"   "breiter Load mit Displacement"

echo "== 2. Echte Frame-Funktionen aus v4_proto.c =="
if ! "$CC" -std=gnu99 -O2 -m68020 -m68881 -noixemul -I. \
        -c v4_proto.c -o "$WORK/proto.o" 2> "$WORK/cc2.log"; then
    echo "  FEHLER: v4_proto.c liess sich nicht uebersetzen"
    sed 's/^/    /' "$WORK/cc2.log"
    exit 1
fi

echo "  -- Frame-Bauer (strikt: keine breiten Stores):"
for sym in _v4p_build_write _v4p_build_read _v4p_build_bulk; do
    disasm "$WORK/proto.o" "$sym"
    insn=$(grep -cE '^[[:space:]]+[0-9a-f]+:' "$WORK/d.txt")
    byte=$(grep -cE "move\\.b.*\\(${REGS}\\)" "$WORK/d.txt")
    wide_store=$(grep -E "$WIDE_STORE" "$WORK/d.txt" | grep -vc 'clr')
    printf '  %-18s Instruktionen=%-4s move.b auf Puffer=%-3s breite Stores=%s\n' \
        "$sym" "$insn" "$byte" "$wide_store"
    if [ "$insn" -lt 20 ]; then
        echo "    FEHLER: Disassembly zu kurz -- Test waere wertlos"
        fail=1
    fi
    if [ "$byte" -lt 4 ]; then
        echo "    FEHLER: keine Byte-Zugriffe auf den Puffer gefunden"
        fail=1
    fi
    if [ "$wide_store" -ne 0 ]; then
        echo "    FEHLER: 16/32-Bit-Store in einen Puffer:"
        grep -E "$WIDE_STORE" "$WORK/d.txt" | grep -v clr | sed 's/^/      /'
        fail=1
    fi
done

echo "  -- Prueffunktionen (byteweiser Frame-Zugriff strikt, Rest Bericht):"
for sym in _v4p_check_read _v4p_check_bulk; do
    disasm "$WORK/proto.o" "$sym"
    insn=$(grep -cE '^[[:space:]]+[0-9a-f]+:' "$WORK/d.txt")
    byte=$(grep -cE "move\\.b.*\\(${REGS}\\)" "$WORK/d.txt")
    wide=$(grep -cE "$WIDE_ANY" "$WORK/d.txt")
    printf '  %-18s Instruktionen=%-4s move.b auf Puffer=%-3s breite Zugriffe=%s (Ausgabestruktur)\n' \
        "$sym" "$insn" "$byte" "$wide"
    if [ "$insn" -lt 20 ]; then
        echo "    FEHLER: Disassembly zu kurz -- Test waere wertlos"
        fail=1
    fi
    if [ "$byte" -lt 4 ]; then
        echo "    FEHLER: keine Byte-Zugriffe auf den Puffer gefunden"
        fail=1
    fi
done

echo "== 3. Nullfuellen per clr.* ist erlaubt (endian-neutral) =="
disasm "$WORK/proto.o" _v4p_build_write
echo "  clr-Zugriffe in _v4p_build_write: $(grep -cE "clr\\.(l|w)[[:space:]]+\\(${REGS}\\)" "$WORK/d.txt")"

if [ "$fail" -eq 0 ]; then
    echo "Ergebnis: Frame-Bauer schreiben ausschliesslich byteweise; Helfer byteweise in beide Richtungen"
else
    echo "Ergebnis: VERSTOSS GEFUNDEN"
fi
exit $fail
