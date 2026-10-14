/*
 * test_joyport.c -- die direkte Abfrage der Joystick-Anschluesse (v4_joyport.c)
 * auf dem WIRT.
 *
 * Warum diese Suite: die Dekodierung der V4-Register ist die eine Stelle, an
 * der eine falsche Maske sofort eine falsche Aussage erzeugt -- und zwar eine,
 * die auf dem Schirm wie eine Messung aussieht. Deshalb steht die Rechnung in
 * einer eigenen Datei hinter EINEM Registerhaken (v4_joy_set_lesen), und hier
 * wird sie ohne Hardware, ohne OS und ohne Bus geprueft:
 *
 *   - die nachgemessenen Einzelrichtungen $0002/$0003 (RECHTS), $0300 (LINKS),
 *     $0100 (VOR), $0001 (ZURUECK) und die Ruhe $0000,
 *   - Diagonalen ueber beide Achsen (z.B. $0103 -> VOR+RECHTS),
 *   - das Stabilitaetstor: zwei GLEICHE Abtastungen werden uebernommen, zwei
 *     UNGLEICHE verworfen (der letzte gueltige Stand bleibt stehen),
 *   - die Feuerknoepfe gegen den gemerkten GRUNDZUSTAND (nicht hart verdrahtet)
 *     samt beiden Polaritaeten -- BTN1 (Pin 6), BTN2 (Pin 9) und BTN3 (Pin 5,
 *     POTINP Bit 9/11),
 *   - die Vergleichszeilen (OK / ABWEICHUNG, BTN3 MIT verglichen) und die
 *     Rohwertzeile mit vier Hexziffern.
 *
 * Was hier NICHT geprueft wird -- das braucht die V4: dass die vier Register
 * auf den beiden DB9-Buchsen wirklich das tun, was der Mitschnitt vom 13.10.
 * zeigt. tests/ kann nur die Rechnung pruefen, nicht die Hardware.
 */

#include "v4_test.h"

#include "../v4_joyport.h"

#include <string.h>

/* ------------------------------------------------------------------ */
/* Hilfen                                                              */
/* ------------------------------------------------------------------ */

static v4_joy_roh_t roh(uint16_t joy0, uint16_t joy1, uint8_t cia,
                        uint16_t pot)
{
    v4_joy_roh_t r;

    r.joy0dat = joy0;
    r.joy1dat = joy1;
    r.ciaapra = cia;
    r.potinp  = pot;
    return r;
}

/* Zwei GLEICHE Abtastungen: das Stabilitaetstor muss uebernehmen. */
static void stabil(v4_joy_t *st, const v4_joy_roh_t *r)
{
    v4_joy_update(st, r, r);
}

/* Der Grundzustand der Feuerleitungen aus dem Auftrag: CIAAPRA=$00F4 (Bit 6
 * und 7 High = losgelassen), POTINP=$FF02 (Bit 8 und 10 High = losgelassen). */
#define CIA_BASIS  0xF4u
#define POT_BASIS  0xFF02u

/* ------------------------------------------------------------------ */
/* Dekodierung: die nachgemessenen Werte                               */
/* ------------------------------------------------------------------ */

static void test_joy_ruhe(void)
{
    v4_test_case("Ruhe: $0000 ergibt keine Richtung, Text ist '-'");
    CHECK_EQ(v4_joy_richtungen(0x0000u), 0x00);
    CHECK_EQ(v4_joy_richtungen(0x0000u) & V4P_IN_ST_MASK, 0x00);
    {
        char t[64];

        (void)v4_joy_richtungen_text(0x00u, t, sizeof(t));
        CHECK_STR(t, "-");
    }
}

static void test_joy_rechts(void)
{
    char t[64];

    v4_test_case("RECHTS: $0002 und der nachgemessene Wert $0003");
    CHECK_EQ(v4_joy_richtungen(0x0002u), (long)V4P_IN_ST_RIGHT);
    /* $0003 ist der am Geraet GEMESSENE Wert beim Druecken von RECHTS: der
     * Zaehler steht auf 11. Er darf NICHT zusaetzlich "zurueck" ergeben --
     * sonst meldete jede Rechtsabtastung eine Abweichung. */
    CHECK_EQ(v4_joy_richtungen(0x0003u), (long)V4P_IN_ST_RIGHT);

    (void)v4_joy_richtungen_text(0x08u, t, sizeof(t));
    CHECK_STR(t, "RECHTS");
}

static void test_joy_links(void)
{
    char t[64];

    v4_test_case("LINKS: $0300 (nachgemessen) und der Zwischenwert $0200");
    CHECK_EQ(v4_joy_richtungen(0x0300u), (long)V4P_IN_ST_LEFT);
    /* $0200 ist Y = 10: ebenfalls LINKS, aber KEIN zusaetzliches VOR. Die
     * XOR-Formel des Auftrags wuerde hier "VOR+LINKS" melden. */
    CHECK_EQ(v4_joy_richtungen(0x0200u), (long)V4P_IN_ST_LEFT);

    (void)v4_joy_richtungen_text(0x04u, t, sizeof(t));
    CHECK_STR(t, "LINKS");
}

static void test_joy_vor(void)
{
    char t[64];

    v4_test_case("VOR: $0100 ergibt genau VOR (nicht VOR+ZURUECK)");
    CHECK_EQ(v4_joy_richtungen(0x0100u), (long)V4P_IN_ST_UP);

    (void)v4_joy_richtungen_text(0x01u, t, sizeof(t));
    CHECK_STR(t, "VOR");
}

static void test_joy_zurueck(void)
{
    char t[64];

    v4_test_case("ZURUECK: $0001 -- der Wert, den die Auftragsformel verliert");
    CHECK_EQ(v4_joy_richtungen(0x0001u), (long)V4P_IN_ST_DOWN);

    (void)v4_joy_richtungen_text(0x02u, t, sizeof(t));
    CHECK_STR(t, "ZURUECK");
}

