/*
 * test_proto.c -- Rahmenformat, Pruefsumme und Nutzlast-Codecs.
 *
 * Der Schwerpunkt liegt auf dem, was zwischen Big Endian (V4) und Little Endian
 * (ESP32) schiefgehen kann: Byte-Lage jedes Feldes, CRC, Rahmengroesse und die
 * Ableitungen fuer die Anzeige. Ein Test, der nur "es kam etwas an" prueft,
 * wuerde einen Byte-Order-Fehler auf dem Linux-Host nicht finden.
 */

#include "v4_test.h"

#include "../v4_proto.h"

/* ------------------------------------------------------------------ */
/* Pruefsumme und Byte-Order                                           */
/* ------------------------------------------------------------------ */

static void test_crc(void)
{
    /* Der Standardpruefwert fuer CRC-8/SMBUS (Poly 0x07, Init 0x00) ist 0xF4.
     * Weicht er ab, ist die Polynomrichtung falsch -- dann antwortet der ESP32
     * auf jeden Rahmen mit BAD_CRC. */
    static const char *const check = "123456789";

    v4_test_case("CRC-8/SMBUS: Standardpruefwert 0xF4");
    CHECK_EQ(v4p_crc8((const uint8_t *)check, 9u), 0xF4u);

    v4_test_case("CRC-8: leere Eingabe ist 0x00");
    CHECK_EQ(v4p_crc8(NULL, 0u), 0x00u);

    v4_test_case("CRC-8: ein Byte 0x00 -> 0x00, 0x01 -> 0x07");
    {
        uint8_t b = 0x00u;

        CHECK_EQ(v4p_crc8(&b, 1u), 0x00u);
        b = 0x01u;
        CHECK_EQ(v4p_crc8(&b, 1u), 0x07u);
    }
}

/* ------------------------------------------------------------------ */
/* WRITE-Rahmen                                                        */
/* ------------------------------------------------------------------ */

static void test_write_frame(void)
{
    uint8_t f[V4P_WRITE_FRAME_LEN];
    uint8_t payload[3];

    payload[0] = 0x01u;
    payload[1] = 0x02u;
    payload[2] = 0x03u;

    v4_test_case("WRITE: 32 Byte, Magic 0xA5, cmd/seq/len an 1/2/3");
    CHECK_EQ(v4p_build_write(f, V4P_CMD_SET_PORT_MODE, 0x42u, payload, 3u), 0);
    CHECK_EQ(f[0], 0xA5u);
    CHECK_EQ(f[1], V4P_CMD_SET_PORT_MODE);
    CHECK_EQ(f[2], 0x42u);
    CHECK_EQ(f[3], 3u);
    CHECK_EQ(f[4], 1u);
    CHECK_EQ(f[5], 2u);
    CHECK_EQ(f[6], 3u);
    CHECK_EQ(f[7], 0u);                 /* Fuellbytes bleiben 0 */
    CHECK_EQ(f[31], v4p_crc8(f, 31u));

    v4_test_case("WRITE: zu lange Nutzlast wird abgewiesen");
    CHECK_EQ(v4p_build_write(f, 0x01u, 0u, payload,
                             (uint8_t)(V4P_WRITE_PAYLOAD_MAX + 1u)), -1);

    v4_test_case("WRITE: 27 Byte Nutzlast passen genau");
    {
        uint8_t big[V4P_WRITE_PAYLOAD_MAX];
        uint8_t i;

        for (i = 0u; i < (uint8_t)V4P_WRITE_PAYLOAD_MAX; i++) {
            big[i] = (uint8_t)(0x10u + i);
        }
        CHECK_EQ(v4p_build_write(f, 0x7Eu, 0xFFu, big,
                                 (uint8_t)V4P_WRITE_PAYLOAD_MAX), 0);
        CHECK_EQ(f[3], V4P_WRITE_PAYLOAD_MAX);
        CHECK_EQ(f[4 + V4P_WRITE_PAYLOAD_MAX - 1u],
                 0x10u + V4P_WRITE_PAYLOAD_MAX - 1u);
    }
}

/* ------------------------------------------------------------------ */
/* READ-Rahmen: die 128-Byte-Falle                                     */
/* ------------------------------------------------------------------ */

static void test_read_frame(void)
{
    uint8_t    f[V4P_READ_FRAME_LEN];
    uint8_t    payload[4];
    v4p_read_t out;

    payload[0] = 0xDEu;
    payload[1] = 0xADu;
    payload[2] = 0xBEu;
    payload[3] = 0xEFu;

    v4_test_case("READ: 128 Byte, Flags an +126, CRC an +127");
    CHECK_EQ(v4p_build_read(f, V4P_CMD_PING, 7u, V4P_ST_OK, V4P_FLAG_NONE,
                            payload, 4u), 0);
    CHECK_EQ(f[0], 0x5Au);
    CHECK_EQ(f[1], V4P_CMD_PING);
    CHECK_EQ(f[2], 7u);
    CHECK_EQ(f[3], V4P_ST_OK);
    CHECK_EQ(f[4], 4u);
    CHECK_EQ(f[5], 0xDEu);
    CHECK_EQ(f[126], V4P_FLAG_NONE);
    CHECK_EQ(f[127], v4p_crc8(f, 127u));

    v4_test_case("READ: gueltiger Rahmen wird angenommen");
    CHECK_EQ(v4p_check_read(f, V4P_READ_FRAME_LEN, V4P_CMD_PING, 7u, &out),
             V4P_CHECK_OK);
    CHECK_EQ(out.cmd, V4P_CMD_PING);
    CHECK_EQ(out.seq, 7u);
    CHECK_EQ(out.status, V4P_ST_OK);
    CHECK_EQ(out.len, 4u);
    CHECK_MEM(out.payload, payload, 4u);
    CHECK_EQ(out.payload[5], 0u);       /* Rest genullt, kein alter Puffer */

    v4_test_case("READ: 64 Byte (Audio-Fassung) wird abgewiesen");
    /* Genau das passiert, wenn eine alte Firmware antwortet: die Laenge passt
     * nicht, und JEDE Auswertung waere um 64 Byte verschoben. */
    CHECK_EQ(v4p_check_read(f, 64u, V4P_CMD_PING, 7u, &out), V4P_CHECK_SIZE);

    v4_test_case("READ: falsche Magic");
    f[0] = 0x00u;
    f[127] = v4p_crc8(f, 127u);
    CHECK_EQ(v4p_check_read(f, V4P_READ_FRAME_LEN, V4P_CMD_PING, 7u, &out),
             V4P_CHECK_MAGIC);
    f[0] = 0x5Au;

    v4_test_case("READ: CRC-Fehler");
    f[10] ^= 0xFFu;
    CHECK_EQ(v4p_check_read(f, V4P_READ_FRAME_LEN, V4P_CMD_PING, 7u, &out),
             V4P_CHECK_CRC);
    f[10] ^= 0xFFu;

    v4_test_case("READ: falsche SEQ (verschobener Strom)");
    f[2] = 8u;
    f[127] = v4p_crc8(f, 127u);
    CHECK_EQ(v4p_check_read(f, V4P_READ_FRAME_LEN, V4P_CMD_PING, 7u, &out),
             V4P_CHECK_ECHO);
    f[2] = 7u;

    v4_test_case("READ: len 122 ist unzulaessig (Grenze 121)");
    f[4] = (uint8_t)(V4P_READ_PAYLOAD_MAX + 1u);
    f[127] = v4p_crc8(f, 127u);
    CHECK_EQ(v4p_check_read(f, V4P_READ_FRAME_LEN, V4P_CMD_PING, 7u, &out),
             V4P_CHECK_LEN);
    f[4] = 4u;

    v4_test_case("READ: 121 Byte Nutzlast passen genau");
    f[4] = (uint8_t)V4P_READ_PAYLOAD_MAX;
    f[127] = v4p_crc8(f, 127u);
    CHECK_EQ(v4p_check_read(f, V4P_READ_FRAME_LEN, V4P_CMD_PING, 7u, &out),
             V4P_CHECK_OK);
    CHECK_EQ(out.len, V4P_READ_PAYLOAD_MAX);
}

