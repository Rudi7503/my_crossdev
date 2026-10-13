/*
 * v4_proto.c -- gemeinsamer Vertrag V4 <-> ESP32, Implementierung.
 * Siehe v4_proto.h fuer die Portabilitaetsregeln (PROTOCOL_V4_MASTER.md §0/§2).
 *
 * Diese Datei ist absichtlich frei von Plattform- und Geraetecode: sie ist die
 * eine Quelle fuer Frame-Format, Pruefsumme und Nutzlast-Codec, benutzt vom
 * Master (V4-Seite) und vom Mock (ESP32-Seite). Gegenueber dem Audio-Projekt
 * fehlen BULK/CRC-32 (es gibt keinen Dateiweg) und alle SD-/Datei-/Audio-/EQ-
 * Codecs -- siehe die Kopfzeile von v4_proto.h.
 */

#include "v4_proto.h"

#include <string.h>

/* ------------------------------------------------------------------ */
/* Pruefsumme -- §3                                                    */
/* ------------------------------------------------------------------ */

uint8_t v4p_crc8(const uint8_t *d, size_t n)
{
    uint8_t crc = 0x00u;
    size_t  i;
    int     b;

    for (i = 0; i < n; i++) {
        crc ^= d[i];
        for (b = 0; b < 8; b++) {
            if (crc & 0x80u) {
                crc = (uint8_t)((uint8_t)(crc << 1) ^ 0x07u);
            } else {
                crc = (uint8_t)(crc << 1);
            }
        }
    }
    return crc;
}

/* ------------------------------------------------------------------ */
/* Byte-Order-Selbsttest -- §0                                         */
/* ------------------------------------------------------------------ */

int v4p_selftest_byteorder(void)
{
    uint8_t b[4];

    v4p_put_u32le(b, 0x12345678u);
    if (b[0] != 0x78u || b[1] != 0x56u || b[2] != 0x34u || b[3] != 0x12u) {
        return 0;
    }
    if (v4p_get_u32le(b) != 0x12345678u) {
        return 0;
    }

    v4p_put_u16le(b, 0x00FFu);
    if (b[0] != 0xFFu || b[1] != 0x00u) {
        return 0;
    }
    if (v4p_get_u16le(b) != 0x00FFu) {
        return 0;
    }

    return 1;
}

/* ------------------------------------------------------------------ */
/* Frame-Aufbau -- §4                                                  */
/* ------------------------------------------------------------------ */

int v4p_build_write(uint8_t *f, uint8_t cmd, uint8_t seq,
                    const uint8_t *payload, uint8_t len)
{
    uint8_t i;

    if (f == NULL) {
        return -1;
    }
    if (len > V4P_WRITE_PAYLOAD_MAX) {
        return -1;
    }
    if (len > 0u && payload == NULL) {
        return -1;
    }

    /* 1. Alle 32 Byte auf 0x00 */
    memset(f, 0, V4P_WRITE_FRAME_LEN);

    /* 2. Kopf */
    f[0] = V4P_MAGIC_WRITE;
    f[1] = cmd;
    f[2] = seq;
    f[3] = len;

    /* 3. Nutzlast nach [4..] */
    for (i = 0; i < len; i++) {
        f[4u + i] = payload[i];
    }

    /* 4. CRC-8 ueber Byte 0..30 */
    f[31] = v4p_crc8(f, 31u);
    return 0;
}

int v4p_build_read(uint8_t *f, uint8_t cmd, uint8_t seq, uint8_t status,
                   uint8_t flags, const uint8_t *payload, uint8_t len)
{
    uint8_t i;

    if (f == NULL) {
        return -1;
    }
    if (len > V4P_READ_PAYLOAD_MAX) {
        return -1;
    }
    if (len > 0u && payload == NULL) {
        return -1;
    }

    memset(f, 0, V4P_READ_FRAME_LEN);

    f[0] = V4P_MAGIC_READ;
    f[1] = cmd;
    f[2] = seq;
    f[3] = status;
    f[4] = len;

    for (i = 0; i < len; i++) {
        f[5u + i] = payload[i];
    }

    f[V4P_READ_OFF_FLAGS] = flags;
    f[V4P_READ_OFF_CRC]   = v4p_crc8(f, V4P_READ_CRC_RANGE);
    return 0;
}

