/*
 * v4_master.c -- I2C-Master der Vampire V4 fuer den ESP32-Slave.
 * Implementierung von PROTOCOL_V4_MASTER.md §6 (Zustandsmaschine) und §11.
 *
 * Portabilitaet: Frame-Bytes werden ausschliesslich ueber v4p_put_*_le /
 * v4p_get_*_le beruehrt (§0/§2). Kein memcpy, kein Cast von Structs auf
 * Frame-Bytes, keine Bitfelder.
 */

#include "v4_master.h"

#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/* Trace                                                               */
/* ------------------------------------------------------------------ */

static void v4_trace(const v4_master_t *m, uint8_t event, uint8_t cmd,
                     uint8_t seq, uint8_t status, uint16_t len, int rc,
                     uint32_t us, const uint8_t *data)
{
    v4_trace_t ev;

    if (m == NULL || m->trace == NULL) {
        return;
    }
    ev.event  = event;
    ev.cmd    = cmd;
    ev.seq    = seq;
    ev.status = status;
    ev.len    = len;
    ev.rc     = rc;
    ev.us     = us;
    ev.data   = data;
    m->trace(&ev, m->trace_ctx);
}

/* ------------------------------------------------------------------ */
/* Initialisierung                                                     */
/* ------------------------------------------------------------------ */

void v4_init(v4_master_t *m)
{
    if (m == NULL) {
        return;
    }
    memset(m, 0, sizeof(*m));
    m->seq        = 0u;
    m->try_max    = V4_RETRIES;     /* Versuche bei Framingfehlern */
    m->t_wait_us  = V4_T_WAIT_US;   /* R3: Wartezeit vor dem Lesen */
    m->chunk      = V4P_CHUNK_DEFAULT;
    m->path_valid = 1u;     /* leerer Spiegel ist gueltig: L=0 = Wurzel */
}

int v4_open(v4_master_t *m, const char *dev)
{
    v4_init(m);
    return v4_plat_open(dev);
}

void v4_close(void)
{
    v4_plat_close();
}

/* ------------------------------------------------------------------ */
/* Transaktionskern -- §6                                              */
/* ------------------------------------------------------------------ */

uint8_t v4_transact_n(v4_master_t *m, uint8_t cmd,
                      const uint8_t *payload, uint8_t len, v4p_read_t *out,
                      unsigned flags, unsigned busy_tries)
{
    uint8_t wf[V4P_WRITE_FRAME_LEN];
    uint8_t raw[V4P_READ_FRAME_LEN];
    int     busy_left    = (int)busy_tries;
    int     badcrc_left  = V4_BADCRC_TRIES;

    if (m == NULL || out == NULL) {
        return V4_ERR_ARG;
    }
    if (len > V4P_WRITE_PAYLOAD_MAX) {
        return V4_ERR_ARG;
    }
    if (len > 0u && payload == NULL) {
        return V4_ERR_ARG;
    }

    for (;;) {
        int attempt;
        int got = 0;

        /* Wiederholungen bei Link- und Framing-Fehlern: SEQ bleibt stehen. */
        for (attempt = 0; (unsigned)attempt < m->try_max; attempt++) {
            int rc;

            if (v4p_build_write(wf, cmd, m->seq, payload, len) != 0) {
                return V4_ERR_ARG;
            }

            m->tx_frames++;
            v4_trace(m, V4_TR_TX_BEGIN, cmd, m->seq, 0u,
                     (uint16_t)V4P_WRITE_FRAME_LEN, 0, 0u, wf);
            {
                int wrc = v4_plat_i2c_write(V4_I2C_ADDR7, wf,
                                            V4P_WRITE_FRAME_LEN);
                v4_trace(m, V4_TR_TX_END, cmd, m->seq, 0u,
                         (uint16_t)V4P_WRITE_FRAME_LEN, wrc, 0u, NULL);
                if (wrc != 0) {
                    m->retries++;
                    v4_trace(m, V4_TR_RETRY, cmd, m->seq, 0u, 0u, wrc, 0u,
                             NULL);
                    continue;
                }
            }

            /* R3: vor jedem Lesen warten -- der Slave kann nicht stretchen. */
            v4_trace(m, V4_TR_WAIT, cmd, m->seq, 0u, 0u, 0, m->t_wait_us, NULL);
            v4_plat_delay_us(m->t_wait_us);

            v4_trace(m, V4_TR_RX_BEGIN, cmd, m->seq, 0u,
                     (uint16_t)V4P_READ_FRAME_LEN, 0, 0u, NULL);
            {
                int rrc = v4_plat_i2c_read(V4_I2C_ADDR7, raw,
                                           V4P_READ_FRAME_LEN);
                v4_trace(m, V4_TR_RX_END, cmd, m->seq, 0u,
                         (uint16_t)V4P_READ_FRAME_LEN, rrc, 0u, raw);
                if (rrc != 0) {
                    m->retries++;
                    /* Das Schreiben war erfolgreich: der Slave kann den Befehl
                     * schon ausgefuehrt haben. Fuer nicht-idempotente Befehle
                     * zaehlt das als unsichere Wiederholung. */
                    m->unsafe_retries++;
                    v4_trace(m, V4_TR_RETRY, cmd, m->seq, 0u, 0u, rrc, 0u,
                             NULL);
                    continue;
                }
            }
            m->rx_frames++;

            rc = v4p_check_read(raw, (size_t)V4P_READ_FRAME_LEN, cmd, m->seq,
                                out);
            v4_trace(m, V4_TR_CHECK, cmd, m->seq,
                     (uint8_t)((rc == V4P_CHECK_OK) ? out->status : 0u),
                     (uint16_t)((rc == V4P_CHECK_OK) ? out->len : 0u), rc, 0u,
                     raw);
            if (rc != V4P_CHECK_OK) {
                m->last_check = rc;
                m->retries++;
                m->unsafe_retries++;
                v4_trace(m, V4_TR_RETRY, cmd, m->seq, 0u, 0u, rc, 0u, NULL);
                continue;
            }
            got = 1;
            break;
        }

        if (!got) {
            return V4_ERR_LINK;
        }

        if (out->status == V4P_ST_BUSY) {
            m->busy_rounds++;
            v4_trace(m, V4_TR_BUSY, cmd, m->seq, out->status, out->len,
                     V4P_ST_BUSY, 0u, NULL);
            if ((flags & V4_X_BUSY_RETRY) == 0u) {
                /* Aufrufer faehrt die Runden selbst (§8.1). SEQ bleibt. */
                return V4P_ST_BUSY;
            }
            busy_left--;
            if (busy_left <= 0) {
                return V4_ERR_BUSY;
            }
            v4_plat_delay_us(V4_T_BUSY_US);
            continue;               /* GANZER Befehl neu, GLEICHE SEQ */
        }

        if (out->status == V4P_ST_BAD_CRC) {
            /* Der Slave hat unser Framing verworfen: Befehl neu, gleiche SEQ. */
            m->badcrc_rounds++;
            badcrc_left--;
            if (badcrc_left <= 0) {
                return V4P_ST_BAD_CRC;
            }
            v4_plat_delay_us(V4_T_BUSY_US);
            continue;
        }

        /* SEQ wird NUR nach einer gueltigen Antwort erhoeht (§6, Punkt 3). */
        m->seq = (uint8_t)((m->seq + 1u) & 0xFFu);
        v4_trace(m, V4_TR_DONE, cmd, (uint8_t)(m->seq - 1u), out->status,
                 out->len, out->status, 0u, out->payload);
        return out->status;
    }
}