/* ------------------------------------------------------------------ */
/* Nutzlast-Codecs                                                     */
/* ------------------------------------------------------------------ */

static void test_codec_info(void)
{
    v4p_info_t in;
    v4p_info_t out;
    uint8_t    p[V4P_INFO_PAYLOAD_LEN];

    v4_test_case("GET_INFO: 12 Byte, Faehigkeiten als u16 LE ab +10");
    in.proto_ver        = 3u;
    in.fw_ver           = 3u;
    in.write_frame_len  = 32u;
    in.read_frame_len   = 128u;
    in.bulk_payload_max = 0u;
    in.chunk            = 128u;
    in.max_devices      = 8u;
    in.path_max         = 128u;
    in.caps             = 0x00FFu;

    CHECK_EQ(v4p_enc_get_info(p, &in), V4P_INFO_PAYLOAD_LEN);
    CHECK_EQ(p[0], 3u);
    CHECK_EQ(p[1], 3u);
    CHECK_EQ(p[2], 32u);
    CHECK_EQ(p[3], 128u);
    CHECK_EQ(p[4], 0u);
    CHECK_EQ(p[5], 0u);
    CHECK_EQ(p[6], 128u);               /* chunk LE: Low zuerst */
    CHECK_EQ(p[7], 0u);
    CHECK_EQ(p[8], 8u);
    CHECK_EQ(p[9], 128u);
    CHECK_EQ(p[V4P_INFO_OFF_CAPS], 0xFFu);      /* Caps LE */
    CHECK_EQ(p[V4P_INFO_OFF_CAPS + 1u], 0x00u);

    CHECK_EQ(v4p_dec_get_info(p, V4P_INFO_PAYLOAD_LEN, &out), 0);
    CHECK_EQ(out.proto_ver, 3u);
    CHECK_EQ(out.fw_ver, 3u);
    CHECK_EQ(out.write_frame_len, 32u);
    CHECK_EQ(out.read_frame_len, 128u);
    CHECK_EQ(out.bulk_payload_max, 0u);
    CHECK_EQ(out.chunk, 128u);
    CHECK_EQ(out.max_devices, 8u);
    CHECK_EQ(out.path_max, 128u);
    CHECK_EQ(out.caps, 0x00FFu);

    v4_test_case("GET_INFO: zu kurze Nutzlast wird abgewiesen");
    CHECK_EQ(v4p_dec_get_info(p, 11u, &out), -1);

    v4_test_case("GET_INFO: Caps werden nicht mit dem alten Reservefeld "
                 "verwechselt");
    /* Bit 0 (PORT_MODE) liegt im LOW-Byte. Wuerde jemand big-endian schreiben,
     * stuende es in p[11]. Genau diese Verwechslung faengt der Test ab. */
    CHECK_EQ(p[10] & 0x01u, 0x01u);
    CHECK_EQ(p[11] & 0x01u, 0x00u);
}

static void test_codec_status(void)
{
    v4p_status_t in;
    v4p_status_t out;
    uint8_t      p[V4P_ST_LEN];

    v4_test_case("GET_STATUS: 17 Byte, alle Offsets little endian");
    in.state            = V4P_STATE_CONNECTED;
    in.conn_index       = 2u;
    in.dev_count        = 3u;
    in.scan_active      = 1u;
    in.sd_mounted       = 0u;           /* in dieser Firmware immer 0 */
    in.audio_flags      = 0u;
    in.scan_gen         = 0x1234u;
    in.sd_free_kb       = 0u;
    in.chunk            = 128u;
    in.proto_ver        = 3u;
    in.fw_ver           = 3u;
    in.sd_card_present  = 0u;

    CHECK_EQ(v4p_enc_get_status(p, &in), V4P_ST_LEN);
    CHECK_EQ(p[V4P_ST_OFF_STATE], V4P_STATE_CONNECTED);
    CHECK_EQ(p[V4P_ST_OFF_CONN_INDEX], 2u);
    CHECK_EQ(p[V4P_ST_OFF_DEV_COUNT], 3u);
    CHECK_EQ(p[V4P_ST_OFF_SCAN_ACTIVE], 1u);
    CHECK_EQ(p[V4P_ST_OFF_SCAN_GEN], 0x34u);    /* LE */
    CHECK_EQ(p[V4P_ST_OFF_SCAN_GEN + 1u], 0x12u);
    CHECK_EQ(p[V4P_ST_OFF_PROTO_VER], 3u);
    CHECK_EQ(p[V4P_ST_OFF_FW_VER], 3u);

    CHECK_EQ(v4p_dec_get_status(p, V4P_ST_LEN, &out), 0);
    CHECK_EQ(out.state, V4P_STATE_CONNECTED);
    CHECK_EQ(out.conn_index, 2u);
    CHECK_EQ(out.dev_count, 3u);
    CHECK_EQ(out.scan_active, 1u);
    CHECK_EQ(out.scan_gen, 0x1234u);
    CHECK_EQ(out.chunk, 128u);
    CHECK_EQ(out.proto_ver, 3u);
    CHECK_EQ(out.fw_ver, 3u);
    CHECK_EQ(out.sd_mounted, 0u);
    CHECK_EQ(out.audio_flags, 0u);
    CHECK_EQ(out.sd_card_present, 0u);

    v4_test_case("GET_STATUS: zu kurze Nutzlast wird abgewiesen");
    CHECK_EQ(v4p_dec_get_status(p, 16u, &out), -1);
}

