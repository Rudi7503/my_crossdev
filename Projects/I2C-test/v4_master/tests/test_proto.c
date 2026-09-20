/*
 * test_proto.c -- Vertragstests: Byte-Order, Pruefsummen, Golden Frames.
 *
 * Deckt ab: PROTOCOL_V4_MASTER.md §0 (Byte-Order), §2 (Helfer),
 * §3.3 (CRC-Tabelle), §4 (Frame-Formate), §12 (verifizierte Beispielframes).
 *
 * Diese Tests sind absichtlich byte-genau: sie wuerden auf einer
 * Big-Endian-Maschine genauso laufen muessen wie hier.
 */

#include "v4_test.h"

#include "../v4_proto.h"

/* ------------------------------------------------------------------ */
/* Hilfsdaten                                                          */
/* ------------------------------------------------------------------ */

typedef struct {
    const char *hex;
    uint8_t     crc8;
} crc8_vec_t;

typedef struct {
    const char *hex;
    uint32_t    crc32;
} crc32_vec_t;

/* §12.1 WRITE PING, seq = 0x00 */
static const char G12_1[] =
    "A5 01 00 00 00 00 00 00 00 00 00 00 00 00 00 00"
    "00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 92";

/* §12.2 WRITE DEV_GET(idx=0), seq = 0x01 */
static const char G12_2[] =
    "A5 13 01 01 00 00 00 00 00 00 00 00 00 00 00 00"
    "00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 6E";

/* §12.3 WRITE SET_CHUNK(256), seq = 0x02 */
static const char G12_3[] =
    "A5 32 02 02 00 01 00 00 00 00 00 00 00 00 00 00"
    "00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 24";

/* §12.4 READ-Antwort auf GET_STATUS, seq = 0x00, status = OK.
 * Seit proto_ver 3 ist der READ-Frame 128 Byte lang: Nutzlast 121 Byte,
 * `flags` bei +126, CRC-8 bei +127. */
static const char G12_4[] =
    "5A 02 00 00 11 03 FF 04 01 01 01 0A 00 00 80 07"
    "00 80 00 03 02 01 00 00 00 00 00 00 00 00 00 00"
    "00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00"
    "00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00"
    "00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00"
    "00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00"
    "00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00"
    "00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 D8";

/* §12.5 BULK FILE_READ, chunk = 16, letzter Block (len = 5) */
static const char G12_5[] =
    "5B 51 00 00 03 00 05 00 DE AD BE EF 42 00 00 00"
    "00 00 00 00 00 00 00 00 6C 11 18 9C";

/* ------------------------------------------------------------------ */

static size_t hex2bin(const char *hex, uint8_t *out, size_t cap)
{
    size_t n = 0;
    int    hi = -1;

    while (*hex != '\0') {
        char c = *hex++;
        int  v;

        if (c == ' ') {
            continue;
        }
        if (c >= '0' && c <= '9') {
            v = c - '0';
        } else if (c >= 'A' && c <= 'F') {
            v = c - 'A' + 10;
        } else if (c >= 'a' && c <= 'f') {
            v = c - 'a' + 10;
        } else {
            return (size_t)-1;
        }
        if (hi < 0) {
            hi = v;
        } else {
            if (n >= cap) {
                return (size_t)-1;
            }
            out[n++] = (uint8_t)((hi << 4) | v);
            hi = -1;
        }
    }
    return (hi < 0) ? n : (size_t)-1;
}

/* ------------------------------------------------------------------ */

static void test_byteorder(void)
{
    uint8_t b[4];
    uint32_t v;

    v4_test_case("§0 Byte-Order-Selbsttest");
    CHECK_EQ(v4p_selftest_byteorder(), 1);

    v4_test_case("§2 u16: alle 65536 Werte byte-genau");
    {
        uint32_t bad = 0u;
        for (v = 0; v <= 0xFFFFu; v++) {
            uint8_t t[2];
            v4p_put_u16le(t, (uint16_t)v);
            if (t[0] != (uint8_t)(v & 0xFFu)
                || t[1] != (uint8_t)((v >> 8) & 0xFFu)
                || v4p_get_u16le(t) != (uint16_t)v) {
                bad++;
            }
        }
        CHECK_EQ(bad, 0u);
    }

    v4_test_case("§2 u32: Positionstreue und Rundlauf");
    {
        static const uint32_t seeds[] = {
            0u, 1u, 0x7Fu, 0x80u, 0xFFu, 0x100u, 0xFFFFu, 0x10000u,
            0x12345678u, 0xFFFFFFFFu, 0xDEADBEEFu, 0x80000000u
        };
        size_t i;
        uint32_t k;

        for (i = 0; i < sizeof(seeds) / sizeof(seeds[0]); i++) {
            v4p_put_u32le(b, seeds[i]);
            CHECK_EQ(b[0], (uint8_t)(seeds[i] & 0xFFu));
            CHECK_EQ(b[1], (uint8_t)((seeds[i] >> 8) & 0xFFu));
            CHECK_EQ(b[2], (uint8_t)((seeds[i] >> 16) & 0xFFu));
            CHECK_EQ(b[3], (uint8_t)((seeds[i] >> 24) & 0xFFu));
            CHECK_EQ(v4p_get_u32le(b), seeds[i]);
        }

        /* Jedes Byte einzeln in jeder Position (256 * 4 Faelle) */
        for (k = 0; k < 256u; k++) {
            uint32_t w;
            for (w = 0; w < 4u; w++) {
                uint32_t val = (uint32_t)k << (8u * w);
                v4p_put_u32le(b, val);
                CHECK_EQ(v4p_get_u32le(b), val);
            }
        }

        /* Deterministischer Lauf ueber den ganzen Wertebereich: Fehler
         * ZAEHLEN und am Ende pruefen -- ein CHECK(1) waere vakuant, weil
         * die Schleife nur im Fehlerfall einen Check erzeugt. */
        {
            uint32_t bad = 0u;

            for (k = 0; k < 200000u; k++) {
                uint32_t val = k * 2654435761u;     /* Knuth-Mix */
                v4p_put_u32le(b, val);
                if (v4p_get_u32le(b) != val) {
                    bad++;
                }
            }
            CHECK_EQ(bad, 0u);
        }
    }
}

