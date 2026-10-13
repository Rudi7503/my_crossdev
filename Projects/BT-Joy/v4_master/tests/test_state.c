/*
 * test_state.c -- die Zustandshilfe der Frontends (v4_state.c) gegen den Mock.
 *
 * Warum diese Suite: die Logik, die frueher in der Auftragsschicht steckte, ist
 * nicht verschwunden -- sie sitzt jetzt in den Frontends und ihrem
 * Zustandsblock. Genau diese Logik laeuft auf dem Wirt gegen den Mock:
 *
 *   - "welche Werte bleiben stehen", wenn eine Antwort ausbleibt,
 *   - wie der Abstand nach Fehlversuchen waechst (0,5 s -> 1 s -> 2 s -> 5 s),
 *   - wann ein Rueckgabewert "keine Verbindung" heisst und wann nicht,
 *   - was nach SET_PORT_MODE/SWAP als bestaetigt gilt,
 *   - wie die Geraeteliste gefuellt wird.
 *
 * Was hier NICHT geprueft wird -- das braucht die V4: dass i2c.library
 * ueberhaupt antwortet, wie lange SendI2C/ReceiveI2C blockieren und ob die MUI
 * mit einem haengenden Buszugriff noch bedienbar bleibt.
 *
 * Die Frontends rufen v4_master DIREKT (das ist die laufende Konstruktion);
 * diese Suite tut dasselbe und legt die Ergebnisse danach in den Block.
 */

#include "v4_test.h"

#include "../v4_state.h"
#include "../v4_master.h"
#include "../v4_mock.h"

#include <string.h>

/* ------------------------------------------------------------------ */
/* Hilfen                                                              */
/* ------------------------------------------------------------------ */

static void setup(v4_state_t *s, v4_master_t *m)
{
    v4p_info_t info;

    v4_mock_reset();
    v4_mock_seed_default();
    v4_state_init(s);
    CHECK_EQ(v4_open(m, NULL), 0);
    CHECK_EQ(v4_ping(m, &info), V4P_ST_OK);
    v4_state_put_info(s, &info, (v4_check_info(&info) == V4P_ST_OK) ? 1 : 0);
    (void)v4_state_note(s, (uint8_t)V4P_ST_OK);
}

static void teardown(v4_master_t *m)
{
    (void)m;
    v4_close();
}

/* ------------------------------------------------------------------ */

static void test_state_init(void)
{
    v4_state_t s;

    v4_test_case("v4_state_init: keine Verbindung, Takt 200 ms, Klartext da");
    v4_state_init(&s);
    CHECK_EQ(s.link_ok, 0);
    CHECK_EQ(s.link_ever_ok, 0);
    CHECK_EQ(s.fail_streak, 0u);
    CHECK_EQ(s.gap_ms, V4_STATE_GAP_MS);
    CHECK(s.err[0] != '\0');
    CHECK_EQ(s.info_valid, 0);
    CHECK_EQ(s.status_valid, 0);
    CHECK_EQ(s.hid_valid, 0);
    CHECK_EQ(s.input_valid, 0);
    CHECK_EQ(s.mode_valid, 0);
    CHECK_EQ(s.dev_n, 0u);
}

static void test_state_ping_fuellt_kopf(void)
{
    v4_state_t  s;
    v4_master_t m;

    v4_test_case("Start: PING fuellt die Kopfdaten, Verbindung steht");
    setup(&s, &m);

    CHECK(s.info_valid);
    CHECK(s.info_checked);
    CHECK_EQ(s.info.proto_ver, V4P_PROTO_VER);
    CHECK_EQ(s.info.write_frame_len, (uint8_t)V4P_WRITE_FRAME_LEN);
    CHECK_EQ(s.info.read_frame_len, (uint8_t)V4P_READ_FRAME_LEN);

    CHECK_EQ(s.link_ok, 1);
    CHECK_EQ(s.fail_streak, 0u);
    CHECK_EQ(s.gap_ms, V4_STATE_GAP_MS);
    CHECK_EQ(s.err[0], '\0');

    teardown(&m);
}