static void test_codec_input(void)
{
    v4p_input_t in;
    v4p_input_t out;
    uint8_t     p[V4P_IN_PAYLOAD_LEN];

    v4_test_case("GET_INPUT: 12 Byte -- Zug, Schritte LE und gemessene Pegel");
    memset(&in, 0, sizeof(in));
    in.state[0] = V4P_IN_ST_UP | V4P_IN_ST_BTN1;
    in.state[1] = V4P_IN_ST_BTN3;
    in.steps_x  = 0x11223344u;
    in.steps_y  = 0xAABBCCDDu;
    in.level[0] = (uint8_t)(V4P_IN_ST_MASK & ~V4P_IN_ST_UP);    /* UP ist Low */
    in.level[1] = V4P_IN_ST_MASK;
    in.level_valid = 1u;

    CHECK_EQ(v4p_enc_get_input(p, &in), V4P_IN_PAYLOAD_LEN);
    CHECK_EQ(V4P_IN_PAYLOAD_LEN, 12u);
    CHECK_EQ(p[V4P_IN_OFF_STATE0], 0x11u);
    CHECK_EQ(p[V4P_IN_OFF_STATE1], 0x40u);
    CHECK_EQ(p[V4P_IN_OFF_STEPS_X], 0x44u);
    CHECK_EQ(p[V4P_IN_OFF_STEPS_X + 3u], 0x11u);
    CHECK_EQ(p[V4P_IN_OFF_STEPS_Y], 0xDDu);
    CHECK_EQ(p[V4P_IN_OFF_STEPS_Y + 3u], 0xAAu);
    CHECK_EQ(p[V4P_IN_OFF_LEVEL0], 0x7Eu);      /* 0x7F ohne UP */
    CHECK_EQ(p[V4P_IN_OFF_LEVEL1], 0x7Fu);

    CHECK_EQ(v4p_dec_get_input(p, V4P_IN_PAYLOAD_LEN, &out), 0);
    CHECK_EQ(out.state[0], V4P_IN_ST_UP | V4P_IN_ST_BTN1);
    CHECK_EQ(out.state[1], V4P_IN_ST_BTN3);
    CHECK_EQ(out.steps_x, 0x11223344u);
    CHECK_EQ(out.steps_y, 0xAABBCCDDu);
    CHECK_EQ(out.level[0], 0x7Eu);
    CHECK_EQ(out.level[1], 0x7Fu);
    CHECK_EQ(out.level_valid, 1u);

    v4_test_case("GET_INPUT: 10 Byte (aeltere Fassung) -- Pegel UNBEKANNT");
    /* Die Laenge ist die Versionsauskunft. Bei 10 Byte duerfen keine Pegel
     * angezeigt werden: level_valid ist 0, und die Levelbytes bleiben 0 --
     * NICHT als "Low" gemeint, sondern als "weiss ich nicht". */
    CHECK_EQ(v4p_dec_get_input(p, V4P_IN_PAYLOAD_LEN_NO_LEVEL, &out), 0);
    CHECK_EQ(V4P_IN_PAYLOAD_LEN_NO_LEVEL, 10u);
    CHECK_EQ(out.level_valid, 0u);
    CHECK_EQ(out.level[0], 0u);
    CHECK_EQ(out.level[1], 0u);
    /* Die Masken und Schritte kommen trotzdem an -- nur die Pegel fehlen. */
    CHECK_EQ(out.state[0], V4P_IN_ST_UP | V4P_IN_ST_BTN1);
    CHECK_EQ(out.steps_x, 0x11223344u);

    v4_test_case("GET_INPUT: 11 Byte ist keine gueltige Fassung");
    CHECK_EQ(v4p_dec_get_input(p, 11u, &out), -1);

    v4_test_case("GET_INPUT: zu kurze Nutzlast wird abgewiesen");
    CHECK_EQ(v4p_dec_get_input(p, 9u, &out), -1);
    CHECK_EQ(v4p_dec_get_input(p, 0u, &out), -1);

    v4_test_case("GET_INPUT: laengere Nutzlast ist vorwaertskompatibel");
    CHECK_EQ(v4p_dec_get_input(p, 13u, &out), 0);
    CHECK_EQ(out.level_valid, 1u);

    v4_test_case("GET_INPUT: Bit 7 gehoert keiner Leitung und wird verworfen");
    p[V4P_IN_OFF_STATE0] = 0xFFu;
    p[V4P_IN_OFF_LEVEL0] = 0xFFu;
    CHECK_EQ(v4p_dec_get_input(p, V4P_IN_PAYLOAD_LEN, &out), 0);
    CHECK_EQ(out.state[0], V4P_IN_ST_MASK);
    CHECK_EQ(out.level[0], V4P_IN_ST_MASK);
}

/*
 * Die elektrische Auswertung. Sie ist der Grund, warum die Pegel ueberhaupt
 * ueber die Leitung gehen: ohne sie sieht "kein Pull-up" wie "nichts
 * gedrueckt" aus.
 */
