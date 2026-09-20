/*
 * v4_mock.c -- ESP32-Emulator fuer den Linux-Harness (§14.1).
 *
 * Wird anstelle von v4_linux_i2c.c gelinkt: derselbe Satz Plattformhaken,
 * dieselben Aufbau- und Prueffunktionen aus v4_proto.c. Was hier steht, ist
 * die Slave-Seite des Vertrags -- inklusive der unangenehmen Faelle.
 *
 * Der Mock wartet NICHT wirklich: v4_plat_delay_us zaehlt nur mit. Damit
 * laufen die Testmatrizen aus §14.1 und der Dauerlauf aus §14.2.3 in
 * Sekundenbruchteilen statt in Minuten. Die Wartezeiten selbst sind in
 * v4_master.c festgelegt und werden hier nicht geprueft.
 */

#include "v4_mock.h"
#include "v4_master.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/* Zustand                                                             */
/* ------------------------------------------------------------------ */

#define V4_MOCK_MAX_NODES   32
#define V4_MOCK_MAX_DEVICES 16

/* Der ESP32 haelt die vollen Namen und kuerzt erst auf der Leitung (§10/§11).
 * Ohne breitere Ablage waere NAME_TRUNCATED im Mock nicht erreichbar. */
#define V4_MOCK_NAME_STORE  132

typedef struct {
    char     key[V4P_PATH_MAX + 1u];    /* normalisiert, Grossbuchstaben    */
    char     parent[V4P_PATH_MAX + 1u]; /* "" = Wurzel                      */
    char     name[V4_MOCK_NAME_STORE];  /* Anzeigename (Originalschreibung) */
    uint8_t  attr;
    uint8_t *data;                      /* nur Dateien                      */
    uint32_t size;
} mock_node_t;

typedef struct {
    int      used;
    int      node;              /* -1 = Wurzel */
    int      cache_valid;
    int      cache_node;
    uint16_t cache_index;
} mock_dir_t;

typedef struct {
    int used;
    int node;
} mock_file_t;

typedef struct {
    int      valid;
    uint8_t  handle;
    uint16_t block;
    uint16_t len;
    int      is_end;
    uint16_t chunk;
    uint8_t  data[V4P_CHUNK_MAX];
} mock_blockcache_t;

v4_mock_knobs_t v4_mock;

static mock_node_t       s_nodes[V4_MOCK_MAX_NODES];
static int               s_node_count;
static mock_dir_t        s_dirs[V4P_MAX_DIR_HANDLES];
static mock_file_t       s_files[V4P_MAX_FILE_HANDLES];
static mock_blockcache_t s_bc;

static char     s_devname[V4_MOCK_MAX_DEVICES][V4_MOCK_NAME_STORE];
static uint8_t  s_devbda [V4_MOCK_MAX_DEVICES][6];
static int      s_dev_count;

static uint16_t s_chunk;    /* chunk geht bis 1024 -- KEIN uint8_t! */
static int      s_mount_pending;    /* Mount laeuft nach der Antwort (§8.1) */
static unsigned long s_mount_at;    /* delay_us-Stand beim force=1 */
static int      s_mounted;
static uint32_t s_total_kb;
static uint32_t s_free_kb;
static uint8_t  s_state;
static uint8_t  s_conn_index;
static uint8_t  s_scan_active;
static uint8_t  s_audio_flags;
static int      s_card_present = 1;     /* Sockelschalter */
static int      s_play_state;           /* 0 = I2S-Eingang, 1 = SD-Datei */
static int      s_play_pending;         /* Umbau laeuft */
static int      s_play_left;            /* verbleibende BUSY-Runden */
static char     s_play_path[V4P_PATH_MAX + 1u];
static uint16_t s_scan_gen;
static int      s_connect_left;

static uint8_t  s_path[V4P_PATH_MAX + 1u];
static uint16_t s_path_len;

static uint8_t  s_ans[V4P_BULK_OVERHEAD + V4P_CHUNK_MAX];
static size_t   s_ans_len;
static int      s_ans_valid;
static unsigned long s_answer_no;

/* ------------------------------------------------------------------ */
/* Kleine Helfer                                                       */
/* ------------------------------------------------------------------ */

static int mock_has_dotdot(const char *p)
{
    const char *s = p;

    while (*s != '\0') {
        while (*s == '/') {
            s++;
        }
        if (s[0] == '.' && s[1] == '.' && (s[2] == '/' || s[2] == '\0')) {
            return 1;
        }
        while (*s != '\0' && *s != '/') {
            s++;
        }
    }
    return 0;
}

/* Normalisiert: fuehrende/doppelte/abschliessende '/' weg, Grossbuchstaben
 * (FAT ist case-insensitiv), Wurzel = "". Liefert -1 wenn zu lang. */
static int mock_norm(const char *in, char *out, size_t cap)
{
    const char *s = in;
    size_t o = 0;
    int first = 1;

    if (strlen(in) > V4P_PATH_MAX) {
        return -1;
    }

    while (*s != '\0') {
        while (*s == '/') {
            s++;
        }
        if (*s == '\0') {
            break;
        }
        if (!first && o + 1u < cap) {
            out[o++] = '/';
        }
        first = 0;
        while (*s != '\0' && *s != '/') {
            char c = *s++;
            if (c >= 'a' && c <= 'z') {
                c = (char)(c - 'a' + 'A');
            }
            if (o + 1u < cap) {
                out[o++] = c;
            }
        }
    }
    out[o] = '\0';
    return 0;
}

static void mock_parent_of(const char *key, char *out, size_t cap)
{
    size_t n = strlen(key);
    size_t i;

    out[0] = '\0';
    for (i = n; i > 0u; i--) {
        if (key[i - 1u] == '/') {
            if (i - 1u < cap) {
                size_t k;
                for (k = 0; k < i - 1u; k++) {
                    out[k] = key[k];
                }
                out[i - 1u] = '\0';
            }
            return;
        }
    }
}

static int mock_find(const char *key)
{
    int i;

    for (i = 0; i < s_node_count; i++) {
        if (strcmp(s_nodes[i].key, key) == 0) {
            return i;
        }
    }
    return -1;
}

