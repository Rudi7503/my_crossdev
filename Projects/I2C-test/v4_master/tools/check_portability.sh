#!/bin/sh
#
# check_portability.sh -- erzwingt die Portabilitaetsregeln aus
# PROTOCOL_V4_MASTER.md §0 und §2 fuer den Code, der auf der V4 laeuft.
#
# Geprueft wird der Protokollcode (v4_proto.*, v4_master.*) und der Mock
# (v4_mock.c, der Frames baut). Die beiden OS-Plattformschichten werden nur
# auf Casts auf mehrbyte Integerzeiger geprueft: sie reichen nur Puffer durch
# und enthalten fuer die Amiga-API unvermeidbare (struct IORequest *)-Casts.
#
# Regeln:
#   1. Jeder Cast auf einen Zeiger, dessen Zieltyp KEIN Byte-Typ ist, ist
#      verboten. Damit werden auch typedefs erfasst (u32, word, size_t,
#      uintptr_t, frame_t ...) -- nicht nur die Standardnamen.
#   2. union-Zugriffe auf Frame-Bytes sind verboten (dieselbe Gefahr).
#   3. memcpy/memmove auf Frame-Bytes ist verboten; Feld fuer Feld ueber die
#      v4p_*_le-Helfer.
#   4. packed-Attribut, #pragma pack und Bitfelder sind verboten.
#   5. Handgeschriebenes Byte-Swapping (htobe32, bswap, ...) ist verboten --
#      dafuer gibt es v4p_put_*_le / v4p_get_*_le.
#
# Kommentare werden vor dem Scan entfernt, weil sie die Verbote beschreiben.
#
# Aufruf:  sh tools/check_portability.sh [--selftest]
# Exit:    0 = sauber, 1 = Verstoss oder Pruefwerkzeug defekt
#
set -u

PROTO_FILES="v4_proto.h v4_proto.c v4_master.h v4_master.c"
MOCK_FILE="v4_mock.c"
PLAT_FILES="v4_linux_i2c.c v4_amiga_i2c.c"
fail=0

# Jeder Cast auf einen Zeigertyp ...
CAST_RE='(\([[:space:]]*(const[[:space:]]+|volatile[[:space:]]+|unsigned[[:space:]]+|signed[[:space:]]+)*[A-Za-z_][A-Za-z_0-9]*[[:space:]]*\*+[[:space:]]*\)|\([[:space:]]*(struct|union|enum)[[:space:]]+[A-Za-z_][A-Za-z_0-9]*[[:space:]]*\*+[[:space:]]*\))'
# ... AUSSER auf einen Byte-Typ (dort ist der Zugriff byteweise).
CAST_OK='\([[:space:]]*(const[[:space:]]+|volatile[[:space:]]+)*(uint8_t|char|UBYTE|void|unsigned[[:space:]]+char)[[:space:]]*\*'

# Nur mehrbyte Integerzeiger -- fuer die OS-Plattformschichten.
PLAT_RE='\((u?int(16|32|64)_t|intptr_t|uintptr_t|size_t|ptrdiff_t|short|int|long|word|u32|u16|u64|LONG|UWORD|ULONG|WORD)[[:space:]]*\*\)'

UNION_RE='\bunion\b'
MEM_RE='\bmem(cpy|move)\b'
PACK_RE='(__attribute__[[:space:]]*\(\([[:space:]]*packed|#[[:space:]]*pragma[[:space:]]+pack)'
# Bitfeld: Typschluesselwort + Name + ":" + Zahl + ";"
BIT_RE='(unsigned|signed|int|char|short|long|uint8_t|uint16_t|uint32_t|uint64_t|UBYTE|UWORD|ULONG)[[:space:]]+[A-Za-z_][A-Za-z_0-9]*[[:space:]]*:[[:space:]]*[0-9]+[[:space:]]*;'
SWAP_RE='\b(htobe(16|32|64)|htole(16|32|64)|be(16|32|64)toh|le(16|32|64)toh|bswap(16|32|64)?|__builtin_bswap(16|32|64))\b'

WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT

# Kommentare entfernen; Zeilennummern bleiben erhalten.
strip_comments() {
    awk '
    BEGIN { inblock = 0 }
    {
        line = $0
        out = ""
        i = 1
        n = length(line)
        while (i <= n) {
            two = substr(line, i, 2)
            if (inblock) {
                if (two == "*/") { inblock = 0; i += 2 } else { i++ }
                continue
            }
            if (two == "/*") { inblock = 1; i += 2; continue }
            if (two == "//") { break }
            out = out substr(line, i, 1)
            i++
        }
        print out
    }' "$1"
}

