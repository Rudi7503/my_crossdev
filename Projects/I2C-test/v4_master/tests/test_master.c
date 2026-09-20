/*
 * test_master.c -- Verhaltensmatrix aus PROTOCOL_V4_MASTER.md §14.1 und
 * Grenzfaelle aus §14.3, gefahren gegen den ESP32-Mock.
 *
 * Jeder Testfall baut einen frischen Mock auf und benutzt denselben
 * Master-Code, der auch auf der V4 laeuft.
 */

#include "v4_test.h"

#include "../v4_master.h"
#include "../v4_mock.h"

static v4_master_t M;
static uint8_t     scratch[V4P_BULK_OVERHEAD + V4P_CHUNK_MAX];

#define COLLECT_MAX (V4P_BULK_OVERHEAD + V4P_CHUNK_MAX)

typedef struct {
    uint8_t  buf[80000];
    uint32_t len;
    int      calls;
    int      overflow;
    uint16_t last_block;
    int      block_gap;
} collect_t;

static collect_t C;

static void collect_cb(uint16_t block, const uint8_t *data, uint16_t len,
                       void *ctx)
{
    collect_t *c = (collect_t *)ctx;

    if ((uint32_t)c->len + (uint32_t)len > (uint32_t)sizeof(c->buf)) {
        c->overflow = 1;
        return;
    }
    if (c->calls > 0 && block != (uint16_t)(c->last_block + 1u)) {
        c->block_gap = 1;
    }
    memcpy(c->buf + c->len, data, len);
    c->len += (uint32_t)len;
    c->last_block = block;
    c->calls++;
}

static void collect_reset(void)
{
    memset(&C, 0, sizeof(C));
}

static void fresh(void)
{
    v4_mock_reset();
    v4_mock_seed_default();
    v4_init(&M);
    CHECK_EQ(v4_plat_open(NULL), 0);
    collect_reset();
}

/* Muster der Seed-Dateien: Byte i ist (i & 0xFF). */
static int pattern_ok(const uint8_t *buf, uint32_t len)
{
    uint32_t i;

    for (i = 0; i < len; i++) {
        if (buf[i] != (uint8_t)(i & 0xFFu)) {
            return 0;
        }
    }
    return 1;
}

/* ------------------------------------------------------------------ */
/* §14.1 Normalbetrieb                                                 */
/* ------------------------------------------------------------------ */

static void test_normal(void)
{
    v4p_info_t   info;
    v4p_status_t st;
    v4p_dev_t    d;
    uint8_t      n = 0u;

    fresh();

    v4_test_case("§14.1 Normalbetrieb: PING/GET_INFO");
    CHECK_EQ(v4_ping(&M, &info), V4P_ST_OK);
    CHECK_EQ(info.proto_ver, V4P_PROTO_VER);
    CHECK_EQ(info.write_frame_len, 32u);
    CHECK_EQ(info.read_frame_len, 128u);
    CHECK_EQ(info.bulk_payload_max, V4P_CHUNK_MAX);
    CHECK_EQ(info.chunk, V4P_CHUNK_DEFAULT);
    CHECK_EQ(info.max_devices, V4P_MAX_DEVICES);
    CHECK_EQ(info.path_max, V4P_PATH_MAX);

    CHECK_EQ(v4_get_info(&M, &info), V4P_ST_OK);
    CHECK_EQ(info.proto_ver, V4P_PROTO_VER);

    v4_test_case("§14.1 Normalbetrieb: GET_STATUS");
    CHECK_EQ(v4_get_status(&M, &st), V4P_ST_OK);
    CHECK_EQ(st.dev_count, 3u);
    CHECK_EQ(st.state, V4P_STATE_IDLE);
    CHECK_EQ(st.conn_index, 0xFFu);
    CHECK_EQ(st.sd_mounted, 1u);
    CHECK_EQ(st.scan_active, 0u);
    CHECK_EQ(st.chunk, V4P_CHUNK_DEFAULT);
    CHECK_EQ(st.proto_ver, V4P_PROTO_VER);
    CHECK_EQ(st.fw_ver, 2u);

    v4_test_case("§14.1 Normalbetrieb: DEV_COUNT/DEV_GET");
    CHECK_EQ(v4_dev_count(&M, &n), V4P_ST_OK);
    CHECK_EQ(n, 3u);
    CHECK_EQ(v4_dev_get(&M, 0u, &d), V4P_ST_OK);
    CHECK_EQ(d.idx, 0u);
    CHECK_STR(d.name, "JBL Flip 5");
    CHECK_EQ(d.name_len, 10u);
    CHECK_EQ(d.bda[0], 0x11u);
    CHECK_EQ(d.bda[5], 0x66u);
    CHECK_EQ(v4_dev_get(&M, 2u, &d), V4P_ST_OK);
    CHECK_STR(d.name, "ESP32-A2DP-Test");

    v4_test_case("kein versteckter Retry im Normalfall");
    CHECK_EQ(M.retries, 0ul);
    CHECK_EQ(M.busy_rounds, 0ul);
    CHECK_EQ(M.badcrc_rounds, 0ul);
    CHECK_EQ(M.tx_frames, 6ul);          /* 6 Befehle, 6 Frames */
}

static void test_seq_and_link(void)
{
    v4p_status_t st;
    v4p_dev_t    d;
    int          i;

    fresh();

    v4_test_case("§6 SEQ zaehlt nur nach gueltiger Antwort weiter");
    for (i = 0; i < 5; i++) {
        CHECK_EQ(v4_get_status(&M, &st), V4P_ST_OK);
    }
    CHECK_EQ(M.seq, 5u);
    for (i = 0; i < 251; i++) {
        CHECK_EQ(v4_dev_get(&M, 1u, &d), V4P_ST_OK);
    }
    CHECK_EQ(M.seq, 0u);                 /* 256. Befehl -> Umlauf */
    CHECK_EQ(M.tx_frames, 256ul);

    v4_test_case("§11 Verbindung: CONNECT/DISCONNECT/Status");
    fresh();
    CHECK_EQ(v4_disconnect(&M), V4P_ST_BAD_STATE);
    CHECK_EQ(v4_connect(&M, 99u), V4P_ST_NOT_FOUND);
    CHECK_EQ(v4_connect(&M, 1u), V4P_ST_OK);
    CHECK_EQ(v4_get_status(&M, &st), V4P_ST_OK);
    CHECK_EQ(st.state, V4P_STATE_CONNECTED);
    CHECK_EQ(st.conn_index, 1u);
    CHECK_EQ(v4_disconnect(&M), V4P_ST_OK);
    CHECK_EQ(v4_get_status(&M, &st), V4P_ST_OK);
    CHECK_EQ(st.state, V4P_STATE_IDLE);
    CHECK_EQ(st.conn_index, 0xFFu);

    v4_test_case("§11 CONNECT_BDA");
    {
        static const uint8_t good[6] = { 0xAAu, 0xBBu, 0xCCu, 0xDDu, 0xEEu, 0xFFu };
        static const uint8_t bad[6]  = { 1u, 2u, 3u, 4u, 5u, 6u };
        CHECK_EQ(v4_connect_bda(&M, good), V4P_ST_OK);
        CHECK_EQ(v4_connect_bda(&M, bad), V4P_ST_BT_ERR);
        CHECK_EQ(v4_get_status(&M, &st), V4P_ST_OK);
        CHECK_EQ(st.conn_index, 1u);
        CHECK_EQ(v4_forget(&M), V4P_ST_OK);
    }
}

/* ------------------------------------------------------------------ */
/* §14.1 Stoerungen                                                    */
/* ------------------------------------------------------------------ */

static void test_framing_errors(void)
{
    v4p_status_t st;
    int          i;

    fresh();

    v4_test_case("§14.1 Framing-Fehler: jede 3. Antwort mit falscher CRC");
    v4_mock.bad_crc_every = 3;
    for (i = 0; i < 20; i++) {
        CHECK_EQ(v4_get_status(&M, &st), V4P_ST_OK);
    }
    CHECK(M.retries > 0ul);
    /* R1 (§1.3): auf jedes abgesetzte Schreiben folgt genau ein Leseversuch.
     * Damit ist die Wiederholung belegt, ohne die Anzahl aus denselben
     * Zaehlern abzuleiten (das waere tautologisch). */
    CHECK_EQ(M.rx_frames, M.tx_frames);
    CHECK(M.tx_frames > 20ul);
    CHECK_EQ(M.last_check, V4P_CHECK_CRC);

    fresh();

    v4_test_case("§14.1 Sequenz-Vertauschung: SEQ nicht zurueckgespiegelt");
    v4_mock.no_seq_echo_every = 2;
    for (i = 0; i < 20; i++) {
        CHECK_EQ(v4_get_status(&M, &st), V4P_ST_OK);
    }
    CHECK(M.retries > 0ul);
    CHECK_EQ(M.last_check, V4P_CHECK_ECHO);

    fresh();

    v4_test_case("§4.2 len-Feld unzulaessig wird verworfen");
    v4_mock.bad_frame_len_every = 2;
    for (i = 0; i < 10; i++) {
        CHECK_EQ(v4_get_status(&M, &st), V4P_ST_OK);
    }
    CHECK(M.retries > 0ul);
    CHECK_EQ(M.last_check, V4P_CHECK_LEN);
}

static void test_busy(void)
{
    v4p_status_t st;

    fresh();

    v4_test_case("§14.1 BUSY-Kette: 3x BUSY, dann OK");
    v4_mock.busy_count = 3;
    CHECK_EQ(v4_get_status(&M, &st), V4P_ST_OK);
    CHECK_EQ(M.busy_rounds, 3ul);
    CHECK_EQ(M.tx_frames, 4ul);
    CHECK_EQ(M.seq, 1u);                 /* nur einmal weitergezaehlt */

    fresh();

    v4_test_case("§14.1 BUSY-Dauerfeuer: V4_ERR_BUSY nach 40 Versuchen");
    v4_mock.never_ready = 1;
    CHECK_EQ(v4_get_status(&M, &st), V4_ERR_BUSY);
    CHECK_EQ(M.tx_frames, (unsigned long)V4_BUSY_TRIES);
    CHECK_EQ(M.busy_rounds, (unsigned long)V4_BUSY_TRIES);
    CHECK_EQ(M.seq, 0u);

    fresh();

    v4_test_case("§5.1 BAD_CRC: ganzer Befehl neu, gleiche SEQ");
    v4_mock.force_status = V4P_ST_BAD_CRC;
    CHECK_EQ(v4_get_status(&M, &st), V4P_ST_OK);
    CHECK_EQ(M.badcrc_rounds, 1ul);
    CHECK_EQ(M.tx_frames, 2ul);
    CHECK_EQ(M.seq, 1u);
    CHECK_EQ(v4_mock.seq_log[0], v4_mock.seq_log[1]);
}