static void test_state_poll_und_slow(void)
{
    v4_state_t   s;
    v4_master_t  m;
    uint8_t      mode[V4P_PORT_COUNT];
    v4p_input_t  in;
    v4p_status_t st;
    v4p_hid_t    hid;

    v4_test_case("Poll liefert Buchsen, der langsame Durchlauf Status und HID");
    setup(&s, &m);

    v4_mock_set_port_state(0, 0x11u);       /* UP und BTN3 gezogen */

    CHECK_EQ(v4_get_port_mode(&m, mode), V4P_ST_OK);
    v4_state_put_modes(&s, mode);
    CHECK_EQ(v4_get_input(&m, &in), V4P_ST_OK);
    v4_state_put_input(&s, &in);

    CHECK(s.input_valid);
    CHECK_EQ(s.input.state[0], 0x11u);
    CHECK_EQ(s.input.level_valid, 1u);
    CHECK(s.mode_valid);
    CHECK_EQ(s.applied[0], (uint8_t)V4P_PORT_JOY);

    CHECK(!s.status_valid);                 /* noch nicht geholt */
    CHECK(!s.hid_valid);

    CHECK_EQ(v4_get_status(&m, &st), V4P_ST_OK);
    v4_state_put_status(&s, &st);
    CHECK_EQ(v4_get_hid(&m, &hid), V4P_ST_OK);
    v4_state_put_hid(&s, &hid);
    CHECK(s.status_valid);
    CHECK(s.hid_valid);
    CHECK_EQ(s.hid.kind, (uint8_t)V4P_HID_KIND_PAD);
    CHECK_EQ(s.count, st.dev_count);

    teardown(&m);
}

static void test_state_letzte_werte_bleiben(void)
{
    v4_state_t  s;
    v4_master_t m;
    v4p_input_t in;
    uint8_t     rc;

    v4_test_case("Ausfall: letzte gueltige Werte bleiben stehen, Klartext da");
    setup(&s, &m);

    v4_mock_set_port_state(0, 0x11u);
    CHECK_EQ(v4_get_input(&m, &in), V4P_ST_OK);
    v4_state_put_input(&s, &in);
    CHECK_EQ(s.input.state[0], 0x11u);

    /* Der ESP32 ist weg: der Mock liefert weder Annahme noch Antwort. */
    v4_mock_set_port_state(0, 0x00u);
    v4_mock.fail_write = 1;
    v4_mock.fail_read  = 1;
    rc = v4_get_input(&m, &in);
    CHECK(rc != V4P_ST_OK);
    CHECK_EQ(v4_state_note(&s, rc), 1);     /* Wechsel: Leitung ist weg */

    CHECK_EQ(s.link_ok, 0);
    CHECK_EQ(s.fail_streak, 1u);
    CHECK(strstr(s.err, "antwortet nicht") != NULL);

    /* Entscheidend: die Anzeige bekommt weiter die ALTEN Werte -- die
     * Oberflaeche friert nicht ein und leert die Anzeige nicht. */
    CHECK(s.input_valid);
    CHECK_EQ(s.input.state[0], 0x11u);

    v4_mock.fail_write = 0;
    v4_mock.fail_read  = 0;
    teardown(&m);
}

static void test_state_backoff_leiter(void)
{
    v4_state_t  s;
    v4_master_t m;
    static const unsigned long erwartet[5] = { 500ul, 1000ul, 2000ul, 5000ul,
                                              5000ul };
    int i;

    v4_test_case("Backoff: 0,5 s -> 1 s -> 2 s -> 5 s, danach bleibt 5 s");
    setup(&s, &m);
    v4_mock.fail_write = 1;
    v4_mock.fail_read  = 1;

    for (i = 0; i < 5; i++) {
        v4p_input_t in;

        CHECK(v4_get_input(&m, &in) != V4P_ST_OK);
        (void)v4_state_note(&s, (uint8_t)V4_ERR_LINK);
        CHECK_EQ(s.link_ok, 0);
        CHECK_EQ(s.fail_streak, (unsigned)(i + 1));
        CHECK_EQ(s.gap_ms, erwartet[i]);
    }

    /* Erholung: sofort wieder normale Taktung, Zaehler zurueck. */
    v4_mock.fail_write = 0;
    v4_mock.fail_read  = 0;
    {
        v4p_input_t in;

        CHECK_EQ(v4_get_input(&m, &in), V4P_ST_OK);
    }
    CHECK_EQ(v4_state_note(&s, (uint8_t)V4P_ST_OK), 1);
    CHECK_EQ(s.link_ok, 1);
    CHECK_EQ(s.link_ever_ok, 1);
    CHECK_EQ(s.fail_streak, 0u);
    CHECK_EQ(s.gap_ms, V4_STATE_GAP_MS);
    CHECK_EQ(s.err[0], '\0');

    /* Und die reine Leiter, ohne Bus -- inklusive der Grenzen. */
    CHECK_EQ(v4_state_backoff(0u), V4_STATE_GAP_MS);
    CHECK_EQ(v4_state_backoff(1u), 500ul);
    CHECK_EQ(v4_state_backoff(2u), 1000ul);
    CHECK_EQ(v4_state_backoff(3u), 2000ul);
    CHECK_EQ(v4_state_backoff(4u), 5000ul);
    CHECK_EQ(v4_state_backoff(9u), 5000ul);

    teardown(&m);
}