static void test_crc(void)
{
    static const crc8_vec_t v8[] = {
        { "31 32 33 34 35 36 37 38 39", 0xF4u },   /* "123456789" */
        { "",                           0x00u },
        { "A5",                         0x72u },
        { "00",                         0x00u },
        { "01 00",                      0x15u },
        { "51 50",                      0xAEu },
        { "A5 01 00 00",                0xDAu }
    };
    static const crc32_vec_t v32[] = {
        { "31 32 33 34 35 36 37 38 39", 0xCBF43926u },
        { "",                           0x00000000u },
        { "5B",                         0x2EBB67F1u },
        { "00 00 00 00 00 00 00 00",    0x6522DF69u },
        { "01 32",                      0x9015723Eu }
    };
    uint8_t buf[32];
    size_t  i;

    v4_test_case("§3.3 CRC-8-Vektoren");
    for (i = 0; i < sizeof(v8) / sizeof(v8[0]); i++) {
        size_t n = hex2bin(v8[i].hex, buf, sizeof(buf));

        /* Abbruch statt Weiterlaufen: bei n == SIZE_MAX wuerde
         * v4p_crc8(buf, SIZE_MAX) wilden Speicher lesen. */
        if (n == (size_t)-1) {
            CHECK(0);
            continue;
        }
        CHECK_EQ(v4p_crc8(buf, n), v8[i].crc8);
    }

    v4_test_case("§3.3 CRC-32-Vektoren");
    for (i = 0; i < sizeof(v32) / sizeof(v32[0]); i++) {
        size_t n = hex2bin(v32[i].hex, buf, sizeof(buf));

        if (n == (size_t)-1) {
            CHECK(0);
            continue;
        }
        CHECK_EQ(v4p_crc32(buf, n), v32[i].crc32);
    }

    v4_test_case("§3 CRC-32 deckt Fuellbytes mit ab (§4.3)");
    {
        /* Gleiche Nutzdaten, unterschiedliche Fuellung => anderer CRC. */
        uint8_t a[8] = { 0u };
        uint8_t c[8] = { 0u };
        a[0] = 0xAAu;
        c[0] = 0xAAu;
        c[7] = 0x01u;
        CHECK(v4p_crc32(a, 8) != v4p_crc32(c, 8));
    }
}

static void test_build_write(void)
{
    uint8_t f[V4P_WRITE_FRAME_LEN];
    uint8_t exp[V4P_WRITE_FRAME_LEN];
    uint8_t pay[V4P_WRITE_PAYLOAD_MAX];

    v4_test_case("§12.1 Golden WRITE PING seq=0");
    CHECK_EQ(v4p_build_write(f, V4P_CMD_PING, 0x00u, NULL, 0u), 0);
    CHECK_EQ(hex2bin(G12_1, exp, sizeof(exp)), 32u);
    CHECK_MEM(f, exp, 32u);

    v4_test_case("§12.2 Golden WRITE DEV_GET(0) seq=1");
    pay[0] = 0x00u;
    CHECK_EQ(v4p_build_write(f, V4P_CMD_DEV_GET, 0x01u, pay, 1u), 0);
    CHECK_EQ(hex2bin(G12_2, exp, sizeof(exp)), 32u);
    CHECK_MEM(f, exp, 32u);

    v4_test_case("§12.3 Golden WRITE SET_CHUNK(256) seq=2");
    v4p_put_u16le(pay, 256u);
    CHECK_EQ(v4p_build_write(f, V4P_CMD_SET_CHUNK, 0x02u, pay, 2u), 0);
    CHECK_EQ(hex2bin(G12_3, exp, sizeof(exp)), 32u);
    CHECK_MEM(f, exp, 32u);

    v4_test_case("§4.1 Aufbau: Magic, Rest 0x00, CRC ueber 0..30");
    CHECK_EQ(f[0], 0xA5u);
    CHECK_EQ(f[1], 0x32u);
    CHECK_EQ(f[2], 0x02u);
    CHECK_EQ(f[3], 0x02u);
    CHECK_EQ(f[4], 0x00u);
    CHECK_EQ(f[5], 0x01u);
    CHECK_EQ(f[6], 0x00u);
    CHECK_EQ(f[30], 0x00u);
    CHECK_EQ(f[31], v4p_crc8(f, 31u));

    v4_test_case("§4.1 Nutzlast > 27 wird abgewiesen");
    CHECK_EQ(v4p_build_write(f, 0x01u, 0u, pay, 28u), -1);

    v4_test_case("§4.1 volle Nutzlast 27 Byte");
    {
        uint8_t big[V4P_WRITE_PAYLOAD_MAX];
        uint8_t k;
        for (k = 0; k < V4P_WRITE_PAYLOAD_MAX; k++) {
            big[k] = (uint8_t)(k + 1u);
        }
        CHECK_EQ(v4p_build_write(f, 0x01u, 0x05u, big, V4P_WRITE_PAYLOAD_MAX), 0);
        CHECK_EQ(f[3], 27u);
        CHECK_EQ(f[4], 1u);
        CHECK_EQ(f[30], 27u);
        CHECK_EQ(f[31], v4p_crc8(f, 31u));
    }
}