/* ------------------------------------------------------------------ */
/* Frame-Pruefung -- §4.2                                              */
/* ------------------------------------------------------------------ */

int v4p_check_read(const uint8_t *f, size_t got, uint8_t cmd, uint8_t seq,
                   v4p_read_t *out)
{
    uint8_t i;

    if (f == NULL || out == NULL) {
        return V4P_CHECK_SIZE;
    }
    /* 1. got == 128 ?
     *
     * Diese Pruefung ist der Waechter gegen die 64-Byte-Fassung. Wird sie
     * gelockert ("lies, was da ist"), laufen alle Felder um 64 Byte verschoben
     * durch den Rest der Pruefung -- und weil Flags und CRC dann zufaellig
     * stimmen koennen, faellt es nicht einmal auf. */
    if (got != (size_t)V4P_READ_FRAME_LEN) {
        return V4P_CHECK_SIZE;
    }
    /* 2. magic */
    if (f[0] != V4P_MAGIC_READ) {
        return V4P_CHECK_MAGIC;
    }
    /* 3. CRC-8 ueber Byte 0..126 */
    if (f[V4P_READ_OFF_CRC] != v4p_crc8(f, V4P_READ_CRC_RANGE)) {
        return V4P_CHECK_CRC;
    }
    /* 4. cmd und seq zurueckgespiegelt? (erkennt verschobene Stroeme) */
    if (f[1] != cmd || f[2] != seq) {
        return V4P_CHECK_ECHO;
    }
    /* 5. len <= 121 ? */
    if (f[4] > V4P_READ_PAYLOAD_MAX) {
        return V4P_CHECK_LEN;
    }

    /* 6. Erst jetzt auswerten. */
    out->cmd    = f[1];
    out->seq    = f[2];
    out->status = f[3];
    out->len    = f[4];
    out->flags  = f[V4P_READ_OFF_FLAGS];
    for (i = 0; i < out->len; i++) {
        out->payload[i] = f[5u + i];
    }
    for (i = out->len; i < V4P_READ_PAYLOAD_MAX; i++) {
        out->payload[i] = 0x00u;    /* kein Blick in alte Pufferinhalte */
    }
    return V4P_CHECK_OK;
}

const char *v4p_check_str(int rc)
{
    switch (rc) {
    case V4P_CHECK_OK:    return "ok";
    case V4P_CHECK_SIZE:  return "falsche Framelaenge";
    case V4P_CHECK_MAGIC: return "falsche Magic";
    case V4P_CHECK_CRC:   return "CRC-Fehler";
    case V4P_CHECK_ECHO:  return "cmd/seq nicht zurueckgespiegelt";
    case V4P_CHECK_LEN:   return "len-Feld unzulaessig";
    default:              return "unbekannt";
    }
}

/* ------------------------------------------------------------------ */
/* Nutzlast-Codec -- §11                                               */
/* ------------------------------------------------------------------ */

size_t v4p_enc_get_info(uint8_t *p, const v4p_info_t *in)
{
    p[0] = in->proto_ver;
    p[1] = in->fw_ver;
    p[2] = in->write_frame_len;
    p[3] = in->read_frame_len;
    v4p_put_u16le(p + 4, in->bulk_payload_max);
    v4p_put_u16le(p + 6, in->chunk);
    p[8] = in->max_devices;
    p[9] = in->path_max;
    v4p_put_u16le(p + V4P_INFO_OFF_CAPS, in->caps);
    return V4P_INFO_PAYLOAD_LEN;
}