static void test_state_rc_klassifikation(void)
{
    v4_test_case("v4_state_rc_is_down: nur echte Verbindungsstoerungen");
    CHECK_EQ(v4_state_rc_is_down(V4P_ST_OK), 0);
    CHECK_EQ(v4_state_rc_is_down(V4P_ST_BAD_ARG), 0);
    CHECK_EQ(v4_state_rc_is_down(V4P_ST_BAD_STATE), 0);
    CHECK_EQ(v4_state_rc_is_down(V4_ERR_FRAME), 0);
    CHECK_EQ(v4_state_rc_is_down(V4_ERR_PROTO), 0);
    CHECK_EQ(v4_state_rc_is_down(V4_ERR_LINK), 1);
    CHECK_EQ(v4_state_rc_is_down(V4_ERR_BUSY), 1);
    CHECK_EQ(v4_state_rc_is_down(V4_ERR_TIMEOUT), 1);
}

static void test_state_befehle(void)
{
    v4_state_t  s;
    v4_master_t m;
    uint8_t     bestaetigt = 0u;

    v4_test_case("SET_PORT_MODE und Buchsentausch, als bestaetigt uebernommen");
    setup(&s, &m);

    CHECK_EQ(v4_set_port_mode(&m, 0u, (uint8_t)V4P_PORT_MOUSE, &bestaetigt),
             V4P_ST_OK);
    v4_state_put_mode(&s, 0u, bestaetigt);
    CHECK(s.mode_valid);
    CHECK_EQ(s.mode[0], (uint8_t)V4P_PORT_MOUSE);
    CHECK_EQ(s.applied[0], (uint8_t)V4P_PORT_MOUSE);

    v4_mock_set_port_mode(0, (uint8_t)V4P_PORT_JOY);
    v4_mock_set_port_mode(1, (uint8_t)V4P_PORT_MOUSE);
    {
        uint8_t now[V4P_PORT_COUNT];

        CHECK_EQ(v4_swap_port_modes(&m, now), V4P_ST_OK);
        v4_state_put_modes(&s, now);
    }
    CHECK_EQ(s.mode[0], (uint8_t)V4P_PORT_MOUSE);
    CHECK_EQ(s.mode[1], (uint8_t)V4P_PORT_JOY);
    CHECK_EQ(s.applied[0], (uint8_t)V4P_PORT_MOUSE);
    CHECK_EQ(s.applied[1], (uint8_t)V4P_PORT_JOY);

    CHECK_EQ(v4_disconnect(&m), V4P_ST_OK);
    CHECK_EQ(v4_forget(&m), V4P_ST_OK);
    CHECK_EQ(v4_reset(&m), V4P_ST_OK);

    teardown(&m);
}

static void test_state_scan(void)
{
    v4_state_t  s;
    v4_master_t m;
    v4p_dev_t   tab[V4P_MAX_DEVICES];
    uint8_t     count = 0u;
    uint8_t     n     = 0u;
    uint8_t     i;

    v4_test_case("Scan: Geraeteliste direkt gelesen und uebernommen");
    setup(&s, &m);

    CHECK_EQ(v4_scan_start(&m, 8u, 1), V4P_ST_OK);
    CHECK_EQ(v4_dev_count(&m, &count), V4P_ST_OK);
    CHECK_EQ(count, 3u);
    for (i = 0u; i < count; i++) {
        v4p_dev_t d;

        if (v4_dev_get(&m, i, &d) == V4P_ST_OK) {
            tab[n++] = d;
        }
    }
    v4_state_put_devs(&s, tab, n);

    CHECK_EQ(s.dev_n, 3u);
    CHECK_STR(s.devs[0].name, "8BitDo Pro 2");
    CHECK_STR(s.devs[1].name, "Logitech MX Anywhere");

    /* Ein unsinnig grosser Zaehler darf die Tabelle nicht ueberlaufen. */
    v4_state_put_devs(&s, tab, (uint8_t)(V4P_MAX_DEVICES + 3));
    CHECK_EQ(s.dev_n, (uint8_t)V4P_MAX_DEVICES);

    teardown(&m);
}