uint8_t v4_transact_ex(v4_master_t *m, uint8_t cmd,
                       const uint8_t *payload, uint8_t len, v4p_read_t *out,
                       unsigned flags)
{
    return v4_transact_n(m, cmd, payload, len, out, flags, V4_BUSY_TRIES);
}

uint8_t v4_transact(v4_master_t *m, uint8_t cmd,
                    const uint8_t *payload, uint8_t len, v4p_read_t *out)
{
    return v4_transact_n(m, cmd, payload, len, out, V4_X_BUSY_RETRY,
                         V4_BUSY_TRIES);
}

uint8_t v4_transact_bulk(v4_master_t *m, uint8_t cmd,
                         const uint8_t *payload, uint8_t len,
                         uint8_t *buf, v4p_bulk_t *out)
{
    uint8_t  wf[V4P_WRITE_FRAME_LEN];
    uint16_t rlen;
    uint8_t  handle;
    uint16_t block;
    int      busy_left   = V4_BUSY_TRIES;
    int      badcrc_left = V4_BADCRC_TRIES;

    if (m == NULL || buf == NULL || out == NULL) {
        return V4_ERR_ARG;
    }
    if (len > V4P_WRITE_PAYLOAD_MAX) {
        return V4_ERR_ARG;
    }
    if (len < 3u || payload == NULL) {
        return V4_ERR_ARG;          /* [0]=handle, [1..2]=block */
    }
    if (m->chunk < V4P_CHUNK_MIN || m->chunk > V4P_CHUNK_MAX) {
        return V4_ERR_ARG;
    }

    handle = payload[0];
    block  = v4p_get_u16le(payload + 1);
    rlen   = (uint16_t)(V4P_BULK_OVERHEAD + m->chunk);

    for (;;) {
        int attempt;
        int got = 0;

        for (attempt = 0; (unsigned)attempt < m->try_max; attempt++) {
            int rc;

            if (v4p_build_write(wf, cmd, m->seq, payload, len) != 0) {
                return V4_ERR_ARG;
            }

            m->tx_frames++;
            v4_trace(m, V4_TR_TX_BEGIN, cmd, m->seq, 0u,
                     (uint16_t)V4P_WRITE_FRAME_LEN, 0, 0u, wf);
            {
                int wrc = v4_plat_i2c_write(V4_I2C_ADDR7, wf,
                                            V4P_WRITE_FRAME_LEN);
                v4_trace(m, V4_TR_TX_END, cmd, m->seq, 0u,
                         (uint16_t)V4P_WRITE_FRAME_LEN, wrc, 0u, NULL);
                if (wrc != 0) {
                    m->retries++;
                    v4_trace(m, V4_TR_RETRY, cmd, m->seq, 0u, 0u, wrc, 0u, NULL);
                    continue;
                }
            }

            v4_trace(m, V4_TR_WAIT, cmd, m->seq, 0u, 0u, 0, m->t_wait_us, NULL);
            v4_plat_delay_us(m->t_wait_us);

            /* R2: konstante Laenge -- auch der letzte, kurze Block ist
             * aufgefuellt, damit nie ueber das Ende gelesen wird. */
            v4_trace(m, V4_TR_RX_BEGIN, cmd, m->seq, 0u, rlen, 0, 0u, NULL);
            {
                int rrc = v4_plat_i2c_read(V4_I2C_ADDR7, buf, rlen);
                v4_trace(m, V4_TR_RX_END, cmd, m->seq, 0u, rlen, rrc, 0u, buf);
                if (rrc != 0) {
                    m->retries++;
                    m->unsafe_retries++;
                    v4_trace(m, V4_TR_RETRY, cmd, m->seq, 0u, 0u, rrc, 0u,
                             NULL);
                    continue;
                }
            }
            m->rx_frames++;

            rc = v4p_check_bulk(buf, (size_t)rlen, m->chunk, cmd, handle,
                                block, out);
            v4_trace(m, V4_TR_CHECK, cmd, m->seq,
                     (uint8_t)((rc == V4P_CHECK_OK) ? out->status : 0u),
                     (uint16_t)((rc == V4P_CHECK_OK) ? out->len : 0u), rc, 0u,
                     buf);
            if (rc != V4P_CHECK_OK) {
                m->last_check = rc;
                m->retries++;
                m->unsafe_retries++;
                v4_trace(m, V4_TR_RETRY, cmd, m->seq, 0u, 0u, rc, 0u, NULL);
                continue;
            }
            got = 1;
            break;
        }

        if (!got) {
            return V4_ERR_LINK;
        }

        if (out->status == V4P_ST_BUSY) {
            m->busy_rounds++;
            v4_trace(m, V4_TR_BUSY, cmd, m->seq, out->status, out->len,
                     V4P_ST_BUSY, 0u, NULL);
            busy_left--;
            if (busy_left <= 0) {
                return V4_ERR_BUSY;
            }
            v4_plat_delay_us(V4_T_BUSY_US);
            continue;
        }

        if (out->status == V4P_ST_BAD_CRC) {
            m->badcrc_rounds++;
            badcrc_left--;
            if (badcrc_left <= 0) {
                return V4P_ST_BAD_CRC;
            }
            v4_plat_delay_us(V4_T_BUSY_US);
            continue;
        }

        m->seq = (uint8_t)((m->seq + 1u) & 0xFFu);
        v4_trace(m, V4_TR_DONE, cmd, (uint8_t)(m->seq - 1u), out->status,
                 out->len, out->status, 0u, buf);
        return out->status;
    }
}