static void test_link_dead(void)
{
    v4p_status_t st;
    int          i;

    fresh();

    v4_test_case("§14.1 Link tot (Schreiben): V4_ERR_LINK nach 4 Versuchen");
    v4_mock.fail_write = 1;
    CHECK_EQ(v4_get_status(&M, &st), V4_ERR_LINK);
    CHECK_EQ(M.tx_frames, (unsigned long)V4_RETRIES);
    CHECK_EQ(M.retries, (unsigned long)V4_RETRIES);

    fresh();

    v4_test_case("§14.1 Link tot (Lesen): V4_ERR_LINK nach 4 Versuchen");
    v4_mock.fail_read = 1;
    CHECK_EQ(v4_get_status(&M, &st), V4_ERR_LINK);
    CHECK_EQ(M.tx_frames, (unsigned long)V4_RETRIES);

    fresh();

    v4_test_case("§6 Wiederholung benutzt IMMER dieselbe SEQ");
    v4_mock.bad_crc_every = 1;           /* jede Antwort unbrauchbar */
    CHECK_EQ(v4_get_status(&M, &st), V4_ERR_LINK);
    CHECK_EQ(v4_mock.seq_log_n, V4_RETRIES);
    for (i = 1; i < V4_RETRIES; i++) {
        CHECK_EQ(v4_mock.seq_log[i], v4_mock.seq_log[0]);
    }
    CHECK_EQ(M.seq, 0u);
}

static void test_status_no_retry(void)
{
    v4p_read_t   r;
    v4p_dirent_t e;
    uint8_t      h = 0u;
    uint32_t     size = 0u;
    uint8_t      attr = 0u;
    unsigned long tx;

    fresh();

    v4_test_case("§14.1 NOT_FOUND: Rueckgabewert 0x07, kein Retry-Sturm");
    {
        v4p_dev_t d;

        tx = M.tx_frames;
        CHECK_EQ(v4_dev_get(&M, 99u, &d), V4P_ST_NOT_FOUND);
        CHECK_EQ(M.tx_frames, tx + 1ul);
    }

    v4_test_case("§11 unbekannter Befehl wird nicht wiederholt");
    tx = M.tx_frames;
    /* 0x55 ist in §11 nicht vergeben (PLAY_FILE/STOP_PLAY sind es jetzt). */
    CHECK_EQ(v4_transact(&M, 0x55u, NULL, 0u, &r), V4P_ST_BAD_CMD);
    CHECK_EQ(M.tx_frames, tx + 1ul);

    v4_test_case("§11 NOT_FOUND fuer Pfade und Geraete");
    tx = M.tx_frames;
    CHECK_EQ(v4_dir_open(&M, "GIBTSNICHT", &h), V4P_ST_NOT_FOUND);
    CHECK_EQ(v4_file_open(&M, "GIBTSNICHT.MP3", &h, &size, &attr), V4P_ST_NOT_FOUND);
    CHECK_EQ(M.tx_frames, tx + 2ul);

    v4_test_case("§11 NO_SD wenn keine Karte steckt");
    fresh();
    v4_mock_set_sd(0, 0u, 0u);
    {
        v4p_sdinfo_t si;
        CHECK_EQ(v4_sd_info(&M, &si), V4P_ST_NO_SD);
        CHECK_EQ(v4_dir_open(&M, "MUSIC", &h), V4P_ST_NO_SD);
        CHECK_EQ(v4_file_open(&M, "TXT.TXT", &h, &size, &attr), V4P_ST_NO_SD);
    }

    v4_test_case("§11 DIR_NEXT ohne offenes Handle");
    CHECK_EQ(v4_dir_next(&M, 1u, 0u, &e), V4P_ST_NO_HANDLE);

    v4_test_case("§11 SET_CHUNK: lokale und Slave-seitige Grenzen");
    fresh();
    /* §11: "Gueltig 16..1024, sonst BAD_ARG" -- der lokale Vorabcheck gibt
     * denselben Statuscode zurueck wie der Slave. */
    CHECK_EQ(v4_set_chunk(&M, 8u), V4P_ST_BAD_ARG);
    CHECK_EQ(v4_set_chunk(&M, 1025u), V4P_ST_BAD_ARG);
    CHECK_EQ(v4_set_chunk(&M, 0u), V4P_ST_BAD_ARG);
    CHECK_EQ(v4_set_chunk(&M, 16u), V4P_ST_OK);
    CHECK_EQ(M.chunk, 16u);
    CHECK_EQ(v4_set_chunk(&M, V4P_CHUNK_MAX), V4P_ST_OK);
    CHECK_EQ(M.chunk, V4P_CHUNK_MAX);
    {
        uint8_t p[2];
        v4p_put_u16le(p, 8u);
        CHECK_EQ(v4_transact(&M, V4P_CMD_SET_CHUNK, p, 2u, &r), V4P_ST_BAD_ARG);
        CHECK_EQ(M.chunk, V4P_CHUNK_MAX);
    }

    v4_test_case("§11 DIR_OPEN auf eine Datei / FILE_OPEN auf ein Verzeichnis");
    fresh();
    CHECK_EQ(v4_dir_open(&M, "TXT.TXT", &h), V4P_ST_IO_ERR);
    CHECK_EQ(v4_file_open(&M, "MUSIC", &h, &size, &attr), V4P_ST_BAD_ARG);
    CHECK_EQ(v4_dir_open(&M, "TXT.TXT/UNTER", &h), V4P_ST_NOT_FOUND);
}

/* ------------------------------------------------------------------ */
/* §14.1/§7.2 Verzeichnis-Retry                                        */
/* ------------------------------------------------------------------ */

static void test_dir_retry(void)
{
    uint8_t      h = 0u;
    v4p_dirent_t e0, e1, e1b, e2;

    fresh();

    v4_test_case("§14.1 DIR_NEXT-Retry: gleicher Eintrag, kein Sprung");
    CHECK_EQ(v4_dir_open(&M, "MUSIC", &h), V4P_ST_OK);
    CHECK_EQ(v4_dir_next(&M, h, 0u, &e0), V4P_ST_OK);

    /* Die Antwort auf Index 1 geht verloren. Der Master wiederholt den
     * Befehl mit derselben SEQ und demselben Index. */
    v4_mock.drop_next_read = 1;
    CHECK_EQ(v4_dir_next(&M, h, 1u, &e1), V4P_ST_OK);
    CHECK(M.retries >= 1ul);

    /* Der Slave muss denselben Eintrag erneut liefern (§7.2) */
    CHECK_EQ(v4_dir_next(&M, h, 1u, &e1b), V4P_ST_OK);
    CHECK_STR(e1.name, e1b.name);
    CHECK_EQ(e1.size, e1b.size);
    CHECK_EQ(e1.attr, e1b.attr);

    CHECK_EQ(v4_dir_next(&M, h, 2u, &e2), V4P_ST_OK);

    CHECK_STR(e0.name, "A.MP3");
    CHECK_STR(e1.name, "B.MP3");
    CHECK_STR(e2.name, "BIG.BIN");
    CHECK_EQ(e0.attr, V4P_ATTR_FILE);
    CHECK_EQ(e2.size, 65536u);
    CHECK_EQ(e0.index, 0u);
    CHECK_EQ(e2.index, 2u);

    v4_test_case("§7.2 nach END ist der Cache ungueltig -> BAD_ARG");
    CHECK_EQ(v4_dir_next(&M, h, 3u, &e2), V4P_ST_END);
    CHECK_EQ(v4_dir_next(&M, h, 4u, &e2), V4P_ST_BAD_ARG);
    CHECK_EQ(v4_dir_close(&M, h), V4P_ST_OK);

    v4_test_case("§7.2 Index ausser Takt wird abgewiesen");
    CHECK_EQ(v4_dir_open(&M, "MUSIC", &h), V4P_ST_OK);
    CHECK_EQ(v4_dir_next(&M, h, 2u, &e2), V4P_ST_BAD_ARG);  /* frisch, index != 0 */
    CHECK_EQ(v4_dir_next(&M, h, 0u, &e0), V4P_ST_OK);
    CHECK_EQ(v4_dir_next(&M, h, 5u, &e2), V4P_ST_BAD_ARG);  /* 5 != 0+1 */
    CHECK_EQ(v4_dir_next(&M, h, 1u, &e1), V4P_ST_OK);       /* 0+1 ist erlaubt */
    CHECK_EQ(v4_dir_close(&M, h), V4P_ST_OK);

    v4_test_case("§11 leeres Verzeichnis liefert sofort END");
    CHECK_EQ(v4_dir_open(&M, "EMPTY", &h), V4P_ST_OK);
    CHECK_EQ(v4_dir_next(&M, h, 0u, &e0), V4P_ST_END);
    CHECK_EQ(v4_dir_close(&M, h), V4P_ST_OK);
}

/* ------------------------------------------------------------------ */
/* §14.1/§8.2 Dateien                                                  */
/* ------------------------------------------------------------------ */