static void test_state_protokollstatus_ist_keine_stoerung(void)
{
    v4_state_t  s;
    v4_master_t m;

    v4_test_case("Protokollstatus (BAD_ARG) laesst die Leitung stehen");
    setup(&s, &m);

    /* Lokal abgewiesener Port: es geht kein Rahmen auf den Bus, und die
     * Leitung ist deshalb KEINEN Deut schlechter als vorher. */
    CHECK_EQ(v4_set_port_mode(&m, 9u, 0u, NULL), V4P_ST_BAD_ARG);
    CHECK_EQ(v4_state_note(&s, (uint8_t)V4P_ST_BAD_ARG), 0);
    CHECK_EQ(s.link_ok, 1);
    CHECK_EQ(s.fail_streak, 0u);
    CHECK_EQ(s.err[0], '\0');

    /* SCAN_START/SCAN_STOP sind Befehle ohne Nutzlast und duerfen den
     * Verbindungszustand ebenfalls nicht kippen. */
    CHECK_EQ(v4_scan_start(&m, 8u, 1), V4P_ST_OK);
    CHECK_EQ(v4_state_note(&s, (uint8_t)V4P_ST_OK), 0);
    CHECK_EQ(v4_scan_stop(&m), V4P_ST_OK);
    CHECK(s.link_ok);

    teardown(&m);
}

static void test_state_erster_ausfall_ist_kein_wechsel(void)
{
    v4_state_t  s;
    v4_master_t m;

    v4_test_case("Der erste Ausfall aus dem Ruhezustand meldet keinen Wechsel");
    setup(&s, &m);

    /* Aus "Leitung steht" heraus ist der Ausfall ein Wechsel ... */
    CHECK_EQ(v4_state_note(&s, (uint8_t)V4_ERR_LINK), 1);
    CHECK_EQ(s.fail_streak, 1u);
    /* ... ein zweiter Fehlversuch derselben Lage nicht mehr. */
    CHECK_EQ(v4_state_note(&s, (uint8_t)V4_ERR_LINK), 0);
    CHECK_EQ(s.fail_streak, 2u);

    /* Und aus dem Ruhezustand (link_ok = 0) ist ein Ausfall kein Wechsel:
     * sonst stuende die Ausfallzeile doppelt im Log. */
    v4_state_init(&s);
    CHECK_EQ(v4_state_note(&s, (uint8_t)V4_ERR_LINK), 0);
    CHECK_EQ(s.fail_streak, 1u);

    teardown(&m);
}

