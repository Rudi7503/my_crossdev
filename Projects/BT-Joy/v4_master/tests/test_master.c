/*
 * test_master.c -- Transaktionskern und Befehls-Wrapper gegen den Mock.
 *
 * Der Kern (SEQ, Wiederholung, BUSY, BAD_CRC) ist aus dem Audio-Projekt
 * uebernommen und dort im Feld erprobt; hier wird er trotzdem geprueft, weil
 * genau diese Regeln den 128-Byte-Rahmen und die Adresse 0x5E tragen: faellt
 * eine davon, sieht man es nicht am Code, sondern erst am Adapter.
 *
 * Die Faelle sind die unangenehmen: Link-Ausfall, BUSY, BAD_CRC, halbe
 * Antwort, falsche Bestaetigung. Ein Test, der nur den Gutfall faehrt, wuerde
 * jeden dieser Fehler durchlassen.
 */

#include "v4_test.h"

#include "../v4_master.h"
#include "../v4_mock.h"

#include <stdlib.h>

/* ------------------------------------------------------------------ */
/* Hilfen                                                              */
/* ------------------------------------------------------------------ */

static uint8_t s_bda[3][6] = {
    { 0xE4u, 0x17u, 0xD8u, 0x11u, 0x22u, 0x33u },
    { 0x00u, 0x1Bu, 0xDCu, 0x44u, 0x55u, 0x66u },
    { 0xACu, 0x12u, 0x2Fu, 0x77u, 0x88u, 0x99u }
};

/* Mock bestuecken und Master oeffnen -- der Regelfall fuer jeden Testfall. */
static void setup(v4_master_t *m)
{
    v4_mock_reset();
    v4_mock_seed_default();
    CHECK_EQ(v4_open(m, NULL), 0);
}

static void teardown(void)
{
    v4_close();
}

/* ------------------------------------------------------------------ */
/* Kern                                                                */
/* ------------------------------------------------------------------ */

static void test_init_and_args(void)
{
    v4_master_t m;
    v4p_read_t  r;

    v4_test_case("v4_init: seq 0, t_wait 2000 us, Versuche 4, Zaehler 0");
    v4_init(&m);
    CHECK_EQ(m.seq, 0u);
    CHECK_EQ(m.t_wait_us, V4_T_WAIT_US);
    CHECK_EQ(m.try_max, V4_RETRIES);
    CHECK_EQ(m.tx_frames, 0ul);
    CHECK_EQ(m.rx_frames, 0ul);
    CHECK_EQ(m.trace, NULL);

    v4_test_case("v4_transact: NULL-Ziele und zu lange Nutzlast -> V4_ERR_ARG");
    CHECK_EQ(v4_transact(NULL, V4P_CMD_PING, NULL, 0u, &r), V4_ERR_ARG);
    setup(&m);
    CHECK_EQ(v4_transact(&m, V4P_CMD_PING, NULL, 0u, NULL), V4_ERR_ARG);
    CHECK_EQ(v4_transact(&m, V4P_CMD_PING, NULL,
                         (uint8_t)(V4P_WRITE_PAYLOAD_MAX + 1u), &r),
             V4_ERR_ARG);
    /* Ein Argumentfehler darf keinen Rahmen abgesetzt haben. */
    CHECK_EQ(v4_mock.writes, 0ul);
    teardown();
}

static void test_ping_and_info(void)
{
    v4_master_t m;
    v4p_info_t  info;
    uint8_t     rc;

    v4_test_case("PING: Antwort wird dekodiert, check_info akzeptiert sie");
    setup(&m);
    rc = v4_ping(&m, &info);
    CHECK_EQ(rc, V4P_ST_OK);
    CHECK_EQ(info.proto_ver, V4P_PROTO_VER);
    CHECK_EQ(info.fw_ver, V4P_FW_VER);
    CHECK_EQ(info.write_frame_len, V4P_WRITE_FRAME_LEN);
    CHECK_EQ(info.read_frame_len, V4P_READ_FRAME_LEN);
    CHECK_EQ(info.bulk_payload_max, 0u);
    CHECK_EQ(info.caps & V4P_CAP_PORT_MODE, V4P_CAP_PORT_MODE);
    CHECK_EQ(info.caps & V4P_CAP_GET_INPUT, V4P_CAP_GET_INPUT);
    CHECK_EQ(info.caps & V4P_CAP_GET_HID, V4P_CAP_GET_HID);
    CHECK_EQ(v4_check_info(&info), V4P_ST_OK);
    CHECK_EQ(v4_mock.bad_write_len, 0);         /* genau 32 Byte geschrieben */
    CHECK_EQ(v4_mock.writes, 1ul);
    CHECK_EQ(m.seq, 1u);                        /* SEQ nach gueltiger Antwort */
    teardown();

    v4_test_case("GET_INFO: gleiche Nutzlast wie PING");
    setup(&m);
    rc = v4_get_info(&m, &info);
    CHECK_EQ(rc, V4P_ST_OK);
    CHECK_EQ(info.proto_ver, V4P_PROTO_VER);
    teardown();

    v4_test_case("v4_check_info: 64-Byte-Firmware wird erkannt");
    info.proto_ver       = 2u;
    info.write_frame_len = 32u;
    info.read_frame_len  = 64u;
    CHECK_EQ(v4_check_info(&info), V4_ERR_PROTO);
    info.proto_ver       = V4P_PROTO_VER;
    CHECK_EQ(v4_check_info(&info), V4_ERR_PROTO);
    info.read_frame_len  = (uint8_t)V4P_READ_FRAME_LEN;
    info.write_frame_len = 31u;
    CHECK_EQ(v4_check_info(&info), V4_ERR_PROTO);
    info.write_frame_len = (uint8_t)V4P_WRITE_FRAME_LEN;
    CHECK_EQ(v4_check_info(&info), V4P_ST_OK);
    CHECK_EQ(v4_check_info(NULL), V4_ERR_ARG);
}

