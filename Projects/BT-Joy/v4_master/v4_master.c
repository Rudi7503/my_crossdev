/*
 * v4_master.c -- I2C-Master der Vampire V4 fuer den ESP32-Slave
 * (Bluetooth-Joystick-Adapter).
 *
 * Der Transaktionskern ist aus dem Audio-Projekt uebernommen; er hat sich im
 * Feld bewaehrt und wird hier NICHT umgebaut. Angepasst sind die Wrapper: nur
 * die Befehle, die diese Firmware kennt (v4_proto.h), kein BULK-Weg.
 *
 * Portabilitaet: Frame-Bytes werden ausschliesslich ueber v4p_put_*_le /
 * v4p_get_*_le beruehrt (§0/§2). Kein memcpy, kein Cast von Structs auf
 * Frame-Bytes, keine Bitfelder.
 */

#include "v4_master.h"

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
    m->seq       = 0u;
    m->try_max   = V4_RETRIES;      /* Versuche bei Framingfehlern */
    m->t_wait_us = V4_T_WAIT_US;    /* R3: Wartezeit vor dem Lesen */
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
    int     busy_left   = (int)busy_tries;
    int     badcrc_left = V4_BADCRC_TRIES;
    uint32_t t0;

    if (m == NULL || out == NULL) {
        return V4_ERR_ARG;
    }
    if (len > V4P_WRITE_PAYLOAD_MAX) {
        return V4_ERR_ARG;
    }
    if (len > 0u && payload == NULL) {
        return V4_ERR_ARG;
    }

    /* Startzeit der Transaktion. Nur fuer die Diagnose (v4_debug.c misst daraus
     * die Dauer); als uint32_t-Differenz, ein Ueberlauf ist eingeplant. */
    t0 = v4_plat_now_us();

    for (;;) {
        int      attempt;
        int      got  = 0;
        uint32_t wait = m->t_wait_us;   /* waechst bei Framing-Fehlern */

        /* Wiederholungen bei Link- und Framing-Fehlern: SEQ bleibt stehen.
         * Das ist keine Bequemlichkeit, sondern Vertrag: der ESP32 verwirft
         * einen Rahmen mit falscher SEQ, und ein wiederholter Befehl mit
         * GLEICHER SEQ wird von ihm als derselbe Befehl erkannt. */
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
                    V4_WAIT_BACKOFF(m, wait);   /* dem Slave mehr Zeit geben */
                    continue;
                }
            }

            /* R3: vor jedem Lesen warten -- der Slave kann nicht stretchen. */
            v4_trace(m, V4_TR_WAIT, cmd, m->seq, 0u, 0u, 0, wait, NULL);
            v4_plat_delay_us(wait);

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
                    V4_WAIT_BACKOFF(m, wait);   /* dem Slave mehr Zeit geben */
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
                V4_WAIT_BACKOFF(m, wait);   /* dem Slave mehr Zeit geben */
                continue;
            }
            got = 1;
            break;
        }

        if (!got) {
            /* Auch ein Fehlschlag ist eine Transaktion: die Dauer und der
             * Grund gehen als DONE an den Trace (siehe v4_master.h). */
            v4_trace(m, V4_TR_DONE, cmd, m->seq, 0u, 0u, V4_ERR_LINK,
                     (uint32_t)(v4_plat_now_us() - t0), NULL);
            return V4_ERR_LINK;
        }

        if (out->status == V4P_ST_BUSY) {
            m->busy_rounds++;
            v4_trace(m, V4_TR_BUSY, cmd, m->seq, out->status, out->len,
                     V4P_ST_BUSY, 0u, NULL);
            if ((flags & V4_X_BUSY_RETRY) == 0u) {
                /* Aufrufer faehrt die Runden selbst. SEQ bleibt. */
                return V4P_ST_BUSY;
            }
            busy_left--;
            if (busy_left <= 0) {
                v4_trace(m, V4_TR_DONE, cmd, m->seq, out->status, out->len,
                         V4_ERR_BUSY,
                         (uint32_t)(v4_plat_now_us() - t0), NULL);
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
                v4_trace(m, V4_TR_DONE, cmd, m->seq, out->status, out->len,
                         (int)out->status,
                         (uint32_t)(v4_plat_now_us() - t0), NULL);
                return V4P_ST_BAD_CRC;
            }
            v4_plat_delay_us(V4_T_BUSY_US);
            continue;
        }

        /* SEQ wird NUR nach einer gueltigen Antwort erhoeht (§6, Punkt 3). */
        m->seq = (uint8_t)((m->seq + 1u) & 0xFFu);
        v4_trace(m, V4_TR_DONE, cmd, (uint8_t)(m->seq - 1u), out->status,
                 out->len, out->status,
                 (uint32_t)(v4_plat_now_us() - t0), out->payload);
        return out->status;
    }
}