static void test_bulk(void)
{
    v4p_bulk_t b;
    uint8_t    h = 0u;
    uint32_t   size = 0u;
    uint32_t   total = 0u;
    uint8_t    attr = 0u;

    fresh();

    v4_test_case("§14.1 falscher Bulk-Handle wird verworfen und wiederholt");
    CHECK_EQ(v4_file_open(&M, "MUSIC/B.MP3", &h, &size, &attr), V4P_ST_OK);
    CHECK_EQ(size, 37u);
    CHECK_EQ(attr, V4P_ATTR_FILE);
    v4_mock.bulk_wrong_handle = 1;
    CHECK_EQ(v4_file_read(&M, h, 0u, scratch, &b), V4P_ST_OK);
    CHECK(M.retries >= 1ul);
    CHECK_EQ(M.last_check, V4P_CHECK_MISMATCH);
    CHECK_EQ(b.len, 37u);
    CHECK_EQ(b.status, V4P_ST_OK);
    CHECK_EQ(b.handle, h);
    CHECK_EQ(b.block, 0u);
    CHECK_EQ(b.data[0], 0u);
    CHECK_EQ(b.data[36], 36u);
    CHECK_EQ(v4_file_close(&M, h), V4P_ST_OK);

    fresh();

    v4_test_case("§14.1 kurzer letzter Block: len=37 bei chunk=128");
    CHECK_EQ(v4_read_file(&M, "MUSIC/B.MP3", scratch, sizeof(scratch),
                          collect_cb, &C, &total), V4P_ST_OK);
    CHECK_EQ(C.calls, 1);
    CHECK_EQ(C.len, 37u);
    CHECK_EQ(total, 37u);
    CHECK_EQ(C.overflow, 0);
    CHECK_EQ(C.block_gap, 0);
    CHECK(pattern_ok(C.buf, 37u));

    v4_test_case("§8.2 ganzzahliges Vielfaches: BUSY-Runde, dann END");
    fresh();
    {
        unsigned long busy0 = M.busy_rounds;

        CHECK_EQ(v4_read_file(&M, "MUSIC/BIG.BIN", scratch, sizeof(scratch),
                              collect_cb, &C, &total), V4P_ST_OK);
        CHECK_EQ(C.len, 65536u);
        CHECK_EQ(total, 65536u);
        CHECK_EQ(C.calls, 512);          /* 512 * 128 */
        CHECK_EQ(C.block_gap, 0);
        CHECK_EQ(C.overflow, 0);
        CHECK(pattern_ok(C.buf, 65536u));
        /* 512 Bloecke (je eine Cache-Runde) + 1 Runde hinter dem Dateiende.
         * Eine lose Schranke (>= 1) waere hier wertlos, weil die 512
         * Block-Runden sie ohnehin erfuellen. */
        CHECK_EQ(M.busy_rounds, busy0 + 513ul);
    }

    v4_test_case("§14.1 FILE_READ mit 104-Byte-Schlussblock (1000 Byte)");
    fresh();
    CHECK_EQ(v4_read_file(&M, "MUSIC/A.MP3", scratch, sizeof(scratch),
                          collect_cb, &C, &total), V4P_ST_OK);
    CHECK_EQ(C.len, 1000u);
    CHECK_EQ(C.calls, 8);                /* 7*128 + 104 */
    CHECK_EQ(C.last_block, 7u);
    CHECK(pattern_ok(C.buf, 1000u));

    v4_test_case("§8.2 Bufferbedarf: zu kleiner Puffer wird gemeldet");
    fresh();
    {
        uint8_t small[64];
        CHECK_EQ(v4_read_file(&M, "MUSIC/A.MP3", small, sizeof(small),
                              collect_cb, &C, &total), V4_ERR_NOSPACE);
    }

    v4_test_case("§8.2 Lesen ohne Puffer allokiert selbst");
    fresh();
    CHECK_EQ(v4_read_file(&M, "MUSIC/A.MP3", NULL, 0u,
                          collect_cb, &C, &total), V4P_ST_OK);
    CHECK_EQ(C.len, 1000u);

    v4_test_case("§8.2 leere Datei: sofort END, kein Callback");
    fresh();
    {
        static const uint8_t dummy[1] = { 0x00u };
        CHECK(v4_mock_add_file("MUSIC/ZERO.TXT", dummy, 0u) >= 0);
    }
    CHECK_EQ(v4_read_file(&M, "MUSIC/ZERO.TXT", scratch, sizeof(scratch),
                          collect_cb, &C, &total), V4P_ST_OK);
    CHECK_EQ(total, 0u);
    CHECK_EQ(C.calls, 0);
    CHECK_EQ(C.len, 0u);

    v4_test_case("§11 FILE_READ nach FILE_CLOSE -> NO_HANDLE");
    fresh();
    CHECK_EQ(v4_file_open(&M, "MUSIC/B.MP3", &h, &size, &attr), V4P_ST_OK);
    CHECK_EQ(v4_file_read(&M, h, 0u, scratch, &b), V4P_ST_OK);
    CHECK_EQ(v4_file_close(&M, h), V4P_ST_OK);
    CHECK_EQ(v4_file_read(&M, h, 0u, scratch, &b), V4P_ST_NO_HANDLE);
}

/* ------------------------------------------------------------------ */
/* §10/§11 Namensgrenzen und NAME_TRUNCATED                            */
/* ------------------------------------------------------------------ */

static void test_name_truncation(void)
{
    uint8_t      n = 0u;
    v4p_dev_t    d;
    v4p_dirent_t e;
    uint8_t      h = 0u;
    char         longname[128];

    fresh();

    v4_test_case("§11 NAME_TRUNCATED: Geraetename ueber 113 Zeichen");
    memset(longname, 'X', 120);            /* > 113, aber < 128 (Pfadgrenze) */
    longname[120] = '\0';
    {
        static const uint8_t bda[6] = { 9u, 9u, 9u, 9u, 9u, 9u };
        CHECK_EQ(v4_mock_dev_add(longname, bda), 3);      /* nach den 3 Seeds */
    }
    CHECK_EQ(v4_dev_count(&M, &n), V4P_ST_OK);
    CHECK_EQ(n, 4u);
    CHECK_EQ(v4_dev_get(&M, 3u, &d), V4P_ST_OK);
    CHECK_EQ(d.name_len, V4P_DEVNAME_MAX);                /* 113 */
    CHECK_EQ(strlen(d.name), (size_t)V4P_DEVNAME_MAX);
    CHECK_EQ(d.name[0], 'X');
    CHECK_EQ(d.name[V4P_DEVNAME_MAX], '\0');
    CHECK((d.flags & V4P_FLAG_NAME_TRUNCATED) != 0u);
    CHECK(sizeof(d.name) >= (size_t)V4P_DEVNAME_MAX + 1u);
    /* Der kurze Name setzt das Flag nicht */
    CHECK_EQ(v4_dev_get(&M, 0u, &d), V4P_ST_OK);
    CHECK_EQ(d.flags, 0u);

    v4_test_case("§11 NAME_TRUNCATED: Verzeichnisname ueber 113 Zeichen");
    fresh();
    {
        static const uint8_t dummy[1] = { 0x00u };
        char name[136];
        char path[160];
        size_t i;

        /* Eigenes Verzeichnis, damit der lange Name der einzige Eintrag ist
         * (die Rueckgabe von v4_mock_add_* ist ein globaler Knotenindex).
         * "LONGDIR/" (8) + 120 Zeichen = 128 = Pfadgrenze. */
        CHECK(v4_mock_add_dir("LONGDIR") >= 0);
        for (i = 0; i < 120u; i++) {
            name[i] = 'Y';
        }
        name[120] = '\0';
        strcpy(path, "LONGDIR/");
        strcat(path, name);
        CHECK_EQ(strlen(path), 128u);
        CHECK(v4_mock_add_file(path, dummy, 1u) >= 0);
    }
    CHECK_EQ(v4_dir_open(&M, "LONGDIR", &h), V4P_ST_OK);
    CHECK_EQ(v4_dir_next(&M, h, 0u, &e), V4P_ST_OK);
    CHECK_EQ(e.name_len, V4P_DIRNAME_MAX);                /* 113 */
    CHECK_EQ(strlen(e.name), (size_t)V4P_DIRNAME_MAX);
    CHECK_EQ(e.name[V4P_DIRNAME_MAX], '\0');
    CHECK((e.flags & V4P_FLAG_NAME_TRUNCATED) != 0u);
    /* Kein Sprung: auch der gekuerzte Name kommt aus genau einem Eintrag */
    CHECK_EQ(v4_dir_next(&M, h, 1u, &e), V4P_ST_END);
    CHECK_EQ(v4_dir_close(&M, h), V4P_ST_OK);

    v4_test_case("kurze Namen setzen NAME_TRUNCATED nicht");
    fresh();
    CHECK_EQ(v4_dir_open(&M, "MUSIC", &h), V4P_ST_OK);
    CHECK_EQ(v4_dir_next(&M, h, 0u, &e), V4P_ST_OK);
    CHECK_EQ(e.flags, 0u);
    CHECK_STR(e.name, "A.MP3");
    CHECK_EQ(v4_dir_close(&M, h), V4P_ST_OK);
}

/* ------------------------------------------------------------------ */
/* §14.3 Grenzfaelle                                                   */
/* ------------------------------------------------------------------ */

static void test_edge_chunk_change(void)
{
    v4p_bulk_t b;
    uint8_t    h = 0u;
    uint32_t   size = 0u;
    uint8_t    attr = 0u;
    unsigned long busy_before;

    fresh();

    v4_test_case("§14.3 SET_CHUNK waehrend ein Handle offen ist");
    CHECK_EQ(v4_file_open(&M, "MUSIC/A.MP3", &h, &size, &attr), V4P_ST_OK);
    CHECK_EQ(v4_file_read(&M, h, 0u, scratch, &b), V4P_ST_OK);
    CHECK_EQ(b.len, 128u);
    CHECK_EQ(b.data[0], 0u);

    CHECK_EQ(v4_set_chunk(&M, 16u), V4P_ST_OK);
    CHECK_EQ(M.chunk, 16u);
    busy_before = M.busy_rounds;
    /* Der Blockcache wurde verworfen -> naechster FILE_READ ist BUSY (§9) */
    CHECK_EQ(v4_file_read(&M, h, 0u, scratch, &b), V4P_ST_OK);
    CHECK(M.busy_rounds > busy_before);
    CHECK_EQ(b.len, 16u);

    /* Offset rechnet mit dem neuen chunk: Block 1 beginnt bei Byte 16 */
    CHECK_EQ(v4_file_read(&M, h, 1u, scratch, &b), V4P_ST_OK);
    CHECK_EQ(b.len, 16u);
    CHECK_EQ(b.data[0], 16u);

    v4_test_case("§14.3 FILE_READ hinter dem Dateiende");
    CHECK_EQ(v4_file_read(&M, h, 1000u, scratch, &b), V4P_ST_END);

    CHECK_EQ(v4_file_close(&M, h), V4P_ST_OK);

    v4_test_case("§8.2 EOF liefert END nach GENAU einer BUSY-Runde");
    CHECK_EQ(v4_set_chunk(&M, 128u), V4P_ST_OK);   /* chunk zurueck auf 128 */
    CHECK_EQ(v4_file_open(&M, "MUSIC/B.MP3", &h, &size, &attr), V4P_ST_OK);
    busy_before = M.busy_rounds;
    CHECK_EQ(v4_file_read(&M, h, 1u, scratch, &b), V4P_ST_END);
    /* §8.2: "FILE_READ(h, n+1) -> BUSY, dann END" (nach 2 Runden).
     * §14.3 formuliert "zweimal BUSY, dann END" -- dieser Widerspruch ist im
     * README dokumentiert; hier wird §8.2 festgenagelt. */
    CHECK_EQ(M.busy_rounds, busy_before + 1ul);
    CHECK_EQ(v4_file_close(&M, h), V4P_ST_OK);

    v4_test_case("Busy-Kette vor END wird ebenfalls abgefangen");
    CHECK_EQ(v4_file_open(&M, "MUSIC/B.MP3", &h, &size, &attr), V4P_ST_OK);
    CHECK_EQ(v4_file_read(&M, h, 0u, scratch, &b), V4P_ST_OK);
    v4_mock.busy_count = 3;
    busy_before = M.busy_rounds;
    /* 3 erzwungene BUSY plus die Cache-Runde fuer Block 1 */
    CHECK_EQ(v4_file_read(&M, h, 1u, scratch, &b), V4P_ST_END);
    CHECK(M.busy_rounds >= busy_before + 3ul);
    CHECK_EQ(v4_file_close(&M, h), V4P_ST_OK);
}