static void test_status_and_hid(void)
{
    v4_master_t  m;
    v4p_status_t st;
    v4p_hid_t    hid;
    uint8_t      rc;

    v4_test_case("GET_STATUS: Zustand, Geraetezahl, Reserviertfelder 0");
    setup(&m);
    rc = v4_get_status(&m, &st);
    CHECK_EQ(rc, V4P_ST_OK);
    CHECK_EQ(st.state, V4P_STATE_IDLE);
    CHECK_EQ(st.conn_index, V4P_NO_INDEX);
    CHECK_EQ(st.dev_count, 3u);
    CHECK_EQ(st.proto_ver, V4P_PROTO_VER);
    CHECK_EQ(st.fw_ver, V4P_FW_VER);
    /* Die SD-/Audiofelder gehoeren zum Audio-Projekt und muessen hier 0 sein;
     * genau daran erkennt die Anzeige eine fremde Firmware. */
    CHECK_EQ(st.sd_mounted, 0u);
    CHECK_EQ(st.audio_flags, 0u);
    CHECK_EQ(st.sd_card_present, 0u);
    CHECK_EQ(st.sd_free_kb, 0ul);
    teardown();

    v4_test_case("GET_HID: Name, VID/PID, Art, Transport, Reports");
    setup(&m);
    rc = v4_get_hid(&m, &hid);
    CHECK_EQ(rc, V4P_ST_OK);
    CHECK_EQ(hid.kind, V4P_HID_KIND_PAD);
    CHECK_EQ(hid.transport, V4P_HID_TR_BREDR);
    CHECK_EQ(hid.vid, 0x2DC8u);
    CHECK_EQ(hid.pid, 0x6100u);
    CHECK_EQ(hid.battery, 78u);
    CHECK_EQ(hid.reports, 1234u);
    CHECK_STR(hid.name, "8BitDo Pro 2");
    teardown();

    v4_test_case("GET_HID: kein Controller -> kind 0, leerer Name");
    v4_mock_reset();
    CHECK_EQ(v4_open(&m, NULL), 0);
    rc = v4_get_hid(&m, &hid);
    CHECK_EQ(rc, V4P_ST_OK);
    CHECK_EQ(hid.kind, V4P_HID_KIND_NONE);
    CHECK_EQ(hid.name_len, 0u);
    teardown();
}

static void test_ports(void)
{
    v4_master_t m;
    v4p_input_t in;
    uint8_t     mode[V4P_PORT_COUNT];
    uint8_t     applied = 0u;
    uint8_t     rc;

    v4_test_case("GET_PORT_MODE: Buchse 0 Joystick, Buchse 1 aus");
    setup(&m);
    rc = v4_get_port_mode(&m, mode);
    CHECK_EQ(rc, V4P_ST_OK);
    CHECK_EQ(mode[0], V4P_PORT_JOY);
    CHECK_EQ(mode[1], V4P_PORT_OFF);
    teardown();

    v4_test_case("GET_INPUT: Zug-Masken, Quadraturzaehler und Pegel");
    setup(&m);
    /* set_port_state setzt den Pegel auf "gezogen = Low, sonst High". Fuer den
     * ersten Fall bleibt es dabei -- Zug und Pegel passen zusammen. */
    v4_mock_set_port_state(0, V4P_IN_ST_UP | V4P_IN_ST_BTN1);
    v4_mock_set_port_state(1, V4P_IN_ST_RIGHT);
    v4_mock_set_steps(0x0000002Au, 0xFFFFFFFEu);
    rc = v4_get_input(&m, &in);
    CHECK_EQ(rc, V4P_ST_OK);
    CHECK_EQ(in.state[0], V4P_IN_ST_UP | V4P_IN_ST_BTN1);
    CHECK_EQ(in.state[1], V4P_IN_ST_RIGHT);
    CHECK_EQ(in.steps_x, 42ul);
    CHECK_EQ(in.steps_y, 0xFFFFFFFEul);
    CHECK_EQ(in.level_valid, 1u);
    CHECK_EQ(in.level[0], (uint8_t)(V4P_IN_ST_MASK & ~V4P_IN_ST_UP
                                    & ~V4P_IN_ST_BTN1));
    CHECK_EQ(in.level[1], (uint8_t)(V4P_IN_ST_MASK & ~V4P_IN_ST_RIGHT));
    CHECK_EQ(v4p_level_missing_pullup(in.state[0], in.level[0]), 0x00u);
    CHECK_EQ(v4p_level_impossible(in.state[0], in.level[0]), 0x00u);
    teardown();

    v4_test_case("GET_INPUT: fehlender Pull-up wird als Widerspruch erkannt");
    setup(&m);
    v4_mock_set_port_state(0, 0u);              /* nichts gezogen ...  */
    v4_mock_set_port_level(0, 0u);              /* ... liest aber Low  */
    v4_mock_set_port_state(1, V4P_IN_ST_BTN2);  /* gezogen ...           */
    v4_mock_set_port_level(1, V4P_IN_ST_MASK);  /* ... liest aber High   */
    rc = v4_get_input(&m, &in);
    CHECK_EQ(rc, V4P_ST_OK);
    CHECK_EQ(in.level_valid, 1u);
    CHECK_EQ(v4p_level_missing_pullup(in.state[0], in.level[0]), V4P_IN_ST_MASK);
    CHECK_EQ(v4p_level_impossible(in.state[1], in.level[1]), V4P_IN_ST_BTN2);
    teardown();

    v4_test_case("GET_INPUT: alte 10-Byte-Firmware -> Pegel unbekannt");
    /* Der Rueckfallweg: die Firmware antwortet kuerzer. Der Master muss das
     * annehmen (sonst waere jede alte Firmware unbrauchbar) und level_valid
     * auf 0 setzen, damit die Anzeigen keine Pegel erfinden. */
    setup(&m);
    v4_mock.legacy_input10 = 1;
    v4_mock_set_port_state(0, V4P_IN_ST_UP);
    v4_mock_set_steps(7u, 8u);
    rc = v4_get_input(&m, &in);
    CHECK_EQ(rc, V4P_ST_OK);
    CHECK_EQ(in.state[0], V4P_IN_ST_UP);
    CHECK_EQ(in.steps_x, 7ul);
    CHECK_EQ(in.steps_y, 8ul);
    CHECK_EQ(in.level_valid, 0u);
    CHECK_EQ(in.level[0], 0u);
    CHECK_EQ(in.level[1], 0u);
    teardown();

    v4_test_case("SET_PORT_MODE: gueltig -> Bestaetigung wird geprueft");
    setup(&m);
    rc = v4_set_port_mode(&m, 1u, V4P_PORT_MOUSE, &applied);
    CHECK_EQ(rc, V4P_ST_OK);
    CHECK_EQ(applied, V4P_PORT_MOUSE);
    rc = v4_get_port_mode(&m, mode);
    CHECK_EQ(rc, V4P_ST_OK);
    CHECK_EQ(mode[1], V4P_PORT_MOUSE);
    teardown();

    v4_test_case("SET_PORT_MODE: port 2 und mode 3 werden lokal abgewiesen");
    setup(&m);
    CHECK_EQ(v4_set_port_mode(&m, 2u, V4P_PORT_JOY, NULL), V4P_ST_BAD_ARG);
    CHECK_EQ(v4_set_port_mode(&m, 0u, 3u, NULL), V4P_ST_BAD_ARG);
    /* Kein Rahmen auf dem Bus: der Fehler ist lokal erkennbar. */
    CHECK_EQ(v4_mock.writes, 0ul);
    teardown();

    v4_test_case("SET_PORT_MODE: falsche Bestaetigung -> V4_ERR_FRAME");
    setup(&m);
    v4_mock.bad_set_port_echo = 1;
    CHECK_EQ(v4_set_port_mode(&m, 0u, V4P_PORT_MOUSE, &applied), V4_ERR_FRAME);
    teardown();

    v4_test_case("Buchsen tauschen: Joystick/aus -> aus/Joystick");
    setup(&m);
    v4_mock_set_port_mode(1, V4P_PORT_MOUSE);
    rc = v4_swap_port_modes(&m, mode);
    CHECK_EQ(rc, V4P_ST_OK);
    CHECK_EQ(mode[0], V4P_PORT_MOUSE);
    CHECK_EQ(mode[1], V4P_PORT_JOY);
    /* Und der Slave steht wirklich so: nachlesen, nicht glauben. */
    CHECK_EQ(v4_get_port_mode(&m, mode), V4P_ST_OK);
    CHECK_EQ(mode[0], V4P_PORT_MOUSE);
    CHECK_EQ(mode[1], V4P_PORT_JOY);

    v4_test_case("Buchsen tauschen (zweites Mal) stellt den Zustand wieder her");
    rc = v4_swap_port_modes(&m, mode);
    CHECK_EQ(rc, V4P_ST_OK);
    CHECK_EQ(mode[0], V4P_PORT_JOY);
    CHECK_EQ(mode[1], V4P_PORT_MOUSE);
    teardown();

    v4_test_case("RESET: beide Buchsen aus, Verbindung bleibt");
    setup(&m);
    CHECK_EQ(v4_connect(&m, 0u), V4P_ST_OK);
    CHECK_EQ(v4_set_port_mode(&m, 0u, V4P_PORT_JOY, NULL), V4P_ST_OK);
    CHECK_EQ(v4_reset(&m), V4P_ST_OK);
    CHECK_EQ(v4_get_port_mode(&m, mode), V4P_ST_OK);
    CHECK_EQ(mode[0], V4P_PORT_OFF);
    CHECK_EQ(mode[1], V4P_PORT_OFF);
    {
        v4p_status_t st;

        CHECK_EQ(v4_get_status(&m, &st), V4P_ST_OK);
        CHECK_EQ(st.state, V4P_STATE_CONNECTED);    /* nicht getrennt */
    }
    teardown();
}