static void test_joy_diagonale(void)
{
    char t[64];

    v4_test_case("Diagonale: je eine Richtung pro Achse, beide Achsen");
    /* VOR + RECHTS: Y = 01, X = 11 (der gemessene Rechtswert). */
    CHECK_EQ(v4_joy_richtungen(0x0102u),
             (long)(V4P_IN_ST_UP | V4P_IN_ST_RIGHT));
    CHECK_EQ(v4_joy_richtungen(0x0103u),
             (long)(V4P_IN_ST_UP | V4P_IN_ST_RIGHT));
    /* ZURUECK + LINKS: X = 01, Y = 11. */
    CHECK_EQ(v4_joy_richtungen(0x0301u),
             (long)(V4P_IN_ST_DOWN | V4P_IN_ST_LEFT));
    CHECK_EQ(v4_joy_richtungen(0x0201u),
             (long)(V4P_IN_ST_DOWN | V4P_IN_ST_LEFT));
    /* Gegenueberliegende Achsen: VOR + ZURUECK ist darstellbar. */
    CHECK_EQ(v4_joy_richtungen(0x0101u),
             (long)(V4P_IN_ST_UP | V4P_IN_ST_DOWN));
    /* Innerhalb EINER Achse gewinnt das hohe Bit: X = 11 ist RECHTS, Y = 11
     * ist LINKS -- "unten+rechts" bzw. "oben+links" sind direkt nicht
     * trennbar (siehe v4_joyport.h). */
    CHECK_EQ(v4_joy_richtungen(0x0302u),
             (long)(V4P_IN_ST_LEFT | V4P_IN_ST_RIGHT));
    CHECK_EQ(v4_joy_richtungen(0x0303u),
             (long)(V4P_IN_ST_LEFT | V4P_IN_ST_RIGHT));
    CHECK_EQ(v4_joy_richtungen(0xFFFFu),
             (long)(V4P_IN_ST_LEFT | V4P_IN_ST_RIGHT));

    (void)v4_joy_richtungen_text((uint8_t)(V4P_IN_ST_UP | V4P_IN_ST_RIGHT),
                                 t, sizeof(t));
    CHECK_STR(t, "VOR+RECHTS");
    (void)v4_joy_richtungen_text(0x0Au, t, sizeof(t));
    CHECK_STR(t, "ZURUECK+RECHTS");
}

/* ------------------------------------------------------------------ */
/* Stabilitaetstor                                                     */
/* ------------------------------------------------------------------ */

static void test_joy_stabil(void)
{
    v4_joy_t     st;
    v4_joy_roh_t r = roh(0x0000u, 0x0100u, CIA_BASIS, POT_BASIS);

    v4_test_case("Stabilitaet: zwei gleiche Abtastungen werden uebernommen");
    v4_joy_init(&st);
    stabil(&st, &r);

    CHECK_EQ(st.richtung_stabil[1], 1u);
    CHECK_EQ(st.richtung_gueltig[1], 1u);
    CHECK_EQ(st.zaehler[0], 0x0000u);
    CHECK_EQ(st.zaehler[1], 0x0100u);
    CHECK_EQ(v4_joy_maske(&st, 1), (long)V4P_IN_ST_UP);
    CHECK_EQ(v4_joy_maske(&st, 0), 0x00);
    CHECK_EQ(st.verworfen, 0ul);
    CHECK_EQ(st.roh.joy1dat, 0x0100u);
    CHECK_EQ(st.basis_ok, 1u);
    CHECK_EQ(st.basis_roh.ciaapra, CIA_BASIS);
    CHECK_EQ(st.basis_roh.potinp, POT_BASIS);
}

static void test_joy_verworfen(void)
{
    v4_joy_t     st;
    v4_joy_roh_t gut   = roh(0x0000u, 0x0100u, CIA_BASIS, POT_BASIS);
    v4_joy_roh_t a     = roh(0x0000u, 0x0100u, CIA_BASIS, POT_BASIS);
    v4_joy_roh_t b     = roh(0x0000u, 0x0102u, CIA_BASIS, POT_BASIS);

    v4_test_case("Verwerfen: ungleiche Abtastungen -- letzter gueltiger Wert "
                 "bleibt stehen");
    v4_joy_init(&st);
    stabil(&st, &gut);
    CHECK_EQ(v4_joy_maske(&st, 1), (long)V4P_IN_ST_UP);

    v4_joy_update(&st, &a, &b);         /* Zaehler springt zwischen den beiden */

    CHECK_EQ(st.richtung_stabil[1], 0u);
    CHECK_EQ(st.verworfen, 1ul);
    /* Der letzte GUELTIGE Stand bleibt: die Anzeige flackert nicht. */
    CHECK_EQ(st.zaehler[1], 0x0100u);
    CHECK_EQ(v4_joy_maske(&st, 1), (long)V4P_IN_ST_UP);
    CHECK_EQ(st.richtung_gueltig[1], 1u);
    /* Der Rohwert der ersten Abtastung steht trotzdem da -- man soll sehen,
     * WAS der Zaehler gerade sagt. */
    CHECK_EQ(st.roh.joy1dat, 0x0100u);

    /* Buchse 0 ist davon unberuehrt: je Buchse ein eigenes Tor. */
    CHECK_EQ(st.richtung_stabil[0], 1u);
    CHECK_EQ(st.verworfen, 1ul);

    /* Und ein einzelner Sprung VOR der ersten gueltigen Abtastung: es gibt
     * nichts zu halten, "gueltig" bleibt 0. */
    v4_joy_init(&st);
    v4_joy_update(&st, &a, &b);
    CHECK_EQ(st.richtung_gueltig[1], 0u);
    CHECK_EQ(v4_joy_maske(&st, 1), 0x00);
    CHECK_EQ(st.richtung_stabil[1], 0u);
}