# Erlaubte Byte-Casts vor dem Scan neutralisieren. Wichtig: NICHT zeilenweise
# ausblenden -- sonst entkaeme ein verbotener typedef-Cast, der in derselben
# Zeile wie ein erlaubter Byte-Cast steht.
neutralize() {
    sed -E 's/\([[:space:]]*(const[[:space:]]+|volatile[[:space:]]+)*(uint8_t|UBYTE|char|void|unsigned[[:space:]]+char)[[:space:]]*\*/@BYTECAST@/g' "$1"
}

# $1 = Muster, $2 = Beschreibung, $3 = optionales Allow-Muster, $4.. = Dateien
report() {
    pat="$1"
    desc="$2"
    allow="$3"
    shift 3
    for f in "$@"; do
        if [ ! -f "$f" ]; then
            echo "  FEHLER: $f fehlt -- Pruefung nicht moeglich"
            fail=1
            continue
        fi
        strip_comments "$f" > "$WORK/stripped.c"
        if [ ! -s "$WORK/stripped.c" ]; then
            echo "  FEHLER: $f liess sich nicht lesen (Kommentarfilter leer)"
            fail=1
            continue
        fi
        src_lines=$(wc -l < "$f")
        st_lines=$(wc -l < "$WORK/stripped.c")
        if [ "$src_lines" != "$st_lines" ]; then
            echo "  FEHLER: Kommentarfilter verlor Zeilen in $f ($src_lines -> $st_lines)"
            fail=1
        fi

        if [ -n "$allow" ]; then
            neutralize "$WORK/stripped.c" > "$WORK/neutralized.c"
            grep -nE "$pat" "$WORK/neutralized.c" > "$WORK/hits" || true
        else
            grep -nE "$pat" "$WORK/stripped.c" > "$WORK/hits" || true
        fi
        if [ -s "$WORK/hits" ]; then
            echo "  VERBOTEN in $f: $desc"
            # Die ORIGINALZEILE ausgeben, nicht die neutralisierte Fassung.
            cut -d: -f1 "$WORK/hits" | while read -r ln; do
                printf '    %s:%s: %s\n' "$f" "$ln" \
                    "$(sed -n "${ln}p" "$WORK/stripped.c")"
            done
            fail=1
        fi
    done
}

scan_proto() {
    report "$CAST_RE" "Cast auf einen Zeiger, dessen Zieltyp kein Byte-Typ ist (statt v4p_*_le)" "$CAST_OK" "$@"
    report "$UNION_RE" "union ueber Frame-Bytes" "" "$@"
    report "$MEM_RE"  "memcpy/memmove auf Frame-Bytes (statt Feld fuer Feld)" "" "$@"
    report "$PACK_RE" "packed-Attribut oder #pragma pack" "" "$@"
    report "$BIT_RE"  "Bitfeld in einer Struktur" "" "$@"
    report "$SWAP_RE" "handgeschriebenes Byte-Swapping (statt v4p_*_le)" "" "$@"
}

# Im Mock gilt der memcpy-Bann nicht: er kopiert ausschliesslich Byte-Felder
# (Pfade, Namen, Dateidaten, Schluessel), und seine Frames entstehen
# ausschliesslich ueber die streng geprueften v4p_build_*-Funktionen. Alle
# anderen Regeln gelten auch hier.
scan_mock() {
    report "$CAST_RE" "Cast auf einen Zeiger, dessen Zieltyp kein Byte-Typ ist (statt v4p_*_le)" "$CAST_OK" "$@"
    report "$UNION_RE" "union ueber Frame-Bytes" "" "$@"
    report "$PACK_RE" "packed-Attribut oder #pragma pack" "" "$@"
    report "$BIT_RE"  "Bitfeld in einer Struktur" "" "$@"
    report "$SWAP_RE" "handgeschriebenes Byte-Swapping (statt v4p_*_le)" "" "$@"
}