static void test_devices_and_connect(void)
{
    v4_master_t  m;
    uint8_t      count = 0u;
    v4p_dev_t    dev;
    v4p_status_t st;
    uint8_t      rc;

    v4_test_case("DEV_COUNT/DEV_GET: drei Geraete mit Namen und Adresse");
    setup(&m);
    rc = v4_dev_count(&m, &count);
    CHECK_EQ(rc, V4P_ST_OK);
    CHECK_EQ(count, 3u);
    memset(&dev, 0, sizeof(dev));
    CHECK_EQ(v4_dev_get(&m, 1u, &dev), V4P_ST_OK);
    CHECK_EQ(dev.idx, 1u);
    CHECK_MEM(dev.bda, s_bda[1], 6u);
    CHECK_STR(dev.name, "Logitech MX Anywhere");

    v4_test_case("DEV_GET: Index ausserhalb -> NOT_FOUND");
    CHECK_EQ(v4_dev_get(&m, 3u, &dev), V4P_ST_NOT_FOUND);
    teardown();

    v4_test_case("SCAN_START/SCAN_STOP: Scan laeuft und endet");
    setup(&m);
    CHECK_EQ(v4_scan_start(&m, 8u, 1), V4P_ST_OK);
    CHECK_EQ(v4_get_status(&m, &st), V4P_ST_OK);
    CHECK_EQ(st.scan_active, 1u);
    CHECK_EQ(st.scan_gen, 1u);
    CHECK_EQ(v4_scan_stop(&m), V4P_ST_OK);
    CHECK_EQ(v4_get_status(&m, &st), V4P_ST_OK);
    CHECK_EQ(st.scan_active, 0u);
    teardown();

    v4_test_case("Scan laeuft asynchron: DEV_COUNT bleibt erst leer");
    /* Genau dieses Verhalten hat im Audio-Projekt die Anzeige "0 Geraete"
     * erzeugt, obwohl der Scan lief. Wer DEV_COUNT einmal liest und dann
     * aufgibt, verliert die Liste. */
    setup(&m);
    v4_mock.scan_delay = 2;             /* zwei DEV_COUNT-Aufrufe ohne Liste */
    v4_mock_set_scan(0);                /* Zustand vor dem Start */
    v4_mock_bump_scan_gen();            /* Scan-Generation 1 */
    CHECK_EQ(v4_scan_start(&m, 8u, 1), V4P_ST_OK);
    CHECK_EQ(v4_dev_count(&m, &count), V4P_ST_OK);
    CHECK_EQ(count, 0u);
    CHECK_EQ(v4_get_status(&m, &st), V4P_ST_OK);
    CHECK_EQ(st.scan_active, 1u);
    CHECK_EQ(st.scan_gen, 2u);          /* bump + SCAN_START */
    CHECK_EQ(v4_dev_count(&m, &count), V4P_ST_OK);
    CHECK_EQ(count, 0u);                /* der Scan laeuft noch */
    CHECK_EQ(v4_dev_count(&m, &count), V4P_ST_OK);
    CHECK_EQ(count, 3u);                /* jetzt ist die Liste da */
    teardown();

    v4_test_case("CONNECT: Index 1 -> CONNECTING, dann CONNECTED");
    setup(&m);
    v4_mock.connect_rounds = 2;
    CHECK_EQ(v4_connect(&m, 1u), V4P_ST_OK);
    rc = v4_wait_state(&m, V4P_STATE_CONNECTED, 10u, &st);
    CHECK_EQ(rc, V4P_ST_OK);
    CHECK_EQ(st.state, V4P_STATE_CONNECTED);
    CHECK_EQ(st.conn_index, 1u);
    teardown();

    v4_test_case("CONNECT: unbekannter Index -> NOT_FOUND");
    setup(&m);
    CHECK_EQ(v4_connect(&m, 9u), V4P_ST_NOT_FOUND);
    teardown();

    v4_test_case("CONNECT_BDA: bekannte Adresse verbindet, fremde nicht");
    setup(&m);
    CHECK_EQ(v4_connect_bda(&m, s_bda[2]), V4P_ST_OK);
    rc = v4_wait_state(&m, V4P_STATE_CONNECTED, 5u, &st);
    CHECK_EQ(rc, V4P_ST_OK);
    CHECK_EQ(st.conn_index, 2u);

    v4_test_case("CONNECT_BDA: unbekannte Adresse -> BAD_ARG");
    {
        static const uint8_t fremd[6] = { 1u, 2u, 3u, 4u, 5u, 6u };

        CHECK_EQ(v4_connect_bda(&m, fremd), V4P_ST_BAD_ARG);
    }
    CHECK_EQ(v4_connect_bda(&m, NULL), V4_ERR_ARG);
    teardown();

    v4_test_case("DISCONNECT und FORGET");
    setup(&m);
    CHECK_EQ(v4_connect(&m, 0u), V4P_ST_OK);
    CHECK_EQ(v4_disconnect(&m), V4P_ST_OK);
    CHECK_EQ(v4_get_status(&m, &st), V4P_ST_OK);
    CHECK_EQ(st.state, V4P_STATE_IDLE);
    CHECK_EQ(st.conn_index, V4P_NO_INDEX);
    CHECK_EQ(v4_forget(&m), V4P_ST_OK);
    teardown();

    v4_test_case("Bereits verbunden: v4_wait_state findet CONNECTED sofort");
    setup(&m);
    v4_mock_set_connected(1);
    CHECK_EQ(v4_get_status(&m, &st), V4P_ST_OK);
    CHECK_EQ(st.state, V4P_STATE_CONNECTED);
    CHECK_EQ(st.conn_index, 1u);
    CHECK_EQ(v4_wait_state(&m, V4P_STATE_CONNECTED, 3u, &st), V4P_ST_OK);
    teardown();

    v4_test_case("v4_wait_state: falscher Zielzustand laeuft in den Timeout");
    setup(&m);
    CHECK_EQ(v4_wait_state(&m, V4P_STATE_CONNECTED, 2u, &st), V4_ERR_TIMEOUT);
    CHECK_EQ(v4_wait_state(&m, V4P_STATE_IDLE, 2u, NULL), V4_ERR_ARG);
    teardown();

    v4_test_case("Unbekannter Befehl (Audio-Code 0x50) -> BAD_CMD");
    setup(&m);
    {
        v4p_read_t r;

        /* 0x50 ist FILE_OPEN im Audio-Projekt; diesen Befehl gibt es hier
         * nicht, und er darf auch nicht versehentlich funktionieren. */
        CHECK_EQ(v4_transact(&m, 0x50u, NULL, 0u, &r), V4P_ST_BAD_CMD);
    }
    teardown();
}