/* ------------------------------------------------------------------ */
/* Feuerknoepfe gegen den gemerkten Grundzustand                       */
/* ------------------------------------------------------------------ */

static void test_joy_feuer_grundzustand(void)
{
    v4_joy_t     st;
    v4_joy_roh_t ruhig = roh(0, 0, CIA_BASIS, POT_BASIS);

    v4_test_case("Feuerknoepfe: losgelassen ist losgelassen (Grundzustand)");
    v4_joy_init(&st);
    CHECK_EQ(v4_joy_feuer(&st, 0), 0x00);   /* ohne Grundzustand: kein Urteil */

    stabil(&st, &ruhig);
    CHECK_EQ(v4_joy_feuer(&st, 0), 0x00);
    CHECK_EQ(v4_joy_feuer(&st, 1), 0x00);
    CHECK_EQ(v4_joy_maske(&st, 0), 0x00);
    CHECK_EQ(v4_joy_maske(&st, 1), 0x00);
}

static void test_joy_feuer_ciaapra(void)
{
    v4_joy_t     st;
    v4_joy_roh_t basis = roh(0, 0, CIA_BASIS, POT_BASIS);
    v4_joy_roh_t p0    = roh(0, 0, 0xB4u, POT_BASIS);   /* Bit 6 auf 0 */
    v4_joy_roh_t p1    = roh(0, 0, 0x74u, POT_BASIS);   /* Bit 7 auf 0 */

    v4_test_case("Knopf 1: CIAAPRA Bit 6 (Buchse 0) und Bit 7 (Buchse 1)");
    v4_joy_init(&st);
    stabil(&st, &basis);

    stabil(&st, &p0);
    CHECK_EQ(v4_joy_feuer(&st, 0), (long)V4P_IN_ST_BTN1);
    CHECK_EQ(v4_joy_feuer(&st, 1), 0x00);
    CHECK_EQ(v4_joy_maske(&st, 0), (long)V4P_IN_ST_BTN1);

    stabil(&st, &p1);
    CHECK_EQ(v4_joy_feuer(&st, 0), 0x00);
    CHECK_EQ(v4_joy_feuer(&st, 1), (long)V4P_IN_ST_BTN1);

    stabil(&st, &basis);
    CHECK_EQ(v4_joy_feuer(&st, 0), 0x00);
    CHECK_EQ(v4_joy_feuer(&st, 1), 0x00);
}

static void test_joy_feuer_potinp(void)
{
    v4_joy_t     st;
    v4_joy_roh_t basis = roh(0, 0, CIA_BASIS, POT_BASIS);
    v4_joy_roh_t p0    = roh(0, 0, CIA_BASIS, 0xFE02u);   /* Bit 8 auf 0  */
    v4_joy_roh_t p1    = roh(0, 0, CIA_BASIS, 0xFB02u);   /* Bit 10 auf 0 */

    v4_test_case("Knopf 2: POTINP Bit 8 (Buchse 0) und Bit 10 (Buchse 1)");
    v4_joy_init(&st);
    stabil(&st, &basis);

    stabil(&st, &p0);
    CHECK_EQ(v4_joy_feuer(&st, 0), (long)V4P_IN_ST_BTN2);
    CHECK_EQ(v4_joy_feuer(&st, 1), 0x00);

    stabil(&st, &p1);
    CHECK_EQ(v4_joy_feuer(&st, 0), 0x00);
    CHECK_EQ(v4_joy_feuer(&st, 1), (long)V4P_IN_ST_BTN2);

    /* Beide Knoepfe einer Buchse zugleich. */
    {
        v4_joy_roh_t beide = roh(0, 0, 0xB4u, 0xFE02u);

        stabil(&st, &beide);
    }
    CHECK_EQ(v4_joy_feuer(&st, 0),
             (long)(V4P_IN_ST_BTN1 | V4P_IN_ST_BTN2));
}

/*
 * Knopf 3 (Pin 5) -- der Fall, der beim Benutzer nicht ankam.
 *
 * Der ESP32 treibt Pin 5; die V4 hat die Leitung bis zum 16.10.2026 NICHT
 * gelesen, weil das ZWEITE Pot-Bit je Buchse nirgends ausgewertet wurde.
 * Hier wird genau das geprueft: POTINP Bit 9 (Buchse 0) und Bit 11 (Buchse 1),
 * getrennt, und "low = gedrueckt" gegen den gemerkten Grundzustand.
 */