static void test_level_diagnosis(void)
{
    v4_test_case("Pegel: Ruhe mit Pull-up ist 0x7F, gezogen liest Low");
    /* Zug 0x00, Pegel 0x7F -> alles in Ordnung, kein Widerspruch. */
    CHECK_EQ(v4p_level_missing_pullup(0x00u, V4P_IN_ST_MASK), 0x00u);
    CHECK_EQ(v4p_level_impossible(0x00u, V4P_IN_ST_MASK), 0x00u);
    /* UP gezogen (0x01), Pegel 0x7E -> UP ist Low, sonst alles High. */
    CHECK_EQ(v4p_level_missing_pullup(V4P_IN_ST_UP, 0x7Eu), 0x00u);
    CHECK_EQ(v4p_level_impossible(V4P_IN_ST_UP, 0x7Eu), 0x00u);

    v4_test_case("Pegel: losgelassen, liest 0 -> fehlender Pull-up");
    /* Keine Leitung gezogen, aber alles liest Low: das ist der Fall "Platine
     * haengt nicht an der Vampire". */
    CHECK_EQ(v4p_level_missing_pullup(0x00u, 0x00u), V4P_IN_ST_MASK);
    /* Nur UP liest faelschlich Low. */
    CHECK_EQ(v4p_level_missing_pullup(0x00u, (uint8_t)(V4P_IN_ST_MASK
                                                       & ~V4P_IN_ST_UP)),
             V4P_IN_ST_UP);
    /* Gezogene Leitungen sind davon NICHT betroffen: dort ist Low richtig. */
    CHECK_EQ(v4p_level_missing_pullup(V4P_IN_ST_UP,
                                      (uint8_t)(V4P_IN_ST_MASK
                                                & ~V4P_IN_ST_UP)),
             0x00u);

    v4_test_case("Pegel: gezogen, liest High -> elektrisch unmoeglich");
    CHECK_EQ(v4p_level_impossible(V4P_IN_ST_UP, V4P_IN_ST_MASK), V4P_IN_ST_UP);
    CHECK_EQ(v4p_level_impossible(V4P_IN_ST_BTN1 | V4P_IN_ST_UP,
                                  V4P_IN_ST_MASK),
             V4P_IN_ST_BTN1 | V4P_IN_ST_UP);
    CHECK_EQ(v4p_level_impossible(V4P_IN_ST_UP, 0x7Eu), 0x00u);

    v4_test_case("Pegel: Bit 7 traegt keine Leitung und aendert nichts");
    /* Bit 7 ist keiner Leitung zugeordnet. Es darf im Ergebnis nie auftauchen;
     * fuer die sieben belegten Leitungen zaehlt nur, dass ein gesetztes Bit 7
     * als "Low" gelesen wird (die Leitung ist ja nicht High). */
    CHECK_EQ(v4p_level_impossible((uint8_t)0x80u, (uint8_t)0x80u), 0x00u);
    CHECK_EQ(v4p_level_missing_pullup((uint8_t)0x80u, (uint8_t)0x80u) & 0x80u,
             0x00u);
    /* Bei Zug 0 und Pegel 0x80 lesen die sieben Leitungen Low -> fehlender
     * Pull-up, und zwar auf allen sieben. */
    CHECK_EQ(v4p_level_missing_pullup(0x00u, 0x80u), V4P_IN_ST_MASK);
}

static void test_codec_hid(void)
{
    v4p_hid_t in;
    v4p_hid_t out;
    uint8_t   p[V4P_HID_PAYLOAD_MAX];

    v4_test_case("GET_HID: Name, VID/PID, Reports und Batterie");
    memset(&in, 0, sizeof(in));
    in.kind       = V4P_HID_KIND_PAD;
    in.transport  = V4P_HID_TR_BREDR;
    in.vid        = 0x2DC8u;
    in.pid        = 0x6100u;
    in.battery    = 78u;
    in.report_id  = 3u;
    in.report_len = 10u;
    in.reports    = 1234u;
    (void)strcpy(in.name, "8BitDo Pro 2");
    in.name_len   = 12u;

    CHECK_EQ(v4p_enc_get_hid(p, &in),
             (size_t)V4P_HID_OFF_NAME + 12u);
    CHECK_EQ(p[V4P_HID_OFF_KIND], V4P_HID_KIND_PAD);
    CHECK_EQ(p[V4P_HID_OFF_TRANSPORT], V4P_HID_TR_BREDR);
    CHECK_EQ(p[V4P_HID_OFF_VID], 0xC8u);        /* 0x2DC8 LE */
    CHECK_EQ(p[V4P_HID_OFF_VID + 1u], 0x2Du);
    CHECK_EQ(p[V4P_HID_OFF_PID], 0x00u);        /* 0x6100 LE */
    CHECK_EQ(p[V4P_HID_OFF_PID + 1u], 0x61u);
    CHECK_EQ(p[V4P_HID_OFF_BATTERY], 78u);
    CHECK_EQ(p[V4P_HID_OFF_REPORT_ID], 3u);
    CHECK_EQ(p[V4P_HID_OFF_REPORT_LEN], 10u);
    CHECK_EQ(p[V4P_HID_OFF_REPORTS], 0xD2u);    /* 1234 = 0x04D2 */
    CHECK_EQ(p[V4P_HID_OFF_REPORTS + 1u], 0x04u);
    CHECK_EQ(p[V4P_HID_OFF_NAME_LEN], 12u);

    CHECK_EQ(v4p_dec_get_hid(p, (uint8_t)(V4P_HID_OFF_NAME + 12u), &out), 0);
    CHECK_EQ(out.kind, V4P_HID_KIND_PAD);
    CHECK_EQ(out.vid, 0x2DC8u);
    CHECK_EQ(out.pid, 0x6100u);
    CHECK_EQ(out.battery, 78u);
    CHECK_EQ(out.report_id, 3u);
    CHECK_EQ(out.report_len, 10u);
    CHECK_EQ(out.reports, 1234u);
    CHECK_EQ(out.name_len, 12u);
    CHECK_STR(out.name, "8BitDo Pro 2");

    v4_test_case("GET_HID: Name laenger als die Nutzlast wird abgewiesen");
    /* Der Kopf passt, aber der angesagte Name steht nicht in den 20 Byte. */
    p[V4P_HID_OFF_NAME_LEN] = 13u;
    CHECK_EQ(v4p_dec_get_hid(p, 20u, &out), -1);

    v4_test_case("GET_HID: 13 Byte (Kopf unvollstaendig) wird abgewiesen");
    p[V4P_HID_OFF_NAME_LEN] = 12u;
    CHECK_EQ(v4p_dec_get_hid(p, 13u, &out), -1);

    v4_test_case("GET_HID: name_len 0 ist gueltig und ergibt \"\"");
    p[V4P_HID_OFF_NAME_LEN] = 0u;
    CHECK_EQ(v4p_dec_get_hid(p, V4P_HID_OFF_NAME, &out), 0);
    CHECK_EQ(out.name_len, 0u);
    CHECK_STR(out.name, "");

    v4_test_case("GET_HID: unmoegliche name_len wird abgewiesen");
    p[V4P_HID_OFF_NAME_LEN] = (uint8_t)(V4P_HID_NAME_MAX + 1u);
    CHECK_EQ(v4p_dec_get_hid(p, V4P_HID_PAYLOAD_MAX, &out), -1);
}