static void test_edge_paths(void)
{
    uint8_t  h = 0u;
    uint32_t size = 0u;
    uint8_t  attr = 0u;
    unsigned long tx;
    char     p27[32];
    char     frag[32];
    v4p_bulk_t b;

    fresh();

    v4_test_case("§14.3 Pfad mit .. wird abgewiesen");
    CHECK_EQ(v4_dir_open(&M, "MUSIC/../ETC", &h), V4P_ST_BAD_ARG);
    CHECK_EQ(v4_dir_open(&M, "..", &h), V4P_ST_BAD_ARG);
    CHECK_EQ(v4_file_open(&M, "MUSIC/../../X", &h, &size, &attr), V4P_ST_BAD_ARG);
    CHECK_EQ(v4_dir_open(&M, "MUSIC/..", &h), V4P_ST_BAD_ARG);
    CHECK_EQ(v4_dir_open(&M, "A..B", &h), V4P_ST_NOT_FOUND);  /* ".." nur als Teil */

    v4_test_case("§14.3 Pfad > 27 Byte ohne PATH_* -> TOO_LONG, kein Busverkehr");
    tx = M.tx_frames;
    CHECK_EQ(v4_dir_open(&M, "MUSIC/ABCDEFGHIJKLMNOPQRSTUVWXYZ", &h),
             V4P_ST_TOO_LONG);
    CHECK_EQ(M.tx_frames, tx);

    v4_test_case("§10 genau 27 Byte gehen noch raus");
    strcpy(p27, "MUSIC/123456789012345678901");
    CHECK_EQ(strlen(p27), 27u);
    tx = M.tx_frames;
    CHECK_EQ(v4_file_open(&M, p27, &h, &size, &attr), V4P_ST_NOT_FOUND);
    CHECK_EQ(M.tx_frames, tx + 1ul);

    v4_test_case("§10/§11 langer Pfad ueber PATH_CLEAR/PATH_APPEND");
    CHECK_EQ(v4_path_clear(&M), V4P_ST_OK);
    CHECK_EQ(v4_path_append(&M, "MUSIC/"), V4P_ST_OK);
    CHECK_EQ(v4_path_append(&M, "A.MP3"), V4P_ST_OK);
    CHECK_EQ(M.path_len, 11u);
    CHECK_EQ(v4_file_open(&M, NULL, &h, &size, &attr), V4P_ST_OK);
    CHECK_EQ(size, 1000u);
    CHECK_EQ(v4_file_close(&M, h), V4P_ST_OK);

    v4_test_case("§10 Pfadpuffer 128 Byte: Ueberlauf bleibt folgenlos");
    CHECK_EQ(v4_path_clear(&M), V4P_ST_OK);
    memset(frag, '/', 27);
    frag[27] = '\0';
    CHECK_EQ(v4_path_append(&M, frag), V4P_ST_OK);   /*  27 */
    CHECK_EQ(v4_path_append(&M, frag), V4P_ST_OK);   /*  54 */
    CHECK_EQ(v4_path_append(&M, frag), V4P_ST_OK);   /*  81 */
    CHECK_EQ(v4_path_append(&M, frag), V4P_ST_OK);   /* 108 */
    {
        unsigned long tx2 = M.tx_frames;
        CHECK_EQ(v4_path_append(&M, frag), V4P_ST_TOO_LONG);   /* 135 > 128 */
        CHECK_EQ(M.tx_frames, tx2);            /* lokal abgefangen */
        CHECK_EQ(M.path_len, 108u);            /* Puffer unveraendert */
    }
    {
        /* Der Slave lehnt denselben Append ebenfalls ab (§11) */
        v4p_read_t r;
        CHECK_EQ(v4_transact(&M, V4P_CMD_PATH_APPEND, (const uint8_t *)frag,
                             27u, &r), V4P_ST_TOO_LONG);
        CHECK_EQ(M.path_len, 108u);
    }

    v4_test_case("§10 Fragment > 27 Byte wird abgelehnt");
    fresh();
    {
        char big[40];
        memset(big, 'A', 39);
        big[39] = '\0';
        CHECK_EQ(v4_path_append(&M, big), V4P_ST_TOO_LONG);
        CHECK_EQ(M.path_len, 0u);
    }

    v4_test_case("§11 PATH_CLEAR setzt den SLAVE-Puffer zurueck");
    {
        v4p_dirent_t e;

        CHECK_EQ(v4_path_append(&M, "MUSIC/"), V4P_ST_OK);
        /* Vorher zeigt der L=0-Weg auf MUSIC (erster Eintrag A.MP3) ... */
        CHECK_EQ(v4_dir_open(&M, NULL, &h), V4P_ST_OK);
        CHECK_EQ(v4_dir_next(&M, h, 0u, &e), V4P_ST_OK);
        CHECK_STR(e.name, "A.MP3");
        CHECK_EQ(v4_dir_close(&M, h), V4P_ST_OK);

        /* ... nach PATH_CLEAR auf die Wurzel (erster Eintrag MUSIC).
         * Ein Test, der nur den Rueckgabecode und M.path_len prueft, wuerde
         * einen NICHT geleerten Slave-Puffer nicht bemerken. */
        CHECK_EQ(v4_path_clear(&M), V4P_ST_OK);
        CHECK_EQ(M.path_len, 0u);
        CHECK_EQ(v4_dir_open(&M, NULL, &h), V4P_ST_OK);
        CHECK_EQ(v4_dir_next(&M, h, 0u, &e), V4P_ST_OK);
        CHECK_STR(e.name, "MUSIC");
        CHECK_EQ(e.attr, V4P_ATTR_DIR);
        CHECK_EQ(v4_dir_close(&M, h), V4P_ST_OK);
    }

    v4_test_case("§11 RESET leert Pfadpuffer UND Blockcache auf dem ESP32");
    {
        unsigned long busy0;

        CHECK_EQ(v4_path_append(&M, "MUSIC/"), V4P_ST_OK);
        /* Blockcache fuellen */
        CHECK_EQ(v4_file_open(&M, "MUSIC/B.MP3", &h, &size, &attr), V4P_ST_OK);
        CHECK_EQ(v4_file_read(&M, h, 0u, scratch, &b), V4P_ST_OK);
        CHECK_EQ(M.busy_rounds, 1ul);       /* der erste Zugriff war BUSY */

        CHECK_EQ(v4_reset(&M), V4P_ST_OK);
        CHECK_EQ(M.path_len, 0u);
        /* Der Cache muss weg sein: derselbe Block ist wieder BUSY. */
        busy0 = M.busy_rounds;
        CHECK_EQ(v4_file_read(&M, h, 0u, scratch, &b), V4P_ST_OK);
        CHECK_EQ(M.busy_rounds, busy0 + 1ul);
        CHECK_EQ(v4_file_close(&M, h), V4P_ST_OK);

        /* Und der Pfadspiegel ist leer -> L=0 oeffnet die Wurzel. */
        {
            v4p_dirent_t e;
            CHECK_EQ(v4_dir_open(&M, NULL, &h), V4P_ST_OK);
            CHECK_EQ(v4_dir_next(&M, h, 0u, &e), V4P_ST_OK);
            CHECK_STR(e.name, "MUSIC");
            CHECK_EQ(v4_dir_close(&M, h), V4P_ST_OK);
        }
    }
}

static void test_edge_handles(void)
{
    uint8_t  d1 = 0u, d2 = 0u, d3 = 0u;
    uint8_t  f[5];
    uint32_t size = 0u;
    uint8_t  attr = 0u;
    int      i;

    fresh();

    v4_test_case("§14.3 zwei Verzeichnis-Handles, das dritte gibt NO_HANDLE");
    CHECK_EQ(v4_dir_open(&M, "MUSIC", &d1), V4P_ST_OK);
    CHECK_EQ(v4_dir_open(&M, "EMPTY", &d2), V4P_ST_OK);
    CHECK(d1 != d2);
    CHECK_EQ(v4_dir_open(&M, "", &d3), V4P_ST_NO_HANDLE);
    CHECK_EQ(v4_dir_close(&M, d1), V4P_ST_OK);
    CHECK_EQ(v4_dir_open(&M, "", &d3), V4P_ST_OK);       /* frei geworden */
    CHECK_EQ(v4_dir_close(&M, d2), V4P_ST_OK);
    CHECK_EQ(v4_dir_close(&M, d3), V4P_ST_OK);

    v4_test_case("§14.3 vier Datei-Handles, das fuenfte gibt NO_HANDLE");
    for (i = 0; i < 4; i++) {
        CHECK_EQ(v4_file_open(&M, "MUSIC/B.MP3", &f[i], &size, &attr), V4P_ST_OK);
    }
    CHECK_EQ(v4_file_open(&M, "MUSIC/B.MP3", &f[4], &size, &attr),
             V4P_ST_NO_HANDLE);
    for (i = 0; i < 4; i++) {
        CHECK_EQ(v4_file_close(&M, f[i]), V4P_ST_OK);
    }
    CHECK_EQ(v4_file_open(&M, "MUSIC/B.MP3", &f[0], &size, &attr), V4P_ST_OK);
    CHECK_EQ(v4_file_close(&M, f[0]), V4P_ST_OK);

    v4_test_case("§11 DIR_CLOSE ist immer OK, auch fuer unbekannte Handles");
    CHECK_EQ(v4_dir_close(&M, 9u), V4P_ST_OK);
    CHECK_EQ(v4_dir_close(&M, 0u), V4P_ST_OK);
    /* Fuer FILE_CLOSE fordert die Spec nur "Response L=0, verwirft den
     * Blockcache" -- "immer OK" sagt sie nur bei DIR_CLOSE. Beide Antworten
     * sind vertretbar, deshalb wird hier nicht auf OK festgenagelt. */
    {
        uint8_t rc = v4_file_close(&M, 9u);

        CHECK(rc == V4P_ST_OK || rc == V4P_ST_NO_HANDLE);
    }
}