static void test_joy_feuer_btn3(void)
{
    v4_joy_t     st;
    /* Grundzustand: Pin 5 liegt HIGH (losgelassen) -- Bit 9 und Bit 11 an. */
    const uint16_t basis_pot = (uint16_t)(POT_BASIS | 0x0200u | 0x0800u);
    v4_joy_roh_t basis = roh(0x0000u, 0x0000u, CIA_BASIS, basis_pot);
    v4_joy_roh_t p0    = roh(0x0000u, 0x0000u, CIA_BASIS,
                             (uint16_t)(basis_pot & ~0x0200u));  /* Bit 9  = 0 */
    v4_joy_roh_t p1    = roh(0x0000u, 0x0000u, CIA_BASIS,
                             (uint16_t)(basis_pot & ~0x0800u));  /* Bit 11 = 0 */
    v4_joy_roh_t beide = roh(0x0000u, 0x0000u, CIA_BASIS,
                             (uint16_t)(basis_pot & ~(0x0200u | 0x0800u)));

    v4_test_case("Knopf 3 (Pin 5): POTINP Bit 9 = Buchse 0, Bit 11 = Buchse 1");
    v4_joy_init(&st);
    stabil(&st, &basis);
    CHECK_EQ(v4_joy_feuer(&st, 0), 0x00);       /* high = nicht gedrueckt */
    CHECK_EQ(v4_joy_feuer(&st, 1), 0x00);

    stabil(&st, &p0);                            /* Buchse 0: low = gedrueckt */
    CHECK_EQ(v4_joy_feuer(&st, 0), (long)V4P_IN_ST_BTN3);
    CHECK_EQ(v4_joy_feuer(&st, 1), 0x00);        /* Buchse 1 bleibt frei */

    stabil(&st, &p1);                            /* Buchse 1: low = gedrueckt */
    CHECK_EQ(v4_joy_feuer(&st, 0), 0x00);
    CHECK_EQ(v4_joy_feuer(&st, 1), (long)V4P_IN_ST_BTN3);

    stabil(&st, &beide);
    CHECK_EQ(v4_joy_feuer(&st, 0), (long)V4P_IN_ST_BTN3);
    CHECK_EQ(v4_joy_feuer(&st, 1), (long)V4P_IN_ST_BTN3);

    /* Die anderen Leitungen kommen dabei NICHT mit: BTN1/BTN2 bleiben im
     * Grundzustand (das Bit steht dort unveraendert). */
    stabil(&st, &basis);
    stabil(&st, &beide);
    CHECK_EQ(v4_joy_feuer(&st, 0)
                 & (long)(V4P_IN_ST_BTN1 | V4P_IN_ST_BTN2), 0);

    v4_test_case("Knopf 3: Polaritaet kommt aus dem Grundzustand, nicht aus "
                 "dem Code");
    /* Umgekehrter Grundzustand (Pin 5 liegt LOW): derselbe Pegelwechsel gilt
     * dann als gedrueckt. Das ist die Zusage "aktiv low ODER aktiv high". */
    {
        v4_joy_roh_t basis_low = roh(0x0000u, 0x0000u, CIA_BASIS,
                                     (uint16_t)(basis_pot
                                                & ~(0x0200u | 0x0800u)));

        /* Nur Bit 9 wechselt (Buchse 0): 0 -> 1. */
        v4_joy_roh_t p0_high = roh(0x0000u, 0x0000u, CIA_BASIS,
                                   (uint16_t)(basis_pot & ~0x0800u));

        v4_joy_init(&st);
        stabil(&st, &basis_low);
        CHECK_EQ(v4_joy_feuer(&st, 0), 0x00);
        stabil(&st, &p0_high);                   /* Bit 9 nun HIGH */
        CHECK_EQ(v4_joy_feuer(&st, 0), (long)V4P_IN_ST_BTN3);
        CHECK_EQ(v4_joy_feuer(&st, 1), 0x00);    /* Buchse 1 unveraendert */
    }
}

static void test_joy_feuer_polaritaet(void)
{
    v4_joy_t     st;
    v4_joy_roh_t basis = roh(0, 0, 0xB4u, 0xFE02u);   /* Bits liegen auf 0 */
    v4_joy_roh_t jetzt = roh(0, 0, CIA_BASIS, POT_BASIS);  /* ... und nun auf 1 */

    v4_test_case("Polaritaet: der Grundzustand entscheidet, nicht eine "
                 "Verdrahtung im Code");
    v4_joy_init(&st);
    stabil(&st, &basis);
    CHECK_EQ(v4_joy_feuer(&st, 0), 0x00);

    /* Dasselbe Bit, andere Richtung: es gilt als GEDRUECKT, weil es sich vom
     * Grundzustand unterscheidet -- aktiv low oder aktiv high ist damit egal. */
    stabil(&st, &jetzt);
    CHECK_EQ(v4_joy_feuer(&st, 0),
             (long)(V4P_IN_ST_BTN1 | V4P_IN_ST_BTN2));
}

static void test_joy_feuer_stabilitaet(void)
{
    v4_joy_t     st;
    v4_joy_roh_t basis = roh(0, 0, CIA_BASIS, POT_BASIS);
    v4_joy_roh_t a     = roh(0x0100u, 0, 0xB4u, POT_BASIS);
    v4_joy_roh_t b     = roh(0x0100u, 0, CIA_BASIS, POT_BASIS);

    v4_test_case("Feuerleitungen: ungleiche Abtastung wird verworfen, der "
                 "Richtungszaehler laeuft weiter");
    v4_joy_init(&st);
    stabil(&st, &basis);
    {
        v4_joy_roh_t richtung = roh(0x0100u, 0, CIA_BASIS, POT_BASIS);

        stabil(&st, &richtung);
    }
    CHECK_EQ(v4_joy_feuer(&st, 0), 0x00);

    v4_joy_update(&st, &a, &b);         /* Feuerbit springt zwischen a und b */
    CHECK_EQ(st.feuer_stabil[0], 0u);
    CHECK_EQ(st.verworfen_feuer, 1ul);
    CHECK_EQ(v4_joy_feuer(&st, 0), 0x00);   /* nichts uebernommen */
    /* Der Zaehler war in beiden Abtastungen gleich -> die Richtung steht. */
    CHECK_EQ(st.richtung_stabil[0], 1u);
    CHECK_EQ(v4_joy_maske(&st, 0), (long)V4P_IN_ST_UP);
}

/* ------------------------------------------------------------------ */
/* Der Registerhaken                                                   */
/* ------------------------------------------------------------------ */

static v4_joy_roh_t s_fake;
static int          s_fake_aufrufe;

static void fake_lesen(v4_joy_roh_t *r)
{
    s_fake_aufrufe++;
    *r = s_fake;
}