/* ------------------------------------------------------------------ */
/* Fehlerbilder des Busses                                             */
/* ------------------------------------------------------------------ */

static void test_link_failures(void)
{
    v4_master_t m;
    v4p_info_t  info;

    v4_test_case("Link tot: alle Versuche scheitern -> V4_ERR_LINK");
    setup(&m);
    v4_mock.fail_write = 1;
    CHECK_EQ(v4_ping(&m, &info), V4_ERR_LINK);
    /* SEQ bleibt stehen: ein wiederholter Befehl benutzt dieselbe Nummer. */
    CHECK_EQ(m.seq, 0u);
    CHECK_EQ(m.tx_frames, (unsigned long)V4_RETRIES);
    CHECK_EQ(m.rx_frames, 0ul);
    teardown();

    v4_test_case("Lesen scheitert: Wiederholung, dann Erfolg");
    setup(&m);
    v4_mock.drop_next_read = 1;
    CHECK_EQ(v4_ping(&m, &info), V4P_ST_OK);
    CHECK_EQ(m.seq, 1u);
    CHECK(m.unsafe_retries >= 1ul);
    teardown();

    v4_test_case("Halbe Antwort: Master wiederholt und heilt sich");
    setup(&m);
    v4_mock.partial_read_once = 1;
    CHECK_EQ(v4_ping(&m, &info), V4P_ST_OK);
    CHECK(m.retries >= 1ul);
    teardown();

    v4_test_case("Falsche CRC im ersten Rahmen: Master verwirft und wiederholt");
    setup(&m);
    v4_mock.bad_crc_once = 1;
    CHECK_EQ(v4_ping(&m, &info), V4P_ST_OK);
    CHECK_EQ(m.retries, 1ul);
    CHECK_EQ(m.last_check, V4P_CHECK_CRC);
    teardown();

    v4_test_case("Jeder zweite Rahmen mit falscher CRC: Befehl kommt durch");
    setup(&m);
    v4_mock.bad_crc_every = 2;
    /* Antwort 1 ist gut, Antwort 2 kaputt: die Wiederholung des ersten Befehls
     * trifft also eine kaputte Antwort -- der Master muss weiter versuchen. */
    CHECK_EQ(v4_get_info(&m, &info), V4P_ST_OK);
    CHECK_EQ(v4_get_info(&m, &info), V4P_ST_OK);
    CHECK(m.retries >= 1ul);
    teardown();

    v4_test_case("SEQ wird nicht gespiegelt: Master merkt es und wiederholt");
    setup(&m);
    v4_mock.no_seq_once = 1;
    CHECK_EQ(v4_get_status(&m, &(v4p_status_t){0}), V4P_ST_OK);
    CHECK_EQ(m.retries, 1ul);
    CHECK_EQ(m.last_check, V4P_CHECK_ECHO);
    teardown();

    v4_test_case("len-Feld unzulaessig: Master verwirft den Rahmen");
    setup(&m);
    v4_mock.bad_frame_len_once = 1;
    CHECK_EQ(v4_get_info(&m, &info), V4P_ST_OK);
    CHECK_EQ(m.retries, 1ul);
    CHECK_EQ(m.last_check, V4P_CHECK_LEN);
    teardown();

    v4_test_case("BUSY: wird intern wiederholt (2 Runden)");
    setup(&m);
    v4_mock.busy_count = 2;
    CHECK_EQ(v4_ping(&m, &info), V4P_ST_OK);
    CHECK_EQ(m.busy_rounds, 2ul);
    teardown();

    v4_test_case("Dauer-BUSY: Budget laeuft ab -> V4_ERR_BUSY");
    setup(&m);
    v4_mock.never_ready = 1;
    m.t_wait_us = 1u;                   /* Testlauf nicht kuenstlich langsam */
    CHECK_EQ(v4_ping(&m, &info), V4_ERR_BUSY);
    CHECK_EQ(m.busy_rounds, (unsigned long)V4_BUSY_TRIES);
    teardown();

    v4_test_case("BAD_CRC-Antwort: Befehl wird mit gleicher SEQ wiederholt");
    setup(&m);
    v4_mock.force_status = V4P_ST_BAD_CRC;
    m.t_wait_us = 1u;
    CHECK_EQ(v4_ping(&m, &info), V4P_ST_OK);
    CHECK_EQ(m.badcrc_rounds, 1ul);
    teardown();

    v4_test_case("Erzwungener Fehlerstatus wird durchgereicht");
    setup(&m);
    v4_mock.force_status = V4P_ST_BT_ERR;
    CHECK_EQ(v4_scan_start(&m, 8u, 0), V4P_ST_BT_ERR);
    teardown();

    v4_test_case("Bus-Reset: naechster Befehl liefert wieder saubere Daten");
    setup(&m);
    CHECK_EQ(v4_ping(&m, &info), V4P_ST_OK);
    v4_mock_bus_reset();
    CHECK_EQ(v4_get_status(&m, &(v4p_status_t){0}), V4P_ST_OK);
    teardown();
}