uint8_t v4_transact(v4_master_t *m, uint8_t cmd,
                    const uint8_t *payload, uint8_t len, v4p_read_t *out)
{
    return v4_transact_n(m, cmd, payload, len, out, V4_X_BUSY_RETRY,
                         V4_BUSY_TRIES);
}

/* ------------------------------------------------------------------ */
/* Hilfen                                                              */
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
/* §11: Die beiden Buchsen (0x04..0x07)                                */
/* ------------------------------------------------------------------ */

uint8_t v4_set_port_mode(v4_master_t *m, uint8_t port, uint8_t mode,
                         uint8_t *applied)
{
    uint8_t    p[V4P_SPM_PAYLOAD_LEN];
    uint8_t    back_port = 0u;
    uint8_t    back_mode = 0u;
    v4p_read_t r;
    uint8_t    rc;

    if (port >= V4P_PORT_COUNT || mode > V4P_PORT_MODE_MAX) {
        /* Lokal abweisen, BEVOR gesendet wird: der Slave wuerde BAD_ARG
         * antworten, aber ein unnoetiger Busrahmen ist im Fehlerfall nur
         * Rauschen im Log. Der Code ist derselbe, den der Slave schickt. */
        return V4P_ST_BAD_ARG;
    }

    (void)v4p_enc_set_port_mode(p, port, mode);
    rc = v4_transact(m, V4P_CMD_SET_PORT_MODE, p, (uint8_t)V4P_SPM_PAYLOAD_LEN,
                     &r);
    if (rc != V4P_ST_OK) {
        return rc;
    }
    /*
     * Der ESP32 bestaetigt mit [port][mode]. Diese Bestaetigung wird geprueft
     * und nicht nur quittiert: bei mode=MOUSE wechselt er die Betriebsart nur,
     * wenn er sie kennt, und ein "OK" auf einen anderen Port waere ein Hinweis
     * auf einen verschobenen Strom. Lieber hier auffallen als im Joystick.
     */
    if (v4p_dec_set_port_mode(r.payload, r.len, &back_port, &back_mode) != 0) {
        return V4_ERR_FRAME;
    }
    if (back_port != port || back_mode != mode) {
        return V4_ERR_FRAME;
    }
    if (applied != NULL) {
        *applied = back_mode;
    }
    return V4P_ST_OK;
}

uint8_t v4_get_port_mode(v4_master_t *m, uint8_t mode[V4P_PORT_COUNT])
{
    v4p_read_t r;
    uint8_t    rc;

    if (mode == NULL) {
        return V4_ERR_ARG;
    }
    mode[0] = V4P_PORT_OFF;
    mode[1] = V4P_PORT_OFF;

    rc = v4_transact(m, V4P_CMD_GET_PORT_MODE, NULL, 0u, &r);
    if (rc != V4P_ST_OK) {
        return rc;
    }
    if (v4p_dec_get_port_mode(r.payload, r.len, mode) != 0) {
        return V4_ERR_FRAME;
    }
    return V4P_ST_OK;
}

uint8_t v4_get_input(v4_master_t *m, v4p_input_t *in)
{
    v4p_read_t r;
    uint8_t    rc;

    if (in == NULL) {
        return V4_ERR_ARG;
    }
    memset(in, 0, sizeof(*in));

    rc = v4_transact(m, V4P_CMD_GET_INPUT, NULL, 0u, &r);
    if (rc != V4P_ST_OK) {
        return rc;
    }
    if (v4p_dec_get_input(r.payload, r.len, in) != 0) {
        return V4_ERR_FRAME;
    }
    return V4P_ST_OK;
}