static void test_joy_haken(void)
{
    v4_joy_roh_t r;

    v4_test_case("Registerhaken: ohne Haken Nullen, mit Haken die Attrappe");
    v4_joy_set_lesen(NULL);
    v4_joy_lesen(&r);
    CHECK_EQ(r.joy0dat, 0u);
    CHECK_EQ(r.joy1dat, 0u);
    CHECK_EQ(r.ciaapra, 0u);
    CHECK_EQ(r.potinp, 0u);

    s_fake = roh(0x085Cu, 0x0003u, 0xF4u, 0xFF02u);
    s_fake_aufrufe = 0;
    v4_joy_set_lesen(fake_lesen);
    v4_joy_lesen(&r);
    CHECK_EQ(s_fake_aufrufe, 1);
    CHECK_EQ(r.joy0dat, 0x085Cu);
    CHECK_EQ(r.joy1dat, 0x0003u);
    CHECK_EQ(r.ciaapra, 0xF4u);
    CHECK_EQ(r.potinp, 0xFF02u);

    /* Und wieder abschalten: die Testsuite darf keinen Haken stehen lassen. */
    v4_joy_set_lesen(NULL);
    v4_joy_lesen(&r);
    CHECK_EQ(r.joy1dat, 0u);
}

/* ------------------------------------------------------------------ */
/* Die Textzeilen -- hier steht das Format fest                        */
/* ------------------------------------------------------------------ */

static void vergleich_setzen(v4_joy_t *st, uint16_t j1, uint8_t cia,
                             uint16_t pot)
{
    v4_joy_roh_t r = roh(0x0000u, j1, cia, pot);

    v4_joy_init(st);
    stabil(st, &r);
}

static void test_joy_port_text_ok(void)
{
    v4_joy_t     st;
    v4p_input_t  in;
    char         t[V4_JOY_DIREKT_MAX];

    v4_test_case("Vergleichszeile: gleiche Werte ergeben OK");
    memset(&in, 0, sizeof(in));
    vergleich_setzen(&st, 0x0102u, CIA_BASIS, POT_BASIS);   /* VOR+RECHTS */
    in.state[1]      = (uint8_t)(V4P_IN_ST_UP | V4P_IN_ST_RIGHT);
    in.level[1]      = 0x76u;
    in.level_valid   = 1u;

    (void)v4_joy_port_text(&st, &in, 1, 1, t, sizeof(t));
    CHECK(strstr(t, "Buchse 1") != NULL);
    CHECK(strstr(t, "direkt: 0x09 VOR+RECHTS") != NULL);
    CHECK(strstr(t, "roh 0x0102") != NULL);
    CHECK(strstr(t, "I2C: state=0x09 level=0x76") != NULL);
    CHECK(strstr(t, "OK") != NULL);
    CHECK(strstr(t, "ABWEICHUNG") == NULL);
}

static void test_joy_port_text_abweichung(void)
{
    v4_joy_t     st;
    v4p_input_t  in;
    char         t[V4_JOY_DIREKT_MAX];

    v4_test_case("Vergleichszeile: Abweichung wird ausdruecklich gemeldet");
    memset(&in, 0, sizeof(in));
    /* Der nachgemessene Rechtswert $0003: direkt RECHTS, der ESP32 meldet
     * aber DOWN+RIGHT (0x0A) -- eine Diagonale INNERHALB einer Achse, die die
     * direkte Quelle nicht sehen kann (siehe v4_joyport.h). */
    vergleich_setzen(&st, 0x0003u, CIA_BASIS, POT_BASIS);
    in.state[1]    = 0x0Au;
    in.level[1]    = 0x75u;
    in.level_valid = 1u;

    (void)v4_joy_port_text(&st, &in, 1, 1, t, sizeof(t));
    CHECK(strstr(t, "direkt: 0x08 RECHTS") != NULL);
    CHECK(strstr(t, "I2C: state=0x0A level=0x75") != NULL);
    CHECK(strstr(t, "ABWEICHUNG") != NULL);
    CHECK(strstr(t, "Differenz 0x02") != NULL);

    /* BTN3 (Pin 5) wird SEIT dem 16.10.2026 direkt gelesen (POTINP Bit 9/11)
     * und deshalb MIT verglichen: meldet die I2C-Seite BTN3 und die Register
     * zeigen nichts, ist das eine echte Abweichung -- genau der Fall, der
     * beim Benutzer unsichtbar blieb. */
    vergleich_setzen(&st, 0x0000u, CIA_BASIS, POT_BASIS);
    in.state[1]      = (uint8_t)V4P_IN_ST_BTN3;
    in.level[1]      = 0x3Fu;
    in.level_valid   = 1u;
    (void)v4_joy_port_text(&st, &in, 1, 1, t, sizeof(t));
    CHECK(strstr(t, "state=0x40") != NULL);
    CHECK(strstr(t, "ABWEICHUNG") != NULL);
    CHECK(strstr(t, "Differenz 0x40") != NULL);

    /* Und umgekehrt: lesen die Register BTN3, waehrend I2C nichts meldet,
     * steht die Abweichung ebenso da. */
    {
        /* Buchse 1: Pin 5 liegt auf POTINP Bit 11 (0x0800). */
        v4_joy_roh_t b3 = roh(0x0000u, 0x0000u, CIA_BASIS,
                              (uint16_t)(POT_BASIS | 0x0800u));
        v4_joy_roh_t g3 = roh(0x0000u, 0x0000u, CIA_BASIS,
                              (uint16_t)((POT_BASIS | 0x0800u) & ~0x0800u));

        v4_joy_init(&st);
        v4_joy_basis(&st, &b3);
        stabil(&st, &g3);               /* Buchse 1: Bit 11 low = gedrueckt */
        in.state[1] = 0x00u;
        (void)v4_joy_port_text(&st, &in, 1, 1, t, sizeof(t));
        CHECK(strstr(t, "direkt: 0x40 BTN3") != NULL);
        CHECK(strstr(t, "ABWEICHUNG") != NULL);
    }
}