static int mock_child_at(const char *dirkey, int n, int *idx_out)
{
    int i;
    int c = 0;

    for (i = 0; i < s_node_count; i++) {
        if (strcmp(s_nodes[i].parent, dirkey) == 0) {
            if (c == n) {
                *idx_out = i;
                return 0;
            }
            c++;
        }
    }
    return -1;
}

/* ------------------------------------------------------------------ */
/* Antworten bauen                                                     */
/* ------------------------------------------------------------------ */

static void mock_noise_read(uint8_t *f)
{
    int crc_bad = 0;

    if (v4_mock.bad_crc_every > 0
        && (s_answer_no % (unsigned long)v4_mock.bad_crc_every) == 0u) {
        f[V4P_READ_OFF_CRC] = (uint8_t)(f[V4P_READ_OFF_CRC] ^ 0xFFu);
        crc_bad = 1;                /* dieser Frame ist absichtlich kaputt */
    }
    if (v4_mock.no_seq_echo_every > 0
        && (s_answer_no % (unsigned long)v4_mock.no_seq_echo_every) == 0u) {
        f[2] = (uint8_t)(f[2] + 1u);
        if (!crc_bad) {
            /* CRC bleibt gueltig: so wird genau der ECHO-Pfad geprueft. */
            f[V4P_READ_OFF_CRC] = v4p_crc8(f, V4P_READ_CRC_RANGE);
        }
    }
    if (v4_mock.bad_frame_len_every > 0
        && (s_answer_no % (unsigned long)v4_mock.bad_frame_len_every) == 0u) {
        f[4] = (uint8_t)(V4P_READ_PAYLOAD_MAX + 1u);    /* 122: unzulaessig */
        if (!crc_bad) {
            f[V4P_READ_OFF_CRC] = v4p_crc8(f, V4P_READ_CRC_RANGE);
        }
    }
}

static void mock_answer_read(uint8_t cmd, uint8_t seq, uint8_t status,
                            uint8_t flags, const uint8_t *payload, uint8_t len)
{
    s_answer_no++;
    if (v4p_build_read(s_ans, cmd, seq, status, flags, payload, len) != 0) {
        s_ans_valid = 0;
        return;
    }
    s_ans_len   = V4P_READ_FRAME_LEN;
    s_ans_valid = 1;
    mock_noise_read(s_ans);
}

static void mock_answer_bulk(uint8_t cmd, uint8_t handle, uint16_t block,
                            uint8_t status, const uint8_t *data, uint16_t len)
{
    s_answer_no++;
    if (v4p_build_bulk(s_ans, s_chunk, cmd, status, handle, block,
                       data, len) != 0) {
        s_ans_valid = 0;
        return;
    }
    s_ans_len   = (size_t)V4P_BULK_OVERHEAD + (size_t)s_chunk;
    s_ans_valid = 1;

    if (v4_mock.bulk_wrong_handle) {
        /* Falsches Handle im Bulk-Frame: der Master muss verwerfen (§14.1) */
        v4_mock.bulk_wrong_handle = 0;
        s_ans[3] = (uint8_t)(handle + 1u);
        v4p_put_u32le(s_ans + V4P_BULK_HDR_LEN + s_chunk,
                      v4p_crc32(s_ans, (size_t)V4P_BULK_HDR_LEN + (size_t)s_chunk));
    }
}

static void mock_answer_read_empty(uint8_t cmd, uint8_t seq, uint8_t status)
{
    mock_answer_read(cmd, seq, status, 0u, NULL, 0u);
}

/* ------------------------------------------------------------------ */
/* Info- und Statusnutzlasten                                          */
/* ------------------------------------------------------------------ */

static void mock_fill_info(v4p_info_t *in)
{
    memset(in, 0, sizeof(*in));
    in->proto_ver       = V4P_PROTO_VER;
    in->fw_ver          = 2u;
    in->write_frame_len = (uint8_t)V4P_WRITE_FRAME_LEN;
    in->read_frame_len  = (uint8_t)V4P_READ_FRAME_LEN;
    in->bulk_payload_max= V4P_CHUNK_MAX;
    in->chunk           = s_chunk;
    in->max_devices     = (uint8_t)V4P_MAX_DEVICES;
    in->path_max        = (uint8_t)V4P_PATH_MAX;
}

static void mock_fill_status(v4p_status_t *st)
{
    memset(st, 0, sizeof(*st));
    st->state       = s_state;
    st->conn_index  = s_conn_index;
    st->dev_count   = (uint8_t)s_dev_count;
    st->scan_active = s_scan_active;
    st->sd_mounted  = s_mounted ? 1u : 0u;
    if (s_play_state == 1) {
        s_audio_flags |= V4P_AUDIO_SD_PLAYBACK;
    } else {
        s_audio_flags = (uint8_t)(s_audio_flags & ~V4P_AUDIO_SD_PLAYBACK);
    }
    st->audio_flags      = s_audio_flags;
    st->sd_card_present  = s_card_present ? 1u : 0u;
    st->scan_gen    = s_scan_gen;
    st->sd_free_kb  = s_free_kb;
    st->chunk       = s_chunk;
    st->proto_ver   = V4P_PROTO_VER;
    st->fw_ver      = 2u;
}

/* ------------------------------------------------------------------ */
/* Pfad aufloesen                                                      */
/* ------------------------------------------------------------------ */

/* Liefert den Protokoll-Status; *idx_out = Knotenindex oder -1 (Wurzel). */
static uint8_t mock_lookup(const uint8_t *pay, uint8_t len, int want_dir,
                           int *idx_out)
{
    char raw[V4P_PATH_MAX + 1u];
    char key[V4P_PATH_MAX + 1u];
    size_t n;
    int idx;

    if (!s_mounted) {
        return V4P_ST_NO_SD;
    }

    if (len == 0u) {
        /* Pfad aus dem PATH_*-Puffer (§11) */
        n = s_path_len;
        if (n > V4P_PATH_MAX) {
            return V4P_ST_TOO_LONG;
        }
        memcpy(raw, s_path, n);
        raw[n] = '\0';
    } else {
        if (len > V4P_WRITE_PAYLOAD_MAX) {
            return V4P_ST_TOO_LONG;
        }
        n = len;
        memcpy(raw, pay, n);
        raw[n] = '\0';
    }

    if (strlen(raw) > V4P_PATH_MAX) {
        return V4P_ST_TOO_LONG;
    }
    if (mock_has_dotdot(raw)) {
        return V4P_ST_BAD_ARG;                  /* §10: kein Ausbruch */
    }
    if (mock_norm(raw, key, sizeof(key)) != 0) {
        return V4P_ST_TOO_LONG;
    }

    if (key[0] == '\0') {                       /* Wurzel */
        if (!want_dir) {
            return V4P_ST_BAD_ARG;              /* Wurzel ist keine Datei */
        }
        *idx_out = -1;
        return V4P_ST_OK;
    }

    idx = mock_find(key);
    if (idx < 0) {
        return V4P_ST_NOT_FOUND;
    }
    if (want_dir) {
        if ((s_nodes[idx].attr & V4P_ATTR_DIR) == 0u) {
            return V4P_ST_IO_ERR;               /* §11: Pfad ist eine Datei */
        }
    } else {
        if ((s_nodes[idx].attr & V4P_ATTR_DIR) != 0u) {
            return V4P_ST_BAD_ARG;
        }
    }
    *idx_out = idx;
    return V4P_ST_OK;
}

