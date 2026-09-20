/*
 * test_fuzz.c -- Robustheit der Parser gegen beliebige Bytes.
 *
 * Warum das wichtig ist: der ESP32 kann nicht clock-stretchen, und nach einem
 * Bus-Reset oder einem abgebrochenen Transfer koennen auf dem Bus beliebige
 * Bytes stehen (§1.3, §14.3). Die Parser muessen Müll erkennen, ohne dabei
 * ueber Puffergrenzen zu lesen. Unter AddressSanitizer ist dieser Test ein
 * echter Out-of-Bounds-Detektor.
 *
 * Deterministisch (eigener Zufallsgenerator, kein time()/random()), damit der
 * Lauf reproduzierbar ist und auch unter qemu-m68k dasselbe Ergebnis liefert.
 */

#include "v4_test.h"

#include "../v4_proto.h"

/* xorshift64*: klein, deterministisch, ausreichend fuer diesen Zweck */
static uint64_t s_rng;

static void rng_seed(uint64_t seed)
{
    s_rng = (seed != 0u) ? seed : 0x9E3779B97F4A7C15ull;
}

static uint32_t rng_next(void)
{
    s_rng ^= s_rng >> 12;
    s_rng ^= s_rng << 25;
    s_rng ^= s_rng >> 27;
    return (uint32_t)((s_rng * 0x2545F4914F6CDD1Dull) >> 32);
}

static uint8_t rng_byte(void)
{
    return (uint8_t)(rng_next() & 0xFFu);
}

/* ------------------------------------------------------------------ */

static void fuzz_check_read(void)
{
    static uint8_t f[V4P_READ_FRAME_LEN];
    unsigned long  accepted = 0ul;
    unsigned long  rejected = 0ul;
    int            i;

    v4_test_case("Fuzzing: v4p_check_read auf Zufallsbytes");

    for (i = 0; i < 100000; i++) {
        v4p_read_t r;
        uint8_t    cmd = rng_byte();
        uint8_t    seq = rng_byte();
        int        rc;
        size_t     k;

        for (k = 0; k < sizeof(f); k++) {
            f[k] = rng_byte();
        }
        /* Gelegentlich eine gueltige Magic einstreuen, damit auch der
         * tiefere Pfad (CRC, Echo, len) getroffen wird. */
        if ((i % 4) == 0) {
            f[0] = V4P_MAGIC_READ;
        }

        rc = v4p_check_read(f, sizeof(f), cmd, seq, &r);
        if (rc == V4P_CHECK_OK) {
            /* Angenommen heisst: ALLE Bedingungen muessen erfuellt sein.
             * Diese Invariante ist der eigentliche Pruefpunkt. */
            accepted++;
            CHECK_EQ(f[0], V4P_MAGIC_READ);
            CHECK_EQ(f[V4P_READ_OFF_CRC], v4p_crc8(f, V4P_READ_CRC_RANGE));
            CHECK_EQ(f[1], cmd);
            CHECK_EQ(f[2], seq);
            CHECK(f[4] <= V4P_READ_PAYLOAD_MAX);
            CHECK_EQ(r.len, f[4]);
        } else {
            rejected++;
            CHECK(rc < 0);
        }
    }
    /* Zufaelliger Muell darf praktisch nie durchkommen. Ein kaputter Pruefer
     * wuerde dagegen einen grossen Anteil akzeptieren -- daher die Schranke
     * statt eines exakten Werts (ein akzeptierter Frame durchlaeuft oben
     * ohnehin alle Invarianten). */
    CHECK(accepted <= 10ul);
    CHECK(accepted + rejected > 0ul);
}

static void fuzz_check_read_wrong_length(void)
{
    static uint8_t f[V4P_READ_FRAME_LEN + 8u];
    v4p_read_t     r;
    size_t         k;

    v4_test_case("Fuzzing: falsche Leselaenge wird immer verworfen");

    for (k = 0; k < sizeof(f); k++) {
        f[k] = rng_byte();
    }
    /* Selbst ein perfekt aussehender Frame muss bei falscher Laenge fallen. */
    (void)v4p_build_read(f, V4P_CMD_PING, 0x11u, V4P_ST_OK, 0u, NULL, 0u);
    CHECK_EQ(v4p_check_read(f, V4P_READ_FRAME_LEN, V4P_CMD_PING, 0x11u, &r),
             V4P_CHECK_OK);
    CHECK_EQ(v4p_check_read(f, V4P_READ_FRAME_LEN - 1u, V4P_CMD_PING, 0x11u, &r),
             V4P_CHECK_SIZE);
    CHECK_EQ(v4p_check_read(f, V4P_READ_FRAME_LEN + 1u, V4P_CMD_PING, 0x11u, &r),
             V4P_CHECK_SIZE);
    CHECK_EQ(v4p_check_read(f, 0u, V4P_CMD_PING, 0x11u, &r), V4P_CHECK_SIZE);
}