uint8_t v4_get_hid(v4_master_t *m, v4p_hid_t *hid)
{
    v4p_read_t r;
    uint8_t    rc;

    if (hid == NULL) {
        return V4_ERR_ARG;
    }
    memset(hid, 0, sizeof(*hid));

    rc = v4_transact(m, V4P_CMD_GET_HID, NULL, 0u, &r);
    if (rc != V4P_ST_OK) {
        return rc;
    }
    if (v4p_dec_get_hid(r.payload, r.len, hid) != 0) {
        return V4_ERR_FRAME;
    }
    hid->flags = r.flags;       /* NAME_TRUNCATED sichtbar machen */
    return V4P_ST_OK;
}

/* ------------------------------------------------------------------ */
/* §11: Scan und Geraeteliste                                          */
/* ------------------------------------------------------------------ */

uint8_t v4_scan_start(v4_master_t *m, uint8_t inq_units, int continuous)
{
    uint8_t p[2];

    p[0] = inq_units;   /* Fensterlaenge in 1,28-s-Einheiten; 0 = Vorgabe */
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
    *count = 0u;
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
    dev->flags = r.flags;       /* NAME_TRUNCATED sichtbar machen */
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

uint8_t v4_reset(v4_master_t *m)
{
    /* Der ESP32 gibt beide Buchsen frei (JOY_MODE_OFF). Eine laufende
     * Bluetooth-Verbindung bleibt dabei bestehen -- ein Protokoll-Reset darf
     * keinen Controller abhaengen. Hier gibt es deshalb auch keinen lokalen
     * Zustand aufzuraeumen (kein Pfadpuffer wie im Audio-Projekt). */
    return v4_simple(m, V4P_CMD_RESET);
}

/* ------------------------------------------------------------------ */
/* Ablaufhilfen -- §13                                                 */
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

uint8_t v4_swap_port_modes(v4_master_t *m, uint8_t now[V4P_PORT_COUNT])
{
    uint8_t vorher[V4P_PORT_COUNT];
    uint8_t rc;

    if (m == NULL) {
        return V4_ERR_ARG;
    }

    rc = v4_get_port_mode(m, vorher);
    if (rc != V4P_ST_OK) {
        return rc;
    }

    /* Beide Werte stehen jetzt fest. Erst danach wird geschrieben -- sonst
     * haetten wir nach dem ersten Schreiben den alten Modus von Buchse 0
     * verloren (siehe v4_master.h). */
    rc = v4_set_port_mode(m, 0u, vorher[1], NULL);
    if (rc != V4P_ST_OK) {
        /* Halb getauscht: nicht verschweigen. Der Aufrufer sieht am Fehler,
         * dass die Buchsen jetzt NICHT mehr zu `vorher` passen. */
        return rc;
    }
    rc = v4_set_port_mode(m, 1u, vorher[0], NULL);
    if (rc != V4P_ST_OK) {
        return rc;
    }

    if (now != NULL) {
        now[0] = vorher[1];
        now[1] = vorher[0];
    }
    return V4P_ST_OK;
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
    case V4P_ST_NO_SD:     return "NO_SD (in diesem Projekt unbenutzt)";
    case V4P_ST_NOT_FOUND: return "NOT_FOUND";
    case V4P_ST_IO_ERR:    return "IO_ERR";
    case V4P_ST_END:       return "END";
    case V4P_ST_TOO_LONG:  return "TOO_LONG";
    case V4P_ST_NO_HANDLE: return "NO_HANDLE (in diesem Projekt unbenutzt)";
    case V4P_ST_BT_ERR:    return "BT_ERR";
    case V4_ERR_LINK:      return "LINK-Fehler (keine gueltige Antwort)";
    case V4_ERR_BUSY:      return "BUSY-Zeitbudget erschoepft";
    case V4_ERR_ARG:       return "ungueltiges Argument";
    case V4_ERR_FRAME:     return "Antwort gueltig, Nutzlast unbrauchbar";
    case V4_ERR_NOSPACE:   return "Puffer zu klein";
    case V4_ERR_TIMEOUT:   return "Zielzustand nicht erreicht";
    case V4_ERR_PROTO:     return "Firmware meldet ein anderes Protokoll";
    default:               return "unbekannter Code";
    }
}

const char *v4_check_str(int rc)
{
    return v4p_check_str(rc);
}