/* ------------------------------------------------------------------ */
/* Befehlsabarbeitung                                                  */
/* ------------------------------------------------------------------ */

static void mock_invalidate_bcache(void)
{
    s_bc.valid = 0;
}

static void mock_dispatch_bulk_read(uint8_t cmd, uint8_t seq,
                                    const uint8_t *pay, uint8_t len)
{
    uint8_t  handle;
    uint16_t block;
    uint32_t offset;
    uint32_t n;
    int      node;
    (void)seq;

    if (len < 3u) {
        mock_answer_bulk(cmd, 0u, 0u, V4P_ST_BAD_ARG, NULL, 0u);
        return;
    }
    handle = pay[0];
    block  = v4p_get_u16le(pay + 1);

    if (!s_mounted) {
        mock_answer_bulk(cmd, handle, block, V4P_ST_NO_SD, NULL, 0u);
        return;
    }
    if (handle >= V4P_MAX_FILE_HANDLES || !s_files[handle].used) {
        mock_answer_bulk(cmd, handle, block, V4P_ST_NO_HANDLE, NULL, 0u);
        return;
    }
    node = s_files[handle].node;

    if (s_bc.valid && s_bc.handle == handle && s_bc.block == block
        && s_bc.chunk == s_chunk) {
        if (s_bc.is_end) {
            mock_answer_bulk(cmd, handle, block, V4P_ST_END, NULL, 0u);
        } else {
            mock_answer_bulk(cmd, handle, block, V4P_ST_OK,
                             s_bc.data, s_bc.len);
        }
        return;
    }

    /* Cache fuellen: die "langsame Arbeit" passiert NACH der Antwort (§8). */
    s_bc.valid  = 1;
    s_bc.handle = handle;
    s_bc.block  = block;
    s_bc.chunk  = s_chunk;
    s_bc.len    = 0u;
    s_bc.is_end = 0;

    offset = (uint32_t)block * (uint32_t)s_chunk;
    if (offset >= s_nodes[node].size) {
        s_bc.is_end = 1;
    } else {
        n = s_nodes[node].size - offset;
        if (n > (uint32_t)s_chunk) {
            n = (uint32_t)s_chunk;
        }
        s_bc.len = (uint16_t)n;
        memcpy(s_bc.data, s_nodes[node].data + offset, (size_t)n);
    }

    mock_answer_bulk(cmd, handle, block, V4P_ST_BUSY, NULL, 0u);
}

