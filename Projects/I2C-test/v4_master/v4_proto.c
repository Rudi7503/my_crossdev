/*
 * v4_proto.c -- gemeinsamer Vertrag V4 <-> ESP32, Implementierung.
 * Siehe v4_proto.h fuer die Portabilitaetsregeln (PROTOCOL_V4_MASTER.md §0/§2).
 *
 * Diese Datei ist absichtlich frei von Plattform- und Geraetecode: sie ist die
 * eine Quelle fuer Frame-Format, Pruefsummen und Nutzlast-Codec, benutzt vom
 * Master (V4-Seite) und vom Mock (ESP32-Seite).
 */

#include "v4_proto.h"

#include <string.h>

/* ------------------------------------------------------------------ */
/* Pruefsummen -- §3                                                   */
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

uint32_t v4p_crc32(const uint8_t *d, size_t n)
{
    uint32_t crc = 0xFFFFFFFFu;
    size_t   i;
    int      b;

    for (i = 0; i < n; i++) {
        crc ^= (uint32_t)d[i];
        for (b = 0; b < 8; b++) {
            if (crc & 1u) {
                crc = (crc >> 1) ^ 0xEDB88320u;
            } else {
                crc = crc >> 1;
            }
        }
    }
    return crc ^ 0xFFFFFFFFu;
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

    if (len > V4P_WRITE_PAYLOAD_MAX) {
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

    if (len > V4P_READ_PAYLOAD_MAX) {
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

int v4p_build_bulk(uint8_t *f, uint16_t chunk, uint8_t cmd, uint8_t status,
                   uint8_t handle, uint16_t block,
                   const uint8_t *data, uint16_t len)
{
    size_t total;
    uint16_t i;

    if (chunk < V4P_CHUNK_MIN || chunk > V4P_CHUNK_MAX) {
        return -1;
    }
    if (len > chunk) {
        return -1;
    }

    total = (size_t)V4P_BULK_OVERHEAD + (size_t)chunk;
    memset(f, 0, total);

    f[0] = V4P_MAGIC_BULK;
    f[1] = cmd;
    f[2] = status;
    f[3] = handle;
    v4p_put_u16le(f + 4, block);
    v4p_put_u16le(f + 6, len);

    for (i = 0; i < len; i++) {
        f[V4P_BULK_HDR_LEN + i] = data[i];
    }
    /* Byte len..chunk-1 bleiben 0x00 -- der CRC deckt sie mit ab (§4.3). */

    v4p_put_u32le(f + V4P_BULK_HDR_LEN + chunk,
                  v4p_crc32(f, (size_t)V4P_BULK_HDR_LEN + (size_t)chunk));
    return 0;
}

/* ------------------------------------------------------------------ */
/* Frame-Pruefung -- §4.2 / §4.3                                       */
/* ------------------------------------------------------------------ */

int v4p_check_read(const uint8_t *f, size_t got, uint8_t cmd, uint8_t seq,
                   v4p_read_t *out)
{
    uint8_t i;

    /* 1. got == 64 ? */
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
    /* 5. len <= 57 ? */
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

int v4p_check_bulk(const uint8_t *f, size_t got, uint16_t chunk, uint8_t cmd,
                   uint8_t handle, uint16_t block, v4p_bulk_t *out)
{
    uint16_t len;

    /* 1. got == 12 + chunk ? */
    if (got != (size_t)V4P_BULK_OVERHEAD + (size_t)chunk) {
        return V4P_CHECK_SIZE;
    }
    /* 2. magic */
    if (f[0] != V4P_MAGIC_BULK) {
        return V4P_CHECK_MAGIC;
    }
    /* 3. len <= chunk ? */
    len = v4p_get_u16le(f + 6);
    if (len > chunk) {
        return V4P_CHECK_LEN;
    }
    /* 4. CRC-32 ueber Byte 0..7+chunk */
    if (v4p_get_u32le(f + V4P_BULK_HDR_LEN + chunk)
            != v4p_crc32(f, (size_t)V4P_BULK_HDR_LEN + (size_t)chunk)) {
        return V4P_CHECK_CRC;
    }
    /* 5. cmd/handle/block absichern -- der Bulk-Frame hat kein SEQ-Feld. */
    if (f[1] != cmd || f[3] != handle || v4p_get_u16le(f + 4) != block) {
        return V4P_CHECK_MISMATCH;
    }

    out->cmd    = f[1];
    out->status = f[2];
    out->handle = f[3];
    out->block  = v4p_get_u16le(f + 4);
    out->len    = len;
    out->data   = f + V4P_BULK_HDR_LEN;
    return V4P_CHECK_OK;
}

const char *v4p_check_str(int rc)
{
    switch (rc) {
    case V4P_CHECK_OK:       return "ok";
    case V4P_CHECK_SIZE:     return "falsche Framelaenge";
    case V4P_CHECK_MAGIC:    return "falsche Magic";
    case V4P_CHECK_CRC:      return "CRC-Fehler";
    case V4P_CHECK_ECHO:     return "cmd/seq nicht zurueckgespiegelt";
    case V4P_CHECK_LEN:      return "len-Feld unzulaessig";
    case V4P_CHECK_MISMATCH: return "cmd/handle/block passt nicht";
    default:                 return "unbekannt";
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
    p[10] = in->reserved[0];
    p[11] = in->reserved[1];
    return 12u;
}

int v4p_dec_get_info(const uint8_t *p, uint8_t len, v4p_info_t *out)
{
    if (len < 12u) {
        return -1;
    }
    out->proto_ver       = p[0];
    out->fw_ver          = p[1];
    out->write_frame_len = p[2];
    out->read_frame_len  = p[3];
    out->bulk_payload_max= v4p_get_u16le(p + 4);
    out->chunk           = v4p_get_u16le(p + 6);
    out->max_devices     = p[8];
    out->path_max        = p[9];
    out->reserved[0]     = p[10];
    out->reserved[1]     = p[11];
    return 0;
}

size_t v4p_enc_get_status(uint8_t *p, const v4p_status_t *in)
{
    uint8_t i;

    for (i = 0; i < V4P_ST_LEN; i++) {
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
    if (len < V4P_ST_LEN) {
        return -1;
    }
    out->state            = p[V4P_ST_OFF_STATE];
    out->conn_index       = p[V4P_ST_OFF_CONN_INDEX];
    out->dev_count        = p[V4P_ST_OFF_DEV_COUNT];
    out->scan_active      = p[V4P_ST_OFF_SCAN_ACTIVE];
    out->sd_mounted       = p[V4P_ST_OFF_SD_MOUNTED];
    out->audio_flags      = p[V4P_ST_OFF_AUDIO_FLAGS];
    out->scan_gen         = v4p_get_u16le(p + V4P_ST_OFF_SCAN_GEN);
    out->sd_free_kb       = v4p_get_u32le(p + V4P_ST_OFF_SD_FREE_KB);
    out->chunk            = v4p_get_u16le(p + V4P_ST_OFF_CHUNK);
    out->proto_ver        = p[V4P_ST_OFF_PROTO_VER];
    out->fw_ver           = p[V4P_ST_OFF_FW_VER];
    out->sd_card_present  = p[V4P_ST_OFF_SD_CARD_PRESENT];
    return 0;
}

size_t v4p_enc_sd_info(uint8_t *p, const v4p_sdinfo_t *in)
{
    v4p_put_u32le(p + 0, in->total_kb);
    v4p_put_u32le(p + 4, in->free_kb);
    v4p_put_u16le(p + 8, in->sector_size);
    p[10] = in->fat_type;
    return 11u;
}

int v4p_dec_sd_info(const uint8_t *p, uint8_t len, v4p_sdinfo_t *out)
{
    if (len < 11u) {
        return -1;
    }
    out->total_kb    = v4p_get_u32le(p + 0);
    out->free_kb     = v4p_get_u32le(p + 4);
    out->sector_size = v4p_get_u16le(p + 8);
    out->fat_type    = p[10];
    return 0;
}

size_t v4p_enc_dev(uint8_t *p, const v4p_dev_t *in)
{
    uint8_t n = in->name_len;
    uint8_t i;

    if (n > V4P_DEVNAME_MAX) {
        n = V4P_DEVNAME_MAX;
    }

    p[0] = in->idx;
    for (i = 0; i < 6u; i++) {
        p[1u + i] = in->bda[i];
    }
    p[7] = n;
    for (i = 0; i < n; i++) {
        p[8u + i] = (uint8_t)in->name[i];
    }
    return (size_t)8u + (size_t)n;
}

int v4p_dec_dev(const uint8_t *p, uint8_t len, v4p_dev_t *out)
{
    uint8_t n;
    uint8_t i;

    if (len < 8u) {
        return -1;
    }
    n = p[7];
    if (n > V4P_DEVNAME_MAX) {
        return -1;
    }
    if ((uint16_t)8u + (uint16_t)n > (uint16_t)len) {
        return -1;
    }

    out->idx      = p[0];
    out->flags    = 0u;     /* kommt aus dem READ-Frame (+62), nicht aus der
                             * Nutzlast -- hier definiert initialisieren */
    for (i = 0; i < 6u; i++) {
        out->bda[i] = p[1u + i];
    }
    out->name_len = n;
    for (i = 0; i < n; i++) {
        out->name[i] = (char)p[8u + i];
    }
    out->name[n] = '\0';
    return 0;
}

size_t v4p_enc_dirent(uint8_t *p, const v4p_dirent_t *in)
{
    uint8_t n = in->name_len;
    uint8_t i;

    if (n > V4P_DIRNAME_MAX) {
        n = V4P_DIRNAME_MAX;
    }

    v4p_put_u16le(p + 0, in->index);
    p[2] = in->attr;
    v4p_put_u32le(p + 3, in->size);
    p[7] = n;
    for (i = 0; i < n; i++) {
        p[8u + i] = (uint8_t)in->name[i];
    }
    return (size_t)8u + (size_t)n;
}

int v4p_dec_dirent(const uint8_t *p, uint8_t len, v4p_dirent_t *out)
{
    uint8_t n;
    uint8_t i;

    if (len < 8u) {
        return -1;
    }
    n = p[7];
    if (n > V4P_DIRNAME_MAX) {
        return -1;
    }
    if ((uint16_t)8u + (uint16_t)n > (uint16_t)len) {
        return -1;
    }

    out->index    = v4p_get_u16le(p + 0);
    out->attr     = p[2];
    out->flags    = 0u;     /* siehe v4p_dec_dev */
    out->size     = v4p_get_u32le(p + 3);
    out->name_len = n;
    for (i = 0; i < n; i++) {
        out->name[i] = (char)p[8u + i];
    }
    out->name[n] = '\0';
    return 0;
}

size_t v4p_enc_file_open(uint8_t *p, uint8_t handle, uint32_t size,
                         uint8_t attr)
{
    p[0] = handle;
    v4p_put_u32le(p + 1, size);
    p[5] = attr;
    return 6u;
}

int v4p_dec_file_open(const uint8_t *p, uint8_t len, uint8_t *handle,
                      uint32_t *size, uint8_t *attr)
{
    if (len < 6u) {
        return -1;
    }
    if (handle != NULL) {
        *handle = p[0];
    }
    if (size != NULL) {
        *size = v4p_get_u32le(p + 1);
    }
    if (attr != NULL) {
        *attr = p[5];
    }
    return 0;
}
