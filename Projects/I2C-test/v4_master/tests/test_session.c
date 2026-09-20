/*
 * test_session.c -- Ablauf einer vollstaendigen Sitzung (§13) gegen den Mock
 * plus den Dauerlauf aus §14.2.3 (10000x GET_STATUS mit Stoerungen).
 *
 * §13 beschreibt die Reihenfolge, die eine echte V4-Sitzung faehrt:
 * PING -> GET_STATUS -> SCAN_START -> warten -> DEV_GET -> CONNECT ->
 * SD_MOUNT -> SD_INFO -> DIR_* -> FILE_* -> DISCONNECT.
 */

#include "v4_test.h"

#include "../v4_master.h"
#include "../v4_mock.h"

static v4_master_t M;
static uint8_t     scratch[V4P_BULK_OVERHEAD + V4P_CHUNK_MAX];

static uint8_t  g_big[70000];
static uint32_t g_big_len;
static int      g_big_overflow;

static void big_cb(uint16_t block, const uint8_t *data, uint16_t len, void *ctx)
{
    (void)block;
    (void)ctx;
    if ((uint32_t)g_big_len + (uint32_t)len > (uint32_t)sizeof(g_big)) {
        g_big_overflow = 1;
        return;
    }
    memcpy(g_big + g_big_len, data, len);
    g_big_len += (uint32_t)len;
}

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
/* §13 -- vollstaendige Sitzung                                        */
/* ------------------------------------------------------------------ */