/* ------------------------------------------------------------------ */
/* Hilfen                                                             */
/* ------------------------------------------------------------------ */

/* Befehl ohne Nutzlast und mit leerer OK-Antwort. */
static uint8_t v4_simple(v4_master_t *m, uint8_t cmd)
{
    v4p_read_t r;

    return v4_transact(m, cmd, NULL, 0u, &r);
}

/* Befehl mit Nutzlast (max 27) und leerer OK-Antwort. */
static uint8_t v4_simple_p(v4_master_t *m, uint8_t cmd,
                           const uint8_t *p, uint8_t len)
{
    v4p_read_t r;

    return v4_transact(m, cmd, p, len, &r);
}

/* Nutzlast eines Pfads in den WRITE-Puffer legen. -1 wenn zu lang. */
static int v4_path_payload(const char *path, uint8_t *p, uint8_t *len)
{
    size_t n;
    size_t i;

    if (path == NULL) {
        *len = 0u;                  /* L=0: Slave benutzt seinen PATH_*-Puffer */
        return 0;
    }
    if (path[0] == '\0') {
        /* §10: "" und "/" bedeuten die Wurzel. NICHT als L=0 senden -- das
         * wuerde den PATH_*-Puffer des Slaves oeffnen. */
        p[0] = (uint8_t)'/';
        *len = 1u;
        return 0;
    }
    n = strlen(path);
    if (n > (size_t)V4P_WRITE_PAYLOAD_MAX) {
        return -1;                  /* §10: direkter Pfad max. 27 Byte */
    }
    for (i = 0; i < n; i++) {
        p[i] = (uint8_t)path[i];
    }
    *len = (uint8_t)n;
    return 0;
}

/* ------------------------------------------------------------------ */
/* §11: PING, GET_INFO, GET_STATUS                                     */
/* ------------------------------------------------------------------ */

static uint8_t v4_info_common(v4_master_t *m, uint8_t cmd, v4p_info_t *info)
{
    v4p_read_t r;
    uint8_t    rc;

    if (info == NULL) {
        return V4_ERR_ARG;
    }
    memset(info, 0, sizeof(*info));

    rc = v4_transact(m, cmd, NULL, 0u, &r);
    if (rc != V4P_ST_OK) {
        return rc;
    }
    if (v4p_dec_get_info(r.payload, r.len, info) != 0) {
        return V4_ERR_FRAME;
    }
    return V4P_ST_OK;
}