static void test_edge_bus_reset(void)
{
    v4p_status_t st;
    uint8_t      raw[V4P_READ_FRAME_LEN];

    fresh();

    v4_test_case("§14.3 Bus-Reset ZWISCHEN zwei Transfers");
    CHECK_EQ(v4_get_status(&M, &st), V4P_ST_OK);
    v4_mock_bus_reset();
    CHECK_EQ(v4_get_status(&M, &st), V4P_ST_OK);
    CHECK_EQ(st.dev_count, 3u);

    v4_test_case("§14.3 Antwort mitten im Transfer verloren: Retry greift");
    v4_mock.drop_next_read = 1;
    CHECK_EQ(v4_get_status(&M, &st), V4P_ST_OK);
    CHECK(M.retries >= 1ul);

    v4_test_case("R1: Read ohne vorherigen Befehl liefert 0xFF");
    CHECK_EQ(v4_plat_i2c_read(V4_I2C_ADDR7, raw, V4P_READ_FRAME_LEN), 0);
    CHECK_EQ(raw[0], 0xFFu);
    CHECK_EQ(raw[63], 0xFFu);
    /* und der regulaere Verkehr laeuft ungestoert weiter */
    CHECK_EQ(v4_get_status(&M, &st), V4P_ST_OK);
}

/* ------------------------------------------------------------------ */
/* Ablaufhilfen                                                        */
/* ------------------------------------------------------------------ */

/* Der Callback muss den Namen kopieren: der Zeiger zeigt in den
 * Aufruferpuffer, der beim naechsten Aufruf ueberschrieben wird. */
static char g_namebuf[8][V4P_NAME_BUF];
static int  g_count;

static void list_cb(const v4p_dirent_t *ent, void *ctx)
{
    size_t n;

    (void)ctx;
    if (g_count >= 8) {
        return;
    }
    n = (size_t)ent->name_len;
    if (n >= sizeof(g_namebuf[0])) {
        n = sizeof(g_namebuf[0]) - 1u;
    }
    memcpy(g_namebuf[g_count], ent->name, n);
    g_namebuf[g_count][n] = '\0';
    g_count++;
}

static void test_helpers(void)
{
    v4p_status_t st;

    fresh();
    v4_test_case("§8.1 SD_MOUNT(force=1) -> BUSY, dann Zustand abfragen");
    CHECK_EQ(v4_sd_mount(&M, 0), V4P_ST_OK);            /* letztes Ergebnis */
    CHECK_EQ(v4_sd_mount_probe(&M, 1), V4P_ST_BUSY);    /* Quittung */
    /* Ohne Warten bleibt der Mount offen; der §6-Autoretry scheitert daran. */
    CHECK_EQ(v4_sd_mount(&M, 0), V4_ERR_BUSY);
    CHECK_EQ(v4_sd_mount_wait(&M, 4u), V4P_ST_OK);      /* der §8.1-Weg */

    v4_test_case("§8.1 v4_sd_mount_wait fuehrt die Runden selbst");
    fresh();
    CHECK_EQ(v4_sd_mount_wait(&M, 4u), V4P_ST_OK);
    CHECK(M.busy_rounds >= 1ul);        /* die BUSY-Quittung wurde gesehen */

    v4_test_case("§8.1 fehlende Karte -> NO_SD");
    fresh();
    v4_mock.sd_present = 0;             /* Karte physisch nicht gesteckt */
    v4_mock_set_sd(0, 0u, 0u);
    CHECK_EQ(v4_sd_mount_wait(&M, 4u), V4P_ST_NO_SD);

    v4_test_case("§13 v4_wait_state pollt bis CONNECTED");
    fresh();
    v4_mock.connect_rounds = 3;
    CHECK_EQ(v4_connect(&M, 1u), V4P_ST_OK);
    CHECK_EQ(v4_wait_state(&M, V4P_STATE_CONNECTED, 10u, &st), V4P_ST_OK);
    CHECK_EQ(st.state, V4P_STATE_CONNECTED);
    CHECK_EQ(st.conn_index, 1u);

    v4_test_case("§13 v4_wait_state meldet Zeitueberschreitung");
    fresh();
    CHECK_EQ(v4_wait_state(&M, V4P_STATE_CONNECTED, 3u, &st), V4_ERR_TIMEOUT);

    v4_test_case("§7.2 v4_list_dir sammelt alle Eintraege");
    fresh();
    g_count = 0;
    CHECK_EQ(v4_list_dir(&M, "MUSIC", list_cb, NULL), V4P_ST_OK);
    CHECK_EQ(g_count, 3);
    CHECK_STR(g_namebuf[0], "A.MP3");
    CHECK_STR(g_namebuf[1], "B.MP3");
    CHECK_STR(g_namebuf[2], "BIG.BIN");

    v4_test_case("§7.2 v4_list_dir auf ein leeres Verzeichnis");
    fresh();
    g_count = 0;
    CHECK_EQ(v4_list_dir(&M, "EMPTY", list_cb, NULL), V4P_ST_OK);
    CHECK_EQ(g_count, 0);

    v4_test_case("§11 v4_list_dir auf eine Datei -> IO_ERR");
    fresh();
    g_count = 0;
    CHECK_EQ(v4_list_dir(&M, "TXT.TXT", list_cb, NULL), V4P_ST_IO_ERR);

    v4_test_case("§11 v4_sd_info liefert Kapazitaet");
    fresh();
    {
        v4p_sdinfo_t si;
        CHECK_EQ(v4_sd_info(&M, &si), V4P_ST_OK);
        CHECK_EQ(si.total_kb, 8000000u);
        CHECK_EQ(si.free_kb, 491520u);
        CHECK_EQ(si.sector_size, 512u);
        CHECK_EQ(si.fat_type, 0u);
    }

    v4_test_case("§11 SCAN_START/SCAN_STOP");
    fresh();
    CHECK_EQ(v4_scan_start(&M, 8u, 1), V4P_ST_OK);
    CHECK_EQ(v4_get_status(&M, &st), V4P_ST_OK);
    CHECK_EQ(st.scan_active, 1u);
    CHECK(st.scan_gen >= 1u);
    CHECK_EQ(v4_scan_stop(&M), V4P_ST_OK);
    CHECK_EQ(v4_get_status(&M, &st), V4P_ST_OK);
    CHECK_EQ(st.scan_active, 0u);

    v4_test_case("§8 GET_STATUS liefert dev_count/scan_gen weiter");
    fresh();
    {
        uint16_t gen_before;
        CHECK_EQ(v4_get_status(&M, &st), V4P_ST_OK);
        gen_before = st.scan_gen;
        v4_mock_bump_scan_gen();
        CHECK_EQ(v4_get_status(&M, &st), V4P_ST_OK);
        CHECK(st.scan_gen > gen_before);
    }
}

/* ------------------------------------------------------------------ */
/* §1.4/§6: Wartezeit-Invariante (von mehreren Tests benutzt)           */
/* ------------------------------------------------------------------ */

/*
 * R3 (§1.3) und §1.4 als pruefbare Invariante: pro abgesetztem WRITE-Frame
 * genau eine Wartezeit t_wait vor dem Lesen, pro BUSY- und pro
 * BAD_CRC-Runde zusaetzlich t_busy. Ein Master ohne Wartezeit oder mit
 * falscher Wartezeit faellt hier durch.
 */
static void check_delay_invariant(const v4_master_t *m,
                                  unsigned long d0, unsigned long u0,
                                  unsigned long tx0, unsigned long busy0,
                                  unsigned long crc0)
{
    unsigned long dtx   = m->tx_frames - tx0;
    unsigned long dbusy = m->busy_rounds - busy0;
    unsigned long dcrc  = m->badcrc_rounds - crc0;

    CHECK_EQ(v4_mock.delay_calls - d0, dtx + dbusy + dcrc);
    CHECK_EQ(v4_mock.delay_us - u0,
             dtx * (unsigned long)V4_T_WAIT_US
             + (dbusy + dcrc) * (unsigned long)V4_T_BUSY_US);
}

/* ------------------------------------------------------------------ */
/* Trace: das Debuginstrument muss die Schritte in der richtigen        */
/* Reihenfolge melden -- sonst zeigt es beim Hardwarefehler Falsches.   */
/* ------------------------------------------------------------------ */

typedef struct {
    uint8_t event[64];
    uint8_t cmd[64];
    int     rc[64];
    int     n;
} trace_rec_t;

static void trace_rec_cb(const v4_trace_t *ev, void *ctx)
{
    trace_rec_t *t = (trace_rec_t *)ctx;

    if (t->n < 64) {
        t->event[t->n] = ev->event;
        t->cmd[t->n]   = ev->cmd;
        t->rc[t->n]    = ev->rc;
        t->n++;
    }
}