static void test_seq_discipline(void)
{
    v4_master_t m;
    v4p_info_t  info;
    int         i;

    v4_test_case("SEQ zaehlt je gueltiger Antwort genau eins weiter");
    setup(&m);
    for (i = 0; i < 5; i++) {
        CHECK_EQ(v4_get_info(&m, &info), V4P_ST_OK);
        CHECK_EQ(m.seq, (uint8_t)(i + 1));
    }
    teardown();

    v4_test_case("SEQ laeuft bei 255 auf 0 ueber");
    setup(&m);
    m.seq = 255u;
    CHECK_EQ(v4_get_info(&m, &info), V4P_ST_OK);
    CHECK_EQ(m.seq, 0u);
    teardown();

    v4_test_case("Fehlversuch erhoeht die SEQ NICHT");
    setup(&m);
    CHECK_EQ(v4_get_info(&m, &info), V4P_ST_OK);    /* seq = 1 */
    v4_mock.fail_read = 1;
    CHECK_EQ(v4_get_info(&m, &info), V4_ERR_LINK);
    CHECK_EQ(m.seq, 1u);
    teardown();
}

static void test_wait_backoff(void)
{
    v4_master_t m;
    v4p_info_t  info;

    v4_test_case("Framing-Fehler verlaengern t_wait (Lernen aus Fehlern)");
    setup(&m);
    m.t_wait_us = 1000u;
    v4_mock.bad_crc_once = 1;
    CHECK_EQ(v4_ping(&m, &info), V4P_ST_OK);
    /* Der erste Fehler verdoppelt auf 2000 und merkt sich das dauerhaft. */
    CHECK(m.t_wait_us >= 2000u);
    CHECK(v4_mock.delay_us >= 2000ul);
    teardown();
}

/* ------------------------------------------------------------------ */

/*
 * Eingangsverteilung und Geraetezuordnung (fw_ver 4): GET_PORT_CONFIG (0x09),
 * SET_PORT_ROUTING (0x0A), SET_DEVICE_PORT (0x08) und SET_PORT_MODE mit
 * 0xFF ("nach Geraeteart").
 *
 * Die Faelle sind wieder die unangenehmen: Doppelbelegung (BAD_ARG, NICHTS
 * geaendert), unbekannte Werte, falsches Echo -- und eine aeltere Firmware
 * (fw_ver 3) ohne die neuen Befehle, die trotzdem ANGENOMMEN werden muss.
 */