uint8_t v4_ping(v4_master_t *m, v4p_info_t *info)
{
    return v4_info_common(m, V4P_CMD_PING, info);
}

uint8_t v4_get_info(v4_master_t *m, v4p_info_t *info)
{
    return v4_info_common(m, V4P_CMD_GET_INFO, info);
}

uint8_t v4_check_info(const v4p_info_t *info)
{
    if (info == NULL) {
        return V4_ERR_ARG;
    }
    if (info->proto_ver != V4P_PROTO_VER) {
        return V4_ERR_PROTO;
    }
    if (info->write_frame_len != (uint8_t)V4P_WRITE_FRAME_LEN
        || info->read_frame_len != (uint8_t)V4P_READ_FRAME_LEN) {
        /* Andere Rahmengroessen: das ist genau die 64/128-Byte-Falle. */
        return V4_ERR_PROTO;
    }
    if (info->chunk < V4P_CHUNK_MIN || info->chunk > V4P_CHUNK_MAX) {
        return V4_ERR_PROTO;
    }
    return V4P_ST_OK;
}

uint8_t v4_get_status(v4_master_t *m, v4p_status_t *st)
{
    v4p_read_t r;
    uint8_t    rc;

    if (st == NULL) {
        return V4_ERR_ARG;
    }
    memset(st, 0, sizeof(*st));

    rc = v4_transact(m, V4P_CMD_GET_STATUS, NULL, 0u, &r);
    if (rc != V4P_ST_OK) {
        return rc;
    }
    if (v4p_dec_get_status(r.payload, r.len, st) != 0) {
        return V4_ERR_FRAME;
    }
    return V4P_ST_OK;
}

/* ------------------------------------------------------------------ */
/* §11: Scan und Geraeteliste                                          */
/* ------------------------------------------------------------------ */

uint8_t v4_scan_start(v4_master_t *m, uint8_t inq_units, int continuous)
{
    uint8_t p[2];

    p[0] = inq_units;
    p[1] = continuous ? 0x01u : 0x00u;      /* Bit0 = Dauerbetrieb */
    return v4_simple_p(m, V4P_CMD_SCAN_START, p, 2u);
}

uint8_t v4_scan_stop(v4_master_t *m)
{
    return v4_simple(m, V4P_CMD_SCAN_STOP);
}

uint8_t v4_dev_count(v4_master_t *m, uint8_t *count)
{
    v4p_read_t r;
    uint8_t    rc;

    if (count == NULL) {
        return V4_ERR_ARG;
    }
    rc = v4_transact(m, V4P_CMD_DEV_COUNT, NULL, 0u, &r);
    if (rc != V4P_ST_OK) {
        return rc;
    }
    if (r.len < 1u) {
        return V4_ERR_FRAME;
    }
    *count = r.payload[0];
    return V4P_ST_OK;
}

uint8_t v4_dev_get(v4_master_t *m, uint8_t idx, v4p_dev_t *dev)
{
    uint8_t    p[1];
    v4p_read_t r;
    uint8_t    rc;

    if (dev == NULL) {
        return V4_ERR_ARG;
    }
    memset(dev, 0, sizeof(*dev));

    p[0] = idx;
    rc = v4_transact(m, V4P_CMD_DEV_GET, p, 1u, &r);
    if (rc != V4P_ST_OK) {
        return rc;
    }
    if (v4p_dec_dev(r.payload, r.len, dev) != 0) {
        return V4_ERR_FRAME;
    }
    dev->flags = r.flags;       /* NAME_TRUNCATED sichtbar machen (§5.2) */
    return V4P_ST_OK;
}

/* ------------------------------------------------------------------ */
/* §11: Verbindung                                                     */
/* ------------------------------------------------------------------ */

uint8_t v4_connect(v4_master_t *m, uint8_t idx)
{
    uint8_t p[1];

    p[0] = idx;
    return v4_simple_p(m, V4P_CMD_CONNECT, p, 1u);
}

uint8_t v4_connect_bda(v4_master_t *m, const uint8_t bda[6])
{
    uint8_t p[6];
    uint8_t i;

    if (bda == NULL) {
        return V4_ERR_ARG;
    }
    for (i = 0; i < 6u; i++) {
        p[i] = bda[i];
    }
    return v4_simple_p(m, V4P_CMD_CONNECT_BDA, p, 6u);
}

uint8_t v4_disconnect(v4_master_t *m)
{
    return v4_simple(m, V4P_CMD_DISCONNECT);
}

uint8_t v4_forget(v4_master_t *m)
{
    return v4_simple(m, V4P_CMD_FORGET);
}

/* ------------------------------------------------------------------ */
/* §11: SD-Karte und Chunk                                             */
/* ------------------------------------------------------------------ */

uint8_t v4_sd_mount_probe(v4_master_t *m, int force)
{
    uint8_t    p[1];
    v4p_read_t r;

    p[0] = force ? 0x01u : 0x00u;
    /* §8.1: BUSY ist hier ein gueltiges Ergebnis, kein Wiederholgrund. */
    return v4_transact_ex(m, V4P_CMD_SD_MOUNT, p, 1u, &r, 0u);
}