static void mock_dispatch(uint8_t cmd, uint8_t seq, const uint8_t *pay,
                          uint8_t len)
{
    int busy = 0;

    v4_mock.last_cmd = (int)cmd;

    /* len-Feld des WRITE-Frames ist laut §4.1 auf 0..27 begrenzt. */
    if (len > (uint8_t)V4P_WRITE_PAYLOAD_MAX) {
        mock_answer_read_empty(cmd, seq, V4P_ST_BAD_ARG);
        return;
    }

    /* Von aussen erzwungenes BUSY (§14.1) */
    if (v4_mock.never_ready) {
        busy = 1;
    } else if (v4_mock.busy_count > 0) {
        v4_mock.busy_count--;
        busy = 1;
    } else if (v4_mock.busy_every > 0
               && (v4_mock.writes % (unsigned long)v4_mock.busy_every) == 0u) {
        busy = 1;
    }

    if (busy) {
        if (cmd == V4P_CMD_FILE_READ) {
            uint8_t  h = (len >= 3u) ? pay[0] : 0u;
            uint16_t b = (len >= 3u) ? v4p_get_u16le(pay + 1) : 0u;
            mock_answer_bulk(cmd, h, b, V4P_ST_BUSY, NULL, 0u);
        } else {
            mock_answer_read_empty(cmd, seq, V4P_ST_BUSY);
        }
        return;
    }

    if (v4_mock.force_status >= 0) {
        uint8_t st = (uint8_t)v4_mock.force_status;
        v4_mock.force_status = -1;
        if (cmd == V4P_CMD_FILE_READ) {
            uint8_t  h = (len >= 3u) ? pay[0] : 0u;
            uint16_t b = (len >= 3u) ? v4p_get_u16le(pay + 1) : 0u;
            mock_answer_bulk(cmd, h, b, st, NULL, 0u);
        } else {
            mock_answer_read_empty(cmd, seq, st);
        }
        return;
    }

    switch (cmd) {
    case V4P_CMD_PING:
    case V4P_CMD_GET_INFO: {
        uint8_t    pl[V4P_READ_PAYLOAD_MAX];
        v4p_info_t in;
        mock_fill_info(&in);
        mock_answer_read(cmd, seq, V4P_ST_OK, 0u, pl, (uint8_t)v4p_enc_get_info(pl, &in));
        break;
    }

    case V4P_CMD_GET_STATUS: {
        uint8_t      pl[V4P_READ_PAYLOAD_MAX];
        v4p_status_t st;

        if (s_state == V4P_STATE_CONNECTING && s_connect_left > 0) {
            s_connect_left--;
            if (s_connect_left == 0) {
                s_state = V4P_STATE_CONNECTED;
            }
        }
        mock_fill_status(&st);
        mock_answer_read(cmd, seq, V4P_ST_OK, 0u, pl,
                         (uint8_t)v4p_enc_get_status(pl, &st));
        break;
    }

    case V4P_CMD_SCAN_START: {
        if (len < 1u) {
            mock_answer_read_empty(cmd, seq, V4P_ST_BAD_ARG);
            break;
        }
        if (s_state == V4P_STATE_CONNECTING) {
            mock_answer_read_empty(cmd, seq, V4P_ST_BAD_STATE);
            break;
        }
        s_scan_active = 1;
        s_scan_gen++;                       /* Liste kann sich geaendert haben */
        mock_answer_read_empty(cmd, seq, V4P_ST_OK);
        break;
    }

    case V4P_CMD_SCAN_STOP:
        s_scan_active = 0;
        mock_answer_read_empty(cmd, seq, V4P_ST_OK);
        break;

    case V4P_CMD_DEV_COUNT: {
        uint8_t pl[1];
        pl[0] = (uint8_t)s_dev_count;
        mock_answer_read(cmd, seq, V4P_ST_OK, 0u, pl, 1u);
        break;
    }

    case V4P_CMD_DEV_GET: {
        uint8_t    pl[V4P_READ_PAYLOAD_MAX];
        v4p_dev_t  d;
        uint8_t    flags = 0u;
        size_t     nlen;
        int        i;

        if (len < 1u || pay[0] >= (uint8_t)s_dev_count) {
            mock_answer_read_empty(cmd, seq, V4P_ST_NOT_FOUND);
            break;
        }
        memset(&d, 0, sizeof(d));
        i = (int)pay[0];
        d.idx = (uint8_t)i;
        for (int k = 0; k < 6; k++) {
            d.bda[k] = s_devbda[i][k];
        }
        nlen = strlen(s_devname[i]);
        if (nlen > V4P_DEVNAME_MAX) {
            flags |= V4P_FLAG_NAME_TRUNCATED;
            nlen = V4P_DEVNAME_MAX;
        }
        d.name_len = (uint8_t)nlen;
        memcpy(d.name, s_devname[i], nlen);
        d.name[nlen] = '\0';
        mock_answer_read(cmd, seq, V4P_ST_OK, flags, pl,
                         (uint8_t)v4p_enc_dev(pl, &d));
        break;
    }

    case V4P_CMD_CONNECT: {
        int i;

        if (len < 1u || pay[0] >= (uint8_t)s_dev_count) {
            mock_answer_read_empty(cmd, seq, V4P_ST_NOT_FOUND);
            break;
        }
        i = (int)pay[0];
        s_conn_index  = (uint8_t)i;
        s_scan_active = 0;                  /* Inquiry stört den Aufbau */
        if (v4_mock.connect_rounds > 0) {
            s_state        = V4P_STATE_CONNECTING;
            s_connect_left = v4_mock.connect_rounds;
        } else {
            s_state = V4P_STATE_CONNECTED;
        }
        mock_answer_read_empty(cmd, seq, V4P_ST_OK);
        break;
    }

    case V4P_CMD_CONNECT_BDA: {
        int i;
        int hit = -1;

        if (len < 6u) {
            mock_answer_read_empty(cmd, seq, V4P_ST_BAD_ARG);
            break;
        }
        for (i = 0; i < s_dev_count; i++) {
            if (memcmp(s_devbda[i], pay, 6u) == 0) {
                hit = i;
                break;
            }
        }
        if (hit < 0) {
            mock_answer_read_empty(cmd, seq, V4P_ST_BT_ERR);
            break;
        }
        s_conn_index = (uint8_t)hit;
        s_state      = V4P_STATE_CONNECTED;
        mock_answer_read_empty(cmd, seq, V4P_ST_OK);
        break;
    }

    case V4P_CMD_DISCONNECT:
        if (s_state != V4P_STATE_CONNECTED && s_state != V4P_STATE_CONNECTING) {
            mock_answer_read_empty(cmd, seq, V4P_ST_BAD_STATE);
            break;
        }
        s_state      = V4P_STATE_IDLE;
        s_conn_index = 0xFFu;
        mock_answer_read_empty(cmd, seq, V4P_ST_OK);
        break;

    case V4P_CMD_FORGET:
        s_state      = V4P_STATE_IDLE;
        s_conn_index = 0xFFu;
        mock_answer_read_empty(cmd, seq, V4P_ST_OK);
        break;

    case V4P_CMD_SD_MOUNT: {
        int force = (len >= 1u && pay[0] != 0u);

        if (force) {
            /* §8.1: force=1 quittiert mit BUSY, der Mount laeuft NACH der
             * Antwort. Fertig ist er erst, wenn der Master mindestens
             * V4_MOUNT_WAIT_US gewartet hat -- sonst waere die
             * vorgeschriebene Wartezeit im Mock unsichtbar und ein Master
             * ganz ohne Warten haette hier bestanden. */
            if (s_mount_pending
                && (v4_mock.delay_us - s_mount_at)
                       >= (unsigned long)V4_MOUNT_WAIT_US) {
                s_mounted       = v4_mock.sd_present ? 1 : 0;
                s_free_kb       = v4_mock.sd_present ? s_free_kb : 0u;
                s_mount_pending = 0;
                mock_answer_read_empty(cmd, seq, V4P_ST_OK);
            } else {
                s_mount_pending = 1;
                s_mount_at      = v4_mock.delay_us;
                mock_answer_read_empty(cmd, seq, V4P_ST_BUSY);
            }
        } else {
            /* Ohne force wird die Karte nicht angefasst: nur das letzte
             * Ergebnis. Laeuft noch ein Mount, wird gewartet (§8.1). */
            if (s_mount_pending) {
                if ((v4_mock.delay_us - s_mount_at)
                        >= (unsigned long)V4_MOUNT_WAIT_US) {
                    s_mounted       = v4_mock.sd_present ? 1 : 0;
                    s_free_kb       = v4_mock.sd_present ? s_free_kb : 0u;
                    s_mount_pending = 0;
                } else {
                    mock_answer_read_empty(cmd, seq, V4P_ST_BUSY);
                    break;
                }
            }
            mock_answer_read_empty(cmd, seq,
                                   s_mounted ? V4P_ST_OK : V4P_ST_NO_SD);
        }
        break;
    }

    case V4P_CMD_SD_INFO: {
        uint8_t       pl[V4P_READ_PAYLOAD_MAX];
        v4p_sdinfo_t  si;

        if (!s_mounted) {
            mock_answer_read_empty(cmd, seq, V4P_ST_NO_SD);
            break;
        }
        memset(&si, 0, sizeof(si));
        si.total_kb    = s_total_kb;
        si.free_kb     = s_free_kb;
        si.sector_size = 512u;
        si.fat_type    = 0u;                /* liefert die IDF-VFS nicht */
        mock_answer_read(cmd, seq, V4P_ST_OK, 0u, pl,
                         (uint8_t)v4p_enc_sd_info(pl, &si));
        break;
    }

    case V4P_CMD_SET_CHUNK: {
        uint16_t c;
        uint8_t  pl[2];

        if (len < 2u) {
            mock_answer_read_empty(cmd, seq, V4P_ST_BAD_ARG);
            break;
        }
        c = v4p_get_u16le(pay);
        if (c < V4P_CHUNK_MIN || c > V4P_CHUNK_MAX) {
            mock_answer_read_empty(cmd, seq, V4P_ST_BAD_ARG);
            break;
        }
        s_chunk = c;
        mock_invalidate_bcache();           /* §9 */
        v4p_put_u16le(pl, c);
        mock_answer_read(cmd, seq, V4P_ST_OK, 0u, pl, 2u);
        break;
    }

    case V4P_CMD_PATH_CLEAR:
        s_path_len = 0u;
        s_path[0]  = '\0';
        mock_answer_read_empty(cmd, seq, V4P_ST_OK);
        break;

    case V4P_CMD_PATH_APPEND:
        if ((size_t)s_path_len + (size_t)len > (size_t)V4P_PATH_MAX) {
            /* Puffer bleibt unveraendert (§11) */
            mock_answer_read_empty(cmd, seq, V4P_ST_TOO_LONG);
            break;
        }
        memcpy(s_path + s_path_len, pay, len);
        s_path_len = (uint16_t)(s_path_len + len);
        s_path[s_path_len] = '\0';
        mock_answer_read_empty(cmd, seq, V4P_ST_OK);
        break;

    case V4P_CMD_DIR_OPEN: {
        int      idx = -2;
        uint8_t  st;
        int      h;
        uint8_t  pl[1];

        st = mock_lookup(pay, len, 1, &idx);
        if (st != V4P_ST_OK) {
            mock_answer_read_empty(cmd, seq, st);
            break;
        }
        for (h = 0; h < (int)V4P_MAX_DIR_HANDLES; h++) {
            if (!s_dirs[h].used) {
                break;
            }
        }
        if (h >= (int)V4P_MAX_DIR_HANDLES) {
            mock_answer_read_empty(cmd, seq, V4P_ST_NO_HANDLE);
            break;
        }
        s_dirs[h].used        = 1;
        s_dirs[h].node        = idx;
        s_dirs[h].cache_valid = 0;
        s_dirs[h].cache_node  = -1;
        s_dirs[h].cache_index = 0u;
        pl[0] = (uint8_t)h;
        mock_answer_read(cmd, seq, V4P_ST_OK, 0u, pl, 1u);
        break;
    }

    case V4P_CMD_DIR_NEXT: {
        uint8_t      h;
        uint16_t     index;
        int          deliver = -1;
        int          status  = V4P_ST_OK;
        const char  *dirkey;
        uint8_t      pl[V4P_READ_PAYLOAD_MAX];
        uint8_t      flags = 0u;
        v4p_dirent_t e;
        size_t       nlen;

        if (len < 3u) {
            mock_answer_read_empty(cmd, seq, V4P_ST_BAD_ARG);
            break;
        }
        h     = pay[0];
        index = v4p_get_u16le(pay + 1);

        if (h >= V4P_MAX_DIR_HANDLES || !s_dirs[h].used) {
            mock_answer_read_empty(cmd, seq, V4P_ST_NO_HANDLE);
            break;
        }
        dirkey = (s_dirs[h].node < 0) ? "" : s_nodes[s_dirs[h].node].key;

        if (!s_dirs[h].cache_valid) {
            if (index != 0u) {
                status = V4P_ST_BAD_ARG;    /* Cache ungueltig, index != 0 */
            } else if (mock_child_at(dirkey, 0, &deliver) != 0) {
                status = V4P_ST_END;
            } else {
                s_dirs[h].cache_valid = 1;
                s_dirs[h].cache_index = 0u;
                s_dirs[h].cache_node  = deliver;
            }
        } else if (index == s_dirs[h].cache_index) {
            deliver = s_dirs[h].cache_node; /* Retry: derselbe Eintrag (§7.2) */
        } else if (index == (uint16_t)(s_dirs[h].cache_index + 1u)) {
            if (mock_child_at(dirkey, (int)index, &deliver) != 0) {
                s_dirs[h].cache_valid = 0;  /* nach END ist der Cache ungueltig */
                status = V4P_ST_END;
            } else {
                s_dirs[h].cache_index = index;
                s_dirs[h].cache_node  = deliver;
            }
        } else {
            status = V4P_ST_BAD_ARG;        /* Index ausser Takt */
        }

        if (status != V4P_ST_OK) {
            mock_answer_read_empty(cmd, seq, (uint8_t)status);
            break;
        }

        memset(&e, 0, sizeof(e));
        e.index = index;
        e.attr  = s_nodes[deliver].attr;
        e.size  = ((s_nodes[deliver].attr & V4P_ATTR_DIR) != 0u)
                  ? 0u : s_nodes[deliver].size;
        nlen = strlen(s_nodes[deliver].name);
        if (nlen > V4P_DIRNAME_MAX) {
            flags |= V4P_FLAG_NAME_TRUNCATED;
            nlen = V4P_DIRNAME_MAX;
        }
        e.name_len = (uint8_t)nlen;
        memcpy(e.name, s_nodes[deliver].name, nlen);
        e.name[nlen] = '\0';
        mock_answer_read(cmd, seq, V4P_ST_OK, flags, pl,
                         (uint8_t)v4p_enc_dirent(pl, &e));
        break;
    }

    case V4P_CMD_DIR_CLOSE: {
        uint8_t h;

        if (len < 1u) {
            mock_answer_read_empty(cmd, seq, V4P_ST_BAD_ARG);
            break;
        }
        h = pay[0];
        if (h < V4P_MAX_DIR_HANDLES) {
            s_dirs[h].used        = 0;
            s_dirs[h].cache_valid = 0;
        }
        mock_answer_read_empty(cmd, seq, V4P_ST_OK);   /* immer OK */
        break;
    }

    case V4P_CMD_FILE_OPEN: {
        int      idx = -2;
        uint8_t  st;
        int      h;
        uint8_t  pl[6];

        st = mock_lookup(pay, len, 0, &idx);
        if (st != V4P_ST_OK) {
            mock_answer_read_empty(cmd, seq, st);
            break;
        }
        for (h = 0; h < (int)V4P_MAX_FILE_HANDLES; h++) {
            if (!s_files[h].used) {
                break;
            }
        }
        if (h >= (int)V4P_MAX_FILE_HANDLES) {
            mock_answer_read_empty(cmd, seq, V4P_ST_NO_HANDLE);
            break;
        }
        s_files[h].used = 1;
        s_files[h].node = idx;
        mock_invalidate_bcache();           /* §8.2 */
        (void)v4p_enc_file_open(pl, (uint8_t)h, s_nodes[idx].size,
                                s_nodes[idx].attr);
        mock_answer_read(cmd, seq, V4P_ST_OK, 0u, pl, 6u);
        break;
    }

    case V4P_CMD_FILE_READ:
        mock_dispatch_bulk_read(cmd, seq, pay, len);
        break;

    case V4P_CMD_FILE_CLOSE: {
        uint8_t h;

        if (len < 1u) {
            mock_answer_read_empty(cmd, seq, V4P_ST_BAD_ARG);
            break;
        }
        h = pay[0];
        if (h < V4P_MAX_FILE_HANDLES) {
            s_files[h].used = 0;
        }
        mock_invalidate_bcache();           /* §8.2 */
        mock_answer_read_empty(cmd, seq, V4P_ST_OK);
        break;
    }

    case V4P_CMD_PLAY_FILE: {
        /* §11: Datei von der SD-Karte selbst abspielen. Erst BUSY (Umbau der
         * Audio-Pipeline), dann OK. Derselbe Pfad ist idempotent. */
        int     idx = -2;
        uint8_t st;

        st = mock_lookup(pay, len, 0, &idx);   /* Datei, kein Verzeichnis */
        if (st != V4P_ST_OK) {
            mock_answer_read_empty(cmd, seq, st);   /* NO_SD/NOT_FOUND/BAD_ARG/IO_ERR */
            break;
        }
        if (s_play_state == 1 && s_play_pending == 0
            && strcmp(s_nodes[idx].key, s_play_path) == 0) {
            mock_answer_read_empty(cmd, seq, V4P_ST_OK);   /* laeuft schon */
            break;
        }
        if (!s_play_pending || strcmp(s_nodes[idx].key, s_play_path) != 0) {
            strcpy(s_play_path, s_nodes[idx].key);
            s_play_pending = 1;
            s_play_left    = v4_mock.play_rounds;
        }
        if (v4_mock.play_fail) {
            /* §8.3: kein Endlos-BUSY, und die Firmware schaltet die Quelle
             * selbst auf den I2S-Eingang zurueck. */
            s_play_pending = 0;
            s_play_state   = 0;
            s_play_path[0] = '\0';
            mock_answer_read_empty(cmd, seq, V4P_ST_IO_ERR);
            break;
        }
        if (s_play_left > 0) {
            s_play_left--;
            mock_answer_read_empty(cmd, seq, V4P_ST_BUSY);
            break;
        }
        s_play_pending = 0;
        s_play_state   = 1;
        mock_answer_read_empty(cmd, seq, V4P_ST_OK);
        break;
    }

    case V4P_CMD_STOP_PLAY:
        /* §11: zurueck auf den I2S-Eingang. Idempotent: laeuft schon der
         * Eingang, kommt sofort OK. */
        if (s_play_state == 0 && s_play_pending == 0 && s_play_left == 0) {
            s_play_pending = 0;
            mock_answer_read_empty(cmd, seq, V4P_ST_OK);
            break;
        }
        if (!s_play_pending) {
            s_play_pending = 1;
            s_play_left    = v4_mock.play_rounds;
        }
        if (s_play_left > 0) {
            s_play_left--;
            mock_answer_read_empty(cmd, seq, V4P_ST_BUSY);
            break;
        }
        s_play_pending = 0;
        s_play_state   = 0;
        s_play_path[0] = '\0';
        mock_answer_read_empty(cmd, seq, V4P_ST_OK);
        break;

    case V4P_CMD_RESET:
        s_path_len = 0u;
        s_path[0]  = '\0';
        mock_invalidate_bcache();
        mock_answer_read_empty(cmd, seq, V4P_ST_OK);
        break;

    default:
        mock_answer_read_empty(cmd, seq, V4P_ST_BAD_CMD);
        break;
    }
}