static void test_codec_dev(void)
{
    v4p_dev_t in;
    v4p_dev_t out;
    uint8_t   p[V4P_DEV_OFF_NAME + V4P_DEVNAME_MAX];
    uint8_t   i;

    v4_test_case("DEV_GET: idx, bda[6], name_len, name");
    memset(&in, 0, sizeof(in));
    in.idx = 4u;
    for (i = 0u; i < 6u; i++) {
        in.bda[i] = (uint8_t)(0xA0u + i);
    }
    (void)strcpy(in.name, "Logitech MX Anywhere");
    in.name_len = 20u;

    CHECK_EQ(v4p_enc_dev(p, &in), (size_t)V4P_DEV_OFF_NAME + 20u);
    CHECK_EQ(p[V4P_DEV_OFF_INDEX], 4u);
    CHECK_EQ(p[V4P_DEV_OFF_BDA], 0xA0u);
    CHECK_EQ(p[V4P_DEV_OFF_BDA + 5u], 0xA5u);
    CHECK_EQ(p[V4P_DEV_OFF_NAME_LEN], 20u);

    CHECK_EQ(v4p_dec_dev(p, (uint8_t)(V4P_DEV_OFF_NAME + 20u), &out), 0);
    CHECK_EQ(out.idx, 4u);
    CHECK_EQ(out.bda[0], 0xA0u);
    CHECK_EQ(out.bda[5], 0xA5u);
    CHECK_EQ(out.name_len, 20u);
    CHECK_STR(out.name, "Logitech MX Anywhere");

    v4_test_case("DEV_GET: abgeschnittene Nutzlast wird abgewiesen");
    CHECK_EQ(v4p_dec_dev(p, (uint8_t)(V4P_DEV_OFF_NAME + 19u), &out), -1);

    v4_test_case("DEV_GET: 49 Zeichen (Leitungsgrenze) passen");
    for (i = 0u; i < (uint8_t)V4P_DEVNAME_MAX; i++) {
        in.name[i] = (char)('a' + (i % 26u));
    }
    in.name[V4P_DEVNAME_MAX] = '\0';
    in.name_len = (uint8_t)V4P_DEVNAME_MAX;
    CHECK_EQ(v4p_enc_dev(p, &in), (size_t)V4P_DEV_OFF_NAME + V4P_DEVNAME_MAX);
    CHECK_EQ(v4p_dec_dev(p, (uint8_t)(V4P_DEV_OFF_NAME + V4P_DEVNAME_MAX),
                         &out), 0);
    CHECK_EQ(out.name_len, V4P_DEVNAME_MAX);

    v4_test_case("DEV_GET: 50 Zeichen werden auf 49 gekuerzt");
    /* name_len sagt 50, der Puffer haelt 49 Zeichen. Der Encoder MUSS kuerzen:
     * sonst schriebe er ein Byte hinter das Ende. */
    for (i = 0u; i < (uint8_t)V4P_DEVNAME_MAX; i++) {
        in.name[i] = 'x';
    }
    in.name[V4P_DEVNAME_MAX] = '\0';
    in.name_len = (uint8_t)(V4P_DEVNAME_MAX + 1u);
    CHECK_EQ(v4p_enc_dev(p, &in), (size_t)V4P_DEV_OFF_NAME + V4P_DEVNAME_MAX);
    CHECK_EQ(p[V4P_DEV_OFF_NAME_LEN], V4P_DEVNAME_MAX);
}

static void test_codec_port_mode(void)
{
    uint8_t mode[V4P_PORT_COUNT];
    uint8_t p[V4P_GPM_PAYLOAD_LEN];
    uint8_t port = 0u;
    uint8_t back = 0u;

    v4_test_case("GET_PORT_MODE: [mode0][mode1]");
    mode[0] = V4P_PORT_JOY;
    mode[1] = V4P_PORT_MOUSE;
    CHECK_EQ(v4p_enc_get_port_mode(p, mode), V4P_GPM_PAYLOAD_LEN);
    CHECK_EQ(p[0], V4P_PORT_JOY);
    CHECK_EQ(p[1], V4P_PORT_MOUSE);

    mode[0] = 0u;
    mode[1] = 0u;
    CHECK_EQ(v4p_dec_get_port_mode(p, V4P_GPM_PAYLOAD_LEN, mode), 0);
    CHECK_EQ(mode[0], V4P_PORT_JOY);
    CHECK_EQ(mode[1], V4P_PORT_MOUSE);

    v4_test_case("GET_PORT_MODE: unmoegliche Betriebsart wird nicht geraten");
    p[1] = 3u;
    CHECK_EQ(v4p_dec_get_port_mode(p, V4P_GPM_PAYLOAD_LEN, mode), -1);
    p[1] = V4P_PORT_MOUSE;

    v4_test_case("GET_PORT_MODE: zu kurze Nutzlast wird abgewiesen");
    CHECK_EQ(v4p_dec_get_port_mode(p, 1u, mode), -1);

    v4_test_case("SET_PORT_MODE: Anfrage und Antwort sind [port][mode]");
    CHECK_EQ(v4p_enc_set_port_mode(p, 1u, V4P_PORT_MOUSE),
             V4P_SPM_PAYLOAD_LEN);
    CHECK_EQ(p[0], 1u);
    CHECK_EQ(p[1], V4P_PORT_MOUSE);
    CHECK_EQ(v4p_dec_set_port_mode(p, V4P_SPM_PAYLOAD_LEN, &port, &back), 0);
    CHECK_EQ(port, 1u);
    CHECK_EQ(back, V4P_PORT_MOUSE);

    v4_test_case("SET_PORT_MODE: port 2 und mode 3 werden abgewiesen");
    p[0] = 2u;
    CHECK_EQ(v4p_dec_set_port_mode(p, V4P_SPM_PAYLOAD_LEN, &port, &back), -1);
    p[0] = 0u;
    p[1] = 3u;
    CHECK_EQ(v4p_dec_set_port_mode(p, V4P_SPM_PAYLOAD_LEN, &port, &back), -1);

    /*
     * fw_ver 4: mode = 0xFF ("nach Geraeteart") ist in SET_PORT_MODE ERLAUBT.
     * GET_PORT_MODE meldet dagegen weiter nur die WIRKSAME Betriebsart 0..2 --
     * ein 0xFF dort waere ein Hinweis auf einen verschobenen Strom und wird
     * abgewiesen (oben schon geprueft).
     */
    v4_test_case("SET_PORT_MODE: mode 0xFF (auto) ist erlaubt, "
                 "GET_PORT_MODE lehnt es ab");
    CHECK_EQ(v4p_enc_set_port_mode(p, 0u, (uint8_t)V4P_MODE_AUTO),
             V4P_SPM_PAYLOAD_LEN);
    CHECK_EQ(p[1], V4P_MODE_AUTO);
    port = 9u;
    back = 0u;
    CHECK_EQ(v4p_dec_set_port_mode(p, V4P_SPM_PAYLOAD_LEN, &port, &back), 0);
    CHECK_EQ(port, 0u);
    CHECK_EQ(back, V4P_MODE_AUTO);
    p[1] = V4P_MODE_AUTO;
    CHECK_EQ(v4p_dec_get_port_mode(p, V4P_GPM_PAYLOAD_LEN, mode), -1);
}