static void test_routing(void)
{
    v4_master_t   m;
    v4p_info_t    info;
    v4p_portcfg_t c;
    uint8_t       btn[V4P_BTN_COUNT];
    uint8_t       bda[6];
    uint8_t       applied = 0u;
    unsigned      k;

    v4_test_case("PING: fw_ver 4 wird angenommen, die neuen Faehigkeitsbits "
                 "sind da");
    setup(&m);
    CHECK_EQ(v4_ping(&m, &info), V4P_ST_OK);
    CHECK_EQ(info.fw_ver, V4P_FW_VER);
    CHECK_EQ(v4_check_info(&info), V4P_ST_OK);      /* KEIN Abbruch! */
    CHECK_EQ(v4_info_has_caps(&info, V4P_CAP_PORT_ROUTING), 1);
    CHECK_EQ(v4_info_has_caps(&info, V4P_CAP_DEVICE_PORT), 1);
    CHECK_EQ(v4_info_has_caps(&info, V4P_CAP_MOUSE_CFG), 1);
    CHECK_EQ(v4_info_has_caps(&info,
                              (uint16_t)(V4P_CAP_PORT_ROUTING
                                         | V4P_CAP_DEVICE_PORT)), 1);
    CHECK_EQ(v4_info_has_caps(&info,
                              (uint16_t)(V4P_CAP_PORT_ROUTING | 0x8000u)), 0);
    CHECK_EQ(v4_info_has_caps(NULL, V4P_CAP_PORT_ROUTING), 0);
    teardown();

    v4_test_case("GET_PORT_CONFIG: Vorgaben ab Werk (dir 0x07, "
                 "Knopfquellen = die drei Gruppen)");
    setup(&m);
    memset(&c, 0, sizeof(c));
    CHECK_EQ(v4_get_port_config(&m, &c), V4P_ST_OK);
    CHECK_EQ(c.mode_soll[0], V4P_MODE_AUTO);        /* Vorgabe ungesetzt */
    CHECK_EQ(c.mode_soll[1], V4P_MODE_AUTO);
    CHECK_EQ(c.mode_ist[0], V4P_PORT_JOY);          /* wirksam: Geraeteart */
    CHECK_EQ(c.mode_ist[1], V4P_PORT_OFF);
    CHECK_EQ(c.dir[0], V4P_DIR_STANDARD);
    CHECK_EQ(c.dir[1], V4P_DIR_STANDARD);
    for (k = 0u; k < V4P_BTN_COUNT; k++) {
        CHECK_EQ(c.btn[0][k], (uint8_t)(V4P_TASTE_GRUPPE1 + k));
        CHECK_EQ(c.btn[1][k], (uint8_t)(V4P_TASTE_GRUPPE1 + k));
    }
    CHECK_EQ(v4_get_port_config(&m, NULL), V4_ERR_ARG);
    teardown();

    v4_test_case("SET_PORT_ROUTING: gueltige Verteilung wird gesetzt und "
                 "bestaetigt");
    setup(&m);
    btn[0] = V4P_TASTE_L1;
    btn[1] = V4P_TASTE_R1;
    btn[2] = V4P_TASTE_KEINE;
    CHECK_EQ(v4_set_port_routing(&m, 0u, V4P_DIR_STICK_L, btn), V4P_ST_OK);
    CHECK_EQ(v4_get_port_config(&m, &c), V4P_ST_OK);
    CHECK_EQ(c.dir[0], V4P_DIR_STICK_L);
    CHECK_EQ(c.btn[0][0], V4P_TASTE_L1);
    CHECK_EQ(c.btn[0][1], V4P_TASTE_R1);
    CHECK_EQ(c.btn[0][2], V4P_TASTE_KEINE);
    /* Buchse 1 bleibt unberuehrt. */
    CHECK_EQ(c.dir[1], V4P_DIR_STANDARD);
    teardown();

    v4_test_case("SET_PORT_ROUTING: doppelte Knopfquelle -> BAD_ARG, "
                 "NICHTS geaendert, kein Rahmen");
    setup(&m);
    btn[0] = V4P_TASTE_L1;
    btn[1] = V4P_TASTE_L1;                          /* doppelt! */
    btn[2] = V4P_TASTE_KEINE;
    {
        unsigned long vorher = v4_mock.writes;

        CHECK_EQ(v4_set_port_routing(&m, 0u, V4P_DIR_ALLE, btn),
                 V4P_ST_BAD_ARG);
        CHECK_EQ(v4_mock.writes, vorher);           /* lokal abgewiesen */
    }
    CHECK_EQ(v4_get_port_config(&m, &c), V4P_ST_OK);
    CHECK_EQ(c.btn[0][0], V4P_TASTE_GRUPPE1);       /* unveraendert */
    CHECK_EQ(c.btn[0][1], V4P_TASTE_GRUPPE2);
    teardown();

    v4_test_case("SET_PORT_ROUTING: unbekannter Code, unbekanntes "
                 "Richtungsbit, port 2 -> BAD_ARG");
    setup(&m);
    btn[0] = (uint8_t)V4P_TASTE_ANZAHL;
    btn[1] = V4P_TASTE_KEINE;
    btn[2] = V4P_TASTE_KEINE;
    CHECK_EQ(v4_set_port_routing(&m, 0u, V4P_DIR_ALLE, btn), V4P_ST_BAD_ARG);
    btn[0] = V4P_TASTE_A;
    CHECK_EQ(v4_set_port_routing(&m, 0u, 0x80u, btn), V4P_ST_BAD_ARG);
    CHECK_EQ(v4_set_port_routing(&m, 2u, V4P_DIR_ALLE, btn), V4P_ST_BAD_ARG);
    CHECK_EQ(v4_set_port_routing(&m, 0u, V4P_DIR_ALLE, NULL), V4_ERR_ARG);
    teardown();

    v4_test_case("SET_PORT_ROUTING: falsches Echo -> V4_ERR_FRAME");
    setup(&m);
    v4_mock.bad_routing_echo = 1;
    btn[0] = V4P_TASTE_A;
    btn[1] = V4P_TASTE_B;
    btn[2] = V4P_TASTE_START;
    CHECK_EQ(v4_set_port_routing(&m, 0u, V4P_DIR_DPAD, btn), V4_ERR_FRAME);
    teardown();

    v4_test_case("SET_PORT_MODE 0xFF: Vorgabe zurueck auf \"nach "
                 "Geraeteart\", wirksame Betriebsart bleibt");
    setup(&m);
    /* Erst eine Vorgabe setzen ... */
    CHECK_EQ(v4_set_port_mode(&m, 0u, V4P_PORT_MOUSE, &applied), V4P_ST_OK);
    CHECK_EQ(applied, V4P_PORT_MOUSE);
    CHECK_EQ(v4_get_port_config(&m, &c), V4P_ST_OK);
    CHECK_EQ(c.mode_soll[0], V4P_PORT_MOUSE);
    CHECK_EQ(c.mode_ist[0], V4P_PORT_MOUSE);
    /* ... dann zuruecknehmen. Das Echo ist 0xFF, NICHT die wirksame
     * Betriebsart -- das ist die Zusage der ESP32-Seite. */
    CHECK_EQ(v4_set_port_mode(&m, 0u, (uint8_t)V4P_MODE_AUTO, &applied),
             V4P_ST_OK);
    CHECK_EQ(applied, V4P_MODE_AUTO);
    CHECK_EQ(v4_get_port_config(&m, &c), V4P_ST_OK);
    CHECK_EQ(c.mode_soll[0], V4P_MODE_AUTO);
    CHECK_EQ(c.mode_ist[0], V4P_PORT_MOUSE);        /* laeuft weiter */
    /* Und GET_PORT_MODE meldet weiter die WIRKSAME Betriebsart. */
    {
        uint8_t mode[V4P_PORT_COUNT];

        CHECK_EQ(v4_get_port_mode(&m, mode), V4P_ST_OK);
        CHECK_EQ(mode[0], V4P_PORT_MOUSE);
    }
    /* Unfug bleibt Unfug: mode 3 wird lokal abgewiesen. */
    CHECK_EQ(v4_set_port_mode(&m, 0u, 3u, NULL), V4P_ST_BAD_ARG);
    teardown();

    v4_test_case("SET_DEVICE_PORT: BDA -> Buchse, Echo geprueft");
    setup(&m);
    bda[0] = 0x00u; bda[1] = 0x1Bu; bda[2] = 0xDCu;
    bda[3] = 0x44u; bda[4] = 0x55u; bda[5] = 0x66u;     /* Index 1 */
    CHECK_EQ(v4_set_device_port(&m, bda, 0u), V4P_ST_OK);
    CHECK_EQ(v4_mock_dev_port(1), 0u);
    CHECK_EQ(v4_mock_dev_port(0), V4P_NO_INDEX);        /* unberuehrt */
    CHECK_EQ(v4_set_device_port(&m, bda, 2u), V4P_ST_BAD_ARG);
    CHECK_EQ(v4_set_device_port(&m, NULL, 0u), V4_ERR_ARG);
    teardown();

    v4_test_case("fw_ver 3 (ohne die neuen Befehle): PING wird ANGENOMMEN, "
                 "die neuen Befehle melden BAD_CMD");
    v4_mock_reset();
    v4_mock_seed_default();
    v4_mock.legacy_fw3 = 1;
    CHECK_EQ(v4_open(&m, NULL), 0);
    CHECK_EQ(v4_ping(&m, &info), V4P_ST_OK);
    CHECK_EQ(info.fw_ver, 3u);
    CHECK_EQ(v4_check_info(&info), V4P_ST_OK);      /* gleicher Rahmen! */
    CHECK_EQ(v4_info_has_caps(&info, V4P_CAP_PORT_ROUTING), 0);
    CHECK_EQ(v4_get_port_config(&m, &c), V4P_ST_BAD_CMD);
    btn[0] = V4P_TASTE_A;
    btn[1] = V4P_TASTE_B;
    btn[2] = V4P_TASTE_START;
    CHECK_EQ(v4_set_port_routing(&m, 0u, V4P_DIR_DPAD, btn), V4P_ST_BAD_CMD);
    CHECK_EQ(v4_set_device_port(&m, s_bda[0], 0u), V4P_ST_BAD_CMD);
    /* SET_PORT_MODE mit 0xFF kennt nur fw_ver 4. */
    CHECK_EQ(v4_set_port_mode(&m, 0u, (uint8_t)V4P_MODE_AUTO, &applied),
             V4P_ST_BAD_ARG);
    /* Die alten Befehle arbeiten weiter. */
    CHECK_EQ(v4_set_port_mode(&m, 0u, V4P_PORT_MOUSE, &applied), V4P_ST_OK);
    CHECK_EQ(applied, V4P_PORT_MOUSE);
    teardown();
}

/* ------------------------------------------------------------------ */
/* Bedienhilfe "Beitrag" -- KEIN Protokollbefehl                       */
/* ------------------------------------------------------------------ */