/* ------------------------------------------------------------------ */
/* Plattformhaken (Slave-Seite)                                        */
/* ------------------------------------------------------------------ */

int v4_plat_open(const char *dev)
{
    (void)dev;
    return 0;
}

void v4_plat_close(void)
{
}

int v4_plat_i2c_write(uint8_t addr7, const uint8_t *data, uint16_t len)
{
    uint8_t cmd;
    uint8_t seq;

    if (v4_mock.fail_write) {
        return -1;
    }
    if (addr7 != V4_I2C_ADDR7 || len != (uint16_t)V4P_WRITE_FRAME_LEN) {
        v4_mock.bad_write_len++;
        return -1;
    }

    v4_mock.writes++;

    /* SEQ der abgesetzten Frames protokollieren: damit laesst sich pruefen,
     * dass Wiederholungen dieselbe SEQ benutzen (§6, Punkt 1). */
    if (v4_mock.seq_log_n < (int)sizeof(v4_mock.seq_log)) {
        v4_mock.seq_log[v4_mock.seq_log_n] = data[2];
        v4_mock.seq_log_n++;
    }

    /* Selbstheilung (§6): beim Senden einer Antwort wird der TX-FIFO
     * zurueckgesetzt, wenn noch ungelesene Daten darin liegen. */
    s_ans_valid = 0;

    if (data[0] != V4P_MAGIC_WRITE) {
        return 0;                   /* Slave erkennt den Frame nicht */
    }
    seq = data[2];
    cmd = data[1];

    if (data[31] != v4p_crc8(data, 31u)) {
        mock_answer_read_empty(cmd, seq, V4P_ST_BAD_CRC);
        return 0;
    }

    mock_dispatch(cmd, seq, data + 4, data[3]);
    return 0;
}