int v4p_dec_get_info(const uint8_t *p, uint8_t len, v4p_info_t *out)
{
    if (p == NULL || out == NULL) {
        return -1;
    }
    if (len < V4P_INFO_PAYLOAD_LEN) {
        return -1;
    }
    memset(out, 0, sizeof(*out));
    out->proto_ver        = p[0];
    out->fw_ver           = p[1];
    out->write_frame_len  = p[2];
    out->read_frame_len   = p[3];
    out->bulk_payload_max = v4p_get_u16le(p + 4);
    out->chunk            = v4p_get_u16le(p + 6);
    out->max_devices      = p[8];
    out->path_max         = p[9];
    out->caps             = v4p_get_u16le(p + V4P_INFO_OFF_CAPS);
    return 0;
}

size_t v4p_enc_get_status(uint8_t *p, const v4p_status_t *in)
{
    uint8_t i;

    for (i = 0; i < (uint8_t)V4P_ST_LEN; i++) {
        p[i] = 0x00u;               /* Reservebytes bleiben definiert */
    }
    p[V4P_ST_OFF_STATE]            = in->state;
    p[V4P_ST_OFF_CONN_INDEX]       = in->conn_index;
    p[V4P_ST_OFF_DEV_COUNT]        = in->dev_count;
    p[V4P_ST_OFF_SCAN_ACTIVE]      = in->scan_active;
    p[V4P_ST_OFF_SD_MOUNTED]       = in->sd_mounted;
    p[V4P_ST_OFF_AUDIO_FLAGS]      = in->audio_flags;
    v4p_put_u16le(p + V4P_ST_OFF_SCAN_GEN, in->scan_gen);
    v4p_put_u32le(p + V4P_ST_OFF_SD_FREE_KB, in->sd_free_kb);
    v4p_put_u16le(p + V4P_ST_OFF_CHUNK, in->chunk);
    p[V4P_ST_OFF_PROTO_VER]        = in->proto_ver;
    p[V4P_ST_OFF_FW_VER]           = in->fw_ver;
    p[V4P_ST_OFF_SD_CARD_PRESENT]  = in->sd_card_present;
    return V4P_ST_LEN;
}

int v4p_dec_get_status(const uint8_t *p, uint8_t len, v4p_status_t *out)
{
    if (p == NULL || out == NULL) {
        return -1;
    }
    if (len < V4P_ST_LEN) {
        return -1;
    }
    memset(out, 0, sizeof(*out));
    out->state           = p[V4P_ST_OFF_STATE];
    out->conn_index      = p[V4P_ST_OFF_CONN_INDEX];
    out->dev_count       = p[V4P_ST_OFF_DEV_COUNT];
    out->scan_active     = p[V4P_ST_OFF_SCAN_ACTIVE];
    out->sd_mounted      = p[V4P_ST_OFF_SD_MOUNTED];
    out->audio_flags     = p[V4P_ST_OFF_AUDIO_FLAGS];
    out->scan_gen        = v4p_get_u16le(p + V4P_ST_OFF_SCAN_GEN);
    out->sd_free_kb      = v4p_get_u32le(p + V4P_ST_OFF_SD_FREE_KB);
    out->chunk           = v4p_get_u16le(p + V4P_ST_OFF_CHUNK);
    out->proto_ver       = p[V4P_ST_OFF_PROTO_VER];
    out->fw_ver          = p[V4P_ST_OFF_FW_VER];
    out->sd_card_present = p[V4P_ST_OFF_SD_CARD_PRESENT];
    return 0;
}