if [ "${1:-}" = "--selftest" ]; then
    W="$WORK"
    printf 'void f(uint8_t *p) { uint16_t v = *(uint16_t *)p; (void)v; }\n' > "$W/bad_std_cast.c"
    printf 'typedef unsigned int u32;\nvoid f(uint8_t *p) { u32 v = *(u32 *)p; (void)v; }\n' > "$W/bad_typedef_cast.c"
    printf 'typedef struct frame frame_t;\nvoid f(uint8_t *p) { frame_t *q = (frame_t *)(void *)(p + 4); (void)q; }\n' > "$W/bad_mixed.c"
    printf 'typedef struct frame frame_t;\nvoid f(uint8_t *p) { frame_t *q = (frame_t *)p; (void)q; }\n' > "$W/bad_struct_cast.c"
    printf 'union u { uint32_t w; uint8_t b[4]; };\nvoid f(union u *x) { (void)x; }\n' > "$W/bad_union.c"
    printf 'void g(uint8_t *d, const uint8_t *s) { memcpy(d, s, 32); }\n' > "$W/bad_memcpy.c"
    printf 'struct s { unsigned a : 3; };\n' > "$W/bad_bitfield.c"
    printf 'struct __attribute__((packed)) t { uint32_t x; };\n' > "$W/bad_packed.c"
    printf '#include <endian.h>\nuint32_t f(uint32_t v) { return htobe32(v); }\n' > "$W/bad_swap.c"
    printf 'void h(uint8_t *d, uint16_t v) { v4p_put_u16le(d, v); }\n' > "$W/good.c"
    printf 'int f(int x) { return x ? 7 : 1; }\nint g(int y) { switch (y) { case 5: return 1; default: return 0; } }\n' > "$W/good_ternary.c"
    printf '/* *(uint32_t *)p und memcpy sind hier nur Text */\nvoid k(void) {}\n' > "$W/good_comment.c"
    printf 'void m(uint8_t *d, const char *s) { (void)d; (void)s; }\nunsigned char *n(unsigned char *p) { return p; }\n' > "$W/good_byte_casts.c"

    detect() {
        strip_comments "$1" > "$W/stripped.c"
        grep -qE "$CAST_RE|$UNION_RE|$MEM_RE|$PACK_RE|$BIT_RE|$SWAP_RE" "$W/stripped.c"
    }
    detect_allowed() {
        strip_comments "$1" > "$W/stripped.c"
        neutralize "$W/stripped.c" > "$W/neutralized.c"
        grep -qE "$CAST_RE" "$W/neutralized.c"
    }

    echo "Selbsttest des Lints:"
    for f in bad_std_cast bad_typedef_cast bad_struct_cast bad_mixed bad_union \
             bad_memcpy bad_bitfield bad_packed bad_swap; do
        if detect "$W/$f.c"; then
            echo "  ok: $f.c wird erkannt"
        else
            echo "  FEHLER: $f.c wurde NICHT erkannt"
            fail=1
        fi
    done
    for f in good good_ternary good_comment good_byte_casts; do
        if detect "$W/$f.c"; then
            echo "  FEHLER: $f.c wurde faelschlich beanstandet"
            fail=1
        else
            echo "  ok: $f.c bleibt sauber"
        fi
    done
    # Byte-Casts muessen auch bei der Allow-Logik durchgehen
    if detect_allowed "$W/good_byte_casts.c"; then
        echo "  FEHLER: Byte-Casts wurden faelschlich als Verstoss gewertet"
        fail=1
    else
        echo "  ok: Byte-Casts bleiben erlaubt"
    fi
    for f in bad_typedef_cast bad_mixed; do
        if detect_allowed "$W/$f.c"; then
            echo "  ok: $f.c wird auch mit Allow-Logik erkannt"
        else
            echo "  FEHLER: $f.c entkam der Allow-Logik"
            fail=1
        fi
    done
    exit $fail
fi

echo "Portabilitaets-Lint (PROTOCOL_V4_MASTER.md §0/§2)"
echo "Streng geprueft (alle Regeln inkl. memcpy-Bann): $PROTO_FILES"
scan_proto $PROTO_FILES

echo "Mock (alle Regeln ausser memcpy-Bann): $MOCK_FILE"
scan_mock $MOCK_FILE

echo "Plattformschichten (nur Mehrbyte-Integerzeiger): $PLAT_FILES"
report "$PLAT_RE" "Cast auf einen 16/32/64-Bit-Zeiger" "" $PLAT_FILES

# Nachweis, dass wirklich etwas gescannt wurde.
total=0
for f in $PROTO_FILES $MOCK_FILE $PLAT_FILES; do
    [ -f "$f" ] || continue
    n=$(strip_comments "$f" | wc -l)
    total=$((total + n))
done
echo "Geprueft: $total Codezeilen (ohne Kommentare)"
if [ "$total" -lt 1000 ]; then
    echo "  FEHLER: unplausibel wenig Code gescannt"
    fail=1
fi

if [ "$fail" -eq 0 ]; then
    echo "Ergebnis: sauber"
else
    echo "Ergebnis: VERSTOSS GEFUNDEN"
fi
exit $fail