static void test_joy_port_text_feuer(void)
{
    v4_joy_t     st;
    v4p_input_t  in;
    char         t[V4_JOY_DIREKT_MAX];

    v4_test_case("Vergleichszeile: Feuerknopf aus CIAAPRA/POTINP erscheint in "
                 "der Maske");
    memset(&in, 0, sizeof(in));
    /* Grundzustand wie am Geraet, dann Knopf 1 auf Buchse 1 gedrueckt
     * (CIAAPRA Bit 7 auf 0) und Knopf 2 auf Buchse 1 (POTINP Bit 10 auf 0). */
    {
        v4_joy_roh_t basis = roh(0, 0, CIA_BASIS, POT_BASIS);
        v4_joy_roh_t jetzt = roh(0, 0, 0x74u, 0xFB02u);

        v4_joy_init(&st);
        stabil(&st, &basis);
        stabil(&st, &jetzt);
    }
    in.state[1]    = (uint8_t)(V4P_IN_ST_BTN1 | V4P_IN_ST_BTN2);
    in.level[1]    = 0x4Fu;
    in.level_valid = 1u;

    (void)v4_joy_port_text(&st, &in, 1, 1, t, sizeof(t));
    CHECK(strstr(t, "direkt: 0x30 BTN1+BTN2") != NULL);
    CHECK(strstr(t, "OK") != NULL);
}

static void test_joy_port_text_ohne_antwort(void)
{
    v4_joy_t st;
    char     t[V4_JOY_TEXT_MAX];

    v4_test_case("Vergleichszeile: ohne I2C-Antwort und ohne stabile "
                 "Abtastung steht kein Urteil da");
    memset(&st, 0, sizeof(st));
    v4_joy_init(&st);

    (void)v4_joy_port_text(&st, NULL, 0, 0, t, sizeof(t));
    CHECK(strstr(t, "noch keine stabile Abtastung") != NULL);
    CHECK(strstr(t, "noch keine Antwort") != NULL);
    CHECK(strstr(t, "OK") == NULL);
    CHECK(strstr(t, "ABWEICHUNG") == NULL);

    /* Direkt stabil, I2C-Antwort aber ALT (der Bus hat gerade nicht
     * geantwortet): die Werte stehen da, das Urteil fehlt. */
    vergleich_setzen(&st, 0x0100u, CIA_BASIS, POT_BASIS);
    {
        v4p_input_t in;

        memset(&in, 0, sizeof(in));
        in.state[0]    = 0x01u;
        in.level_valid = 1u;
        (void)v4_joy_port_text(&st, &in, 0, 0, t, sizeof(t));
        CHECK(strstr(t, "alte Antwort") != NULL);
        CHECK(strstr(t, "OK") == NULL);
        CHECK(strstr(t, "ABWEICHUNG") == NULL);
    }
}

static void test_joy_port_text_ohne_pegel(void)
{
    v4_joy_t    st;
    v4p_input_t in;
    char        t[V4_JOY_TEXT_MAX];

    v4_test_case("Vergleichszeile: 10-Byte-Firmware -- keine Zahl, wo keine "
                 "Messung ist");
    memset(&in, 0, sizeof(in));
    vergleich_setzen(&st, 0x0100u, CIA_BASIS, POT_BASIS);
    in.state[1]    = (uint8_t)V4P_IN_ST_UP;
    in.level_valid = 0u;                /* alte Fassung: Pegel unbekannt */

    (void)v4_joy_port_text(&st, &in, 1, 1, t, sizeof(t));
    CHECK(strstr(t, "level=unbekannt") != NULL);
    CHECK(strstr(t, "OK") != NULL);
}

static void test_joy_reg_text(void)
{
    v4_joy_t     st;
    v4_joy_roh_t r = roh(0x085Cu, 0x0000u, CIA_BASIS, POT_BASIS);
    char         t[V4_JOY_DIREKT_MAX];

    v4_test_case("Rohwertzeile: CIAAPRA und POTINP in vier Hexziffern");
    v4_joy_init(&st);
    stabil(&st, &r);

    (void)v4_joy_reg_text(&st, t, sizeof(t));
    CHECK(strstr(t, "CIAAPRA=0x00F4") != NULL);
    CHECK(strstr(t, "POTINP=0xFF02") != NULL);
    CHECK(strstr(t, "Grundzustand CIAAPRA=0x00F4 POTINP=0xFF02") != NULL);
}

static void test_joy_debug_text(void)
{
    v4_joy_t    st;
    v4p_input_t in;
    char        t[V4_JOY_TEXT_MAX];

    v4_test_case("Debugzeile: Praefix [vergleich], beide Buchsen und die "
                 "Feuerregister in EINER Zeile");
    memset(&in, 0, sizeof(in));
    vergleich_setzen(&st, 0x0100u, CIA_BASIS, POT_BASIS);
    in.state[1]    = (uint8_t)V4P_IN_ST_UP;
    in.level[1]    = 0x7Eu;
    in.level_valid = 1u;

    (void)v4_joy_debug_text(&st, &in, 1, t, sizeof(t));
    CHECK(strstr(t, "[vergleich]") == t);
    CHECK(strstr(t, "Buchse0") != NULL);
    CHECK(strstr(t, "Buchse1") != NULL);
    CHECK(strstr(t, "direkt=0x01 stabil=ja roh=0x0100") != NULL);
    CHECK(strstr(t, "I2C state=0x01 level=0x7E OK") != NULL);
    CHECK(strstr(t, "CIAAPRA=0x00F4 POTINP=0xFF02") != NULL);
    /* Und die Zeile passt in den Puffer der Debugausgabe (V4_DBG_LINE = 300). */
    CHECK(strlen(t) < 300u);

    /* Mit Abweichung: die Kurzform nennt die Differenz. */
    in.state[1] = 0x05u;                /* statt UP nun UP+LEFT */
    (void)v4_joy_debug_text(&st, &in, 1, t, sizeof(t));
    CHECK(strstr(t, "ABWEICHUNG(Diff 0x04)") != NULL);
    CHECK(strlen(t) < 300u);
}