int v4_plat_i2c_read(uint8_t addr7, uint8_t *data, uint16_t len)
{
    size_t i;

    v4_mock.reads++;

    if (addr7 != V4_I2C_ADDR7) {
        return -1;
    }
    if (v4_mock.fail_read) {
        s_ans_valid = 0;
        return -1;
    }
    if (v4_mock.drop_next_read) {
        v4_mock.drop_next_read = 0;
        s_ans_valid = 0;
        return -1;                  /* genau diese Antwort geht verloren */
    }
    if (v4_mock.partial_read_once) {
        size_t half;

        v4_mock.partial_read_once = 0;
        half = (size_t)len / 2u;
        for (i = 0; i < half; i++) {
            data[i] = s_ans[i];
        }
        for (; i < len; i++) {
            data[i] = 0xFFu;        /* Rest fehlt */
        }
        /* s_ans_valid bleibt 1: die Antwort ist nur halb konsumiert. Genau
         * diese Lage loest der ESP32 beim naechsten Senden per FIFO-Reset
         * auf (§6, Selbstheilung). */
        return 0;
    }
    if (v4_mock.drop_read_every > 0
        && (v4_mock.reads % (unsigned long)v4_mock.drop_read_every) == 0u) {
        s_ans_valid = 0;
        return -1;                  /* Antwort verloren */
    }

    if (!s_ans_valid) {
        /* Ohne Befehl hat der Slave nichts: der Bus liefert 0xFF (§1.3). */
        for (i = 0; i < len; i++) {
            data[i] = 0xFFu;
        }
        return 0;
    }

    if ((size_t)len != s_ans_len) {
        /* Falsche Laenge verlangt -- hier gibt es keine guten Daten. */
        s_ans_valid = 0;
        return -1;
    }

    for (i = 0; i < len; i++) {
        data[i] = s_ans[i];
    }
    s_ans_valid = 0;                /* R1: genau eine Antwort pro Befehl */
    return 0;
}