/* ------------------------------------------------------------------ */
/* Eingangsverteilung (fw_ver 4)                                       */
/* ------------------------------------------------------------------ */

static void test_codec_port_config(void)
{
    v4p_portcfg_t c;
    v4p_portcfg_t zurueck;
    uint8_t       p[V4P_GPC_PAYLOAD_LEN];
    uint8_t       dir = 0u;
    uint8_t       port = 0u;
    uint8_t       btn[V4P_BTN_COUNT];
    uint8_t       bda[V4P_BDA_LEN];
    unsigned      i;
    unsigned      k;

    v4_test_case("GET_PORT_CONFIG: 12 Byte, Vorgabe UND wirksame Betriebsart");
    memset(&c, 0, sizeof(c));
    c.mode_soll[0] = (uint8_t)V4P_MODE_AUTO;    /* "nach Geraeteart" */
    c.mode_soll[1] = V4P_PORT_JOY;
    c.mode_ist[0]  = V4P_PORT_MOUSE;            /* laeuft aber als Maus */
    c.mode_ist[1]  = V4P_PORT_JOY;
    c.dir[0]       = V4P_DIR_STICK_L;
    c.dir[1]       = V4P_DIR_DPAD;
    c.btn[0][0]    = V4P_TASTE_L1;
    c.btn[0][1]    = V4P_TASTE_R1;
    c.btn[0][2]    = V4P_TASTE_KEINE;
    c.btn[1][0]    = V4P_TASTE_A;
    c.btn[1][1]    = V4P_TASTE_B;
    c.btn[1][2]    = V4P_TASTE_START;

    CHECK_EQ(v4p_enc_get_port_config(p, &c), V4P_GPC_PAYLOAD_LEN);
    CHECK_EQ(p[V4P_GPC_OFF_MODE_SOLL0], V4P_MODE_AUTO);
    CHECK_EQ(p[V4P_GPC_OFF_MODE_SOLL1], V4P_PORT_JOY);
    CHECK_EQ(p[V4P_GPC_OFF_MODE_IST0], V4P_PORT_MOUSE);
    CHECK_EQ(p[V4P_GPC_OFF_MODE_IST1], V4P_PORT_JOY);
    CHECK_EQ(p[V4P_GPC_OFF_DIR0], V4P_DIR_STICK_L);
    CHECK_EQ(p[V4P_GPC_OFF_DIR1], V4P_DIR_DPAD);
    CHECK_EQ(p[V4P_GPC_OFF_BTN0 + 0u], V4P_TASTE_L1);
    CHECK_EQ(p[V4P_GPC_OFF_BTN0 + 1u], V4P_TASTE_R1);
    CHECK_EQ(p[V4P_GPC_OFF_BTN0 + 2u], V4P_TASTE_KEINE);
    CHECK_EQ(p[V4P_GPC_OFF_BTN1 + 0u], V4P_TASTE_A);
    CHECK_EQ(p[V4P_GPC_OFF_BTN1 + 1u], V4P_TASTE_B);
    CHECK_EQ(p[V4P_GPC_OFF_BTN1 + 2u], V4P_TASTE_START);

    memset(&zurueck, 0, sizeof(zurueck));
    CHECK_EQ(v4p_dec_get_port_config(p, V4P_GPC_PAYLOAD_LEN, &zurueck), 0);
    for (i = 0u; i < V4P_PORT_COUNT; i++) {
        CHECK_EQ(zurueck.mode_soll[i], c.mode_soll[i]);
        CHECK_EQ(zurueck.mode_ist[i], c.mode_ist[i]);
        CHECK_EQ(zurueck.dir[i], c.dir[i]);
        for (k = 0u; k < V4P_BTN_COUNT; k++) {
            CHECK_EQ(zurueck.btn[i][k], c.btn[i][k]);
        }
    }

    v4_test_case("GET_PORT_CONFIG: zu kurz / unmoegliche Werte werden "
                 "abgewiesen (nicht geraten)");
    CHECK_EQ(v4p_dec_get_port_config(p, (uint8_t)(V4P_GPC_PAYLOAD_LEN - 1u),
                                     &zurueck), -1);
    CHECK_EQ(v4p_dec_get_port_config(NULL, V4P_GPC_PAYLOAD_LEN, &zurueck), -1);
    CHECK_EQ(v4p_dec_get_port_config(p, V4P_GPC_PAYLOAD_LEN, NULL), -1);
    /* Wirksame Betriebsart 3 gibt es nicht. */
    p[V4P_GPC_OFF_MODE_IST1] = 3u;
    CHECK_EQ(v4p_dec_get_port_config(p, V4P_GPC_PAYLOAD_LEN, &zurueck), -1);
    p[V4P_GPC_OFF_MODE_IST1] = V4P_PORT_JOY;
    /* Vorgabe 7 gibt es nicht (nur 0..2 und 0xFF). */
    p[V4P_GPC_OFF_MODE_SOLL1] = 7u;
    CHECK_EQ(v4p_dec_get_port_config(p, V4P_GPC_PAYLOAD_LEN, &zurueck), -1);
    p[V4P_GPC_OFF_MODE_SOLL1] = V4P_PORT_JOY;
    /* Unbekanntes Richtungsbit. */
    p[V4P_GPC_OFF_DIR0] = 0x88u;
    CHECK_EQ(v4p_dec_get_port_config(p, V4P_GPC_PAYLOAD_LEN, &zurueck), -1);
    p[V4P_GPC_OFF_DIR0] = V4P_DIR_STICK_L;
    /* Unbekannter Knopfcode. */
    p[V4P_GPC_OFF_BTN0] = (uint8_t)V4P_TASTE_ANZAHL;
    CHECK_EQ(v4p_dec_get_port_config(p, V4P_GPC_PAYLOAD_LEN, &zurueck), -1);
    p[V4P_GPC_OFF_BTN0] = V4P_TASTE_L1;
    /* Doppelte Knopfquelle: die kann der ESP32 gar nicht liefern. */
    p[V4P_GPC_OFF_BTN0 + 1u] = V4P_TASTE_L1;
    CHECK_EQ(v4p_dec_get_port_config(p, V4P_GPC_PAYLOAD_LEN, &zurueck), -1);

    v4_test_case("SET_PORT_ROUTING: 5 Byte [port][dir][b1][b2][b3]");
    btn[0] = V4P_TASTE_A;
    btn[1] = V4P_TASTE_B;
    btn[2] = V4P_TASTE_START;
    CHECK_EQ(v4p_enc_set_port_routing(p, 1u, V4P_DIR_DPAD, btn),
             V4P_SPR_PAYLOAD_LEN);
    CHECK_EQ(p[V4P_SPR_OFF_PORT], 1u);
    CHECK_EQ(p[V4P_SPR_OFF_DIR], V4P_DIR_DPAD);
    CHECK_EQ(p[V4P_SPR_OFF_BTN1], V4P_TASTE_A);
    CHECK_EQ(p[V4P_SPR_OFF_BTN2], V4P_TASTE_B);
    CHECK_EQ(p[V4P_SPR_OFF_BTN3], V4P_TASTE_START);
    CHECK_EQ(v4p_dec_set_port_routing(p, V4P_SPR_PAYLOAD_LEN, &port, &dir,
                                      NULL), 0);
    CHECK_EQ(port, 1u);
    CHECK_EQ(dir, V4P_DIR_DPAD);

    v4_test_case("SET_PORT_ROUTING: port 2, unbekannter Code und "
                 "Doppelbelegung werden abgewiesen");
    p[V4P_SPR_OFF_PORT] = 2u;
    CHECK_EQ(v4p_dec_set_port_routing(p, V4P_SPR_PAYLOAD_LEN, &port, &dir,
                                      NULL), -1);
    p[V4P_SPR_OFF_PORT] = 1u;
    p[V4P_SPR_OFF_DIR]  = 0x80u;
    CHECK_EQ(v4p_dec_set_port_routing(p, V4P_SPR_PAYLOAD_LEN, &port, &dir,
                                      NULL), -1);
    p[V4P_SPR_OFF_DIR] = V4P_DIR_DPAD;
    p[V4P_SPR_OFF_BTN2] = p[V4P_SPR_OFF_BTN1];      /* A doppelt */
    CHECK_EQ(v4p_dec_set_port_routing(p, V4P_SPR_PAYLOAD_LEN, &port, &dir,
                                      btn), -1);
    p[V4P_SPR_OFF_BTN2] = V4P_TASTE_KEINE;
    p[V4P_SPR_OFF_BTN3] = V4P_TASTE_KEINE;          /* "keine" mehrfach OK */
    CHECK_EQ(v4p_dec_set_port_routing(p, V4P_SPR_PAYLOAD_LEN, &port, &dir,
                                      btn), 0);
    CHECK_EQ(v4p_dec_set_port_routing(p, (uint8_t)(V4P_SPR_PAYLOAD_LEN - 1u),
                                      &port, &dir, btn), -1);

    v4_test_case("SET_DEVICE_PORT: 7 Byte [bda 6][port]");
    for (i = 0u; i < V4P_BDA_LEN; i++) {
        bda[i] = (uint8_t)(0x10u + i);
    }
    CHECK_EQ(v4p_enc_set_device_port(p, bda, 0u), V4P_SDP_PAYLOAD_LEN);
    CHECK_EQ(p[V4P_SDP_OFF_BDA], 0x10u);
    CHECK_EQ(p[V4P_SDP_OFF_PORT], 0u);
    for (i = 0u; i < V4P_BDA_LEN; i++) {
        bda[i] = 0u;
    }
    port = 9u;
    CHECK_EQ(v4p_dec_set_device_port(p, V4P_SDP_PAYLOAD_LEN, bda, &port), 0);
    CHECK_EQ(bda[5], 0x15u);
    CHECK_EQ(port, 0u);
    p[V4P_SDP_OFF_PORT] = 2u;
    CHECK_EQ(v4p_dec_set_device_port(p, V4P_SDP_PAYLOAD_LEN, bda, &port), -1);
    p[V4P_SDP_OFF_PORT] = 0u;
    CHECK_EQ(v4p_dec_set_device_port(p, (uint8_t)(V4P_SDP_PAYLOAD_LEN - 1u),
                                     bda, &port), -1);

    v4_test_case("Prueffunktionen: dir, mode_soll, taste, Knopfsatz");
    CHECK_EQ(v4p_dir_valid(0u), 1);
    CHECK_EQ(v4p_dir_valid((uint8_t)V4P_DIR_ALLE), 1);
    CHECK_EQ(v4p_dir_valid(0x08u), 0);
    CHECK_EQ(v4p_dir_valid(0xFFu), 0);
    CHECK_EQ(v4p_mode_soll_valid(0u), 1);
    CHECK_EQ(v4p_mode_soll_valid(V4P_PORT_MOUSE), 1);
    CHECK_EQ(v4p_mode_soll_valid((uint8_t)V4P_MODE_AUTO), 1);
    CHECK_EQ(v4p_mode_soll_valid(3u), 0);
    CHECK_EQ(v4p_taste_valid(0u), 1);
    CHECK_EQ(v4p_taste_valid((uint8_t)(V4P_TASTE_ANZAHL - 1u)), 1);
    CHECK_EQ(v4p_taste_valid((uint8_t)V4P_TASTE_ANZAHL), 0);
    btn[0] = V4P_TASTE_A;
    btn[1] = V4P_TASTE_B;
    btn[2] = V4P_TASTE_KEINE;
    CHECK_EQ(v4p_btn_set_valid(btn), 1);
    btn[1] = V4P_TASTE_A;
    CHECK_EQ(v4p_btn_set_valid(btn), 0);
    btn[0] = V4P_TASTE_KEINE;
    btn[1] = V4P_TASTE_KEINE;
    btn[2] = V4P_TASTE_KEINE;
    CHECK_EQ(v4p_btn_set_valid(btn), 1);        /* "keine" mehrfach erlaubt */
    CHECK_EQ(v4p_btn_set_valid(NULL), 0);

    v4_test_case("Der Knopfsatz steht in der Reihenfolge Pin 6, Pin 9, Pin 5");
    CHECK_STR(v4p_bit_pin(4u), "Pin 6");
    CHECK_STR(v4p_bit_pin(5u), "Pin 9");
    CHECK_STR(v4p_bit_pin(6u), "Pin 5");
}