uint8_t v4_sd_mount(v4_master_t *m, int force)
{
    uint8_t p[1];

    p[0] = force ? 0x01u : 0x00u;
    return v4_simple_p(m, V4P_CMD_SD_MOUNT, p, 1u);
}

uint8_t v4_sd_mount_wait(v4_master_t *m, unsigned tries)
{
    uint8_t rc;
    unsigned i;

    rc = v4_sd_mount_probe(m, 1);
    if (rc == V4P_ST_BUSY) {
        for (i = 0; i < tries; i++) {
            v4_plat_delay_us(V4_MOUNT_WAIT_US);     /* ~200 ms */
            rc = v4_sd_mount_probe(m, 0);
            if (rc != V4P_ST_BUSY) {
                return rc;
            }
        }
    }
    return rc;
}

uint8_t v4_sd_info(v4_master_t *m, v4p_sdinfo_t *si)
{
    v4p_read_t r;
    uint8_t    rc;

    if (si == NULL) {
        return V4_ERR_ARG;
    }
    memset(si, 0, sizeof(*si));

    rc = v4_transact(m, V4P_CMD_SD_INFO, NULL, 0u, &r);
    if (rc != V4P_ST_OK) {
        return rc;
    }
    if (v4p_dec_sd_info(r.payload, r.len, si) != 0) {
        return V4_ERR_FRAME;
    }
    return V4P_ST_OK;
}

uint8_t v4_set_chunk(v4_master_t *m, uint16_t chunk)
{
    uint8_t    p[2];
    v4p_read_t r;
    uint8_t    rc;
    uint16_t   confirmed;

    if (m == NULL) {
        return V4_ERR_ARG;
    }
    if (chunk < V4P_CHUNK_MIN || chunk > V4P_CHUNK_MAX) {
        /* §11: unzulaessiger Wert -> BAD_ARG. Der Statuscode wird lokal
         * vergeben, damit der Aufrufer denselben Code sieht wie vom Slave. */
        return V4P_ST_BAD_ARG;
    }

    v4p_put_u16le(p, chunk);
    rc = v4_transact(m, V4P_CMD_SET_CHUNK, p, 2u, &r);
    if (rc != V4P_ST_OK) {
        return rc;
    }
    if (r.len < 2u) {
        return V4_ERR_FRAME;
    }

    /* Der ESP32 bestaetigt den Wert und verwirft seinen Blockcache (§9). */
    confirmed = v4p_get_u16le(r.payload);
    if (confirmed < V4P_CHUNK_MIN || confirmed > V4P_CHUNK_MAX) {
        return V4_ERR_FRAME;
    }
    m->chunk = confirmed;
    return V4P_ST_OK;
}

/* ------------------------------------------------------------------ */
/* §11: Pfadpuffer -- §10                                              */
/* ------------------------------------------------------------------ */

uint8_t v4_path_clear(v4_master_t *m)
{
    uint8_t rc;

    if (m == NULL) {
        return V4_ERR_ARG;
    }
    rc = v4_simple(m, V4P_CMD_PATH_CLEAR);
    if (rc == V4P_ST_OK) {
        m->path_len   = 0u;
        m->path[0]    = '\0';
        m->path_valid = 1u;         /* Spiegel und Slave sind wieder gleich */
    }
    return rc;
}

/* Baut den PATH_*-Puffer des Slaves aus dem lokalen Spiegel neu auf.
 * Noetig, wenn ein PATH_APPEND wiederholt werden musste und der Slave das
 * Fragment dadurch zweimal angehaengt haben kann: §7 erklaert alle Befehle
 * fuer wiederholbar, die Befehlsliste in §6 wiederholt sie aber blind --
 * fuer PATH_APPEND trifft beides nicht ohne Weiteres zu. */
static uint8_t v4_path_rebuild(v4_master_t *m)
{
    uint8_t  tmp[V4P_PATH_MAX + 1u];
    uint16_t n = m->path_len;
    uint16_t off;
    uint16_t i;
    uint8_t  rc;

    for (i = 0; i < n; i++) {
        tmp[i] = m->path[i];
    }
    tmp[n] = '\0';

    rc = v4_simple(m, V4P_CMD_PATH_CLEAR);
    if (rc != V4P_ST_OK) {
        return rc;
    }
    for (off = 0u; off < n; ) {
        uint16_t take = (uint16_t)(n - off);

        if (take > (uint16_t)V4P_WRITE_PAYLOAD_MAX) {
            take = (uint16_t)V4P_WRITE_PAYLOAD_MAX;
        }
        rc = v4_simple_p(m, V4P_CMD_PATH_APPEND, tmp + off, (uint8_t)take);
        if (rc != V4P_ST_OK) {
            return rc;
        }
        off = (uint16_t)(off + take);
    }
    return V4P_ST_OK;
}