static void test_golden_read(void)
{
    uint8_t      f[V4P_READ_FRAME_LEN];
    v4p_read_t   r;
    v4p_status_t st;

    v4_test_case("§12.4 Golden READ GET_STATUS (128 Byte)");
    CHECK_EQ(hex2bin(G12_4, f, sizeof(f)), 128u);
    CHECK_EQ(f[V4P_READ_OFF_FLAGS], 0x00u);
    CHECK_EQ(f[V4P_READ_OFF_CRC], 0xD8u);
    CHECK_EQ(v4p_check_read(f, 128u, V4P_CMD_GET_STATUS, 0x00u, &r),
             V4P_CHECK_OK);
    CHECK_EQ(r.cmd, V4P_CMD_GET_STATUS);
    CHECK_EQ(r.seq, 0x00u);
    CHECK_EQ(r.status, V4P_ST_OK);
    CHECK_EQ(r.len, 17u);
    CHECK_EQ(r.flags, 0u);

    CHECK_EQ(v4p_dec_get_status(r.payload, r.len, &st), 0);
    CHECK_EQ(st.state, V4P_STATE_CONNECTED);
    CHECK_EQ(st.conn_index, 0xFFu);
    CHECK_EQ(st.dev_count, 4u);
    CHECK_EQ(st.scan_active, 1u);
    CHECK_EQ(st.sd_mounted, 1u);
    CHECK_EQ(st.audio_flags, V4P_AUDIO_A2DP_STREAMING);
    CHECK_EQ(st.scan_gen, 0x000Au);
    CHECK_EQ(st.sd_free_kb, 0x00078000u);
    CHECK_EQ(st.chunk, 128u);
    CHECK_EQ(st.proto_ver, V4P_PROTO_VER);
    CHECK_EQ(st.fw_ver, 2u);
    CHECK_EQ(st.sd_card_present, 1u);   /* §11: neues Byte bei +16 */

    v4_test_case("§12.5 Golden BULK FILE_READ chunk=16");
    {
        uint8_t    b[V4P_BULK_OVERHEAD + 16u];
        static const uint8_t want[5] = { 0xDEu, 0xADu, 0xBEu, 0xEFu, 0x42u };
        v4p_bulk_t bk;

        CHECK_EQ(hex2bin(G12_5, b, sizeof(b)), 28u);
        CHECK_EQ(v4p_check_bulk(b, 28u, 16u, V4P_CMD_FILE_READ, 0u, 3u, &bk),
                 V4P_CHECK_OK);
        CHECK_EQ(bk.status, V4P_ST_OK);
        CHECK_EQ(bk.handle, 0u);
        CHECK_EQ(bk.block, 3u);
        CHECK_EQ(bk.len, 5u);
        CHECK_MEM(bk.data, want, 5u);
        CHECK_EQ(v4p_get_u32le(b + 8u + 16u), 0x9C18116Cu);
    }
}

static void test_check_negative(void)
{
    uint8_t    f[V4P_READ_FRAME_LEN];
    v4p_read_t r;

    v4_test_case("§4.2 Pruefreihenfolge: jede Verletzung wird erkannt");
    CHECK_EQ(hex2bin(G12_4, f, sizeof(f)), 128u);
    CHECK_EQ(v4p_check_read(f, 128u, V4P_CMD_GET_STATUS, 0x00u, &r), V4P_CHECK_OK);

    CHECK_EQ(v4p_check_read(f, 127u, V4P_CMD_GET_STATUS, 0x00u, &r), V4P_CHECK_SIZE);
    CHECK_EQ(v4p_check_read(f, 129u, V4P_CMD_GET_STATUS, 0x00u, &r), V4P_CHECK_SIZE);

    f[0] = 0x5Bu;
    CHECK_EQ(v4p_check_read(f, 128u, V4P_CMD_GET_STATUS, 0x00u, &r), V4P_CHECK_MAGIC);
    f[0] = 0x5Au;

    f[V4P_READ_OFF_CRC] = (uint8_t)(f[V4P_READ_OFF_CRC] ^ 0x01u);
    CHECK_EQ(v4p_check_read(f, 128u, V4P_CMD_GET_STATUS, 0x00u, &r), V4P_CHECK_CRC);
    f[V4P_READ_OFF_CRC] = (uint8_t)(f[V4P_READ_OFF_CRC] ^ 0x01u);

    CHECK_EQ(v4p_check_read(f, 128u, V4P_CMD_PING, 0x00u, &r), V4P_CHECK_ECHO);
    CHECK_EQ(v4p_check_read(f, 128u, V4P_CMD_GET_STATUS, 0x01u, &r), V4P_CHECK_ECHO);

    f[4] = (uint8_t)(V4P_READ_PAYLOAD_MAX + 1u);      /* 122: unzulaessig */
    f[V4P_READ_OFF_CRC] = v4p_crc8(f, V4P_READ_CRC_RANGE);
    CHECK_EQ(v4p_check_read(f, 128u, V4P_CMD_GET_STATUS, 0x00u, &r), V4P_CHECK_LEN);
    f[4] = (uint8_t)V4P_READ_PAYLOAD_MAX;             /* 121: erlaubt */
    f[V4P_READ_OFF_CRC] = v4p_crc8(f, V4P_READ_CRC_RANGE);
    CHECK_EQ(v4p_check_read(f, 128u, V4P_CMD_GET_STATUS, 0x00u, &r), V4P_CHECK_OK);
    CHECK_EQ(r.len, V4P_READ_PAYLOAD_MAX);

    v4_test_case("§4.3 BULK: jede Verletzung wird erkannt");
    {
        uint8_t    b[V4P_BULK_OVERHEAD + 16u];
        v4p_bulk_t bk;

        CHECK_EQ(hex2bin(G12_5, b, sizeof(b)), 28u);
        CHECK_EQ(v4p_check_bulk(b, 27u, 16u, V4P_CMD_FILE_READ, 0u, 3u, &bk),
                 V4P_CHECK_SIZE);
        CHECK_EQ(v4p_check_bulk(b, 28u, 32u, V4P_CMD_FILE_READ, 0u, 3u, &bk),
                 V4P_CHECK_SIZE);

        b[0] = 0x5Au;
        CHECK_EQ(v4p_check_bulk(b, 28u, 16u, V4P_CMD_FILE_READ, 0u, 3u, &bk),
                 V4P_CHECK_MAGIC);
        b[0] = 0x5Bu;

        /* len > chunk */
        v4p_put_u16le(b + 6, 17u);
        v4p_put_u32le(b + 8u + 16u,
                      v4p_crc32(b, (size_t)V4P_BULK_HDR_LEN + 16u));
        CHECK_EQ(v4p_check_bulk(b, 28u, 16u, V4P_CMD_FILE_READ, 0u, 3u, &bk),
                 V4P_CHECK_LEN);
        v4p_put_u16le(b + 6, 5u);
        v4p_put_u32le(b + 8u + 16u,
                      v4p_crc32(b, (size_t)V4P_BULK_HDR_LEN + 16u));

        b[8u + 16u + 3u] = (uint8_t)(b[8u + 16u + 3u] ^ 0xFFu);
        CHECK_EQ(v4p_check_bulk(b, 28u, 16u, V4P_CMD_FILE_READ, 0u, 3u, &bk),
                 V4P_CHECK_CRC);
        b[8u + 16u + 3u] = (uint8_t)(b[8u + 16u + 3u] ^ 0xFFu);

        CHECK_EQ(v4p_check_bulk(b, 28u, 16u, V4P_CMD_FILE_READ, 1u, 3u, &bk),
                 V4P_CHECK_MISMATCH);
        CHECK_EQ(v4p_check_bulk(b, 28u, 16u, V4P_CMD_FILE_READ, 0u, 4u, &bk),
                 V4P_CHECK_MISMATCH);
        CHECK_EQ(v4p_check_bulk(b, 28u, 16u, V4P_CMD_FILE_CLOSE, 0u, 3u, &bk),
                 V4P_CHECK_MISMATCH);
    }
}