void v4_plat_delay_us(uint32_t us)
{
    v4_mock.delay_calls++;
    v4_mock.delay_us += (unsigned long)us;
}

unsigned long v4_plat_last_error(void)
{
    /* Der Mock scheitert nur auf Anforderung; ein "Fehlercode" im Sinne der
     * i2c.library existiert hier nicht. */
    return 0ul;
}

const char *v4_plat_error_text(void)
{
    return "Mock";
}

/* Der Mock schreibt die serielle Ausgabe in eine Datei -- damit laesst sich
 * die Spiegelung im Smoketest pruefen (genau wie am echten UART). */
static FILE *s_ser = NULL;

int v4_plat_serial_open(const char *dev)
{
    if (dev == NULL) {
        return -1;              /* kein Standardgeraet im Mock */
    }
    s_ser = fopen(dev, "a");
    if (s_ser == NULL) {
        return -1;
    }
    setvbuf(s_ser, NULL, _IONBF, 0);
    return 0;
}

long v4_plat_serial_write(const char *s, unsigned long len)
{
    size_t n;

    if (s_ser == NULL || len == 0ul) {
        return 0;
    }
    n = fwrite(s, 1u, (size_t)len, s_ser);
    fflush(s_ser);
    return (long)n;
}

void v4_plat_serial_close(void)
{
    if (s_ser != NULL) {
        fclose(s_ser);
        s_ser = NULL;
    }
}

void v4_mock_bus_reset(void)
{
    s_ans_valid = 0;
    s_ans_len   = 0u;
}

/* ------------------------------------------------------------------ */
/* Aufbau                                                             */
/* ------------------------------------------------------------------ */

void v4_mock_reset(void)
{
    int i;

    memset(&v4_mock, 0, sizeof(v4_mock));
    v4_mock.force_status = -1;
    v4_mock.sd_present   = 1;
    v4_mock.sd_card_present = 1;
    v4_mock.play_rounds  = 1;   /* §11: erst BUSY, dann OK */
    v4_mock.last_cmd     = -1;

    for (i = 0; i < s_node_count; i++) {
        if (s_nodes[i].data != NULL) {
            free(s_nodes[i].data);
        }
        memset(&s_nodes[i], 0, sizeof(s_nodes[i]));
    }
    s_node_count = 0;
    memset(s_dirs, 0, sizeof(s_dirs));
    memset(s_files, 0, sizeof(s_files));
    memset(&s_bc, 0, sizeof(s_bc));
    memset(s_devname, 0, sizeof(s_devname));
    memset(s_devbda, 0, sizeof(s_devbda));
    s_dev_count = 0;

    s_chunk       = (uint16_t)V4P_CHUNK_DEFAULT;
    s_mounted     = 1;
    s_total_kb    = 8000000u;
    s_free_kb     = 491520u;
    s_state       = V4P_STATE_IDLE;
    s_conn_index  = 0xFFu;
    s_scan_active = 0u;
    s_audio_flags = 0u;
    s_card_present  = 1;
    s_play_state    = 0;
    s_play_pending  = 0;
    s_play_left     = 0;
    s_play_path[0]  = '\0';
    s_scan_gen    = 0u;
    s_connect_left= 0;
    s_path_len    = 0u;
    s_path[0]     = '\0';
    s_ans_valid     = 0;
    s_ans_len       = 0u;
    s_answer_no     = 0u;
    s_mount_pending = 0;
    s_mount_at      = 0ul;
}

int v4_mock_add_dir(const char *path)
{
    char key[V4P_PATH_MAX + 1u];

    if (path == NULL || s_node_count >= V4_MOCK_MAX_NODES) {
        return -1;
    }
    if (mock_has_dotdot(path)) {
        return -1;
    }
    if (mock_norm(path, key, sizeof(key)) != 0 || key[0] == '\0') {
        return -1;
    }
    if (mock_find(key) >= 0) {
        return -1;
    }

    {
        mock_node_t *n = &s_nodes[s_node_count];
        size_t i;
        size_t last = 0;
        size_t k;

        memset(n, 0, sizeof(*n));
        memcpy(n->key, key, strlen(key) + 1u);
        mock_parent_of(key, n->parent, sizeof(n->parent));
        for (i = 0; key[i] != '\0'; i++) {
            if (key[i] == '/') {
                last = i + 1u;
            }
        }
        k = strlen(key + last);
        if (k >= sizeof(n->name)) {
            k = sizeof(n->name) - 1u;
        }
        memcpy(n->name, key + last, k);
        n->name[k] = '\0';
        n->attr = V4P_ATTR_DIR;
        n->data = NULL;
        n->size = 0u;
    }
    s_node_count++;
    return s_node_count - 1;
}