static void test_joy_vergleich_text(void)
{
    v4_joy_t    st;
    v4p_input_t in;
    char        t[3u * V4_JOY_TEXT_MAX];

    v4_test_case("Anzeigetext: zwei Buchsenzeilen und die Rohwertzeile");
    memset(&in, 0, sizeof(in));
    vergleich_setzen(&st, 0x0000u, CIA_BASIS, POT_BASIS);
    in.level[0]    = 0x7Fu;
    in.level[1]    = 0x7Fu;
    in.level_valid = 1u;

    (void)v4_joy_vergleich_text(&st, &in, 1, t, sizeof(t));
    CHECK(strstr(t, "Buchse 0") != NULL);
    CHECK(strstr(t, "Buchse 1") != NULL);
    CHECK(strstr(t, "CIAAPRA=0x00F4") != NULL);
    CHECK(strstr(t, "OK") != NULL);
    CHECK(strstr(t, "ABWEICHUNG") == NULL);
    /* Drei Zeilen: zweimal Umbruch. */
    CHECK_EQ(t[0] != '\0', 1);
    {
        int n = 0;
        const char *p;

        for (p = t; *p != '\0'; p++) {
            if (*p == '\n') {
                n++;
            }
        }
        CHECK_EQ(n, 2);
    }
}

/* ------------------------------------------------------------------ */
/* Die Fensterzeile: NUR direkte Register                              */
/* ------------------------------------------------------------------ */

static void test_joy_direkt_text(void)
{
    v4_joy_t     st;
    v4_joy_roh_t ruh   = roh(0x0000u, 0x0000u, CIA_BASIS, POT_BASIS);
    v4_joy_roh_t jetzt = roh(0x0000u, 0x0100u, 0x74u, POT_BASIS);
    char         t[V4_JOY_DIREKT_MAX];

    v4_test_case("Direktzeile: nur Register, KEIN I2C-Wert, kein Urteil");
    v4_joy_init(&st);
    v4_joy_basis(&st, &ruh);            /* Loslassen ist der Grundzustand */
    stabil(&st, &jetzt);                /* Buchse 1: VOR, BTN1 gezogen */

    (void)v4_joy_direkt_text(&st, 1, t, sizeof(t));
    CHECK(strstr(t, "direkt aus den Registern") != NULL);
    CHECK(strstr(t, "Leitung  Pin   Pegel    direkt") != NULL);
    /* Buchse 1: VOR aktiv, BTN1 gedrueckt -- die Zeile nennt beides. */
    CHECK(strstr(t, "VOR") != NULL);
    CHECK(strstr(t, "gedrueckt") != NULL);
    CHECK(strstr(t, "gedrueckt (direkt): VOR+BTN1") != NULL);
    CHECK(strstr(t, "Roh: Zaehler=0x0100 (stabil=ja)") != NULL);
    CHECK(strstr(t, "CIAAPRA=0x0074 POTINP=0xFF02") != NULL);   /* Rohwert */
    CHECK(strstr(t, "0 (Low)") != NULL);        /* Pegel Bit 7 liegt auf 0 */
    /* BTN3 (Pin 5) hat seit dem 16.10.2026 eine EIGENE Pegelspalte -- kein
     * "nicht lesbar" mehr. */
    CHECK(strstr(t, "BTN3") != NULL);
    CHECK(strstr(t, "nicht lesbar") == NULL);
    /* Und AUSDRUECKLICH nichts aus der I2C-Sicht: das Fenster zeigt nur die
     * Portdaten, der Vergleich steht im Protokoll. */
    CHECK(strstr(t, "I2C") == NULL);
    CHECK(strstr(t, "state=") == NULL);
    CHECK(strstr(t, "level=") == NULL);
    CHECK(strstr(t, "ABWEICHUNG") == NULL);
    CHECK(strlen(t) < V4_JOY_DIREKT_MAX);

    v4_test_case("Direktzeile: BTN3 (Pin 5) erscheint mit Pegel und Urteil");
    {
        v4_joy_roh_t basis3 = roh(0x0000u, 0x0000u, CIA_BASIS,
                                  (uint16_t)(POT_BASIS | 0x0200u | 0x0800u));
        v4_joy_roh_t gedr3  = roh(0x0000u, 0x0000u, CIA_BASIS,
                                  (uint16_t)((POT_BASIS | 0x0200u | 0x0800u)
                                             & ~0x0200u));

        v4_joy_init(&st);
        v4_joy_basis(&st, &basis3);
        stabil(&st, &gedr3);
        (void)v4_joy_direkt_text(&st, 0, t, sizeof(t));
        CHECK(strstr(t, "BTN3") != NULL);
        CHECK(strstr(t, "0 (Low)") != NULL);     /* das Rohbit steht da */
        CHECK(strstr(t, "gedrueckt (direkt): BTN3") != NULL);
    }

    v4_test_case("Direktzeile: leere Buchse zeigt keine erfundene Aktivitaet");
    v4_joy_init(&st);
    v4_joy_basis(&st, &ruh);
    stabil(&st, &ruh);
    (void)v4_joy_direkt_text(&st, 0, t, sizeof(t));
    CHECK(strstr(t, "aktiv") == NULL);
    CHECK(strstr(t, "gedrueckt (direkt): -") != NULL);
}

/* ------------------------------------------------------------------ */
/* Die Protokoll-/Herzzeile: beide Buchsen direkt, ohne I2C           */
/* ------------------------------------------------------------------ */