static void test_codec(void)
{
    uint8_t p[V4P_READ_PAYLOAD_MAX];

    v4_test_case("Codec GET_INFO Rundlauf");
    {
        v4p_info_t in;
        v4p_info_t out;

        memset(&in, 0, sizeof(in));
        in.proto_ver        = 3u;
        in.fw_ver           = 7u;
        in.write_frame_len  = 32u;
        in.read_frame_len   = 64u;
        in.bulk_payload_max = 1024u;
        in.chunk            = 256u;
        in.max_devices      = 16u;
        in.path_max         = 128u;
        CHECK_EQ(v4p_enc_get_info(p, &in), 12u);
        memset(&out, 0, sizeof(out));
        CHECK_EQ(v4p_dec_get_info(p, 12u, &out), 0);
        CHECK_EQ(out.proto_ver, V4P_PROTO_VER);
        CHECK_EQ(out.fw_ver, 7u);
        CHECK_EQ(out.bulk_payload_max, 1024u);
        CHECK_EQ(out.chunk, 256u);
        CHECK_EQ(out.max_devices, 16u);
        CHECK_EQ(out.path_max, 128u);
        CHECK_EQ(v4p_dec_get_info(p, 11u, &out), -1);
    }

    v4_test_case("Codec GET_STATUS Rundlauf");
    {
        v4p_status_t in;
        v4p_status_t out;

        memset(&in, 0, sizeof(in));
        in.state       = V4P_STATE_CONNECTED;
        in.conn_index  = 0xFFu;
        in.dev_count   = 4u;
        in.scan_active = 1u;
        in.sd_mounted  = 1u;
        in.audio_flags    = V4P_AUDIO_A2DP_STREAMING;
        in.scan_gen    = 0x000Au;
        in.sd_free_kb  = 0x00078000u;
        in.chunk       = 128u;
        in.proto_ver   = V4P_PROTO_VER;
        in.fw_ver      = 2u;
        in.sd_card_present = 1u;
        CHECK_EQ(V4P_ST_LEN, 17u);
        CHECK_EQ(v4p_enc_get_status(p, &in), V4P_ST_LEN);
        memset(&out, 0, sizeof(out));
        CHECK_EQ(v4p_dec_get_status(p, V4P_ST_LEN, &out), 0);
        CHECK_EQ(out.audio_flags, V4P_AUDIO_A2DP_STREAMING);
        CHECK_EQ(out.sd_card_present, 1u);
        CHECK_EQ(out.state, V4P_STATE_CONNECTED);
        CHECK_EQ(out.conn_index, 0xFFu);
        CHECK_EQ(out.dev_count, 4u);
        CHECK_EQ(out.scan_gen, 0x000Au);
        CHECK_EQ(out.sd_free_kb, 0x00078000u);
        CHECK_EQ(out.chunk, 128u);
        CHECK_EQ(v4p_dec_get_status(p, V4P_ST_LEN - 1u, &out), -1);
    }

    v4_test_case("Codec SD_INFO Rundlauf");
    {
        v4p_sdinfo_t in;
        v4p_sdinfo_t out;

        memset(&in, 0, sizeof(in));
        in.total_kb    = 8000000u;
        in.free_kb     = 491520u;
        in.sector_size = 512u;
        in.fat_type    = 0u;
        CHECK_EQ(v4p_enc_sd_info(p, &in), 11u);
        memset(&out, 0, sizeof(out));
        CHECK_EQ(v4p_dec_sd_info(p, 11u, &out), 0);
        CHECK_EQ(out.total_kb, 8000000u);
        CHECK_EQ(out.free_kb, 491520u);
        CHECK_EQ(out.sector_size, 512u);
        CHECK_EQ(out.fat_type, 0u);
        CHECK_EQ(v4p_dec_sd_info(p, 10u, &out), -1);
    }

    v4_test_case("Codec DEV_GET: 113 Zeichen fuellen die Nutzlast exakt (121)");
    {
        v4p_dev_t in;
        v4p_dev_t out;
        uint8_t   i;

        memset(&in, 0, sizeof(in));
        in.idx = 3u;
        for (i = 0; i < 6u; i++) {
            in.bda[i] = (uint8_t)(0xA0u + i);
        }
        for (i = 0; i < V4P_DEVNAME_MAX; i++) {
            in.name[i] = (char)('A' + (i % 26));
        }
        in.name[V4P_DEVNAME_MAX] = '\0';
        in.name_len = V4P_DEVNAME_MAX;

        CHECK_EQ(v4p_enc_dev(p, &in), V4P_READ_PAYLOAD_MAX);
        memset(&out, 0, sizeof(out));
        CHECK_EQ(v4p_dec_dev(p, V4P_READ_PAYLOAD_MAX, &out), 0);
        CHECK_EQ(out.idx, 3u);
        CHECK_MEM(out.bda, in.bda, 6u);
        CHECK_EQ(out.name_len, V4P_DEVNAME_MAX);
        CHECK_STR(out.name, in.name);

        /* Zu kurz abgeschnittenes Feld wird abgewiesen */
        CHECK_EQ(v4p_dec_dev(p, V4P_READ_PAYLOAD_MAX - 1u, &out), -1);
        /* name_len ueber der Grenze wird abgewiesen */
        p[7] = (uint8_t)(V4P_DEVNAME_MAX + 1u);
        CHECK_EQ(v4p_dec_dev(p, V4P_READ_PAYLOAD_MAX, &out), -1);
    }

    v4_test_case("Codec DIR_NEXT: 113 Zeichen fuellen die Nutzlast (121)");
    {
        v4p_dirent_t in;
        v4p_dirent_t out;
        uint8_t      i;

        memset(&in, 0, sizeof(in));
        in.index = 7u;
        in.attr  = V4P_ATTR_FILE;
        in.size  = 0xDEADBEEFu;
        for (i = 0; i < V4P_DIRNAME_MAX; i++) {
            in.name[i] = (char)('a' + (i % 26));
        }
        in.name[V4P_DIRNAME_MAX] = '\0';
        in.name_len = V4P_DIRNAME_MAX;

        CHECK_EQ(v4p_enc_dirent(p, &in), V4P_READ_PAYLOAD_MAX);
        memset(&out, 0, sizeof(out));
        CHECK_EQ(v4p_dec_dirent(p, V4P_READ_PAYLOAD_MAX, &out), 0);
        CHECK_EQ(out.index, 7u);
        CHECK_EQ(out.attr, V4P_ATTR_FILE);
        CHECK_EQ(out.size, 0xDEADBEEFu);
        CHECK_EQ(out.name_len, V4P_DIRNAME_MAX);
        CHECK_STR(out.name, in.name);
        CHECK_EQ(v4p_dec_dirent(p, V4P_READ_PAYLOAD_MAX - 1u, &out), -1);

        p[7] = (uint8_t)(V4P_DIRNAME_MAX + 1u);
        CHECK_EQ(v4p_dec_dirent(p, V4P_READ_PAYLOAD_MAX, &out), -1);
    }

    v4_test_case("Codec-Feldgrenzen: Nutzlastlaengen passen in 121 Byte");
    {
        v4p_dev_t d;
        v4p_dirent_t e;

        memset(&d, 0, sizeof(d));
        d.name_len = 0xFFu;                 /* Encoder muss klemmen */
        CHECK_EQ(v4p_enc_dev(p, &d), V4P_READ_PAYLOAD_MAX);
        memset(&e, 0, sizeof(e));
        e.name_len = 0xFFu;
        CHECK_EQ(v4p_enc_dirent(p, &e), V4P_READ_PAYLOAD_MAX);
        CHECK_EQ((size_t)8u + (size_t)V4P_DEVNAME_MAX,
                 (size_t)V4P_READ_PAYLOAD_MAX);
        CHECK_EQ((size_t)8u + (size_t)V4P_DIRNAME_MAX,
                 (size_t)V4P_READ_PAYLOAD_MAX);
    }
}