static void test_trace(void)
{
    static trace_rec_t T;
    v4p_status_t       st;

    v4_test_case("Trace: Schritte einer Transaktion in richtiger Reihenfolge");
    fresh();
    memset(&T, 0, sizeof(T));
    M.trace     = trace_rec_cb;
    M.trace_ctx = &T;
    CHECK_EQ(v4_get_status(&M, &st), V4P_ST_OK);
    CHECK_EQ(T.n, 7);
    CHECK_EQ(T.event[0], V4_TR_TX_BEGIN);
    CHECK_EQ(T.event[1], V4_TR_TX_END);
    CHECK_EQ(T.event[2], V4_TR_WAIT);
    CHECK_EQ(T.event[3], V4_TR_RX_BEGIN);
    CHECK_EQ(T.event[4], V4_TR_RX_END);
    CHECK_EQ(T.event[5], V4_TR_CHECK);
    CHECK_EQ(T.event[6], V4_TR_DONE);
    CHECK_EQ(T.cmd[0], V4P_CMD_GET_STATUS);
    CHECK_EQ(T.cmd[6], V4P_CMD_GET_STATUS);
    CHECK_EQ(T.rc[5], V4P_CHECK_OK);
    CHECK_EQ(T.rc[6], V4P_ST_OK);

    v4_test_case("Trace: BUSY-Runde und Framing-Wiederholung sind sichtbar");
    fresh();
    memset(&T, 0, sizeof(T));
    M.trace     = trace_rec_cb;
    M.trace_ctx = &T;
    v4_mock.busy_count = 1;
    CHECK_EQ(v4_get_status(&M, &st), V4P_ST_OK);
    CHECK_EQ(T.n, 14);                      /* 2 Transaktionen a 7 Schritte */
    CHECK_EQ(T.event[6], V4_TR_BUSY);
    CHECK_EQ(T.rc[T.n - 1], V4P_ST_OK);

    fresh();
    memset(&T, 0, sizeof(T));
    M.trace     = trace_rec_cb;
    M.trace_ctx = &T;
    v4_mock.bad_crc_every = 2;              /* zweite Antwort ist kaputt */
    CHECK_EQ(v4_get_status(&M, &st), V4P_ST_OK);
    CHECK_EQ(v4_get_status(&M, &st), V4P_ST_OK);
    {
        int i;
        int saw_retry = 0;

        for (i = 0; i < T.n; i++) {
            if (T.event[i] == V4_TR_RETRY && T.rc[i] == V4P_CHECK_CRC) {
                saw_retry = 1;
            }
        }
        CHECK_EQ(saw_retry, 1);
    }

    v4_test_case("Trace: abgeschaltet (NULL) kostet nichts und stoert nicht");
    fresh();
    M.trace     = NULL;
    M.trace_ctx = NULL;
    CHECK_EQ(v4_get_status(&M, &st), V4P_ST_OK);
    CHECK_EQ(v4_get_status(&M, &st), V4P_ST_OK);
}

/* ------------------------------------------------------------------ */
/* §11: Wiedergabe von der SD-Karte (PLAY_FILE / STOP_PLAY)             */
/* ------------------------------------------------------------------ */

static void test_playback(void)
{
    v4p_status_t  st;
    unsigned long d0, u0, tx0, busy0, crc0;

    v4_test_case("§11 PLAY_FILE: BUSY-Runden, dann OK, Bit gesetzt");
    fresh();
    v4_mock.play_rounds = 2;
    d0 = v4_mock.delay_calls;  u0 = v4_mock.delay_us;
    tx0 = M.tx_frames; busy0 = M.busy_rounds; crc0 = M.badcrc_rounds;
    CHECK_EQ(v4_play_file(&M, "MUSIC/A.MP3"), V4P_ST_OK);
    CHECK_EQ(M.busy_rounds - busy0, 2ul);       /* der Umbau brauchte 2 Runden */
    check_delay_invariant(&M, d0, u0, tx0, busy0, crc0);
    CHECK_EQ(v4_get_status(&M, &st), V4P_ST_OK);
    CHECK((st.audio_flags & V4P_AUDIO_SD_PLAYBACK) != 0u);

    v4_test_case("§11 PLAY_FILE ist bei gleichem Pfad idempotent");
    d0 = v4_mock.delay_calls;  u0 = v4_mock.delay_us;
    tx0 = M.tx_frames; busy0 = M.busy_rounds; crc0 = M.badcrc_rounds;
    CHECK_EQ(v4_play_file(&M, "MUSIC/A.MP3"), V4P_ST_OK);
    CHECK_EQ(M.tx_frames - tx0, 1ul);           /* sofort OK, keine BUSY-Runde */
    CHECK_EQ(M.busy_rounds - busy0, 0ul);
    check_delay_invariant(&M, d0, u0, tx0, busy0, crc0);

    v4_test_case("§11 PLAY_FILE mit anderem Pfad ersetzt die Wiedergabe");
    busy0 = M.busy_rounds;
    CHECK_EQ(v4_play_file(&M, "MUSIC/B.MP3"), V4P_ST_OK);
    CHECK(M.busy_rounds > busy0);

    v4_test_case("§11 STOP_PLAY: zurueck auf den I2S-Eingang, idempotent");
    CHECK_EQ(v4_stop_play(&M), V4P_ST_OK);
    CHECK_EQ(v4_get_status(&M, &st), V4P_ST_OK);
    CHECK_EQ(st.audio_flags & V4P_AUDIO_SD_PLAYBACK, 0u);
    CHECK_EQ(v4_stop_play(&M), V4P_ST_OK);      /* schon Eingang -> sofort OK */

    v4_test_case("§11 PLAY_FILE: Fehlerfaelle ohne Endlos-BUSY");
    fresh();
    CHECK_EQ(v4_play_file(&M, "GIBTSNICHT.MP3"), V4P_ST_NOT_FOUND);
    CHECK_EQ(v4_play_file(&M, "MUSIC"), V4P_ST_BAD_ARG);       /* Verzeichnis */
    CHECK_EQ(v4_play_file(&M, "MUSIC/../X"), V4P_ST_BAD_ARG);  /* .. */
    CHECK_EQ(v4_play_file(&M, "MUSIC/A.MP3"), V4P_ST_OK);
    v4_mock_set_sd(0, 0u, 0u);
    CHECK_EQ(v4_play_file(&M, "MUSIC/A.MP3"), V4P_ST_NO_SD);
    v4_mock_set_sd(1, 8000000u, 491520u);
    v4_mock.play_fail = 1;
    CHECK_EQ(v4_play_file(&M, "MUSIC/B.MP3"), V4P_ST_IO_ERR);
    CHECK(M.tx_frames < (unsigned long)V4_PLAY_BUSY_TRIES);   /* kein Endlos-BUSY */
    /* §8.3: nach einem gescheiterten Umbau schaltet die Firmware die Quelle
     * SELBST auf den I2S-Eingang zurueck. */
    CHECK_EQ(v4_get_status(&M, &st), V4P_ST_OK);
    CHECK_EQ(st.audio_flags & V4P_AUDIO_SD_PLAYBACK, 0u);

    v4_test_case("§11 das Ende der Wiedergabe schaltet selbst zurueck");
    fresh();
    CHECK_EQ(v4_play_file(&M, "MUSIC/A.MP3"), V4P_ST_OK);
    v4_mock_end_playback();
    CHECK_EQ(v4_get_status(&M, &st), V4P_ST_OK);
    CHECK_EQ(st.audio_flags & V4P_AUDIO_SD_PLAYBACK, 0u);

    v4_test_case("§11 PLAY_FILE ueber den PATH_*-Puffer (L = 0)");
    fresh();
    CHECK_EQ(v4_path_clear(&M), V4P_ST_OK);
    CHECK_EQ(v4_path_append(&M, "MUSIC/A.MP3"), V4P_ST_OK);
    CHECK_EQ(v4_play_file(&M, NULL), V4P_ST_OK);
    CHECK_EQ(v4_stop_play(&M), V4P_ST_OK);
}

/* ------------------------------------------------------------------ */
/* §1.4/§6: Zahlen, Wartezeiten und R1 sind protokolltragend            */
/* ------------------------------------------------------------------ */