static void test_session_flow(void)
{
    v4p_info_t   info;
    v4p_status_t st;
    v4p_sdinfo_t si;
    uint8_t      count = 0u;
    uint16_t     gen;
    uint32_t     total = 0u;
    int          i;

    v4_mock_reset();
    v4_mock_seed_default();
    v4_init(&M);
    CHECK_EQ(v4_plat_open(NULL), 0);

    v4_test_case("§13.2 Byte-Order-Selbsttest");
    CHECK_EQ(v4p_selftest_byteorder(), 1);

    v4_test_case("§13.3 PING: proto_ver muss 3 sein (128-Byte-Format)");
    CHECK_EQ(v4_ping(&M, &info), V4P_ST_OK);
    CHECK_EQ(info.proto_ver, V4P_PROTO_VER);
    CHECK_EQ(info.write_frame_len, V4P_WRITE_FRAME_LEN);
    CHECK_EQ(info.read_frame_len, V4P_READ_FRAME_LEN);

    v4_test_case("§13.4 GET_STATUS: Startzustand erfassen");
    CHECK_EQ(v4_get_status(&M, &st), V4P_ST_OK);
    CHECK_EQ(st.state, V4P_STATE_IDLE);
    CHECK_EQ(st.conn_index, 0xFFu);
    CHECK_EQ(st.dev_count, 3u);
    CHECK_EQ(st.sd_mounted, 1u);

    v4_test_case("§13.5 SCAN_START(8, continuous=1)");
    CHECK_EQ(v4_scan_start(&M, 8u, 1), V4P_ST_OK);
    CHECK_EQ(v4_get_status(&M, &st), V4P_ST_OK);
    CHECK_EQ(st.scan_active, 1u);
    CHECK_EQ(st.state, V4P_STATE_IDLE);
    gen = st.scan_gen;

    v4_test_case("§13.6 auf Aenderung von scan_gen warten");
    {
        static const uint8_t bda[6] = { 0x01u, 0x02u, 0x03u, 0x04u, 0x05u, 0x06u };
        int rounds = 0;

        /* Erster Poll: die Liste hat sich noch NICHT geaendert. Erst danach
         * kommt ein Geraet dazu -- sonst liefe die Warteschleife genau
         * einmal und das Warten waere gar nicht geprueft. */
        CHECK_EQ(v4_get_status(&M, &st), V4P_ST_OK);
        CHECK_EQ(st.scan_gen, gen);

        CHECK_EQ(v4_mock_dev_add("Neu Gefunden", bda), 3);
        v4_mock_bump_scan_gen();

        do {
            CHECK_EQ(v4_get_status(&M, &st), V4P_ST_OK);
            rounds++;
        } while (st.scan_gen == gen && rounds < 10);
        CHECK(st.scan_gen != gen);
        CHECK(rounds >= 1);
    }

    v4_test_case("§13.7 DEV_GET(0..dev_count-1)");
    CHECK_EQ(v4_dev_count(&M, &count), V4P_ST_OK);
    CHECK_EQ(count, 4u);
    for (i = 0; i < (int)count; i++) {
        v4p_dev_t d;

        CHECK_EQ(v4_dev_get(&M, (uint8_t)i, &d), V4P_ST_OK);
        CHECK_EQ(d.idx, (uint8_t)i);
        CHECK(d.name_len > 0u);
        CHECK(d.name[0] != '\0');
    }

    v4_test_case("§13.8 CONNECT(idx=1) und auf state == CONNECTED warten");
    v4_mock.connect_rounds = 2;
    CHECK_EQ(v4_connect(&M, 1u), V4P_ST_OK);
    CHECK_EQ(v4_wait_state(&M, V4P_STATE_CONNECTED, 10u, &st), V4P_ST_OK);
    CHECK_EQ(st.state, V4P_STATE_CONNECTED);
    CHECK_EQ(st.conn_index, 1u);

    v4_test_case("§13.9 Liste bleibt waehrend der Verbindung aktuell");
    CHECK_EQ(v4_get_status(&M, &st), V4P_ST_OK);
    CHECK_EQ(st.dev_count, count);
    CHECK_EQ(st.state, V4P_STATE_CONNECTED);

    v4_test_case("§13.10 SD_MOUNT(force=1) -> BUSY, dann force=0 -> OK");
    CHECK_EQ(v4_sd_mount_wait(&M, 4u), V4P_ST_OK);
    CHECK(M.busy_rounds >= 1ul);

    v4_test_case("§13.11 SD_INFO");
    CHECK_EQ(v4_sd_info(&M, &si), V4P_ST_OK);
    CHECK_EQ(si.total_kb, 8000000u);
    CHECK_EQ(si.free_kb, 491520u);
    CHECK_EQ(si.sector_size, 512u);
    CHECK_EQ(si.fat_type, 0u);

    v4_test_case("§13.12-14 DIR_OPEN / DIR_NEXT bis END / DIR_CLOSE");
    g_count = 0;
    CHECK_EQ(v4_list_dir(&M, "/MUSIC", list_cb, NULL), V4P_ST_OK);
    CHECK_EQ(g_count, 3);
    CHECK_STR(g_namebuf[0], "A.MP3");
    CHECK_STR(g_namebuf[1], "B.MP3");
    CHECK_STR(g_namebuf[2], "BIG.BIN");

    v4_test_case("§10 \"/MUSIC\" und \"MUSIC\" sind derselbe Ordner");
    g_count = 0;
    CHECK_EQ(v4_list_dir(&M, "MUSIC", list_cb, NULL), V4P_ST_OK);
    CHECK_EQ(g_count, 3);
    g_count = 0;
    CHECK_EQ(v4_list_dir(&M, "music", list_cb, NULL), V4P_ST_OK);  /* FAT-case-blind */
    CHECK_EQ(g_count, 3);

    v4_test_case("§13.15-17 FILE_OPEN / FILE_READ bis kurzer Block / CLOSE");
    g_big_len = 0;
    g_big_overflow = 0;
    CHECK_EQ(v4_read_file(&M, "/MUSIC/A.MP3", scratch, sizeof(scratch),
                          big_cb, NULL, &total), V4P_ST_OK);
    CHECK_EQ(total, 1000u);
    CHECK_EQ(g_big_len, 1000u);
    CHECK_EQ(g_big_overflow, 0);
    CHECK(pattern_ok(g_big, 1000u));
    CHECK_EQ(v4p_crc32(g_big, 1000u), 0x74E3FB41u);   /* unabhaengig berechnet */

    v4_test_case("§9 Massentransfer mit chunk=1024 (65536 Byte)");
    CHECK_EQ(v4_set_chunk(&M, 1024u), V4P_ST_OK);
    CHECK_EQ(M.chunk, 1024u);
    g_big_len = 0;
    g_big_overflow = 0;
    CHECK_EQ(v4_read_file(&M, "MUSIC/BIG.BIN", scratch, sizeof(scratch),
                          big_cb, NULL, &total), V4P_ST_OK);
    CHECK_EQ(total, 65536u);
    CHECK_EQ(g_big_len, 65536u);
    CHECK_EQ(g_big_overflow, 0);
    CHECK(pattern_ok(g_big, 65536u));
    CHECK_EQ(v4p_crc32(g_big, g_big_len), 0xB11DE6A1u); /* §14.2.5 */

    v4_test_case("§9 zurueck auf chunk=16 (USB-Bridge-tauglich)");
    CHECK_EQ(v4_set_chunk(&M, 16u), V4P_ST_OK);
    g_big_len = 0;
    CHECK_EQ(v4_read_file(&M, "MUSIC/B.MP3", scratch, sizeof(scratch),
                          big_cb, NULL, &total), V4P_ST_OK);
    CHECK_EQ(total, 37u);
    CHECK_EQ(v4p_crc32(g_big, 37u), 0x8222EFE9u);
    CHECK_EQ(v4_get_status(&M, &st), V4P_ST_OK);
    CHECK_EQ(st.chunk, 16u);

    v4_test_case("§11 PLAY_FILE / STOP_PLAY in der Sitzung");
    {
        v4p_read_t r;

        /* Datei von der SD-Karte selbst abspielen: nur Steuerbytes. */
        CHECK_EQ(v4_play_file(&M, "MUSIC/B.MP3"), V4P_ST_OK);
        CHECK_EQ(v4_get_status(&M, &st), V4P_ST_OK);
        CHECK((st.audio_flags & V4P_AUDIO_SD_PLAYBACK) != 0u);
        CHECK_EQ(v4_stop_play(&M), V4P_ST_OK);
        CHECK_EQ(v4_get_status(&M, &st), V4P_ST_OK);
        CHECK_EQ(st.audio_flags & V4P_AUDIO_SD_PLAYBACK, 0u);

        /* Ein in §11 nicht vergebener Code muss BAD_CMD liefern. */
        CHECK_EQ(v4_transact(&M, 0x55u, NULL, 0u, &r), V4P_ST_BAD_CMD);
    }

    v4_test_case("§13.18 DISCONNECT ohne Neustart");
    CHECK_EQ(v4_disconnect(&M), V4P_ST_OK);
    CHECK_EQ(v4_get_status(&M, &st), V4P_ST_OK);
    CHECK_EQ(st.state, V4P_STATE_IDLE);
    CHECK_EQ(v4_connect(&M, 0u), V4P_ST_OK);            /* sofort wieder */
    CHECK_EQ(v4_disconnect(&M), V4P_ST_OK);
}