size_t v4p_enc_get_input(uint8_t *p, const v4p_input_t *in)
{
    p[V4P_IN_OFF_STATE0] = in->state[0];
    p[V4P_IN_OFF_STATE1] = in->state[1];
    v4p_put_u32le(p + V4P_IN_OFF_STEPS_X, in->steps_x);
    v4p_put_u32le(p + V4P_IN_OFF_STEPS_Y, in->steps_y);
    /* Pegel immer mitschreiben: der Master entscheidet anhand der LAENGE, ob
     * sie gueltig sind. Ein Encoder, der sie weglaesst, wuerde die
     * 10-Byte-Fassung nur nachaeffen -- dafuer gibt es den Schalter im Mock. */
    p[V4P_IN_OFF_LEVEL0] = (uint8_t)(in->level[0] & V4P_IN_ST_MASK);
    p[V4P_IN_OFF_LEVEL1] = (uint8_t)(in->level[1] & V4P_IN_ST_MASK);
    return V4P_IN_PAYLOAD_LEN;
}

/*
 * Der wichtigste Teil dieser Funktion ist die Laengenpruefung: sie ist die
 * VERSIONSauskunft. 12 Byte = Pegel dabei, 10 Byte = aeltere Firmware ohne.
 * Eine Laenge von 11 Byte gibt es nicht -- sie wird abgewiesen und nicht auf
 * "10, aber irgendwie laenger" gebogen. Wer hier grosszuegig ist, zeigt
 * spaeter Pegel an, die nie auf der Leitung standen.
 */
int v4p_dec_get_input(const uint8_t *p, uint8_t len, v4p_input_t *out)
{
    uint8_t with_level;

    if (p == NULL || out == NULL) {
        return -1;
    }
    if (len >= (uint8_t)V4P_IN_PAYLOAD_LEN) {
        with_level = 1u;
    } else if (len == (uint8_t)V4P_IN_PAYLOAD_LEN_NO_LEVEL) {
        with_level = 0u;
    } else {
        return -1;          /* 11 Byte o.ae.: keine gueltige Fassung */
    }

    memset(out, 0, sizeof(*out));
    out->state[0] = p[V4P_IN_OFF_STATE0] & V4P_IN_ST_MASK;
    out->state[1] = p[V4P_IN_OFF_STATE1] & V4P_IN_ST_MASK;
    out->steps_x  = v4p_get_u32le(p + V4P_IN_OFF_STEPS_X);
    out->steps_y  = v4p_get_u32le(p + V4P_IN_OFF_STEPS_Y);

    if (with_level != 0u) {
        out->level[0]    = p[V4P_IN_OFF_LEVEL0] & V4P_IN_ST_MASK;
        out->level[1]    = p[V4P_IN_OFF_LEVEL1] & V4P_IN_ST_MASK;
        out->level_valid = 1u;
    } else {
        /* NICHT auf 0 setzen und "0 = Low" nennen: die Pegel sind unbekannt.
         * level bleibt 0, aber level_valid == 0 verbietet jede Anzeige als
         * Messwert -- auch eine abgeleitete. */
        out->level[0]    = 0u;
        out->level[1]    = 0u;
        out->level_valid = 0u;
    }
    return 0;
}

size_t v4p_enc_get_hid(uint8_t *p, const v4p_hid_t *in)
{
    uint8_t n = in->name_len;
    uint8_t i;

    if (n > V4P_HID_NAME_MAX) {
        n = (uint8_t)V4P_HID_NAME_MAX;
    }

    for (i = 0; i < (uint8_t)V4P_HID_PAYLOAD_MAX; i++) {
        p[i] = 0x00u;
    }
    p[V4P_HID_OFF_KIND]      = in->kind;
    p[V4P_HID_OFF_TRANSPORT] = in->transport;
    v4p_put_u16le(p + V4P_HID_OFF_VID, in->vid);
    v4p_put_u16le(p + V4P_HID_OFF_PID, in->pid);
    p[V4P_HID_OFF_BATTERY]   = in->battery;
    v4p_put_u16le(p + V4P_HID_OFF_REPORT_ID, in->report_id);
    v4p_put_u16le(p + V4P_HID_OFF_REPORT_LEN, in->report_len);
    v4p_put_u16le(p + V4P_HID_OFF_REPORTS, in->reports);
    p[V4P_HID_OFF_NAME_LEN]  = n;
    for (i = 0; i < n; i++) {
        p[V4P_HID_OFF_NAME + i] = (uint8_t)in->name[i];
    }
    return (size_t)V4P_HID_OFF_NAME + (size_t)n;
}