/*
 * Diese Nutzlasten sind von Hand aus den Tabellen in §11 abgeschrieben --
 * unabhaengig vom Codec im Programm. Nur so faellt auf, wenn Encoder UND
 * Decoder denselben falschen Offset benutzen (der Mock wuerde das verdecken,
 * weil er dieselben Funktionen verwendet).
 */
static void test_golden_payloads(void)
{
    uint8_t p[V4P_READ_PAYLOAD_MAX];
    uint8_t exp[V4P_READ_PAYLOAD_MAX];

    v4_test_case("§11 Golden GET_INFO/PING Nutzlast (12 Byte)");
    {
        static const char hex[] = "03 07 20 80 00 04 00 01 10 80 00 00";
        v4p_info_t in;
        v4p_info_t out;

        CHECK_EQ(hex2bin(hex, exp, sizeof(exp)), 12u);
        memset(&in, 0, sizeof(in));
        in.proto_ver        = 3u;      /* +0  */
        in.fw_ver           = 7u;      /* +1  */
        in.write_frame_len  = 32u;     /* +2  0x20 */
        in.read_frame_len   = 128u;    /* +3  0x80 */
        in.bulk_payload_max = 1024u;   /* +4..5  00 04 */
        in.chunk            = 256u;    /* +6..7  00 01 */
        in.max_devices      = 16u;     /* +8  0x10 */
        in.path_max         = 128u;    /* +9  0x80 */

        CHECK_EQ(v4p_enc_get_info(p, &in), 12u);
        CHECK_MEM(p, exp, 12u);

        memset(&out, 0, sizeof(out));
        CHECK_EQ(v4p_dec_get_info(exp, 12u, &out), 0);
        CHECK_EQ(out.proto_ver, V4P_PROTO_VER);
        CHECK_EQ(out.fw_ver, 7u);
        CHECK_EQ(out.write_frame_len, 32u);
        CHECK_EQ(out.read_frame_len, 128u);
        CHECK_EQ(out.bulk_payload_max, 1024u);
        CHECK_EQ(out.chunk, 256u);
        CHECK_EQ(out.max_devices, 16u);
        CHECK_EQ(out.path_max, 128u);
    }

    v4_test_case("§11 Golden GET_STATUS Nutzlast (17 Byte, L = 17)");
    {
        /* Genau die Nutzlast des Golden Frames §12.4: die alten Offsets
         * bleiben, `sd_card_present` kommt als 17. Byte hinzu. */
        static const char hex[] =
            "03 FF 04 01 01 01 0A 00 00 80 07 00 80 00 03 02 01";
        v4p_status_t in;
        v4p_status_t out;

        CHECK_EQ(hex2bin(hex, exp, sizeof(exp)), 17u);
        memset(&in, 0, sizeof(in));
        in.state       = V4P_STATE_CONNECTED;  /* +0  03 */
        in.conn_index  = 0xFFu;                /* +1  FF */
        in.dev_count   = 4u;                   /* +2  04 */
        in.scan_active = 1u;                   /* +3  01 */
        in.sd_mounted  = 1u;                   /* +4  01 */
        in.audio_flags    = V4P_AUDIO_A2DP_STREAMING;  /* +5  01 */
        in.scan_gen    = 0x000Au;              /* +6..7  0A 00 */
        in.sd_free_kb  = 0x00078000u;          /* +8..11 00 80 07 00 */
        in.chunk       = 0x0080u;              /* +12..13 80 00 */
        in.proto_ver   = 3u;                   /* +14 */
        in.fw_ver      = 2u;                   /* +15 */
        in.sd_card_present = 1u;               /* +16  NEU */

        CHECK_EQ(v4p_enc_get_status(p, &in), V4P_ST_LEN);
        CHECK_MEM(p, exp, V4P_ST_LEN);

        memset(&out, 0, sizeof(out));
        CHECK_EQ(v4p_dec_get_status(exp, V4P_ST_LEN, &out), 0);
        CHECK_EQ(out.state, V4P_STATE_CONNECTED);
        CHECK_EQ(out.conn_index, 0xFFu);
        CHECK_EQ(out.dev_count, 4u);
        CHECK_EQ(out.scan_active, 1u);
        CHECK_EQ(out.sd_mounted, 1u);
        CHECK_EQ(out.audio_flags, V4P_AUDIO_A2DP_STREAMING);
        CHECK_EQ(out.scan_gen, 0x000Au);
        CHECK_EQ(out.sd_free_kb, 0x00078000u);
        CHECK_EQ(out.chunk, 0x0080u);
        CHECK_EQ(out.proto_ver, V4P_PROTO_VER);
        CHECK_EQ(out.fw_ver, 2u);
    }

    v4_test_case("§11 Golden SD_INFO Nutzlast (11 Byte)");
    {
        static const char hex[] = "00 12 7A 00 00 80 07 00 00 02 00";
        v4p_sdinfo_t in;
        v4p_sdinfo_t out;

        CHECK_EQ(hex2bin(hex, exp, sizeof(exp)), 11u);
        memset(&in, 0, sizeof(in));
        in.total_kb    = 8000000u;      /* +0..3  00 12 7A 00 */
        in.free_kb     = 491520u;       /* +4..7  00 80 07 00 */
        in.sector_size = 512u;          /* +8..9  00 02 */
        in.fat_type    = 0u;            /* +10 */

        CHECK_EQ(v4p_enc_sd_info(p, &in), 11u);
        CHECK_MEM(p, exp, 11u);

        memset(&out, 0, sizeof(out));
        CHECK_EQ(v4p_dec_sd_info(exp, 11u, &out), 0);
        CHECK_EQ(out.total_kb, 8000000u);
        CHECK_EQ(out.free_kb, 491520u);
        CHECK_EQ(out.sector_size, 512u);
        CHECK_EQ(out.fat_type, 0u);
    }

    v4_test_case("§11 Golden DEV_GET Nutzlast (8 + name_len)");
    {
        static const char hex[] =
            "01 AA BB CC DD EE FF 04 54 45 53 54";      /* idx=1, "TEST" */
        v4p_dev_t in;
        v4p_dev_t out;
        uint8_t   i;

        CHECK_EQ(hex2bin(hex, exp, sizeof(exp)), 12u);
        memset(&in, 0, sizeof(in));
        in.idx = 1u;
        in.bda[0] = 0xAAu; in.bda[1] = 0xBBu; in.bda[2] = 0xCCu;
        in.bda[3] = 0xDDu; in.bda[4] = 0xEEu; in.bda[5] = 0xFFu;
        in.name_len = 4u;
        for (i = 0; i < 4u; i++) {
            in.name[i] = "TEST"[i];
        }
        in.name[4] = '\0';

        CHECK_EQ(v4p_enc_dev(p, &in), 12u);
        CHECK_MEM(p, exp, 12u);

        memset(&out, 0, sizeof(out));
        CHECK_EQ(v4p_dec_dev(exp, 12u, &out), 0);
        CHECK_EQ(out.idx, 1u);
        CHECK_MEM(out.bda, in.bda, 6u);
        CHECK_EQ(out.name_len, 4u);
        CHECK_STR(out.name, "TEST");
    }

    v4_test_case("§11 Golden DIR_NEXT Nutzlast (8 + name_len)");
    {
        static const char hex[] =
            "02 01 20 EF BE AD DE 03 41 2E 42";  /* idx=258, size=DEADBEEF */
        v4p_dirent_t in;
        v4p_dirent_t out;

        CHECK_EQ(hex2bin(hex, exp, sizeof(exp)), 11u);
        memset(&in, 0, sizeof(in));
        in.index    = 0x0102u;          /* +0..1  02 01 */
        in.attr     = V4P_ATTR_FILE;    /* +2  20 */
        in.size     = 0xDEADBEEFu;      /* +3..6  EF BE AD DE */
        in.name_len = 3u;               /* +7  03 */
        in.name[0] = 'A'; in.name[1] = '.'; in.name[2] = 'B';
        in.name[3] = '\0';

        CHECK_EQ(v4p_enc_dirent(p, &in), 11u);
        CHECK_MEM(p, exp, 11u);

        memset(&out, 0, sizeof(out));
        CHECK_EQ(v4p_dec_dirent(exp, 11u, &out), 0);
        CHECK_EQ(out.index, 0x0102u);
        CHECK_EQ(out.attr, V4P_ATTR_FILE);
        CHECK_EQ(out.size, 0xDEADBEEFu);
        CHECK_EQ(out.name_len, 3u);
        CHECK_STR(out.name, "A.B");
    }

    v4_test_case("§11 Golden FILE_OPEN Nutzlast (6 Byte)");
    {
        static const char hex[] = "01 E8 03 00 00 20";  /* size = 1000 */
        uint8_t  handle = 0u;
        uint32_t size = 0u;
        uint8_t  attr = 0u;

        CHECK_EQ(hex2bin(hex, exp, sizeof(exp)), 6u);
        CHECK_EQ(v4p_enc_file_open(p, 1u, 1000u, V4P_ATTR_FILE), 6u);
        CHECK_MEM(p, exp, 6u);

        CHECK_EQ(v4p_dec_file_open(exp, 6u, &handle, &size, &attr), 0);
        CHECK_EQ(handle, 1u);
        CHECK_EQ(size, 1000u);
        CHECK_EQ(attr, V4P_ATTR_FILE);

        CHECK_EQ(v4p_dec_file_open(exp, 5u, &handle, &size, &attr), -1);
        CHECK_EQ(v4p_dec_file_open(exp, 6u, NULL, NULL, NULL), 0);
    }
}