static void test_spec_pinning(void)
{
    v4p_status_t st;
    v4p_status_t st2;
    unsigned long d0;
    unsigned long u0;
    unsigned long tx0;
    unsigned long busy0;
    unsigned long crc0;
    int           i;

    fresh();

    v4_test_case("§1.4 Zeit- und Zaehlparameter sind numerisch gepinnt");
    CHECK_EQ(V4_T_WAIT_US, 2000);
    CHECK_EQ(V4_T_BUSY_US, 2000);
    CHECK_EQ(V4_MOUNT_WAIT_US, 200000);
    CHECK_EQ(V4_RETRIES, 4);
    CHECK_EQ(V4_BUSY_TRIES, 40);
    CHECK_EQ(V4_POLL_WAIT_US, 50000);
    CHECK_EQ(V4_I2C_ADDR7, 0x50);
    CHECK_EQ(V4_I2C_ADDR8_W, 0xA0);
    CHECK_EQ(V4_BADCRC_TRIES, 4);        /* Klarstellung, siehe README */
    /* §8.3: eigener Umbau-Budgetwert "~1 s (etwa 500 Runden a 2 ms)" */
    CHECK_EQ(V4_PLAY_BUSY_TRIES, 500);
    CHECK_EQ(V4_PLAY_BUSY_TRIES * (int)V4_T_BUSY_US, 1000000);   /* 1 s */

    v4_test_case("i2c.library-Konvention: CC != 0 heisst OK");
    /* Werte nach i2c.generic.s: Erfolg setzt CC = 0xFF; Fehler haben CC = 0,
     * BB = I/O-Fehler (1..8) bzw. AA = Allokationsfehler. Diese Regel war
     * zuerst falsch herum implementiert -- jede erfolgreiche Transaktion galt
     * damit als Fehler und PING endete in V4_ERR_LINK. */
    CHECK_EQ(v4_i2c_err_is_ok(0x000000FFul), 1);   /* Erfolg */
    CHECK_EQ(v4_i2c_err_is_ok(0x00000001ul), 1);   /* CC != 0 -> OK */
    CHECK_EQ(v4_i2c_err_is_ok(0x00000200ul), 0);   /* BB=2 (I2C_NO_REPLY) */
    CHECK_EQ(v4_i2c_err_is_ok(0x00000100ul), 0);   /* BB=1 (I2C_REJECT) */
    CHECK_EQ(v4_i2c_err_is_ok(0x00000800ul), 0);   /* BB=8 (I2C_HARDW_BUSY) */
    CHECK_EQ(v4_i2c_err_is_ok(0x00010800ul), 0);   /* AA=1 + BB=8 */
    CHECK_EQ(v4_i2c_err_is_ok(0x00000000ul), 0);   /* unspezifischer Fehler */
    /* Der Diagnosehaken muss in jeder Plattformschicht existieren (Linktest) */
    CHECK_EQ(v4_plat_last_error(), 0ul);           /* Mock: immer 0 */

    v4_test_case("R3 (§1.3): vor JEDEM Lesen wird gewartet");
    /* Ein Master ohne Wartezeit wuerde hier glatt durchfallen: 10 Befehle
     * -> 10 Schreibvorgaenge -> genau 10 Wartezeiten von t_wait. */
    d0 = v4_mock.delay_calls;
    u0 = v4_mock.delay_us;
    for (i = 0; i < 10; i++) {
        CHECK_EQ(v4_get_status(&M, &st), V4P_ST_OK);
    }
    CHECK_EQ(M.tx_frames, 10ul);
    CHECK_EQ(v4_mock.delay_calls, d0 + 10ul);
    CHECK_EQ(v4_mock.delay_us, u0 + 10ul * V4_T_WAIT_US);

    v4_test_case("R3: bei Framing-Wiederholungen wird jedes Mal gewartet");
    fresh();
    /* Jede 2. Antwort ist kaputt: Befehl 1 geht durch, Befehl 2 wird
     * wiederholt -- also muss danach mehr geschrieben als befohlen sein. */
    v4_mock.bad_crc_every = 2;
    CHECK_EQ(v4_get_status(&M, &st), V4P_ST_OK);
    d0 = v4_mock.delay_calls;  u0 = v4_mock.delay_us;
    tx0 = M.tx_frames; busy0 = M.busy_rounds; crc0 = M.badcrc_rounds;
    CHECK_EQ(v4_get_status(&M, &st), V4P_ST_OK);
    CHECK(M.tx_frames > tx0 + 1ul);          /* es gab eine Wiederholung */
    CHECK(M.badcrc_rounds >= crc0);          /* CRC-Pfad, nicht BUSY */
    check_delay_invariant(&M, d0, u0, tx0, busy0, crc0);
    CHECK_EQ(M.retries, 1ul);

    v4_test_case("R3/§1.4: BUSY-Runden kosten t_busy zusaetzlich");
    fresh();
    v4_mock.busy_count = 3;
    d0 = v4_mock.delay_calls;  u0 = v4_mock.delay_us;
    tx0 = M.tx_frames; busy0 = M.busy_rounds; crc0 = M.badcrc_rounds;
    CHECK_EQ(v4_get_status(&M, &st), V4P_ST_OK);
    CHECK_EQ(M.busy_rounds - busy0, 3ul);
    CHECK_EQ(M.tx_frames - tx0, 4ul);
    check_delay_invariant(&M, d0, u0, tx0, busy0, crc0);
    CHECK_EQ(v4_mock.delay_us - u0,
             4ul * V4_T_WAIT_US + 3ul * V4_T_BUSY_US);

    v4_test_case("R3 gilt auch fuer BULK-Transaktionen (FILE_READ)");
    fresh();
    {
        uint8_t h = 0u;
        uint32_t size = 0u;
        uint8_t attr = 0u;
        v4p_bulk_t b;

        CHECK_EQ(v4_file_open(&M, "MUSIC/B.MP3", &h, &size, &attr), V4P_ST_OK);
        d0 = v4_mock.delay_calls;  u0 = v4_mock.delay_us;
        tx0 = M.tx_frames; busy0 = M.busy_rounds; crc0 = M.badcrc_rounds;
        CHECK_EQ(v4_file_read(&M, h, 0u, scratch, &b), V4P_ST_OK);
        /* Erster Zugriff auf den Block ist BUSY: 2 Writes, 1 BUSY-Runde */
        CHECK_EQ(M.tx_frames - tx0, 2ul);
        CHECK_EQ(M.busy_rounds - busy0, 1ul);
        check_delay_invariant(&M, d0, u0, tx0, busy0, crc0);
        CHECK_EQ(v4_file_close(&M, h), V4P_ST_OK);
    }

    v4_test_case("R1 (§1.3): eine Antwort pro Befehl -- rx == tx");
    fresh();
    for (i = 0; i < 5; i++) {
        CHECK_EQ(v4_get_status(&M, &st), V4P_ST_OK);
    }
    CHECK_EQ(M.tx_frames, 5ul);
    CHECK_EQ(M.rx_frames, 5ul);
    /* Auch bei BUSY und BAD_CRC bleibt das Verhaeltnis 1:1 */
    v4_mock.busy_count = 2;
    v4_mock.force_status = V4P_ST_BAD_CRC;
    d0 = v4_mock.delay_calls;  u0 = v4_mock.delay_us;
    tx0 = M.tx_frames; busy0 = M.busy_rounds; crc0 = M.badcrc_rounds;
    CHECK_EQ(v4_get_status(&M, &st), V4P_ST_OK);
    CHECK_EQ(M.rx_frames, M.tx_frames);
    check_delay_invariant(&M, d0, u0, tx0, busy0, crc0);

    v4_test_case("§8.1 Mount-Wartezeit wird wirklich angefordert");
    fresh();
    d0 = v4_mock.delay_calls;
    u0 = v4_mock.delay_us;
    CHECK_EQ(v4_sd_mount_wait(&M, 4u), V4P_ST_OK);
    /* probe(force=1): 1x t_wait; dann genau 200 ms; dann probe(force=0) */
    CHECK_EQ(v4_mock.delay_us, u0 + 2ul * V4_T_WAIT_US + V4_MOUNT_WAIT_US);
    CHECK_EQ(v4_mock.delay_calls, d0 + 3ul);

    v4_test_case("§11 SCAN_START waehrend CONNECTING -> BAD_STATE");
    fresh();
    v4_mock.connect_rounds = 3;
    CHECK_EQ(v4_connect(&M, 1u), V4P_ST_OK);
    CHECK_EQ(v4_get_status(&M, &st), V4P_ST_OK);
    CHECK_EQ(st.state, V4P_STATE_CONNECTING);
    CHECK_EQ(v4_scan_start(&M, 8u, 1), V4P_ST_BAD_STATE);

    v4_test_case("§11 CONNECT bricht einen laufenden Scan ab");
    fresh();
    CHECK_EQ(v4_scan_start(&M, 8u, 1), V4P_ST_OK);
    CHECK_EQ(v4_get_status(&M, &st), V4P_ST_OK);
    CHECK_EQ(st.scan_active, 1u);
    CHECK_EQ(v4_connect(&M, 0u), V4P_ST_OK);
    CHECK_EQ(v4_get_status(&M, &st2), V4P_ST_OK);
    CHECK_EQ(st2.scan_active, 0u);
    CHECK_EQ(st2.state, V4P_STATE_CONNECTED);

    v4_test_case("§11 SD_MOUNT(force=0) meldet nur das LETZTE Ergebnis");
    fresh();
    /* Karte steckt und ist gemountet; danach wird sie entfernt. */
    v4_mock.sd_present = 0;
    CHECK_EQ(v4_sd_mount_probe(&M, 0), V4P_ST_OK);   /* altes Ergebnis */
    CHECK_EQ(v4_sd_mount_probe(&M, 1), V4P_ST_BUSY); /* nachfassen */
    CHECK_EQ(v4_sd_mount_wait(&M, 4u), V4P_ST_NO_SD);
    CHECK_EQ(v4_sd_mount_probe(&M, 0), V4P_ST_NO_SD);

    v4_test_case("§6-Autoretry kann den 200-ms-Mount nicht ueberbruecken");
    fresh();
    CHECK_EQ(v4_sd_mount_probe(&M, 1), V4P_ST_BUSY);
    /* 40 BUSY-Runden a 2 ms sind 80 ms -- zu wenig fuer einen Mount. Genau
     * dafuer gibt es den Aufrufer-gesteuerten Weg (§8.1). */
    CHECK_EQ(v4_sd_mount(&M, 0), V4_ERR_BUSY);
    CHECK_EQ(v4_sd_mount_wait(&M, 4u), V4P_ST_OK);

    v4_test_case("§6 FIFO-Selbstheilung nach halb konsumierter Antwort");
    fresh();
    v4_mock.partial_read_once = 1;
    CHECK_EQ(v4_get_status(&M, &st), V4P_ST_OK);
    CHECK(M.retries >= 1ul);
    CHECK_EQ(M.last_check, V4P_CHECK_CRC);
    /* Der naechste Befehl ist wieder sauber (kein verschobener Strom). */
    CHECK_EQ(v4_get_status(&M, &st), V4P_ST_OK);
    CHECK_EQ(st.dev_count, 3u);

    v4_test_case("§5.1 der Slave meldet BAD_CRC bei kaputtem Request-CRC");
    fresh();
    {
        uint8_t wf[V4P_WRITE_FRAME_LEN];
        uint8_t raw[V4P_READ_FRAME_LEN];
        v4p_read_t r;

        CHECK_EQ(v4p_build_write(wf, V4P_CMD_GET_STATUS, 0u, NULL, 0u), 0);
        wf[31] = (uint8_t)(wf[31] ^ 0xFFu);          /* CRC kaputtmachen */
        CHECK_EQ(v4_plat_i2c_write(V4_I2C_ADDR7, wf, 32u), 0);
        CHECK_EQ(v4_plat_i2c_read(V4_I2C_ADDR7, raw, V4P_READ_FRAME_LEN), 0);
        CHECK_EQ(v4p_check_read(raw, V4P_READ_FRAME_LEN, V4P_CMD_GET_STATUS,
                                0u, &r), V4P_CHECK_OK);
        CHECK_EQ(r.status, V4P_ST_BAD_CRC);
    }

    v4_test_case("Mock-Diagnose: Laengen, Zaehler und audio");
    fresh();
    CHECK_EQ(v4_mock.bad_write_len, 0);
    /* v4_plat_i2c_write mit falscher Laenge: der Slave lehnt ab. */
    {
        uint8_t wf[V4P_WRITE_FRAME_LEN];
        CHECK_EQ(v4p_build_write(wf, V4P_CMD_GET_STATUS, 0u, NULL, 0u), 0);
        CHECK_EQ(v4_plat_i2c_write(V4_I2C_ADDR7, wf, 31u), -1);
        CHECK_EQ(v4_mock.bad_write_len, 1);
    }
    d0 = v4_mock.writes;
    CHECK_EQ(v4_get_status(&M, &st), V4P_ST_OK);
    CHECK_EQ(v4_mock.writes, d0 + 1ul);
    CHECK_EQ(v4_mock.last_cmd, V4P_CMD_GET_STATUS);
    v4_mock_set_audio(1);
    CHECK_EQ(v4_get_status(&M, &st), V4P_ST_OK);
    CHECK((st.audio_flags & V4P_AUDIO_A2DP_STREAMING) != 0u);
    CHECK_EQ(st.sd_card_present, 1u);
    v4_mock_set_audio(0);
    CHECK_EQ(v4_get_status(&M, &st), V4P_ST_OK);
    CHECK_EQ(st.audio_flags, 0u);

    v4_test_case("§11 FILE_READ meldet NO_SD im BULK-Frame");
    fresh();
    {
        uint8_t h = 0u;
        uint32_t size = 0u;
        uint8_t attr = 0u;
        v4p_bulk_t b;

        CHECK_EQ(v4_file_open(&M, "MUSIC/B.MP3", &h, &size, &attr), V4P_ST_OK);
        /* Karte wird entfernt, das Handle bleibt offen. */
        v4_mock_set_sd(0, 0u, 0u);
        CHECK_EQ(v4_file_read(&M, h, 0u, scratch, &b), V4P_ST_NO_SD);
        v4_mock_set_sd(1, 8000000u, 491520u);
        CHECK_EQ(v4_file_read(&M, h, 0u, scratch, &b), V4P_ST_OK);
        CHECK_EQ(v4_file_close(&M, h), V4P_ST_OK);
    }

    v4_test_case("§11 QUEUE: busy_every trifft jeden n-ten Befehl");
    fresh();
    v4_mock.busy_every = 3;
    for (i = 0; i < 9; i++) {
        CHECK_EQ(v4_get_status(&M, &st), V4P_ST_OK);
    }
    CHECK(M.busy_rounds >= 2ul);      /* Befehle 3, 6, 9 */
    CHECK_EQ(M.seq, 9u);
}