uint8_t v4_path_append(v4_master_t *m, const char *fragment)
{
    uint8_t p[V4P_WRITE_PAYLOAD_MAX];
    size_t  n;
    size_t  i;
    uint8_t rc;
    unsigned long unsafe_before;

    if (m == NULL || fragment == NULL) {
        return V4_ERR_ARG;
    }
    if (!m->path_valid) {
        /* Der Spiegel ist unbekannt (siehe unten): erst PATH_CLEAR. */
        return V4_ERR_ARG;
    }

    n = strlen(fragment);
    if (n > (size_t)V4P_WRITE_PAYLOAD_MAX) {
        return V4P_ST_TOO_LONG;     /* Fragment max. 27 Byte (§10) */
    }
    /* Lokaler Spiegel: ueberlaeuft er, laeuft auch der Slave-Puffer ueber,
     * weil beide leer starten und nur um erfolgreiche Appends wachsen.
     * Der Puffer bleibt bei TOO_LONG unveraendert (§11). */
    if ((size_t)m->path_len + n > (size_t)V4P_PATH_MAX) {
        return V4P_ST_TOO_LONG;
    }

    memset(p, 0, sizeof(p));
    for (i = 0; i < n; i++) {
        p[i] = (uint8_t)fragment[i];
    }

    unsafe_before = m->unsafe_retries;
    rc = v4_simple_p(m, V4P_CMD_PATH_APPEND, p, (uint8_t)n);
    if (rc == V4P_ST_OK) {
        for (i = 0; i < n; i++) {
            m->path[m->path_len + i] = (uint8_t)fragment[i];
        }
        m->path_len = (uint16_t)(m->path_len + n);
        m->path[m->path_len] = '\0';

        if (m->unsafe_retries != unsafe_before) {
            /* Der Slave kann das Fragment zweimal angehaengt haben. Der
             * Spiegel ist die Absicht -- Slave daraus neu aufbauen. */
            unsigned long before_rebuild = m->unsafe_retries;
            uint8_t       hrc = v4_path_rebuild(m);

            if (hrc != V4P_ST_OK || m->unsafe_retries != before_rebuild) {
                /* Auch der Neuaufbau war nicht sauber: der Spiegel ist nicht
                 * mehr vertrauenswuerdig. Erst PATH_CLEAR schafft Klarheit. */
                m->path_valid = 0;
                return (hrc != V4P_ST_OK) ? hrc : V4_ERR_LINK;
            }
        }
    }
    return rc;
}

/* ------------------------------------------------------------------ */
/* §11: Verzeichnisse                                                  */
/* ------------------------------------------------------------------ */

uint8_t v4_dir_open(v4_master_t *m, const char *path, uint8_t *handle)
{
    uint8_t    p[V4P_WRITE_PAYLOAD_MAX];
    uint8_t    len;
    v4p_read_t r;
    uint8_t    rc;
    unsigned long unsafe_before;

    if (m == NULL || handle == NULL) {
        return V4_ERR_ARG;
    }
    if (path == NULL && !m->path_valid) {
        return V4_ERR_ARG;          /* Spiegel unbekannt: erst PATH_CLEAR */
    }
    if (v4_path_payload(path, p, &len) != 0) {
        return V4P_ST_TOO_LONG;
    }

    unsafe_before = m->unsafe_retries;
    rc = v4_transact(m, V4P_CMD_DIR_OPEN, p, len, &r);
    if (rc != V4P_ST_OK) {
        return rc;
    }
    if (m->unsafe_retries != unsafe_before) {
        m->possible_handle_leaks++; /* Zweite Ausfuehrung -> evtl. 2. Handle */
    }
    if (r.len < 1u) {
        return V4_ERR_FRAME;
    }
    *handle = r.payload[0];
    return V4P_ST_OK;
}

uint8_t v4_dir_next(v4_master_t *m, uint8_t handle, uint16_t index,
                    v4p_dirent_t *ent)
{
    uint8_t    p[3];
    v4p_read_t r;
    uint8_t    rc;

    if (ent == NULL) {
        return V4_ERR_ARG;
    }
    memset(ent, 0, sizeof(*ent));

    p[0] = handle;
    v4p_put_u16le(p + 1, index);
    rc = v4_transact(m, V4P_CMD_DIR_NEXT, p, 3u, &r);
    if (rc != V4P_ST_OK) {
        return rc;
    }
    if (v4p_dec_dirent(r.payload, r.len, ent) != 0) {
        return V4_ERR_FRAME;
    }
    ent->flags = r.flags;       /* NAME_TRUNCATED sichtbar machen (§5.2) */
    return V4P_ST_OK;
}

uint8_t v4_dir_close(v4_master_t *m, uint8_t handle)
{
    uint8_t p[1];

    p[0] = handle;
    return v4_simple_p(m, V4P_CMD_DIR_CLOSE, p, 1u);
}

/* ------------------------------------------------------------------ */
/* §11: Dateien                                                        */
/* ------------------------------------------------------------------ */