/*
 * Pinnt die ZAHLEN der Spezifikation. Ohne das bliebe z.B. ein auf 0x17
 * geaenderter Statuscode unbemerkt, weil Master und Mock dasselbe enum
 * benutzen und die Tests nur symbolisch vergleichen.
 */
static void test_spec_constants(void)
{
    v4_test_case("§5.1 Statuscodes sind numerisch gepinnt");
    CHECK_EQ(V4P_ST_OK, 0x00);
    CHECK_EQ(V4P_ST_BUSY, 0x01);
    CHECK_EQ(V4P_ST_BAD_CRC, 0x02);
    CHECK_EQ(V4P_ST_BAD_CMD, 0x03);
    CHECK_EQ(V4P_ST_BAD_ARG, 0x04);
    CHECK_EQ(V4P_ST_BAD_STATE, 0x05);
    CHECK_EQ(V4P_ST_NO_SD, 0x06);
    CHECK_EQ(V4P_ST_NOT_FOUND, 0x07);
    CHECK_EQ(V4P_ST_IO_ERR, 0x08);
    CHECK_EQ(V4P_ST_END, 0x09);
    CHECK_EQ(V4P_ST_TOO_LONG, 0x0A);
    CHECK_EQ(V4P_ST_NO_HANDLE, 0x0B);
    CHECK_EQ(V4P_ST_BT_ERR, 0x0C);

    v4_test_case("§11 Befehlscodes sind numerisch gepinnt");
    CHECK_EQ(V4P_CMD_PING, 0x01);
    CHECK_EQ(V4P_CMD_GET_STATUS, 0x02);
    CHECK_EQ(V4P_CMD_GET_INFO, 0x03);
    CHECK_EQ(V4P_CMD_SCAN_START, 0x10);
    CHECK_EQ(V4P_CMD_SCAN_STOP, 0x11);
    CHECK_EQ(V4P_CMD_DEV_COUNT, 0x12);
    CHECK_EQ(V4P_CMD_DEV_GET, 0x13);
    CHECK_EQ(V4P_CMD_CONNECT, 0x20);
    CHECK_EQ(V4P_CMD_CONNECT_BDA, 0x21);
    CHECK_EQ(V4P_CMD_DISCONNECT, 0x22);
    CHECK_EQ(V4P_CMD_FORGET, 0x23);
    CHECK_EQ(V4P_CMD_SD_MOUNT, 0x30);
    CHECK_EQ(V4P_CMD_SD_INFO, 0x31);
    CHECK_EQ(V4P_CMD_SET_CHUNK, 0x32);
    CHECK_EQ(V4P_CMD_PATH_CLEAR, 0x38);
    CHECK_EQ(V4P_CMD_PATH_APPEND, 0x39);
    CHECK_EQ(V4P_CMD_DIR_OPEN, 0x40);
    CHECK_EQ(V4P_CMD_DIR_NEXT, 0x41);
    CHECK_EQ(V4P_CMD_DIR_CLOSE, 0x42);
    CHECK_EQ(V4P_CMD_FILE_OPEN, 0x50);
    CHECK_EQ(V4P_CMD_FILE_READ, 0x51);
    CHECK_EQ(V4P_CMD_FILE_CLOSE, 0x52);
    CHECK_EQ(V4P_CMD_PLAY_FILE, 0x60);
    CHECK_EQ(V4P_CMD_STOP_PLAY, 0x61);
    CHECK_EQ(V4P_CMD_RESET, 0x7E);

    v4_test_case("§4/§5.2/§9/§10 Rahmengroessen und Grenzen");
    CHECK_EQ(V4P_MAGIC_WRITE, 0xA5);
    CHECK_EQ(V4P_MAGIC_READ, 0x5A);
    CHECK_EQ(V4P_MAGIC_BULK, 0x5B);
    CHECK_EQ(V4P_WRITE_FRAME_LEN, 32);
    CHECK_EQ(V4P_READ_FRAME_LEN, 128);          /* §4.2: seit v2.0 128 */
    CHECK_EQ(V4P_WRITE_PAYLOAD_MAX, 27);
    CHECK_EQ(V4P_READ_PAYLOAD_MAX, 121);        /* 128 - 5 - 1 - 1 */
    CHECK_EQ(V4P_READ_OFF_FLAGS, 126);
    CHECK_EQ(V4P_READ_OFF_CRC, 127);
    CHECK_EQ(V4P_READ_CRC_RANGE, 127);
    CHECK_EQ(V4P_BULK_HDR_LEN, 8);
    CHECK_EQ(V4P_BULK_CRC_LEN, 4);
    CHECK_EQ(V4P_BULK_OVERHEAD, 12);
    CHECK_EQ((unsigned)(4u + V4P_WRITE_PAYLOAD_MAX + 1u), 32u);
    CHECK_EQ((unsigned)(5u + V4P_READ_PAYLOAD_MAX + 1u + 1u), 128u);
    CHECK((unsigned)(V4P_BULK_OVERHEAD + V4P_CHUNK_MAX) == 1036u);

    v4_test_case("§11 GET_STATUS-Offsets (der massgebliche Hinweis)");
    CHECK_EQ(V4P_ST_LEN, 17);
    CHECK_EQ(V4P_ST_OFF_STATE, 0);
    CHECK_EQ(V4P_ST_OFF_CONN_INDEX, 1);
    CHECK_EQ(V4P_ST_OFF_DEV_COUNT, 2);
    CHECK_EQ(V4P_ST_OFF_SCAN_ACTIVE, 3);
    CHECK_EQ(V4P_ST_OFF_SD_MOUNTED, 4);
    CHECK_EQ(V4P_ST_OFF_AUDIO_FLAGS, 5);
    CHECK_EQ(V4P_ST_OFF_SCAN_GEN, 6);
    CHECK_EQ(V4P_ST_OFF_SD_FREE_KB, 8);
    CHECK_EQ(V4P_ST_OFF_CHUNK, 12);
    CHECK_EQ(V4P_ST_OFF_PROTO_VER, 14);
    CHECK_EQ(V4P_ST_OFF_FW_VER, 15);
    CHECK_EQ(V4P_ST_OFF_SD_CARD_PRESENT, 16);   /* das neue Byte */
    CHECK_EQ(V4P_ST_OFF_SD_CARD_PRESENT, V4P_ST_LEN - 1u);
    CHECK_EQ(V4P_AUDIO_A2DP_STREAMING, 0x01);
    CHECK_EQ(V4P_AUDIO_SD_PLAYBACK, 0x02);

    CHECK_EQ(V4P_CHUNK_DEFAULT, 128);
    CHECK_EQ(V4P_CHUNK_MIN, 16);
    CHECK_EQ(V4P_CHUNK_MAX, 1024);
    CHECK_EQ(V4P_PATH_MAX, 128);
    CHECK_EQ(V4P_DIRNAME_MAX, 113);      /* §10: seit v2.0 113 Zeichen */
    CHECK_EQ(V4P_DEVNAME_MAX, 113);
    CHECK_EQ(V4P_NAME_BUF, 114);         /* §10: mindestens 114 Byte */
    /* 8 Byte Kopf + 113 Zeichen fuellen die READ-Nutzlast exakt aus. */
    CHECK_EQ((unsigned)(8u + V4P_DEVNAME_MAX), (unsigned)V4P_READ_PAYLOAD_MAX);
    CHECK_EQ(V4P_MAX_DEVICES, 16);
    CHECK_EQ(V4P_MAX_DIR_HANDLES, 2);
    CHECK_EQ(V4P_MAX_FILE_HANDLES, 4);

    CHECK_EQ(V4P_FLAG_NAME_TRUNCATED, 0x01);
    CHECK_EQ(V4P_FLAG_MORE, 0x02);
    CHECK_EQ(V4P_ATTR_RO, 0x01);
    CHECK_EQ(V4P_ATTR_DIR, 0x10);
    CHECK_EQ(V4P_ATTR_FILE, 0x20);

    CHECK_EQ(V4P_STATE_IDLE, 0);
    CHECK_EQ(V4P_STATE_SCANNING, 1);
    CHECK_EQ(V4P_STATE_CONNECTING, 2);
    CHECK_EQ(V4P_STATE_CONNECTED, 3);
    CHECK_EQ(V4P_STATE_SUSPENDED, 4);

    CHECK_EQ(V4P_PROTO_VER, 3);          /* proto_ver 3 = 128-Byte-READ */
}

int test_proto(void)
{
    v4_test_begin("test_proto");

    test_byteorder();
    test_crc();
    test_build_write();
    test_golden_read();
    test_check_negative();
    test_codec();
    test_golden_payloads();
    test_spec_constants();

    return v4_test_failures;
}