static void test_joy_direkt_protokoll(void)
{
    v4_joy_t     st;
    v4_joy_roh_t ruh   = roh(0x0000u, 0x0000u, CIA_BASIS, POT_BASIS);
    v4_joy_roh_t jetzt = roh(0x0000u, 0x0002u, 0x74u, POT_BASIS);
    char         t[V4_JOY_TEXT_MAX];

    v4_test_case("Protokollzeile: beide Buchsen direkt, KEIN I2C, kein Urteil");
    v4_joy_init(&st);
    v4_joy_basis(&st, &ruh);            /* Loslassen ist der Grundzustand */
    stabil(&st, &jetzt);                /* Buchse 1: RECHTS, BTN1 gezogen */

    (void)v4_joy_direkt_protokoll_text(&st, t, sizeof(t));
    /* Buchse 0 ruht, Buchse 1 traegt RECHTS|BTN1 = 0x18. */
    CHECK(strstr(t, "Buchse0 direkt=0x00 - stabil=ja roh=0x0000") != NULL);
    CHECK(strstr(t, "Buchse1 direkt=0x18 RECHTS+BTN1 stabil=ja roh=0x0002")
          != NULL);
    CHECK(strstr(t, "CIAAPRA=0x0074 POTINP=0xFF02") != NULL);
    /* Ausdruecklich NICHTS aus der I2C-Sicht und kein Urteil. */
    CHECK(strstr(t, "I2C") == NULL);
    CHECK(strstr(t, "state=") == NULL);
    CHECK(strstr(t, "level=") == NULL);
    CHECK(strstr(t, "ABWEICHUNG") == NULL);
    CHECK(strstr(t, "OK") == NULL);
    CHECK(strlen(t) < V4_JOY_TEXT_MAX);

    v4_test_case("Protokollzeile: Randfaelle NULL/leerer Puffer");
    CHECK_EQ(v4_joy_direkt_protokoll_text(NULL, t, sizeof(t)), 0u);
    CHECK_EQ(v4_joy_direkt_protokoll_text(&st, NULL, 100u), 0u);
    CHECK_EQ(v4_joy_direkt_protokoll_text(&st, t, 0u), 0u);
}

/* ------------------------------------------------------------------ */
/* Randfaelle                                                          */
/* ------------------------------------------------------------------ */

static void test_joy_randfaelle(void)
{
    v4_joy_t  st;
    char      t[8];
    size_t    n;

    v4_test_case("Randfaelle: NULL, unbekannte Buchse, kleiner Puffer");
    v4_joy_init(&st);
    v4_joy_init(NULL);                  /* muss still bleiben */
    v4_joy_update(NULL, NULL, NULL);
    v4_joy_basis(NULL, NULL);
    CHECK_EQ(v4_joy_feuer(NULL, 0), 0x00);
    CHECK_EQ(v4_joy_richtung(NULL, 0), 0x00);
    CHECK_EQ(v4_joy_maske(NULL, 0), 0x00);
    CHECK_EQ(v4_joy_maske(&st, -1), 0x00);
    CHECK_EQ(v4_joy_maske(&st, 2), 0x00);
    CHECK_EQ(v4_joy_feuer(&st, 9), 0x00);
    CHECK_EQ(v4_joy_richtung(&st, 9), 0x00);
    v4_joy_lesen(NULL);

    CHECK_EQ(v4_joy_port_text(&st, NULL, 0, 0, NULL, 100u), 0u);
    CHECK_EQ(v4_joy_port_text(&st, NULL, 0, 0, t, 0u), 0u);
    CHECK_EQ(v4_joy_reg_text(NULL, t, sizeof(t)), 0u);
    CHECK_EQ(v4_joy_direkt_text(NULL, 0, t, sizeof(t)), 0u);
    CHECK_EQ(v4_joy_direkt_text(&st, -1, t, sizeof(t)), 0u);
    CHECK_EQ(v4_joy_direkt_text(&st, 2, t, sizeof(t)), 0u);
    CHECK_EQ(v4_joy_direkt_text(&st, 0, NULL, 100u), 0u);
    CHECK_EQ(v4_joy_direkt_text(&st, 0, t, 0u), 0u);
    CHECK_EQ(v4_joy_debug_text(NULL, NULL, 0, t, sizeof(t)), 0u);
    CHECK_EQ(v4_joy_vergleich_text(NULL, NULL, 0, t, sizeof(t)), 0u);

    /* Kleiner Puffer: gekuerzt, aber nullterminiert -- kein Lauf. */
    n = v4_joy_port_text(&st, NULL, 0, 0, t, sizeof(t));
    CHECK(n < sizeof(t));
    CHECK(t[sizeof(t) - 1u] == '\0');

    (void)v4_joy_richtungen_text(0x7Fu, t, 2u);
    CHECK_EQ(t[1], '\0');
}

/* ------------------------------------------------------------------ */

int test_joyport(void)
{
    v4_test_begin("Joystick-Anschluesse direkt: Dekodierung, Stabilitaet, "
                  "Vergleich");

    test_joy_ruhe();
    test_joy_rechts();
    test_joy_links();
    test_joy_vor();
    test_joy_zurueck();
    test_joy_diagonale();

    test_joy_stabil();
    test_joy_verworfen();

    test_joy_feuer_grundzustand();
    test_joy_feuer_ciaapra();
    test_joy_feuer_potinp();
    test_joy_feuer_btn3();
    test_joy_feuer_polaritaet();
    test_joy_feuer_stabilitaet();

    test_joy_haken();

    test_joy_port_text_ok();
    test_joy_port_text_abweichung();
    test_joy_port_text_feuer();
    test_joy_port_text_ohne_antwort();
    test_joy_port_text_ohne_pegel();
    test_joy_reg_text();
    test_joy_debug_text();
    test_joy_vergleich_text();
    test_joy_direkt_text();
    test_joy_direkt_protokoll();

    test_joy_randfaelle();

    return v4_test_failures;
}