static void test_beitrag(void)
{
    v4_master_t   m;
    v4p_portcfg_t c;
    uint8_t       btn[V4P_BTN_COUNT];
    uint8_t       dir;

    v4_test_case("Beitrag: Tabelle Variante -> Richtungen und Knopfquellen");
    CHECK_EQ(v4_beitrag_dir(V4P_BEITRAG_ANALOG), V4P_DIR_STICK_L);
    CHECK_EQ(v4_beitrag_dir(V4P_BEITRAG_DIGITAL), V4P_DIR_DPAD);
    CHECK_EQ(v4_beitrag_dir(V4P_BEITRAG_BEIDE), V4P_DIR_ALLE);
    CHECK_EQ(v4_beitrag_dir(V4P_BEITRAG_EIGENE), 0u);   /* keine Variante */
    CHECK_EQ(v4_beitrag_dir(200u), 0u);

    v4_beitrag_btn(V4P_BEITRAG_ANALOG, btn);
    CHECK_EQ(btn[0], V4P_TASTE_L1);                 /* Pin 6  links  */
    CHECK_EQ(btn[1], V4P_TASTE_R1);                 /* Pin 9  rechts */
    CHECK_EQ(btn[2], V4P_TASTE_KEINE);              /* Pin 5  offen  */
    v4_beitrag_btn(V4P_BEITRAG_DIGITAL, btn);
    CHECK_EQ(btn[0], V4P_TASTE_A);
    CHECK_EQ(btn[1], V4P_TASTE_B);
    CHECK_EQ(btn[2], V4P_TASTE_START);
    v4_beitrag_btn(V4P_BEITRAG_BEIDE, btn);
    CHECK_EQ(btn[0], V4P_TASTE_GRUPPE1);            /* zusammen alle Tasten */
    CHECK_EQ(btn[1], V4P_TASTE_GRUPPE2);
    CHECK_EQ(btn[2], V4P_TASTE_GRUPPE3);
    CHECK_EQ(v4p_btn_set_valid(btn), 1);            /* keine Doppelbelegung */
    v4_beitrag_btn(V4P_BEITRAG_EIGENE, btn);
    CHECK_EQ(btn[0], V4P_TASTE_KEINE);
    CHECK_EQ(btn[1], V4P_TASTE_KEINE);
    CHECK_EQ(btn[2], V4P_TASTE_KEINE);
    v4_beitrag_btn(V4P_BEITRAG_ANALOG, NULL);       /* darf nicht stuerzen */

    v4_test_case("Beitrag: Richtungsmaske -> Variante (alle acht Masken)");
    CHECK_EQ(v4_beitrag_aus_dir(0x00u), V4P_BEITRAG_EIGENE);
    CHECK_EQ(v4_beitrag_aus_dir(V4P_DIR_STICK_L), V4P_BEITRAG_ANALOG);
    CHECK_EQ(v4_beitrag_aus_dir(V4P_DIR_STICK_R), V4P_BEITRAG_ANALOG);
    CHECK_EQ(v4_beitrag_aus_dir((uint8_t)(V4P_DIR_STICK_L
                                          | V4P_DIR_STICK_R)),
             V4P_BEITRAG_ANALOG);
    CHECK_EQ(v4_beitrag_aus_dir(V4P_DIR_DPAD), V4P_BEITRAG_DIGITAL);
    CHECK_EQ(v4_beitrag_aus_dir((uint8_t)(V4P_DIR_STICK_L | V4P_DIR_DPAD)),
             V4P_BEITRAG_BEIDE);
    CHECK_EQ(v4_beitrag_aus_dir((uint8_t)(V4P_DIR_STICK_R | V4P_DIR_DPAD)),
             V4P_BEITRAG_BEIDE);
    CHECK_EQ(v4_beitrag_aus_dir(V4P_DIR_ALLE), V4P_BEITRAG_BEIDE);

    v4_test_case("Beitrag: Klartext");
    CHECK_STR(v4_beitrag_name(V4P_BEITRAG_ANALOG), "analog");
    CHECK_STR(v4_beitrag_name(V4P_BEITRAG_DIGITAL), "digital");
    CHECK_STR(v4_beitrag_name(V4P_BEITRAG_BEIDE), "beide");
    CHECK_STR(v4_beitrag_name(V4P_BEITRAG_EIGENE), "eigene");

    v4_test_case("Beitrag analog/digital/beide setzen nur SET_PORT_ROUTING");
    setup(&m);
    CHECK_EQ(v4_set_port_beitrag(&m, 0u, V4P_BEITRAG_ANALOG), V4P_ST_OK);
    CHECK_EQ(v4_get_port_config(&m, &c), V4P_ST_OK);
    CHECK_EQ(c.dir[0], V4P_DIR_STICK_L);
    CHECK_EQ(c.btn[0][0], V4P_TASTE_L1);
    CHECK_EQ(c.btn[0][1], V4P_TASTE_R1);
    CHECK_EQ(c.btn[0][2], V4P_TASTE_KEINE);
    CHECK_EQ(v4_beitrag_aus_dir(c.dir[0]), V4P_BEITRAG_ANALOG);

    CHECK_EQ(v4_set_port_beitrag(&m, 1u, V4P_BEITRAG_DIGITAL), V4P_ST_OK);
    CHECK_EQ(v4_get_port_config(&m, &c), V4P_ST_OK);
    CHECK_EQ(c.dir[1], V4P_DIR_DPAD);
    CHECK_EQ(c.btn[1][0], V4P_TASTE_A);
    CHECK_EQ(c.btn[1][1], V4P_TASTE_B);
    CHECK_EQ(c.btn[1][2], V4P_TASTE_START);
    CHECK_EQ(v4_beitrag_aus_dir(c.dir[1]), V4P_BEITRAG_DIGITAL);

    CHECK_EQ(v4_set_port_beitrag(&m, 0u, V4P_BEITRAG_BEIDE), V4P_ST_OK);
    CHECK_EQ(v4_get_port_config(&m, &c), V4P_ST_OK);
    CHECK_EQ(c.dir[0], V4P_DIR_ALLE);
    CHECK_EQ(c.btn[0][0], V4P_TASTE_GRUPPE1);
    CHECK_EQ(c.btn[0][1], V4P_TASTE_GRUPPE2);
    CHECK_EQ(c.btn[0][2], V4P_TASTE_GRUPPE3);
    CHECK_EQ(v4_beitrag_aus_dir(c.dir[0]), V4P_BEITRAG_BEIDE);
    /* Buchse 1 bleibt bei ihrem digitalen Stand. */
    CHECK_EQ(c.dir[1], V4P_DIR_DPAD);
    teardown();

    v4_test_case("Beitrag \"eigene\"/unbekannt -> V4_ERR_ARG, kein Rahmen");
    setup(&m);
    {
        unsigned long vorher = v4_mock.writes;

        CHECK_EQ(v4_set_port_beitrag(&m, 0u, V4P_BEITRAG_EIGENE), V4_ERR_ARG);
        CHECK_EQ(v4_set_port_beitrag(&m, 0u, 200u), V4_ERR_ARG);
        CHECK_EQ(v4_set_port_beitrag(&m, 2u, V4P_BEITRAG_ANALOG),
                 V4P_ST_BAD_ARG);
        CHECK_EQ(v4_mock.writes, vorher);
    }
    /* Die Vorgabe ab Werk ist "beide" -- sie bleibt unberuehrt. */
    CHECK_EQ(v4_get_port_config(&m, &c), V4P_ST_OK);
    CHECK_EQ(c.dir[0], V4P_DIR_STANDARD);
    dir = v4_beitrag_aus_dir(c.dir[0]);
    CHECK_EQ(dir, V4P_BEITRAG_BEIDE);
    teardown();
}

