/*
 * test_main.c -- Einstiegspunkt des Testbinarys.
 *
 * Ein einziges Binary enthaelt ALLE Suiten. Das ist Absicht: es wird nativ
 * (Little Endian) und -- sobald gcc-m68k-linux-gnu + qemu-user-static
 * installiert sind -- unter qemu-m68k (Big Endian) gestartet. Damit prueft
 * derselbe Lauf die Byte-Order-Behandlung auf beiden Architekturen (§0).
 */

#include "v4_test.h"

#include "../v4_proto.h"

int         v4_test_checks;
int         v4_test_failures;
int         v4_test_cases;
const char *v4_test_case_name = "(init)";

void v4_test_begin(const char *suite)
{
    printf("== %s ==\n", suite);
}

void v4_test_case(const char *name)
{
    v4_test_case_name = name;
    v4_test_cases++;
    printf(" - %s\n", name);
}

/* Nur zur Diagnose: die Byte-Reihenfolge der Wirtsmaschine. */
static const char *host_endian(void)
{
    uint32_t              v = 1u;
    const unsigned char  *p = (const unsigned char *)&v;

    return (p[0] != 0u) ? "little" : "big";
}

int main(void)
{
    printf("v4_master Testsuite -- PROTOCOL_V4_MASTER.md v2.0\n");
    printf("Wirt: %s Endian, sizeof(int)=%u sizeof(long)=%u sizeof(void*)=%u\n",
           host_endian(), (unsigned)sizeof(int), (unsigned)sizeof(long),
           (unsigned)sizeof(void *));

    /* §0: muss auf beiden Architekturen 1 ergeben. */
    if (v4p_selftest_byteorder() != 1) {
        printf("ABBruch: Byte-Order-Selbsttest fehlgeschlagen\n");
        return 2;
    }
    printf("Selbsttest Byte-Order: OK\n");

    /* Zusatzdiagnose: auf Big Endian MUSS das native Layout eines u16 vom
     * Leitungslayout abweichen -- sonst waere der Selbsttest wertlos. */
    {
        uint16_t             native = 0x0102u;
        uint8_t              le[2];
        const unsigned char *p = (const unsigned char *)&native;

        v4p_put_u16le(le, native);
        printf("u16 nativ: %02X %02X | Leitung (LE): %02X %02X\n",
               (unsigned)p[0], (unsigned)p[1],
               (unsigned)le[0], (unsigned)le[1]);
        if (host_endian()[0] == 'b'
            && p[0] == le[0] && p[1] == le[1]) {
            printf("FEHLER: Big-Endian-Wirt mit identischem Layout "
                   "-- Helfer pruefen!\n");
            return 2;
        }
    }

    (void)test_proto();
    (void)test_master();
    (void)test_session();
    (void)test_fuzz();

    printf("\n-----------------------------------------------\n");
    printf("Faelle : %d\n", v4_test_cases);
    printf("Checks : %d\n", v4_test_checks);
    printf("Fehler : %d\n", v4_test_failures);
    printf("-----------------------------------------------\n");

    if (v4_test_failures == 0) {
        printf("ERGEBNIS: OK\n");
        return 0;
    }
    printf("ERGEBNIS: FEHLGESCHLAGEN\n");
    return 1;
}