uint8_t v4_file_open(v4_master_t *m, const char *path, uint8_t *handle,
                     uint32_t *size, uint8_t *attr)
{
    uint8_t    p[V4P_WRITE_PAYLOAD_MAX];
    uint8_t    len;
    v4p_read_t r;
    uint8_t    rc;
    unsigned long unsafe_before;

    if (m == NULL || handle == NULL) {
        return V4_ERR_ARG;
    }
    if (path == NULL && !m->path_valid) {
        return V4_ERR_ARG;          /* Spiegel unbekannt: erst PATH_CLEAR */
    }
    if (v4_path_payload(path, p, &len) != 0) {
        return V4P_ST_TOO_LONG;
    }

    unsafe_before = m->unsafe_retries;
    rc = v4_transact(m, V4P_CMD_FILE_OPEN, p, len, &r);
    if (rc != V4P_ST_OK) {
        return rc;
    }
    if (m->unsafe_retries != unsafe_before) {
        m->possible_handle_leaks++; /* Zweite Ausfuehrung -> evtl. 2. Handle */
    }
    if (v4p_dec_file_open(r.payload, r.len, handle, size, attr) != 0) {
        return V4_ERR_FRAME;
    }
    return V4P_ST_OK;
}

uint8_t v4_file_read(v4_master_t *m, uint8_t handle, uint16_t block,
                     uint8_t *buf, v4p_bulk_t *out)
{
    uint8_t p[3];

    if (buf == NULL || out == NULL) {
        return V4_ERR_ARG;
    }
    p[0] = handle;
    v4p_put_u16le(p + 1, block);
    return v4_transact_bulk(m, V4P_CMD_FILE_READ, p, 3u, buf, out);
}

uint8_t v4_file_close(v4_master_t *m, uint8_t handle)
{
    uint8_t p[1];

    p[0] = handle;
    return v4_simple_p(m, V4P_CMD_FILE_CLOSE, p, 1u);
}

uint8_t v4_reset(v4_master_t *m)
{
    uint8_t rc;

    if (m == NULL) {
        return V4_ERR_ARG;
    }
    rc = v4_simple(m, V4P_CMD_RESET);
    if (rc == V4P_ST_OK) {
        /* Der ESP32 leert Pfadpuffer und Blockcache; SEQ bleibt gueltig. */
        m->path_len   = 0u;
        m->path[0]    = '\0';
        m->path_valid = 1u;
    }
    return rc;
}

/* ------------------------------------------------------------------ */
/* §11: Wiedergabe von der SD-Karte des ESP32                          */
/* ------------------------------------------------------------------ */

uint8_t v4_play_file(v4_master_t *m, const char *path)
{
    uint8_t    p[V4P_WRITE_PAYLOAD_MAX];
    uint8_t    len;
    v4p_read_t r;

    if (m == NULL) {
        return V4_ERR_ARG;
    }
    if (path == NULL && !m->path_valid) {
        return V4_ERR_ARG;          /* Spiegel unbekannt: erst PATH_CLEAR */
    }
    if (v4_path_payload(path, p, &len) != 0) {
        return V4P_ST_TOO_LONG;
    }
    /* Der Umbau der Audio-Pipeline braucht laenger als eine Cache-Runde:
     * eigenes BUSY-Budget. Derselbe Pfad ist idempotent, ein anderer ersetzt
     * die laufende Wiedergabe (§11). */
    return v4_transact_n(m, V4P_CMD_PLAY_FILE, p, len, &r,
                         V4_X_BUSY_RETRY, V4_PLAY_BUSY_TRIES);
}

uint8_t v4_stop_play(v4_master_t *m)
{
    v4p_read_t r;

    if (m == NULL) {
        return V4_ERR_ARG;
    }
    return v4_transact_n(m, V4P_CMD_STOP_PLAY, NULL, 0u, &r,
                         V4_X_BUSY_RETRY, V4_PLAY_BUSY_TRIES);
}

/* ------------------------------------------------------------------ */
/* Ablaufhilfen -- §7.2, §8.2, §13                                     */
/* ------------------------------------------------------------------ */

uint8_t v4_wait_state(v4_master_t *m, uint8_t want, unsigned tries,
                      v4p_status_t *last)
{
    unsigned i;
    uint8_t  rc;

    if (last == NULL) {
        return V4_ERR_ARG;
    }
    for (i = 0; i < tries; i++) {
        rc = v4_get_status(m, last);
        if (rc != V4P_ST_OK) {
            return rc;
        }
        if (last->state == want) {
            return V4P_ST_OK;
        }
        v4_plat_delay_us(V4_POLL_WAIT_US);
    }
    return V4_ERR_TIMEOUT;
}

uint8_t v4_list_dir(v4_master_t *m, const char *path,
                    v4_dirent_cb cb, void *ctx)
{
    uint8_t  handle = 0u;
    uint16_t index  = 0u;
    uint8_t  rc;

    if (m == NULL || cb == NULL) {
        return V4_ERR_ARG;
    }

    rc = v4_dir_open(m, path, &handle);
    if (rc != V4P_ST_OK) {
        return rc;
    }

    for (;;) {
        v4p_dirent_t ent;

        rc = v4_dir_next(m, handle, index, &ent);
        if (rc == V4P_ST_END) {
            rc = V4P_ST_OK;
            break;
        }
        if (rc == V4P_ST_BAD_ARG) {
            /* Index ausser Takt (§7.2): Iteration ist nicht fortsetzbar. */
            break;
        }
        if (rc != V4P_ST_OK) {
            break;
        }
        cb(&ent, ctx);
        index++;                    /* erst nach Erfolg erhoehen */
    }

    {
        uint8_t crc = v4_dir_close(m, handle);
        if (rc == V4P_ST_OK) {
            rc = crc;
        }
    }
    return rc;
}