static void test_names_routing(void)
{
    char txt[64];

    v4_test_case("Klartexte der Eingangsverteilung");
    CHECK_STR(v4p_port_mode_soll_name(V4P_PORT_OFF), "aus");
    CHECK_STR(v4p_port_mode_soll_name(V4P_PORT_JOY), "Joystick");
    CHECK_STR(v4p_port_mode_soll_name(V4P_PORT_MOUSE), "Maus");
    CHECK_STR(v4p_port_mode_soll_name((uint8_t)V4P_MODE_AUTO), "auto");
    CHECK_STR(v4p_dir_name(0u), "Stick links");
    CHECK_STR(v4p_dir_name(1u), "Stick rechts");
    CHECK_STR(v4p_dir_name(2u), "D-Pad");
    CHECK_STR(v4p_taste_name(V4P_TASTE_KEINE), "keine");
    CHECK_STR(v4p_taste_name(V4P_TASTE_L1), "L1");
    CHECK_STR(v4p_taste_name(V4P_TASTE_HOME), "Home/Capture");
    CHECK_STR(v4p_taste_name(V4P_TASTE_GRUPPE1), "Gruppe 1 (A,L1,L2,L3)");
    CHECK_STR(v4p_taste_name_short(V4P_TASTE_GRUPPE1), "Gruppe 1");
    CHECK_STR(v4p_taste_name_short(V4P_TASTE_A), "A");

    v4_test_case("Richtungsmaske als Text");
    v4p_dir_text((uint8_t)(V4P_DIR_STICK_L | V4P_DIR_DPAD), txt, sizeof(txt));
    CHECK_STR(txt, "Stick links+D-Pad");
    v4p_dir_text(0u, txt, sizeof(txt));
    CHECK_STR(txt, "keine");
    v4p_dir_text((uint8_t)V4P_DIR_ALLE, txt, sizeof(txt));
    CHECK_STR(txt, "Stick links+Stick rechts+D-Pad");
    /* Kleiner Puffer: immer nullterminiert, nie ueberlaufen. */
    v4p_dir_text((uint8_t)V4P_DIR_ALLE, txt, 8u);
    CHECK_EQ((int)strlen(txt) < 8, 1);
    /* Unbekannte Bits erzeugen keinen erfundenen Namen. */
    v4p_dir_text(0x80u, txt, sizeof(txt));
    CHECK_STR(txt, "keine");
}