int v4p_dec_get_hid(const uint8_t *p, uint8_t len, v4p_hid_t *out)
{
    uint8_t n;
    uint8_t i;

    if (p == NULL || out == NULL) {
        return -1;
    }
    if (len < (uint8_t)V4P_HID_OFF_NAME) {
        return -1;
    }
    n = p[V4P_HID_OFF_NAME_LEN];
    if (n > V4P_HID_NAME_MAX) {
        return -1;
    }
    if ((uint16_t)V4P_HID_OFF_NAME + (uint16_t)n > (uint16_t)len) {
        return -1;
    }

    memset(out, 0, sizeof(*out));
    out->kind       = p[V4P_HID_OFF_KIND];
    out->transport  = p[V4P_HID_OFF_TRANSPORT];
    out->vid        = v4p_get_u16le(p + V4P_HID_OFF_VID);
    out->pid        = v4p_get_u16le(p + V4P_HID_OFF_PID);
    out->battery    = p[V4P_HID_OFF_BATTERY];
    out->report_id  = v4p_get_u16le(p + V4P_HID_OFF_REPORT_ID);
    out->report_len = v4p_get_u16le(p + V4P_HID_OFF_REPORT_LEN);
    out->reports    = v4p_get_u16le(p + V4P_HID_OFF_REPORTS);
    out->name_len   = n;
    out->flags      = 0u;       /* kommt aus dem READ-Frame, nicht aus der
                                 * Nutzlast -- hier definiert initialisieren */
    for (i = 0; i < n; i++) {
        out->name[i] = (char)p[V4P_HID_OFF_NAME + i];
    }
    out->name[n] = '\0';
    return 0;
}

size_t v4p_enc_dev(uint8_t *p, const v4p_dev_t *in)
{
    uint8_t n = in->name_len;
    uint8_t i;

    if (n > V4P_DEVNAME_MAX) {
        n = (uint8_t)V4P_DEVNAME_MAX;
    }

    p[V4P_DEV_OFF_INDEX] = in->idx;
    for (i = 0; i < 6u; i++) {
        p[V4P_DEV_OFF_BDA + i] = in->bda[i];
    }
    p[V4P_DEV_OFF_NAME_LEN] = n;
    for (i = 0; i < n; i++) {
        p[V4P_DEV_OFF_NAME + i] = (uint8_t)in->name[i];
    }
    return (size_t)V4P_DEV_OFF_NAME + (size_t)n;
}

int v4p_dec_dev(const uint8_t *p, uint8_t len, v4p_dev_t *out)
{
    uint8_t n;
    uint8_t i;

    if (p == NULL || out == NULL) {
        return -1;
    }
    if (len < (uint8_t)V4P_DEV_OFF_NAME) {
        return -1;
    }
    n = p[V4P_DEV_OFF_NAME_LEN];
    if (n > V4P_DEVNAME_MAX) {
        return -1;
    }
    if ((uint16_t)V4P_DEV_OFF_NAME + (uint16_t)n > (uint16_t)len) {
        return -1;
    }

    memset(out, 0, sizeof(*out));
    out->idx      = p[V4P_DEV_OFF_INDEX];
    for (i = 0; i < 6u; i++) {
        out->bda[i] = p[V4P_DEV_OFF_BDA + i];
    }
    out->name_len = n;
    out->flags    = 0u;         /* siehe v4p_dec_get_hid */
    for (i = 0; i < n; i++) {
        out->name[i] = (char)p[V4P_DEV_OFF_NAME + i];
    }
    out->name[n] = '\0';
    return 0;
}