static void fuzz_check_bulk(void)
{
    static uint8_t f[V4P_BULK_OVERHEAD + V4P_CHUNK_MAX];
    unsigned long  accepted = 0ul;
    unsigned long  rejected = 0ul;
    int            i;

    v4_test_case("Fuzzing: v4p_check_bulk auf Zufallsbytes");

    for (i = 0; i < 20000; i++) {
        v4p_bulk_t b;
        uint16_t   chunk = (uint16_t)(V4P_CHUNK_MIN
                                      + (rng_next() % 1009u));  /* 16..1024 */
        uint8_t    cmd   = rng_byte();
        uint8_t    handle= rng_byte();
        uint16_t   block = (uint16_t)rng_next();
        size_t     total = (size_t)V4P_BULK_OVERHEAD + (size_t)chunk;
        size_t     k;
        int        rc;

        if (chunk > V4P_CHUNK_MAX) {
            chunk = V4P_CHUNK_MAX;
            total = (size_t)V4P_BULK_OVERHEAD + (size_t)chunk;
        }
        for (k = 0; k < total; k++) {
            f[k] = rng_byte();
        }
        if ((i % 4) == 0) {
            f[0] = V4P_MAGIC_BULK;
        }

        rc = v4p_check_bulk(f, total, chunk, cmd, handle, block, &b);
        if (rc == V4P_CHECK_OK) {
            accepted++;
            CHECK_EQ(f[0], V4P_MAGIC_BULK);
            CHECK(v4p_get_u16le(f + 6) <= chunk);
            CHECK_EQ(v4p_get_u32le(f + V4P_BULK_HDR_LEN + chunk),
                     v4p_crc32(f, (size_t)V4P_BULK_HDR_LEN + (size_t)chunk));
            CHECK_EQ(f[1], cmd);
            CHECK_EQ(f[3], handle);
            CHECK_EQ(v4p_get_u16le(f + 4), block);
            CHECK_EQ(b.len, v4p_get_u16le(f + 6));
            CHECK(b.data == f + V4P_BULK_HDR_LEN);
        } else {
            rejected++;
            CHECK(rc < 0);
        }
    }
    CHECK(accepted <= 10ul);
    CHECK(accepted + rejected > 0ul);
}

static void fuzz_decoders(void)
{
    uint8_t       p[V4P_READ_PAYLOAD_MAX];
    int           i;
    unsigned long ok_info = 0ul, ok_status = 0ul, ok_sd = 0ul;
    unsigned long ok_dev = 0ul, ok_ent = 0ul, ok_fo = 0ul;

    v4_test_case("Fuzzing: Decoder auf Zufallsnutzlasten");

    for (i = 0; i < 20000; i++) {
        v4p_info_t   info;
        v4p_status_t st;
        v4p_sdinfo_t si;
        v4p_dev_t    dev;
        v4p_dirent_t ent;
        uint8_t      len = (uint8_t)(rng_next() % 128u);
        uint8_t      handle = 0u;
        uint32_t     size = 0u;
        uint8_t      attr = 0u;
        size_t       k;

        for (k = 0; k < sizeof(p); k++) {
            p[k] = rng_byte();
        }

        /* Rueckgabe 0 oder -1 -- beides ist richtig, Absturz oder
         * Pufferueberlauf waere der Fehler. Bei Erfolg muss das Ergebnis
         * in sich stimmig sein. */
        if (v4p_dec_get_info(p, len, &info) == 0) {
            ok_info++;
            CHECK_EQ(info.proto_ver, p[0]);
            CHECK_EQ(info.chunk, v4p_get_u16le(p + 6));
        }
        if (v4p_dec_get_status(p, len, &st) == 0) {
            ok_status++;
            CHECK_EQ(st.state, p[0]);
            CHECK_EQ(st.sd_free_kb, v4p_get_u32le(p + 8));
        }
        if (v4p_dec_sd_info(p, len, &si) == 0) {
            ok_sd++;
            CHECK_EQ(si.total_kb, v4p_get_u32le(p));
            CHECK_EQ(si.fat_type, p[10]);
        }
        if (v4p_dec_dev(p, len, &dev) == 0) {
            ok_dev++;
            CHECK(dev.name_len <= V4P_DEVNAME_MAX);
            CHECK_EQ(dev.name[dev.name_len], '\0');
        }
        if (v4p_dec_dirent(p, len, &ent) == 0) {
            ok_ent++;
            CHECK(ent.name_len <= V4P_DIRNAME_MAX);
            CHECK_EQ(ent.name[ent.name_len], '\0');
        }
        if (v4p_dec_file_open(p, len, &handle, &size, &attr) == 0) {
            ok_fo++;
            CHECK_EQ(handle, p[0]);
            CHECK_EQ(size, v4p_get_u32le(p + 1));
            CHECK_EQ(attr, p[5]);
        }
    }
    /* Gegenprobe: ein Decoder, der konstant -1 liefert, wuerde oben nie
     * einen Check erzeugen -- deshalb muss jeder mindestens einmal
     * erfolgreich gewesen sein. */
    CHECK(ok_info   > 0u);
    CHECK(ok_status > 0u);
    CHECK(ok_sd     > 0u);
    CHECK(ok_dev    > 0u);
    CHECK(ok_ent    > 0u);
    CHECK(ok_fo     > 0u);
}