/* ------------------------------------------------------------------ */
/* Ableitungen fuer die Anzeige                                        */
/* ------------------------------------------------------------------ */

static void test_display_helpers(void)
{
    char buf[64];

    v4_test_case("v4p_bits_text: nichts gezogen ergibt \"-\"");
    v4p_bits_text(0u, buf, sizeof(buf));
    CHECK_STR(buf, "-");

    v4_test_case("v4p_bits_text: UP und BTN1");
    v4p_bits_text(V4P_IN_ST_UP | V4P_IN_ST_BTN1, buf, sizeof(buf));
    CHECK_STR(buf, "UP BTN1");

    v4_test_case("v4p_bits_text: alle sieben Leitungen");
    v4p_bits_text(V4P_IN_ST_MASK, buf, sizeof(buf));
    CHECK_STR(buf, "UP DOWN LEFT RIGHT BTN1 BTN2 BTN3");

    v4_test_case("v4p_bits_text: Bit 7 wird ignoriert");
    v4p_bits_text(0x80u, buf, sizeof(buf));
    CHECK_STR(buf, "-");

    v4_test_case("v4p_bits_text: zu kleiner Puffer endet mit \"...\"");
    {
        char klein[12];

        v4p_bits_text(V4P_IN_ST_MASK, klein, sizeof(klein));
        /* "UP DOWN" passt noch, danach ist Schluss: 7 sichtbare Zeichen
         * plus drei Punkte plus Null. Ein halber Name ("BTN") waere
         * schlimmer als das sichtbare "...". */
        CHECK_EQ(strlen(klein), 10u);
        CHECK_STR(klein + 7u, "...");
    }

    v4_test_case("v4p_level_expected gibt es nicht mehr (Pegel sind gemessen)");
    /* Frueher stand hier eine Ableitung "gezogen = Low". Seit die Firmware die
     * Pegel mitschickt, waere sie eine Zahl, die wie eine Messung aussieht.
     * Der Test haelt fest, dass die Auswertung nur noch WIDERSPRUECHE kennt. */
    CHECK_EQ(v4p_level_missing_pullup(0x00u, V4P_IN_ST_MASK), 0x00u);
    CHECK_EQ(v4p_level_impossible(0x00u, V4P_IN_ST_MASK), 0x00u);

    v4_test_case("v4p_port_mode_next: aus -> Joystick -> Maus -> aus");
    CHECK_EQ(v4p_port_mode_next(V4P_PORT_OFF), V4P_PORT_JOY);
    CHECK_EQ(v4p_port_mode_next(V4P_PORT_JOY), V4P_PORT_MOUSE);
    CHECK_EQ(v4p_port_mode_next(V4P_PORT_MOUSE), V4P_PORT_OFF);
    CHECK_EQ(v4p_port_mode_next(99u), V4P_PORT_OFF);

    v4_test_case("Klartexte sind nicht leer");
    CHECK_STR(v4p_port_mode_name(V4P_PORT_JOY), "Joystick");
    CHECK_STR(v4p_port_mode_name(V4P_PORT_MOUSE), "Maus");
    CHECK_STR(v4p_port_mode_name(V4P_PORT_OFF), "aus");
    CHECK_STR(v4p_bt_state_name(V4P_STATE_CONNECTED), "CONNECTED");
    CHECK_STR(v4p_hid_kind_name(V4P_HID_KIND_PAD), "Pad");
    CHECK_STR(v4p_hid_kind_name(V4P_HID_KIND_MOUSE), "Maus");
    CHECK_STR(v4p_transport_name(V4P_HID_TR_BLE), "BLE");
    CHECK_STR(v4p_transport_name(V4P_HID_TR_BREDR), "BR/EDR");
    CHECK_STR(v4p_bit_name(0u), "UP");
    CHECK_STR(v4p_bit_name(6u), "BTN3");
    CHECK_STR(v4p_bit_pin(4u), "Pin 6");
    CHECK_STR(v4p_bit_pin(5u), "Pin 9");
    CHECK_STR(v4p_bit_pin(6u), "Pin 5");
}

/* ------------------------------------------------------------------ */

int test_proto(void)
{
    v4_test_begin("Rahmenformat, Pruefsumme und Codecs (proto_ver 3)");

    test_crc();
    test_write_frame();
    test_read_frame();
    test_codec_info();
    test_codec_status();
    test_codec_input();
    test_level_diagnosis();
    test_codec_hid();
    test_codec_dev();
    test_codec_port_mode();
    test_codec_port_config();
    test_names_routing();
    test_display_helpers();

    return v4_test_failures;
}