size_t v4p_enc_get_port_mode(uint8_t *p, const uint8_t mode[V4P_PORT_COUNT])
{
    p[V4P_GPM_OFF_MODE0] = mode[0];
    p[V4P_GPM_OFF_MODE1] = mode[1];
    return V4P_GPM_PAYLOAD_LEN;
}

int v4p_dec_get_port_mode(const uint8_t *p, uint8_t len,
                          uint8_t mode[V4P_PORT_COUNT])
{
    if (p == NULL || mode == NULL) {
        return -1;
    }
    if (len < V4P_GPM_PAYLOAD_LEN) {
        return -1;
    }
    if (p[V4P_GPM_OFF_MODE0] > V4P_PORT_MODE_MAX
        || p[V4P_GPM_OFF_MODE1] > V4P_PORT_MODE_MAX) {
        return -1;              /* unmoegliche Betriebsart -> nicht raten */
    }
    mode[0] = p[V4P_GPM_OFF_MODE0];
    mode[1] = p[V4P_GPM_OFF_MODE1];
    return 0;
}

size_t v4p_enc_set_port_mode(uint8_t *p, uint8_t port, uint8_t mode)
{
    p[V4P_SPM_OFF_PORT] = port;
    p[V4P_SPM_OFF_MODE] = mode;
    return V4P_SPM_PAYLOAD_LEN;
}

int v4p_dec_set_port_mode(const uint8_t *p, uint8_t len, uint8_t *port,
                          uint8_t *mode)
{
    if (p == NULL) {
        return -1;
    }
    if (len < V4P_SPM_PAYLOAD_LEN) {
        return -1;
    }
    if (p[V4P_SPM_OFF_PORT] >= V4P_PORT_COUNT
        || p[V4P_SPM_OFF_MODE] > V4P_PORT_MODE_MAX) {
        return -1;
    }
    if (port != NULL) {
        *port = p[V4P_SPM_OFF_PORT];
    }
    if (mode != NULL) {
        *mode = p[V4P_SPM_OFF_MODE];
    }
    return 0;
}

/* ------------------------------------------------------------------ */
/* Klartexte und Ableitungen                                           */
/* ------------------------------------------------------------------ */

const char *v4p_port_mode_name(uint8_t mode)
{
    switch (mode) {
    case V4P_PORT_OFF:   return "aus";
    case V4P_PORT_JOY:   return "Joystick";
    case V4P_PORT_MOUSE: return "Maus";
    default:             return "?";
    }
}

uint8_t v4p_port_mode_next(uint8_t mode)
{
    if (mode >= V4P_PORT_MODE_MAX) {
        return V4P_PORT_OFF;    /* aus "Maus" bzw. aus Unfug wird "aus" */
    }
    return (uint8_t)(mode + 1u);
}

const char *v4p_bt_state_name(uint8_t state)
{
    switch (state) {
    case V4P_STATE_IDLE:       return "IDLE";
    case V4P_STATE_SCANNING:   return "SCANNING";
    case V4P_STATE_CONNECTING: return "CONNECTING";
    case V4P_STATE_CONNECTED:  return "CONNECTED";
    case V4P_STATE_SUSPENDED:  return "SUSPENDED";
    default:                   return "?";
    }
}

const char *v4p_hid_kind_name(uint8_t kind)
{
    switch (kind) {
    case V4P_HID_KIND_NONE:  return "keine";
    case V4P_HID_KIND_PAD:   return "Pad";
    case V4P_HID_KIND_MOUSE: return "Maus";
    case V4P_HID_KIND_OTHER: return "andere";
    default:                 return "?";
    }
}

const char *v4p_transport_name(uint8_t tr)
{
    switch (tr) {
    case V4P_HID_TR_BREDR: return "BR/EDR";
    case V4P_HID_TR_BLE:   return "BLE";
    default:               return "?";
    }
}