static void test_state_eingangsverteilung(void)
{
    v4_state_t    s;
    v4_master_t   m;
    v4p_portcfg_t c;
    uint8_t       btn[V4P_BTN_COUNT];

    v4_test_case("GET_PORT_CONFIG uebernommen: mode_soll UND mode_ist "
                 "getrennt, mode[] traegt die WIRKSAME Betriebsart");
    setup(&s, &m);
    CHECK(!s.portcfg_valid);

    /* Vorgabe "auto", wirksam Maus -- die beiden duerfen sich NICHT
     * vermischen: 0xFF ist keine Betriebsart. */
    v4_mock_set_mode_soll(0, (uint8_t)V4P_MODE_AUTO);
    v4_mock_set_port_mode(0, (uint8_t)V4P_PORT_MOUSE);
    v4_mock_set_mode_soll(0, (uint8_t)V4P_MODE_AUTO);   /* wieder auto */
    CHECK_EQ(v4_get_port_config(&m, &c), V4P_ST_OK);
    v4_state_put_portcfg(&s, &c);

    CHECK(s.portcfg_valid);
    CHECK_EQ(s.portcfg.mode_soll[0], (uint8_t)V4P_MODE_AUTO);
    CHECK_EQ(s.portcfg.mode_ist[0], (uint8_t)V4P_PORT_MOUSE);
    CHECK_EQ(s.mode[0], (uint8_t)V4P_PORT_MOUSE);
    CHECK_EQ(s.applied[0], (uint8_t)V4P_PORT_MOUSE);
    CHECK(s.mode_valid);

    v4_test_case("Der falsche Slave antwortet nicht mit 0xFF in mode_ist");
    /* Das ist keine echte Antwort, sondern eine Gegenprobe der Regel: der
     * Decoder lehnt 0xFF in mode_ist ab. */
    CHECK_EQ(v4p_mode_soll_valid((uint8_t)V4P_MODE_AUTO), 1);
    CHECK_EQ(v4p_mode_soll_valid(3u), 0);

    v4_test_case("Geraetezuordnung: merken, ersetzen, nachlesen");
    {
        /* Zwei ausgedachte Adressen: die Zuordnung haengt NICHT an der
         * Scanliste (sie kommt aus SET_DEVICE_PORT). */
        static const uint8_t a0[V4P_BDA_LEN] = { 0xE4u, 0x17u, 0xD8u,
                                                 0x11u, 0x22u, 0x33u };
        static const uint8_t a1[V4P_BDA_LEN] = { 0x00u, 0x1Bu, 0xDCu,
                                                 0x44u, 0x55u, 0x66u };

        CHECK_EQ(v4_state_devport(&s, a0), 0xFFu);
        v4_state_put_devport(&s, a0, 1u);
        CHECK_EQ(v4_state_devport(&s, a0), 1u);
        CHECK_EQ(s.devport_n, 1u);
        /* Gleiche Adresse: der Eintrag wird ERSETZT, nicht doppelt angelegt. */
        v4_state_put_devport(&s, a0, 0u);
        CHECK_EQ(s.devport_n, 1u);
        CHECK_EQ(v4_state_devport(&s, a0), 0u);
        /* Ein zweites Geraet kommt dazu. */
        v4_state_put_devport(&s, a1, 1u);
        CHECK_EQ(s.devport_n, 2u);
        CHECK_EQ(v4_state_devport(&s, a1), 1u);
        /* Zugriff ueber den Index. */
        {
            uint8_t bda[V4P_BDA_LEN];

            CHECK_EQ(v4_state_devport_at(&s, 0u, bda), 0u);
            CHECK_MEM(bda, a0, V4P_BDA_LEN);
            CHECK_EQ(v4_state_devport_at(&s, 9u, bda), 0xFFu);
        }
        /* Unbekannte Adresse: 0xFF = "in dieser Sitzung nicht gesetzt". */
        {
            static const uint8_t fremd[V4P_BDA_LEN] = { 1u, 2u, 3u,
                                                        4u, 5u, 6u };

            CHECK_EQ(v4_state_devport(&s, fremd), 0xFFu);
        }
        /* Und die Tabelle laeuft nicht ueber. */
        {
            uint8_t i;

            for (i = 0u; i < (uint8_t)(V4P_MAX_DEVICES + 4u); i++) {
                uint8_t bda[V4P_BDA_LEN];

                bda[0] = (uint8_t)(0xA0u + i);
                bda[1] = 0u; bda[2] = 0u;
                bda[3] = 0u; bda[4] = 0u; bda[5] = 0u;
                v4_state_put_devport(&s, bda, (uint8_t)(i & 1u));
            }
            CHECK_EQ(s.devport_n, (uint8_t)V4P_MAX_DEVICES);
        }
    }

    (void)btn;
    teardown(&m);
}

/* ------------------------------------------------------------------ */

int test_state(void)
{
    v4_test_begin("Zustandshilfe: letzte Werte, Backoff, Klassifikation");

    test_state_init();
    test_state_ping_fuellt_kopf();
    test_state_poll_und_slow();
    test_state_letzte_werte_bleiben();
    test_state_backoff_leiter();
    test_state_rc_klassifikation();
    test_state_protokollstatus_ist_keine_stoerung();
    test_state_erster_ausfall_ist_kein_wechsel();
    test_state_befehle();
    test_state_scan();
    test_state_eingangsverteilung();

    return v4_test_failures;
}