/* ------------------------------------------------------------------ */
/* §14.2.3 Dauerlauf                                                   */
/* ------------------------------------------------------------------ */

static void clear_faults(void)
{
    v4_mock.bad_crc_every     = 0;
    v4_mock.drop_read_every   = 0;
    v4_mock.drop_next_read    = 0;
    v4_mock.no_seq_echo_every = 0;
    v4_mock.bad_frame_len_every = 0;
    v4_mock.busy_count        = 0;
    v4_mock.busy_every        = 0;
    v4_mock.never_ready       = 0;
    v4_mock.force_status      = -1;
    v4_mock.fail_read         = 0;
    v4_mock.fail_write        = 0;
}

static void test_stress(void)
{
    v4p_status_t st;
    int          i;
    unsigned long ok = 0ul;
    unsigned long retries_before;

    v4_mock_reset();
    v4_mock_seed_default();
    v4_init(&M);
    CHECK_EQ(v4_plat_open(NULL), 0);
    clear_faults();

    v4_test_case("§14.2.3 Dauerlauf: 10000x GET_STATUS mit Stoerungen");
    retries_before = M.retries;

    for (i = 0; i < 10000; i++) {
        uint8_t rc;

        clear_faults();
        switch (i % 5) {
        case 0:
            v4_mock.bad_crc_every = 3;
            break;
        case 1:
            v4_mock.drop_read_every = 5;
            break;
        case 2:
            v4_mock.busy_count = 2;
            break;
        case 3:
            v4_mock.no_seq_echo_every = 4;
            break;
        default:
            v4_mock.bad_frame_len_every = 7;
            break;
        }

        rc = v4_get_status(&M, &st);
        if (rc != V4P_ST_OK) {
            CHECK_EQ(rc, V4P_ST_OK);
            break;
        }
        if (st.dev_count != 3u || st.chunk != V4P_CHUNK_DEFAULT) {
            CHECK_EQ(st.dev_count, 3u);
            break;
        }
        ok++;
    }
    clear_faults();

    CHECK_EQ(ok, 10000ul);
    CHECK(M.retries > retries_before);      /* die Stoerungen kamen an */
    CHECK(M.retries > 100ul);
    CHECK(M.busy_rounds >= 2000ul);

    v4_test_case("§14.2.3 danach ist der Link weiter benutzbar");
    CHECK_EQ(v4_get_status(&M, &st), V4P_ST_OK);
    CHECK_EQ(v4_ping(&M, NULL), V4_ERR_ARG);        /* Argumentpruefung */
    {
        v4p_info_t info;
        CHECK_EQ(v4_ping(&M, &info), V4P_ST_OK);
        CHECK_EQ(info.proto_ver, V4P_PROTO_VER);
    }
}

/* ------------------------------------------------------------------ */

int test_session(void)
{
    v4_test_begin("test_session");

    test_session_flow();
    test_stress();

    return v4_test_failures;
}