/* ------------------------------------------------------------------ */
/* Regressionsfaelle aus der adversarialen Fremdprufung                */
/* ------------------------------------------------------------------ */

static int g_bump_chunk;

static void chunkchange_cb(uint16_t block, const uint8_t *data, uint16_t len,
                           void *ctx)
{
    v4_master_t *m = (v4_master_t *)ctx;

    (void)block;
    (void)data;
    (void)len;
    if (g_bump_chunk) {
        g_bump_chunk = 0;
        /* Callback vergroessert die Chunkgroesse mitten im Lesen. */
        (void)v4_set_chunk(m, V4P_CHUNK_MAX);
    }
}

static void test_review_regressions(void)
{
    uint8_t  h = 0u;
    uint32_t size = 0u;
    uint8_t  attr = 0u;
    uint32_t total = 0u;

    v4_test_case("Review: Chunkwechsel im Callback ueberlaeuft den Puffer nicht");
    fresh();
    {
        /* Nur fuer chunk=128 ausgelegt -- 1024 waeren 1036 Byte noetig. */
        uint8_t small[V4P_BULK_OVERHEAD + 128u];

        g_bump_chunk = 1;
        CHECK_EQ(v4_read_file(&M, "MUSIC/A.MP3", small, sizeof(small),
                              chunkchange_cb, &M, &total), V4_ERR_ARG);
        CHECK_EQ(M.chunk, V4P_CHUNK_MAX);
    }

    v4_test_case("Review: derselbe Fall mit selbst allokiertem Puffer");
    fresh();
    g_bump_chunk = 1;
    CHECK_EQ(v4_read_file(&M, "MUSIC/A.MP3", NULL, 0u,
                          chunkchange_cb, &M, &total), V4_ERR_ARG);
    CHECK_EQ(M.chunk, V4P_CHUNK_MAX);

    v4_test_case("Review: PATH_APPEND bei verlorener Antwort bleibt konsistent");
    fresh();
    CHECK_EQ(v4_path_clear(&M), V4P_ST_OK);
    /* Die Antwort auf den Append geht verloren: der Slave hat ihn aber
     * ausgefuehrt, der Retry haengt sonst ein zweites Mal an. */
    v4_mock.drop_next_read = 1;
    CHECK_EQ(v4_path_append(&M, "MUSIC/A.MP3"), V4P_ST_OK);
    CHECK(M.unsafe_retries >= 1ul);
    CHECK_EQ(M.path_valid, 1u);
    CHECK_EQ(M.path_len, 11u);
    /* Der Slave muss denselben Pfad haben: der L=0-Open muss aufgeben. */
    CHECK_EQ(v4_file_open(&M, NULL, &h, &size, &attr), V4P_ST_OK);
    CHECK_EQ(size, 1000u);
    CHECK_EQ(v4_file_close(&M, h), V4P_ST_OK);

    v4_test_case("Review: \"\" oeffnet die Wurzel, nicht den PATH_*-Puffer (§10)");
    fresh();
    CHECK_EQ(v4_path_clear(&M), V4P_ST_OK);
    CHECK_EQ(v4_path_append(&M, "MUSIC/"), V4P_ST_OK);   /* Puffer ist belegt */
    {
        v4p_dirent_t e;

        CHECK_EQ(v4_dir_open(&M, "", &h), V4P_ST_OK);
        CHECK_EQ(v4_dir_next(&M, h, 0u, &e), V4P_ST_OK);
        /* Wurzel -> erster Eintrag ist das Verzeichnis MUSIC;
         * PATH_*-Puffer "MUSIC/" -> erste Datei waere A.MP3. */
        CHECK_STR(e.name, "MUSIC");
        CHECK_EQ(e.attr, V4P_ATTR_DIR);
        CHECK_EQ(v4_dir_close(&M, h), V4P_ST_OK);
    }
    /* "/" liefert dasselbe */
    CHECK_EQ(v4_dir_open(&M, "/", &h), V4P_ST_OK);
    CHECK_EQ(v4_dir_close(&M, h), V4P_ST_OK);
    /* Ohne PATH_CLEAR ist der L=0-Weg weiterhin der gebaute Pfad "MUSIC/"
     * -- also der MUSIC-Inhalt, nicht die Wurzel. */
    {
        v4p_dirent_t e;

        CHECK_EQ(v4_dir_open(&M, NULL, &h), V4P_ST_OK);
        CHECK_EQ(v4_dir_next(&M, h, 0u, &e), V4P_ST_OK);
        CHECK_STR(e.name, "A.MP3");
        CHECK_EQ(v4_dir_close(&M, h), V4P_ST_OK);
    }
    /* Und als DATEI geoeffnet muss derselbe Pfad BAD_ARG liefern, weil
     * "MUSIC/" ein Verzeichnis ist. */
    CHECK_EQ(v4_file_open(&M, NULL, &h, &size, &attr), V4P_ST_BAD_ARG);

    v4_test_case("Review: OPEN mit unsicherer Wiederholung wird gezaehlt");
    fresh();
    v4_mock.drop_next_read = 1;
    CHECK_EQ(v4_file_open(&M, "MUSIC/A.MP3", &h, &size, &attr), V4P_ST_OK);
    CHECK(M.unsafe_retries >= 1ul);
    CHECK(M.possible_handle_leaks >= 1ul);   /* Slave hat evtl. 2 Handles */
    CHECK_EQ(v4_file_close(&M, h), V4P_ST_OK);

    v4_test_case("Review: PATH_APPEND nach unbrauchbarem Spiegel gesperrt");
    fresh();
    CHECK_EQ(v4_path_clear(&M), V4P_ST_OK);
    /* Simuliert einen unbrauchbaren Spiegel: erst sperren, dann pruefen. */
    M.path_valid = 0u;
    CHECK_EQ(v4_path_append(&M, "X"), V4_ERR_ARG);
    CHECK_EQ(v4_dir_open(&M, NULL, &h), V4_ERR_ARG);
    CHECK_EQ(v4_file_open(&M, NULL, &h, &size, &attr), V4_ERR_ARG);
    /* Der direkte Pfad bleibt erlaubt -- der Spiegel wird nicht gebraucht. */
    CHECK_EQ(v4_dir_open(&M, "MUSIC", &h), V4P_ST_OK);
    CHECK_EQ(v4_dir_close(&M, h), V4P_ST_OK);
    /* PATH_CLEAR macht den Spiegel wieder gueltig. */
    CHECK_EQ(v4_path_clear(&M), V4P_ST_OK);
    CHECK_EQ(M.path_valid, 1u);
    CHECK_EQ(v4_dir_open(&M, NULL, &h), V4P_ST_OK);
    CHECK_EQ(v4_dir_close(&M, h), V4P_ST_OK);
}

/* Der Vertrag der Logdatei: im Mock gibt es keinen Standardpfad, ein
 * ausdruecklicher Pfad muss aber gehen, und ohne offene Datei darf Schreiben
 * niemals etwas tun. Dieselbe Semantik hat die Amiga-Schicht mit
 * V4_LOG_DEFAULT_AMIGA. */
static void test_log_hooks(void)
{
    const char *path = "build/v4_log_unit.txt";
    char        buf[64];
    FILE       *f;
    size_t      n;

    v4_test_case("log hooks");

    /* Kein Standardpfad im Mock -> sauberes "nicht verfuegbar". */
    CHECK_EQ(v4_plat_log_open(NULL), -1);
    /* Ohne offene Datei ist Schreiben ein No-op, kein Absturz. */
    CHECK_EQ(v4_plat_log_write("x", 1ul), 0);
    /* Schliessen ohne offene Datei ist erlaubt. */
    v4_plat_log_close();

    remove(path);
    CHECK_EQ(v4_plat_log_open(path), 0);
    CHECK_EQ(v4_plat_log_write("Zeile 1\n", 8ul), 8);
    CHECK_EQ(v4_plat_log_write("egal", 0ul), 0);     /* len 0 -> nichts */
    v4_plat_log_close();
    v4_plat_log_close();                             /* idempotent */
    CHECK_EQ(v4_plat_log_write("nach dem Schliessen", 19ul), 0);

    f = fopen(path, "rb");
    CHECK(f != NULL);
    if (f != NULL) {
        n = fread(buf, 1u, sizeof(buf) - 1u, f);
        buf[n] = '\0';
        fclose(f);
        CHECK_EQ(n, 8u);
        CHECK_STR(buf, "Zeile 1\n");
    }

    /* Ein unbrauchbarer Pfad wird gemeldet, nicht verschluckt. */
    CHECK_EQ(v4_plat_log_open("build/kein/verzeichnis/x.txt"), -1);
    v4_plat_log_close();
}

/* ------------------------------------------------------------------ */

int test_master(void)
{
    v4_test_begin("test_master");

    test_normal();
    test_seq_and_link();
    test_framing_errors();
    test_busy();
    test_link_dead();
    test_status_no_retry();
    test_dir_retry();
    test_bulk();
    test_name_truncation();
    test_edge_chunk_change();
    test_edge_paths();
    test_edge_handles();
    test_edge_bus_reset();
    test_helpers();
    test_trace();
    test_playback();
    test_spec_pinning();
    test_review_regressions();
    test_log_hooks();

    return v4_test_failures;
}