static void fuzz_helpers(void)
{
    int      i;

    v4_test_case("Fuzzing: Frame-Aufbau mit zufaelligen Laengen");

    for (i = 0; i < 20000; i++) {
        /* Der Puffer muss so gross sein wie die GROESSTE Laenge, die der
         * jeweilige Aufbau akzeptiert: 27 (WRITE) bzw. 121 (READ). Sonst
         * liest der Aufbau ueber den Puffer -- genau das hat der
         * ASan-Lauf dieses Tests zunaechst aufgedeckt. */
        uint8_t payload[V4P_READ_PAYLOAD_MAX];
        uint8_t wf[V4P_WRITE_FRAME_LEN];
        uint8_t rd[V4P_READ_FRAME_LEN];
        uint8_t len = (uint8_t)(rng_next() % 128u);  /* auch > 121 */
        size_t  k;
        int     rc;

        for (k = 0; k < sizeof(payload); k++) {
            payload[k] = rng_byte();
        }

        rc = v4p_build_write(wf, rng_byte(), rng_byte(), payload, len);
        if (len > V4P_WRITE_PAYLOAD_MAX) {
            CHECK_EQ(rc, -1);
        } else {
            CHECK_EQ(rc, 0);
            CHECK_EQ(wf[0], V4P_MAGIC_WRITE);
            CHECK_EQ(wf[3], len);
            CHECK_EQ(wf[31], v4p_crc8(wf, 31u));
            CHECK_MEM(wf + 4, payload, len);
        }

        /* READ-Frame: Laenge ueber 121 wird geprueft, BEVOR gelesen wird */
        rc = v4p_build_read(rd, rng_byte(), rng_byte(), rng_byte(), rng_byte(),
                            payload, len);
        if (len > V4P_READ_PAYLOAD_MAX) {
            CHECK_EQ(rc, -1);
        } else {
            CHECK_EQ(rc, 0);
        }

        /* BULK-Frame: unzulaessige chunk-Groessen muessen abgelehnt werden */
        {
            uint8_t  bf[V4P_BULK_OVERHEAD + V4P_CHUNK_MAX];
            uint16_t chunk = (uint16_t)(rng_next() % 1200u);
            uint16_t dlen  = (uint16_t)(rng_next() % 1200u);
            uint16_t dlen_c;
            int      expect_ok;

            /* `payload` hat nur 27 gueltige Byte -- hier klemmen, sonst
             * liest der Test selbst ueber den Puffer. */
            dlen_c = (dlen > V4P_WRITE_PAYLOAD_MAX) ? V4P_WRITE_PAYLOAD_MAX
                                                   : dlen;
            expect_ok = (chunk >= V4P_CHUNK_MIN && chunk <= V4P_CHUNK_MAX
                         && dlen_c <= chunk);
            rc = v4p_build_bulk(bf, chunk, 0x51u, 0x00u, 0u, 0u, payload,
                                dlen_c);
            CHECK_EQ(rc, expect_ok ? 0 : -1);
        }
    }
}

int test_fuzz(void)
{
    v4_test_begin("test_fuzz");

    rng_seed(0x123456789ABCDEFull);

    fuzz_check_read();
    fuzz_check_read_wrong_length();
    fuzz_check_bulk();
    fuzz_decoders();
    fuzz_helpers();

    return v4_test_failures;
}