uint8_t v4_read_file(v4_master_t *m, const char *path,
                     uint8_t *scratch, size_t scratch_cap,
                     v4_file_cb cb, void *ctx, uint32_t *out_bytes)
{
    uint8_t  handle = 0u;
    uint32_t size   = 0u;
    uint8_t  attr   = 0u;
    uint16_t chunk;
    uint16_t block = 0u;
    uint32_t total = 0u;
    uint8_t *buf;
    size_t   need;
    int      own = 0;
    uint8_t  rc;

    if (m == NULL || cb == NULL) {
        return V4_ERR_ARG;
    }
    if (out_bytes != NULL) {
        *out_bytes = 0u;
    }

    rc = v4_file_open(m, path, &handle, &size, &attr);
    if (rc != V4P_ST_OK) {
        return rc;
    }

    chunk = m->chunk;
    need  = (size_t)V4P_BULK_OVERHEAD + (size_t)chunk;
    buf   = scratch;
    if (buf == NULL) {
        buf = (uint8_t *)malloc(need);
        if (buf == NULL) {
            (void)v4_file_close(m, handle);
            return V4_ERR_NOMEM;
        }
        own = 1;
    } else if (scratch_cap < need) {
        (void)v4_file_close(m, handle);
        return V4_ERR_NOSPACE;
    }

    for (;;) {
        v4p_bulk_t b;

        /* v4_file_read liest 12 + m->chunk Byte. Der Puffer wurde fuer die
         * Chunkgroesse vom Beginn ausgelegt; eine Aenderung durch den
         * Callback wuerde ihn ueberlaufen lassen (und die Blocknummerierung
         * block * chunk ungueltig machen). Deshalb hier hart abbrechen. */
        if (m->chunk != chunk) {
            rc = V4_ERR_ARG;
            break;
        }

        rc = v4_file_read(m, handle, block, buf, &b);
        if (rc == V4P_ST_END) {
            rc = V4P_ST_OK;         /* regulaeres Dateiende */
            break;
        }
        if (rc != V4P_ST_OK) {
            break;
        }
        if (b.len == 0u) {
            break;                  /* defensiv: OK ohne Daten */
        }

        cb(b.block, b.data, b.len, ctx);
        total += (uint32_t)b.len;

        if (b.len < chunk) {
            break;                  /* kurzer Block = letzter Block (§8.2) */
        }
        if (block == 0xFFFFu) {
            rc = V4_ERR_ARG;        /* block ist u16 -- Grenze des Protokolls */
            break;
        }
        block++;
    }

    {
        uint8_t crc = v4_file_close(m, handle);
        if (rc == V4P_ST_OK) {
            rc = crc;
        }
    }
    if (own) {
        free(buf);
    }
    if (rc == V4P_ST_OK && out_bytes != NULL) {
        *out_bytes = total;
    }
    return rc;
}

/* ------------------------------------------------------------------ */
/* Diagnose                                                            */
/* ------------------------------------------------------------------ */

const char *v4_strerror(uint8_t code)
{
    switch (code) {
    case V4P_ST_OK:        return "OK";
    case V4P_ST_BUSY:      return "BUSY";
    case V4P_ST_BAD_CRC:   return "BAD_CRC";
    case V4P_ST_BAD_CMD:   return "BAD_CMD";
    case V4P_ST_BAD_ARG:   return "BAD_ARG";
    case V4P_ST_BAD_STATE: return "BAD_STATE";
    case V4P_ST_NO_SD:     return "NO_SD";
    case V4P_ST_NOT_FOUND: return "NOT_FOUND";
    case V4P_ST_IO_ERR:    return "IO_ERR";
    case V4P_ST_END:       return "END";
    case V4P_ST_TOO_LONG:  return "TOO_LONG";
    case V4P_ST_NO_HANDLE: return "NO_HANDLE";
    case V4P_ST_BT_ERR:    return "BT_ERR";
    case V4_ERR_LINK:      return "LINK-Fehler (4 Versuche erfolglos)";
    case V4_ERR_BUSY:      return "BUSY-Zeitbudget erschoepft (40 Versuche)";
    case V4_ERR_ARG:       return "ungueltiges Argument";
    case V4_ERR_FRAME:     return "Antwort gueltig, Nutzlast zu kurz";
    case V4_ERR_NOSPACE:   return "Puffer zu klein";
    case V4_ERR_NOMEM:     return "Allokation fehlgeschlagen";
    case V4_ERR_TIMEOUT:   return "Zielzustand nicht erreicht";
    case V4_ERR_PROTO:     return "Firmware meldet ein anderes Protokoll";
    default:               return "unbekannter Code";
    }
}

const char *v4_check_str(int rc)
{
    return v4p_check_str(rc);
}