const char *v4p_bit_name(unsigned bit)
{
    switch (bit) {
    case 0u: return "UP";
    case 1u: return "DOWN";
    case 2u: return "LEFT";
    case 3u: return "RIGHT";
    case 4u: return "BTN1";
    case 5u: return "BTN2";
    case 6u: return "BTN3";
    default: return "?";
    }
}

const char *v4p_bit_pin(unsigned bit)
{
    switch (bit) {
    case 0u: return "Pin 1";
    case 1u: return "Pin 2";
    case 2u: return "Pin 3";
    case 3u: return "Pin 4";
    case 4u: return "Pin 6";
    case 5u: return "Pin 9";
    case 6u: return "Pin 5";
    default: return "?";
    }
}

void v4p_bits_text(uint8_t bits, char *out, size_t cap)
{
    static const char *const names[7] = {
        "UP", "DOWN", "LEFT", "RIGHT", "BTN1", "BTN2", "BTN3"
    };
    size_t   n         = 0u;
    unsigned bit;
    int      first     = 1;
    int      truncated = 0;

    if (out == NULL || cap == 0u) {
        return;
    }
    out[0] = '\0';

    /* Bitmaske auf die sieben belegten Leitungen begrenzen: Bit 7 ist frei und
     * darf in keiner Anzeige als "unbekannte Leitung" auftauchen. */
    bits = (uint8_t)(bits & V4P_IN_ST_MASK);

    for (bit = 0u; bit < V4P_IN_ST_COUNT; bit++) {
        const char *name;
        size_t      len;

        if ((bits & (uint8_t)(1u << bit)) == 0u) {
            continue;
        }
        name = names[bit];
        len  = strlen(name);

        /* "+1" fuer das trennende Leerzeichen, "+1" fuer die Null am Ende. */
        if (n + (first ? 0u : 1u) + len + 1u > cap) {
            truncated = 1;
            break;
        }
        if (!first) {
            out[n++] = ' ';
        }
        {
            size_t k;

            for (k = 0u; k < len; k++) {
                out[n++] = name[k];
            }
        }
        first = 0;
    }

    if (first) {
        /* Nichts gezogen: ein sichtbares "-" statt einer leeren Zeile, damit
         * die Spalte nicht als "vergessen" durchgeht. */
        if (cap >= 2u) {
            out[0] = '-';
            out[1] = '\0';
        }
        return;
    }
    if (truncated && cap >= 5u) {
        /* Nicht mitten im Namen abbrechen: ein halber Leitungsname ("BTN")
         * waere schlimmer als ein sichtbares "...". */
        if (n + 4u > cap) {
            n = cap - 4u;
        }
        out[n++] = '.';
        out[n++] = '.';
        out[n++] = '.';
    }
    out[n] = '\0';
}

/*
 * Widerspruch zwischen Zug und Pegel.
 *
 * Die Wahrheitstabelle je Leitung (drive = gezogen, level = High):
 *
 *   drive  level   Bedeutung
 *   -----  -----   ------------------------------------------------------
 *     0      1     losgelassen, High  -> Pull-up wirkt. Der Normalfall.
 *     0      0     losgelassen, Low   -> FEHLENDER PULL-UP (missing_pullup)
 *     1      0     gezogen, Low       -> richtig, die Leitung liegt auf GND
 *     1      1     gezogen, High      -> elektrisch unmoeglich (impossible)
 *
 * Bitweise gerechnet: missing_pullup = ~drive & ~level, impossible =
 * drive & level, jeweils auf die sieben belegten Leitungen begrenzt.
 */
uint8_t v4p_level_missing_pullup(uint8_t drive_bits, uint8_t level_bits)
{
    return (uint8_t)((~drive_bits) & (~level_bits) & V4P_IN_ST_MASK);
}

uint8_t v4p_level_impossible(uint8_t drive_bits, uint8_t level_bits)
{
    return (uint8_t)(drive_bits & level_bits & V4P_IN_ST_MASK);
}