/* ------------------------------------------------------------------ */
/* Maus-Einstellungen (fw_ver 5)                                       */
/* ------------------------------------------------------------------ */

static void test_mousecfg(void)
{
    v4_master_t    m;
    v4p_info_t     info;
    v4p_mousecfg_t c;

    v4_test_case("GET_MOUSE_CFG: Vorgaben der Firmware (Tempo 7, Totzone 12)");
    setup(&m);
    memset(&c, 0, sizeof(c));
    CHECK_EQ(v4_get_mouse_cfg(&m, &c), V4P_ST_OK);
    CHECK_EQ(c.speed[0], V4P_MOUSE_SPEED_VORGABE);
    CHECK_EQ(c.speed[1], V4P_MOUSE_SPEED_VORGABE);
    CHECK_EQ(c.totzone[0], V4P_MOUSE_TOTZONE_VORGABE);
    CHECK_EQ(c.totzone[1], V4P_MOUSE_TOTZONE_VORGABE);
    CHECK_EQ(v4_get_mouse_cfg(&m, NULL), V4_ERR_ARG);
    teardown();

    v4_test_case("SET_MOUSE_CFG: eine Buchse wird gesetzt, die andere bleibt");
    setup(&m);
    CHECK_EQ(v4_set_mouse_cfg(&m, 0u, 3u, 40u), V4P_ST_OK);
    CHECK_EQ(v4_get_mouse_cfg(&m, &c), V4P_ST_OK);
    CHECK_EQ(c.speed[0], 3u);
    CHECK_EQ(c.totzone[0], 40u);
    CHECK_EQ(c.speed[1], V4P_MOUSE_SPEED_VORGABE);      /* unberuehrt */
    CHECK_EQ(c.totzone[1], V4P_MOUSE_TOTZONE_VORGABE);
    CHECK_EQ(v4_set_mouse_cfg(&m, 1u, 10u, 100u), V4P_ST_OK);
    CHECK_EQ(v4_get_mouse_cfg(&m, &c), V4P_ST_OK);
    CHECK_EQ(c.speed[1], 10u);
    CHECK_EQ(c.totzone[1], 100u);
    teardown();

    v4_test_case("SET_MOUSE_CFG: unzulaessige Werte lokal -> BAD_ARG, "
                 "kein Rahmen, nichts geaendert");
    setup(&m);
    {
        unsigned long vorher = v4_mock.writes;

        CHECK_EQ(v4_set_mouse_cfg(&m, 0u, 0u, 12u), V4P_ST_BAD_ARG);
        CHECK_EQ(v4_set_mouse_cfg(&m, 0u, 11u, 12u), V4P_ST_BAD_ARG);
        CHECK_EQ(v4_set_mouse_cfg(&m, 0u, 5u, 101u), V4P_ST_BAD_ARG);
        CHECK_EQ(v4_set_mouse_cfg(&m, 2u, 5u, 12u), V4P_ST_BAD_ARG);
        CHECK_EQ(v4_mock.writes, vorher);
    }
    CHECK_EQ(v4_get_mouse_cfg(&m, &c), V4P_ST_OK);
    CHECK_EQ(c.speed[0], V4P_MOUSE_SPEED_VORGABE);
    teardown();

    v4_test_case("SET_MOUSE_CFG: BAD_ARG des Slaves kommt durch, nichts "
                 "geaendert");
    setup(&m);
    v4_mock.force_status = V4P_ST_BAD_ARG;
    CHECK_EQ(v4_set_mouse_cfg(&m, 0u, 4u, 30u), V4P_ST_BAD_ARG);
    CHECK_EQ(v4_get_mouse_cfg(&m, &c), V4P_ST_OK);
    CHECK_EQ(c.speed[0], V4P_MOUSE_SPEED_VORGABE);
    CHECK_EQ(c.totzone[0], V4P_MOUSE_TOTZONE_VORGABE);
    teardown();

    v4_test_case("SET_MOUSE_CFG: falsches Echo -> V4_ERR_FRAME");
    setup(&m);
    v4_mock.bad_mousecfg_echo = 1;
    CHECK_EQ(v4_set_mouse_cfg(&m, 0u, 4u, 30u), V4_ERR_FRAME);
    teardown();

    v4_test_case("fw_ver 4 (ohne Maus-Einstellungen): PING wird ANGENOMMEN, "
                 "0x0B/0x0C melden BAD_CMD");
    v4_mock_reset();
    v4_mock_seed_default();
    v4_mock.legacy_fw4 = 1;
    CHECK_EQ(v4_open(&m, NULL), 0);
    CHECK_EQ(v4_ping(&m, &info), V4P_ST_OK);
    CHECK_EQ(info.fw_ver, 4u);
    CHECK_EQ(v4_check_info(&info), V4P_ST_OK);      /* gleicher Rahmen! */
    CHECK_EQ(v4_info_has_caps(&info, V4P_CAP_MOUSE_CFG), 0);
    /* Die Eingangsverteilung ist bei fw_ver 4 sehr wohl da. */
    CHECK_EQ(v4_info_has_caps(&info, V4P_CAP_PORT_ROUTING), 1);
    CHECK_EQ(v4_get_mouse_cfg(&m, &c), V4P_ST_BAD_CMD);
    CHECK_EQ(v4_set_mouse_cfg(&m, 0u, 4u, 30u), V4P_ST_BAD_CMD);
    teardown();
}

/* ------------------------------------------------------------------ */

int test_master(void)
{
    v4_test_begin("Transaktionskern und Wrapper gegen den Mock");

    test_init_and_args();
    test_ping_and_info();
    test_status_and_hid();
    test_ports();
    test_routing();
    test_beitrag();
    test_mousecfg();
    test_devices_and_connect();
    test_link_failures();
    test_seq_discipline();
    test_wait_backoff();

    return v4_test_failures;
}