int v4_mock_add_file(const char *path, const uint8_t *data, uint32_t size)
{
    char key[V4P_PATH_MAX + 1u];
    const char *slash;

    if (path == NULL || data == NULL || s_node_count >= V4_MOCK_MAX_NODES) {
        return -1;
    }
    if (mock_has_dotdot(path)) {
        return -1;
    }
    if (mock_norm(path, key, sizeof(key)) != 0 || key[0] == '\0') {
        return -1;
    }
    if (mock_find(key) >= 0) {
        return -1;
    }

    {
        mock_node_t *n = &s_nodes[s_node_count];
        size_t i;
        size_t last = 0;
        size_t k;

        memset(n, 0, sizeof(*n));
        memcpy(n->key, key, strlen(key) + 1u);
        mock_parent_of(key, n->parent, sizeof(n->parent));
        for (i = 0; key[i] != '\0'; i++) {
            if (key[i] == '/') {
                last = i + 1u;
            }
        }
        /* Der letzte Pfadteil kann bis 128 Byte lang sein -- klemmen, sonst
         * laeuft die Ablage (V4_MOCK_NAME_STORE) ueber. */
        k = strlen(key + last);
        if (k >= sizeof(n->name)) {
            k = sizeof(n->name) - 1u;
        }
        memcpy(n->name, key + last, k);
        n->name[k] = '\0';

        /* Anzeigename in Originalschreibweise: letzten Pfadteil aus `path`
         * uebernehmen, damit §10 (Originalschreibweise aus dem Eintrag)
         * nachvollziehbar bleibt. */
        slash = strrchr(path, '/');
        if (slash != NULL && slash[1] != '\0') {
            size_t k = strlen(slash + 1);
            if (k >= sizeof(n->name)) {
                k = sizeof(n->name) - 1u;
            }
            memcpy(n->name, slash + 1, k);
            n->name[k] = '\0';
        }

        n->attr = V4P_ATTR_FILE;
        n->size = size;
        n->data = (uint8_t *)malloc(size == 0u ? 1u : (size_t)size);
        if (n->data == NULL) {
            memset(n, 0, sizeof(*n));
            return -1;
        }
        memcpy(n->data, data, (size_t)size);
    }
    s_node_count++;
    return s_node_count - 1;
}

int v4_mock_dev_add(const char *name, const uint8_t bda[6])
{
    size_t n;

    if (name == NULL || bda == NULL || s_dev_count >= V4_MOCK_MAX_DEVICES) {
        return -1;
    }
    n = strlen(name);
    if (n >= (size_t)V4_MOCK_NAME_STORE) {
        n = (size_t)V4_MOCK_NAME_STORE - 1u;
    }
    memcpy(s_devname[s_dev_count], name, n);
    s_devname[s_dev_count][n] = '\0';
    memcpy(s_devbda[s_dev_count], bda, 6u);
    s_dev_count++;
    return s_dev_count - 1;
}

void v4_mock_set_sd(int mounted, uint32_t total_kb, uint32_t free_kb)
{
    s_mounted  = mounted ? 1 : 0;
    s_total_kb = total_kb;
    s_free_kb  = free_kb;
}

void v4_mock_set_audio(int streaming)
{
    if (streaming) {
        s_audio_flags |= V4P_AUDIO_A2DP_STREAMING;
    } else {
        s_audio_flags = (uint8_t)(s_audio_flags & ~V4P_AUDIO_A2DP_STREAMING);
    }
}

void v4_mock_set_sd_playback(int playing)
{
    if (playing) {
        s_audio_flags |= V4P_AUDIO_SD_PLAYBACK;
    } else {
        s_audio_flags = (uint8_t)(s_audio_flags & ~V4P_AUDIO_SD_PLAYBACK);
    }
}

void v4_mock_end_playback(void)
{
    s_play_state   = 0;
    s_play_pending = 0;
    s_play_left    = 0;
    s_play_path[0] = '\0';
    s_audio_flags  = (uint8_t)(s_audio_flags & ~V4P_AUDIO_SD_PLAYBACK);
}

void v4_mock_bump_scan_gen(void)
{
    s_scan_gen++;
}

/* ------------------------------------------------------------------ */
/* Standardszenario (§13)                                              */
/* ------------------------------------------------------------------ */

static uint8_t s_seed_pat[65536];

void v4_mock_seed_default(void)
{
    static const uint8_t bda1[6] = { 0x11u, 0x22u, 0x33u, 0x44u, 0x55u, 0x66u };
    static const uint8_t bda2[6] = { 0xAAu, 0xBBu, 0xCCu, 0xDDu, 0xEEu, 0xFFu };
    static const uint8_t bda3[6] = { 0x0Au, 0x0Bu, 0x0Cu, 0x0Du, 0x0Eu, 0x0Fu };
    uint8_t  txt[12];
    uint32_t i;

    for (i = 0; i < sizeof(s_seed_pat); i++) {
        s_seed_pat[i] = (uint8_t)(i & 0xFFu);
    }
    for (i = 0; i < sizeof(txt); i++) {
        txt[i] = (uint8_t)('a' + (i % 26u));
    }

    (void)v4_mock_dev_add("JBL Flip 5", bda1);
    (void)v4_mock_dev_add("Sony WH-1000XM4", bda2);
    (void)v4_mock_dev_add("ESP32-A2DP-Test", bda3);

    (void)v4_mock_add_dir("MUSIC");
    (void)v4_mock_add_dir("EMPTY");
    (void)v4_mock_add_file("MUSIC/A.MP3", s_seed_pat, 1000u);
    (void)v4_mock_add_file("MUSIC/B.MP3", s_seed_pat, 37u);       /* kurz */
    (void)v4_mock_add_file("MUSIC/BIG.BIN", s_seed_pat, 65536u);  /* 2 Bloecke @1024 */
    (void)v4_mock_add_file("TXT.TXT", txt, 12u);
}
