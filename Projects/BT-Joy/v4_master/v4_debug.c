/*
 * v4_debug.c -- die gemeinsame Debugausgabe beider Frontends.
 *
 * Ausfuehrliche Begruendung steht in v4_debug.h. Kurz:
 *   - dieselbe Zeile geht an stdout, an den Hardware-Debug-UART der V4 und in
 *     die Logdatei -- nach dem Lauf lassen sich beide vergleichen,
 *   - es gibt GENAU EINE Zeile je I2C-Transaktion,
 *   - Ausgaben werden zusammengefasst: eine fertige Zeile, EIN
 *     ApolloDebugPutStr je Zeile (kein Zeichen fuer Zeichen).
 *
 * Hier steht KEIN Forbid() und kein Delay(): das ist reine Ausgabe. Wer hier
 * spaeter sperren zu muessen glaubt, nimmt eine SignalSemaphore -- niemals
 * Forbid.
 */

#include "v4_debug.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#ifdef V4_PLAT_AMIGA
#include "ApolloCrossDev_Debug.h"   /* Debug-UART der V4 (Projects/ApolloLib) */
#include <dos/datetime.h>
#include <dos/dos.h>
#include <proto/dos.h>
#else
#include <time.h>
#endif

/* ------------------------------------------------------------------ */
/* Zustand der Ausgabe                                                 */
/* ------------------------------------------------------------------ */

/*
 * Groesste Zeile, die diese Datei baut.
 *
 * 300 reichten fuer die Zeile je Transaktion und fuer die Vergleichszeile
 * direkt <-> I2C. Die HERZzeile traegt seit dem Umbau den Vergleich mit
 * (siehe v4_dbg_herz_zusatz): Zaehler + letzter Status + beide Buchsen mit
 * Rohwerten ergeben rund 350 Zeichen. Wird der Puffer zu klein, schneidet
 * vsnprintf ab -- dann fehlt am Ende genau die Aussage, auf die es ankommt
 * (CIAAPRA/POTINP). Deshalb 420; tests/test_debug.c prueft, dass die Zeile
 * wirklich vollstaendig hineinpasst.
 */
#define V4_DBG_LINE V4_DBG_ZEILE_MAX

/* Ohne v4_dbg_init() ist alles aus -- so bleibt die Testsuite still. */
static int          s_stdout = 0;
static int          s_uart   = 0;
static int          s_quiet  = 1;
static int          s_detail = 0;
static int          s_hex    = 0;
static const char  *s_prog   = "?";
static const char  *s_ver    = "?";
static const char  *s_datum  = "?";
static const char  *s_logpfad = NULL;
static unsigned     s_addr7  = V4_I2C_ADDR7;
static void       (*s_hook)(const char *) = NULL;

/* Zusatz fuer die naechste Herzzeile (siehe v4_dbg_herz_zusatz). */
static char         s_herz_zusatz[V4_DBG_LINE] = "";

/* Zaehler fuer den Herzschlag. */
static unsigned long s_zeilen         = 0ul;   /* Transaktionszeilen        */
static unsigned long s_verworfen      = 0ul;   /* verworfene Rahmen         */
static unsigned long s_wiederholungen = 0ul;   /* Wiederholungen            */
static unsigned long s_herz_n         = 0ul;   /* Herzschlaege              */
static char          s_letzter_status[64] = "noch keiner";

/* Flankenerkennung fuer "nur bei Aenderung melden". */
static int          s_st_valid = 0;
static v4p_status_t s_st_alt;
static int          s_sitz_valid = 0;
static uint8_t      s_sitz_state = 0u;
static uint8_t      s_sitz_index = 0u;
static int          s_in_valid = 0;
static v4p_input_t  s_in_alt;
static int          s_hid_valid = 0;
static v4p_hid_t    s_hid_alt;
static int          s_m_valid = 0;
static uint8_t      s_m_alt[V4P_PORT_COUNT];

/* ------------------------------------------------------------------ */
/* Ausgabe                                                             */
/* ------------------------------------------------------------------ */

/* Eine fertige Zeile in alle Senken. Der Aufrufer uebergibt sie mit Umbruch. */
static void dbg_senke(const char *s)
{
    size_t len = strlen(s);

    if (len == 0u) {
        return;
    }
    if (s_stdout != 0) {
        (void)fputs(s, stdout);
        (void)fflush(stdout);
    }
#ifdef V4_PLAT_AMIGA
    if (s_uart != 0) {
        /* EIN Aufruf je Zeile. Ein Zeichen je Aufruf waere auf der V4 der
         * falsche Weg (ein Byte Empfangspuffer, siehe tools/SERIELL.md). */
        ApolloDebugPutStr(s);
    }
#endif
    /* Logdatei: derselbe Text. Ist keine offen, tut der Haken nichts. */
    (void)v4_plat_log_write(s, (unsigned long)len);

    if (s_hook != NULL) {
        char   kopie[V4_DBG_LINE];
        size_t i;

        for (i = 0u; i + 1u < sizeof(kopie) && s[i] != '\0'; i++) {
            kopie[i] = s[i];
        }
        kopie[i] = '\0';
        while (i > 0u && (kopie[i - 1u] == '\n' || kopie[i - 1u] == '\r')) {
            kopie[--i] = '\0';
        }
        if (i > 0u) {
            s_hook(kopie);
        }
    }
}

void v4_dbg(const char *fmt, ...)
{
    static char buf[V4_DBG_LINE];       /* ortsfest: 4-KB-Stapel auf dem Amiga */
    va_list     ap;
    int         n;

    if (fmt == NULL) {
        return;
    }
    va_start(ap, fmt);
    n = vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    if (n <= 0) {
        return;
    }
    if ((size_t)n > sizeof(buf) - 1u) {
        n = (int)(sizeof(buf) - 1u);    /* gekuerzt, aber terminiert */
    }
    dbg_senke(buf);
}

void v4_dbg_zeile(const char *zeile)
{
    if (zeile == NULL) {
        return;
    }
    dbg_senke(zeile);
}

/* ------------------------------------------------------------------ */
/* Konfiguration                                                       */
/* ------------------------------------------------------------------ */

void v4_dbg_init(const v4_dbg_cfg_t *cfg)
{
    if (cfg == NULL) {
        return;
    }
    s_stdout  = (cfg->stdout_an != 0) ? 1 : 0;
    s_uart    = (cfg->uart != 0) ? 1 : 0;
    s_quiet   = (cfg->quiet != 0) ? 1 : 0;
    s_prog    = (cfg->programm != NULL) ? cfg->programm : "?";
    s_ver     = (cfg->version != NULL) ? cfg->version : "?";
    s_datum   = (cfg->datum != NULL) ? cfg->datum : "?";
    s_logpfad = cfg->logdatei;
    s_addr7   = (cfg->addr7 != 0u) ? cfg->addr7 : (unsigned)V4_I2C_ADDR7;

#ifdef V4_PLAT_AMIGA
    if (s_uart != 0) {
        ApolloDebugInit();      /* CIAB + SERPER: 115200 Baud am COM-Port */
    }
#endif
}

void v4_dbg_set_detail(int an)
{
    s_detail = (an != 0) ? 1 : 0;
}

void v4_dbg_set_hex(int an)
{
    s_hex = (an != 0) ? 1 : 0;
}

void v4_dbg_set_line_hook(void (*hook)(const char *zeile))
{
    s_hook = hook;
}

/* ------------------------------------------------------------------ */
/* Uhrzeit der Plattform                                               */
/* ------------------------------------------------------------------ */

void v4_dbg_zeit(char *out, size_t cap)
{
    if (out == NULL || cap == 0u) {
        return;
    }
    out[0] = '\0';
#ifdef V4_PLAT_AMIGA
    {
        struct DateTime  dt;
        struct DateStamp ds;

        (void)DateStamp(&ds);
        dt.dat_Stamp  = ds;
        dt.dat_Format = (LONG)FORMAT_INT;   /* Datum und Uhrzeit */
        dt.dat_Flags  = 0;
        dt.dat_StrDay  = NULL;
        dt.dat_StrDate = NULL;
        dt.dat_StrTime = NULL;
        if (DateToStr(&dt) != 0) {
            (void)snprintf(out, cap, "%s %s",
                           (const char *)dt.dat_StrDate,
                           (const char *)dt.dat_StrTime);
        } else {
            (void)snprintf(out, cap, "(Uhrzeit nicht lesbar)");
        }
    }
#else
    {
        time_t    t = time(NULL);
        struct tm *lt = localtime(&t);

        if (lt == NULL
            || strftime(out, cap, "%d.%m.%Y %H:%M:%S", lt) == 0u) {
            (void)snprintf(out, cap, "(Uhrzeit nicht lesbar)");
        }
    }
#endif
}

void v4_dbg_kopf(void)
{
    char zeit[64];

    v4_dbg_zeit(zeit, sizeof(zeit));
    v4_dbg("========================================================\n");
    v4_dbg("%s %s (%s)\n", s_prog, s_ver, s_datum);
    v4_dbg("Start am %s\n", zeit);
    v4_dbg("Logdatei: %s\n",
           (s_logpfad != NULL) ? s_logpfad : "(keine benannt)");
    v4_dbg("I2C-Adresse 0x%02X (7 Bit), Debug-UART %s\n",
           (unsigned)s_addr7,
           (s_uart != 0) ? "an (115200 Baud)" : "aus");
    v4_dbg("Transaktionszeilen %s, Schritt-Trace %s\n",
           (s_quiet != 0) ? "aus (-quiet)" : "an",
           (s_detail != 0) ? "an" : "aus");
    v4_dbg("========================================================\n");
}

/* ------------------------------------------------------------------ */
/* Klartexte                                                           */
/* ------------------------------------------------------------------ */

const char *v4_dbg_cmd_name(uint8_t cmd)
{
    switch (cmd) {
    case V4P_CMD_PING:          return "PING";
    case V4P_CMD_GET_STATUS:    return "GET_STATUS";
    case V4P_CMD_GET_INFO:      return "GET_INFO";
    case V4P_CMD_SET_PORT_MODE: return "SET_PORT_MODE";
    case V4P_CMD_GET_PORT_MODE: return "GET_PORT_MODE";
    case V4P_CMD_GET_INPUT:     return "GET_INPUT";
    case V4P_CMD_GET_HID:       return "GET_HID";
    case V4P_CMD_SCAN_START:    return "SCAN_START";
    case V4P_CMD_SCAN_STOP:     return "SCAN_STOP";
    case V4P_CMD_DEV_COUNT:     return "DEV_COUNT";
    case V4P_CMD_DEV_GET:       return "DEV_GET";
    case V4P_CMD_CONNECT:       return "CONNECT";
    case V4P_CMD_CONNECT_BDA:   return "CONNECT_BDA";
    case V4P_CMD_DISCONNECT:    return "DISCONNECT";
    case V4P_CMD_FORGET:        return "FORGET";
    case V4P_CMD_RESET:         return "RESET";
    default:                    return "?";
    }
}

static const char *trace_name(uint8_t event)
{
    switch (event) {
    case V4_TR_TX_BEGIN: return "tx-begin";
    case V4_TR_TX_END:   return "tx-ende ";
    case V4_TR_WAIT:     return "warten  ";
    case V4_TR_RX_BEGIN: return "rx-begin";
    case V4_TR_RX_END:   return "rx-ende ";
    case V4_TR_CHECK:    return "pruefen ";
    case V4_TR_RETRY:    return "wiederh.";
    case V4_TR_BUSY:     return "busy    ";
    case V4_TR_DONE:     return "fertig  ";
    default:             return "?       ";
    }
}

/* ------------------------------------------------------------------ */
/* Kurzform der Antwort                                                */
/*                                                                     */
/* Eine Zeile, kein Hexdump: was die Gegenstelle geantwortet hat, in    */
/* der Form, in der man es beim Mitlesen braucht.                      */
/* ------------------------------------------------------------------ */

static void kurzform(uint8_t cmd, const uint8_t *p, uint16_t len,
                     char *out, size_t cap)
{
    if (out == NULL || cap == 0u) {
        return;
    }
    out[0] = '\0';
    if (p == NULL || len == 0u) {
        return;
    }

    switch (cmd) {
    case V4P_CMD_PING:
    case V4P_CMD_GET_INFO: {
        v4p_info_t i;

        if (v4p_dec_get_info(p, len, &i) == 0) {
            (void)snprintf(out, cap,
                           "proto=%u fw=%u write=%u read=%u caps=0x%04X",
                           (unsigned)i.proto_ver, (unsigned)i.fw_ver,
                           (unsigned)i.write_frame_len,
                           (unsigned)i.read_frame_len, (unsigned)i.caps);
        }
        break;
    }
    case V4P_CMD_GET_STATUS: {
        v4p_status_t st;

        if (v4p_dec_get_status(p, len, &st) == 0) {
            (void)snprintf(out, cap,
                           "state=%u (%s) conn=%u scan=%u dev=%u",
                           (unsigned)st.state, v4p_bt_state_name(st.state),
                           (unsigned)st.conn_index, (unsigned)st.scan_active,
                           (unsigned)st.dev_count);
        }
        break;
    }
    case V4P_CMD_GET_PORT_MODE: {
        uint8_t m[V4P_PORT_COUNT];

        if (v4p_dec_get_port_mode(p, len, m) == 0) {
            (void)snprintf(out, cap, "port0=%s port1=%s",
                           v4p_port_mode_name(m[0]),
                           v4p_port_mode_name(m[1]));
        }
        break;
    }
    case V4P_CMD_SET_PORT_MODE: {
        uint8_t port = 0u;
        uint8_t mode = 0u;

        if (v4p_dec_set_port_mode(p, len, &port, &mode) == 0) {
            (void)snprintf(out, cap, "bestaetigt port=%u mode=%s",
                           (unsigned)port, v4p_port_mode_name(mode));
        }
        break;
    }
    case V4P_CMD_GET_INPUT: {
        v4p_input_t in;

        if (v4p_dec_get_input(p, len, &in) == 0) {
            if (in.level_valid != 0u) {
                (void)snprintf(out, cap,
                               "state0=0x%02X state1=0x%02X "
                               "level0=0x%02X level1=0x%02X steps=%lu/%lu",
                               (unsigned)in.state[0], (unsigned)in.state[1],
                               (unsigned)in.level[0], (unsigned)in.level[1],
                               (unsigned long)in.steps_x,
                               (unsigned long)in.steps_y);
            } else {
                /* Kein "Pegel=0x": die 10-Byte-Fassung hat keine Pegel, und
                 * eine erfundene Zahl waere von einer Messung nicht zu
                 * unterscheiden. */
                (void)snprintf(out, cap,
                               "state0=0x%02X state1=0x%02X "
                               "level=unbekannt (10-Byte-Fassung)",
                               (unsigned)in.state[0], (unsigned)in.state[1]);
            }
        }
        break;
    }
    case V4P_CMD_GET_HID: {
        v4p_hid_t hid;

        if (v4p_dec_get_hid(p, len, &hid) == 0) {
            (void)snprintf(out, cap, "kind=%s name=\"%s\" vid:pid=%04X:%04X",
                           v4p_hid_kind_name(hid.kind),
                           (hid.name[0] != '\0') ? hid.name : "(ohne Namen)",
                           (unsigned)hid.vid, (unsigned)hid.pid);
        }
        break;
    }
    case V4P_CMD_DEV_COUNT:
        if (len >= 1u) {
            (void)snprintf(out, cap, "count=%u", (unsigned)p[0]);
        }
        break;
    case V4P_CMD_DEV_GET: {
        v4p_dev_t dev;

        if (v4p_dec_dev(p, len, &dev) == 0) {
            (void)snprintf(out, cap, "name=\"%s\" bda=%02X:%02X:%02X:%02X:%02X:%02X",
                           (dev.name[0] != '\0') ? dev.name : "(ohne Namen)",
                           dev.bda[0], dev.bda[1], dev.bda[2], dev.bda[3],
                           dev.bda[4], dev.bda[5]);
        }
        break;
    }
    default:
        (void)snprintf(out, cap, "nutzlast=%u Byte", (unsigned)len);
        break;
    }
}

/* Hexdump der Rahmenbytes -- nur mit -trace -x (Konsole). */
static void hexdump(const uint8_t *d, uint16_t len)
{
    char     zeile[80];
    uint16_t i;

    for (i = 0u; i < len; i += 16u) {
        uint16_t k;
        int      pos = 0;
        int      n;

        n = snprintf(zeile, sizeof(zeile), "    %04X:", (unsigned)i);
        if (n <= 0) {
            return;
        }
        pos = n;
        for (k = i; k < len && k < (uint16_t)(i + 16u); k++) {
            n = snprintf(zeile + pos, sizeof(zeile) - (size_t)pos, " %02X",
                         (unsigned)d[k]);
            if (n <= 0 || (size_t)(pos + n) >= sizeof(zeile)) {
                break;
            }
            pos += n;
        }
        v4_dbg("%s\n", zeile);
    }
}

/* ------------------------------------------------------------------ */
/* Trace-Haken: die Zeile je Transaktion                               */
/* ------------------------------------------------------------------ */

void v4_dbg_trace(const v4_trace_t *ev, void *ctx)
{
    (void)ctx;
    if (ev == NULL) {
        return;
    }

    if (s_detail != 0) {
        v4_dbg("[trace] %s cmd=0x%02X (%-11s) seq=%u len=%u rc=%d "
               "status=0x%02X us=%lu\n",
               trace_name(ev->event), (unsigned)ev->cmd,
               v4_dbg_cmd_name(ev->cmd), (unsigned)ev->seq,
               (unsigned)ev->len, ev->rc, (unsigned)ev->status,
               (unsigned long)ev->us);
        if (s_hex != 0 && ev->data != NULL && ev->len > 0u) {
            hexdump(ev->data, ev->len);
        }
    }

    switch (ev->event) {
    case V4_TR_RETRY:
        s_wiederholungen++;
        break;

    case V4_TR_CHECK:
        if (ev->rc != V4P_CHECK_OK) {
            /* Ein Rahmen, der die Pruefung nicht besteht: genau die
             * "verworfenen Rahmen" aus dem Herzschlag. Im Detailmodus steht
             * er schon als eigene Zeile. */
            s_verworfen++;
            if (s_detail == 0) {
                v4_dbg("[i2c] Rahmen verworfen: cmd=%s grund=%s (%d)\n",
                       v4_dbg_cmd_name(ev->cmd), v4_check_str(ev->rc), ev->rc);
            }
        }
        break;

    case V4_TR_DONE: {
        char kurz[200];

        kurzform(ev->cmd, ev->data, ev->len, kurz, sizeof(kurz));
        s_zeilen++;

        if (ev->rc == (int)V4P_ST_OK) {
            (void)snprintf(s_letzter_status, sizeof(s_letzter_status),
                           "OK (%s)", v4_dbg_cmd_name(ev->cmd));
        } else {
            (void)snprintf(s_letzter_status, sizeof(s_letzter_status),
                           "%s (0x%02X)", v4_strerror((uint8_t)ev->rc),
                           (unsigned)ev->rc);
        }

        if (s_quiet == 0) {
            if (ev->rc == (int)V4P_ST_OK) {
                v4_dbg("[i2c] cmd=%-13s seq=%u status=OK len=%u dauer=%luus%s%s\n",
                       v4_dbg_cmd_name(ev->cmd), (unsigned)ev->seq,
                       (unsigned)ev->len, (unsigned long)ev->us,
                       (kurz[0] != '\0') ? " | " : "", kurz);
            } else {
                v4_dbg("[i2c] cmd=%-13s seq=%u status=ERR rc=0x%02X (%s) "
                       "len=%u dauer=%luus%s%s\n",
                       v4_dbg_cmd_name(ev->cmd), (unsigned)ev->seq,
                       (unsigned)ev->rc, v4_strerror((uint8_t)ev->rc),
                       (unsigned)ev->len, (unsigned long)ev->us,
                       (kurz[0] != '\0') ? " | " : "", kurz);
            }
        }
        break;
    }

    default:
        break;
    }
}

/* ------------------------------------------------------------------ */
/* Kurzformen mit Flankenerkennung                                     */
/* ------------------------------------------------------------------ */

/* Faehigkeitsliste im Klartext -- 0x00FF sagt niemandem etwas. */
static void dbg_caps_text(uint16_t caps, char *out, size_t cap)
{
    static const struct {
        uint16_t    bit;
        const char *name;
    } tab[] = {
        { V4P_CAP_PORT_MODE, "Port-Modus" },
        { V4P_CAP_GET_INPUT, "Eingaenge" },
        { V4P_CAP_GET_HID,   "HID-Info" },
        { V4P_CAP_HID_BT,    "BT" },
        { V4P_CAP_HID_BLE,   "BLE" },
        { V4P_CAP_MOUSE,     "Maus" },
        { V4P_CAP_PAD,       "Pad" },
        { V4P_CAP_BATTERY,   "Batterie" }
    };
    size_t   n     = 0u;
    int      first = 1;
    unsigned i;

    if (out == NULL || cap == 0u) {
        return;
    }
    out[0] = '\0';
    for (i = 0u; i < (unsigned)(sizeof(tab) / sizeof(tab[0])); i++) {
        const char *name;
        size_t      len;
        size_t      k;

        if ((caps & tab[i].bit) == 0u) {
            continue;
        }
        name = tab[i].name;
        len  = strlen(name);
        if (n + (first ? 0u : 1u) + len + 1u > cap) {
            break;
        }
        if (first == 0) {
            out[n++] = ',';
        }
        for (k = 0u; k < len; k++) {
            out[n++] = name[k];
        }
        first = 0;
    }
    if (first != 0) {
        const char *keine = "(keine)";

        for (n = 0u; keine[n] != '\0' && n + 1u < cap; n++) {
            out[n] = keine[n];
        }
    }
    out[n] = '\0';
}

void v4_dbg_info(const v4p_info_t *info)
{
    char caps[160];

    if (info == NULL) {
        return;
    }
    dbg_caps_text(info->caps, caps, sizeof(caps));
    v4_dbg("[info] proto=%u fw=%u write=%u read=%u chunk=%u max_dev=%u "
           "caps=0x%04X (%s)\n",
           (unsigned)info->proto_ver, (unsigned)info->fw_ver,
           (unsigned)info->write_frame_len, (unsigned)info->read_frame_len,
           (unsigned)info->chunk, (unsigned)info->max_devices,
           (unsigned)info->caps, caps);
}

void v4_dbg_status(const v4p_status_t *st)
{
    int geaendert;

    if (st == NULL) {
        return;
    }
    geaendert = (s_st_valid == 0
                 || st->state != s_st_alt.state
                 || st->conn_index != s_st_alt.conn_index
                 || st->scan_active != s_st_alt.scan_active
                 || st->dev_count != s_st_alt.dev_count
                 || st->scan_gen != s_st_alt.scan_gen) ? 1 : 0;
    s_st_alt   = *st;
    s_st_valid = 1;
    if (geaendert == 0) {
        return;
    }
    v4_dbg("[status] Zustand %u (%s), Verbindung %s, Scan %s, Geraete %u, "
           "Scan-Generation %u\n",
           (unsigned)st->state, v4p_bt_state_name(st->state),
           (st->conn_index == V4P_NO_INDEX) ? "keine" : "aktiv",
           (st->scan_active != 0u) ? "laeuft" : "aus",
           (unsigned)st->dev_count, (unsigned)st->scan_gen);
}

void v4_dbg_sitzung(const v4p_status_t *st)
{
    int neu;

    if (st == NULL) {
        return;
    }
    neu = (s_sitz_valid == 0
           || st->state != s_sitz_state
           || st->conn_index != s_sitz_index) ? 1 : 0;
    s_sitz_state = st->state;
    s_sitz_index = st->conn_index;
    s_sitz_valid = 1;
    if (neu == 0) {
        return;
    }
    v4_dbg("[sitzung] Zustand %s, Index %s\n",
           v4p_bt_state_name(st->state),
           (st->conn_index == V4P_NO_INDEX) ? "keiner" : "vergeben");
    if (st->conn_index != V4P_NO_INDEX) {
        v4_dbg("[sitzung] Sitzung an Index %u vergeben (Scan-Generation %u)\n",
               (unsigned)st->conn_index, (unsigned)st->scan_gen);
    }
}

void v4_dbg_hid(const v4p_hid_t *hid)
{
    int geaendert;

    if (hid == NULL) {
        return;
    }
    geaendert = (s_hid_valid == 0
                 || hid->kind != s_hid_alt.kind
                 || hid->transport != s_hid_alt.transport
                 || hid->vid != s_hid_alt.vid
                 || hid->pid != s_hid_alt.pid
                 || hid->battery != s_hid_alt.battery
                 || hid->report_len != s_hid_alt.report_len
                 || strcmp(hid->name, s_hid_alt.name) != 0) ? 1 : 0;
    s_hid_alt   = *hid;
    s_hid_valid = 1;
    if (geaendert == 0) {
        return;
    }
    if (hid->kind == V4P_HID_KIND_NONE) {
        v4_dbg("[hid] kein Controller verbunden\n");
        return;
    }
    v4_dbg("[hid] %s, Art %s, Transport %s, VID:PID %04X:%04X, "
           "Report-ID %u, Reportlaenge %u, Batterie %s\n",
           (hid->name[0] != '\0') ? hid->name : "(ohne Namen)",
           v4p_hid_kind_name(hid->kind), v4p_transport_name(hid->transport),
           (unsigned)hid->vid, (unsigned)hid->pid,
           (unsigned)hid->report_id, (unsigned)hid->report_len,
           (hid->battery == V4P_HID_BATTERY_UNKNOWN)
               ? "unbekannt" : "gemeldet");
}

void v4_dbg_input(const v4p_input_t *in)
{
    char        gezogen[80];
    uint8_t     geaendert;
    v4p_input_t alt;                  /* Stand VOR dieser Antwort */
    int         port;
    int         erste;

    if (in == NULL) {
        return;
    }
    erste = (s_in_valid == 0) ? 1 : 0;

    /* WICHTIG: erst den ALTEN Stand sichern, dann nachziehen. Vorher stand hier
     * `s_in_alt = *in;` VOR der Differenzbildung -- damit war "vorher" immer
     * gleich "jetzt", die Differenz immer 0, und die Zeile [taste] konnte NIE
     * erscheinen. Ein gedrueckter Feuerknopf ging also spurlos vorbei, obwohl
     * genau das die Aussage ist, auf die es ankommt. */
    alt        = s_in_alt;
    s_in_alt   = *in;                 /* immer nachziehen */
    s_in_valid = 1;

    geaendert = (uint8_t)((in->state[0] ^ alt.state[0])
                          | (in->state[1] ^ alt.state[1])
                          | (in->level[0] ^ alt.level[0])
                          | (in->level[1] ^ alt.level[1])
                          | (uint8_t)(in->level_valid ^ alt.level_valid));

    if (erste == 0 && geaendert == 0u) {
        return;
    }

    /* Zustandszeile: die Kurzform, die auch in der Transaktionszeile steht. */
    if (in->level_valid != 0u) {
        v4_dbg("[eingang] state0=0x%02X state1=0x%02X level0=0x%02X "
               "level1=0x%02X steps=%lu/%lu\n",
               (unsigned)in->state[0], (unsigned)in->state[1],
               (unsigned)in->level[0], (unsigned)in->level[1],
               (unsigned long)in->steps_x, (unsigned long)in->steps_y);
    } else {
        v4_dbg("[eingang] state0=0x%02X state1=0x%02X level=unbekannt "
               "(10-Byte-Fassung) steps=%lu/%lu\n",
               (unsigned)in->state[0], (unsigned)in->state[1],
               (unsigned long)in->steps_x, (unsigned long)in->steps_y);
    }

    if (erste != 0) {
        return;
    }

    /* Tastendruck-Wechsel je Buchse: was dazukam, was losgelassen wurde. */
    for (port = 0; port < (int)V4P_PORT_COUNT; port++) {
        uint8_t jetzt  = in->state[port];
        uint8_t vorher = alt.state[port];
        uint8_t diff   = (uint8_t)(jetzt ^ vorher);
        char    dazu[80];
        char    weg[80];

        if (diff == 0u) {
            continue;
        }
        v4p_bits_text((uint8_t)(diff & jetzt), dazu, sizeof(dazu));
        v4p_bits_text((uint8_t)(diff & vorher), weg, sizeof(weg));
        v4p_bits_text(jetzt, gezogen, sizeof(gezogen));
        v4_dbg("[taste] Port %d: gedrueckt=[%s] geloest=[%s] gezogen=[%s]\n",
               port, dazu, weg, gezogen);
    }
}

void v4_dbg_modes(const uint8_t mode[V4P_PORT_COUNT], const char *warum)
{
    int geaendert;

    if (mode == NULL) {
        return;
    }
    geaendert = (s_m_valid == 0
                 || mode[0] != s_m_alt[0]
                 || mode[1] != s_m_alt[1]) ? 1 : 0;
    s_m_alt[0] = mode[0];
    s_m_alt[1] = mode[1];
    s_m_valid  = 1;
    if (geaendert == 0) {
        return;
    }
    v4_dbg("[modus] Buchse 0 = %s, Buchse 1 = %s%s%s\n",
           v4p_port_mode_name(mode[0]), v4p_port_mode_name(mode[1]),
           (warum != NULL && warum[0] != '\0') ? " -- " : "",
           (warum != NULL) ? warum : "");
}

void v4_dbg_devs(const v4p_dev_t *tab, uint8_t n)
{
    uint8_t i;

    v4_dbg("[scan] %u Geraet(e) gefunden\n", (unsigned)n);
    if (tab == NULL) {
        return;
    }
    for (i = 0u; i < n; i++) {
        v4_dbg("[scan]   [%u] %s (%02X:%02X:%02X:%02X:%02X:%02X)\n",
               (unsigned)i,
               (tab[i].name[0] != '\0') ? tab[i].name : "(ohne Namen)",
               tab[i].bda[0], tab[i].bda[1], tab[i].bda[2],
               tab[i].bda[3], tab[i].bda[4], tab[i].bda[5]);
    }
}

void v4_dbg_fehler(const char *was, uint8_t rc, const v4_master_t *m)
{
    v4_dbg("[fehler] %s: %s (rc=0x%02X)\n",
           (was != NULL) ? was : "?", v4_strerror(rc), (unsigned)rc);
    if (m != NULL) {
        v4_dbg("[fehler]   tx_frames=%lu rx_frames=%lu wiederholungen=%lu "
               "letzte_Rahmenpruefung=%s busfehler=0x%08lX (%s)\n",
               (unsigned long)m->tx_frames, (unsigned long)m->rx_frames,
               (unsigned long)m->retries, v4_check_str(m->last_check),
               v4_plat_last_error(), v4_plat_error_text());
    }
    (void)snprintf(s_letzter_status, sizeof(s_letzter_status),
                   "%s (0x%02X)", v4_strerror(rc), (unsigned)rc);
}

/* ------------------------------------------------------------------ */
/* Lebenszeichen und Schlusszeile                                      */
/* ------------------------------------------------------------------ */

void v4_dbg_herz_zusatz(const char *text)
{
    if (text == NULL) {
        s_herz_zusatz[0] = '\0';
        return;
    }
    /* Kopieren, nicht merken: der Aufrufer darf einen fluechtigen Puffer
     * uebergeben (die Oberflaeche baut den Vergleich je Takt neu). */
    (void)snprintf(s_herz_zusatz, sizeof(s_herz_zusatz), "%s", text);
}

void v4_dbg_herz(const v4_master_t *m)
{
    const char *trenner = (s_herz_zusatz[0] != '\0') ? " | " : "";
    const char *text    = s_herz_zusatz;

    s_herz_n++;
    if (m != NULL) {
        v4_dbg("[herz] takt=%lus aufrufe=%lu tx_frames=%lu rx_frames=%lu "
               "wiederholungen=%lu verworfene_rahmen=%lu letzter_status=%s%s%s\n",
               (unsigned long)s_herz_n, (unsigned long)s_zeilen,
               (unsigned long)m->tx_frames, (unsigned long)m->rx_frames,
               (unsigned long)m->retries, (unsigned long)s_verworfen,
               s_letzter_status, trenner, text);
    } else {
        v4_dbg("[herz] takt=%lus aufrufe=%lu wiederholungen=%lu "
               "verworfene_rahmen=%lu letzter_status=%s%s%s\n",
               (unsigned long)s_herz_n, (unsigned long)s_zeilen,
               (unsigned long)s_wiederholungen, (unsigned long)s_verworfen,
               s_letzter_status, trenner, text);
    }
}

void v4_dbg_ende(const v4_master_t *m)
{
    if (m != NULL) {
        v4_dbg("[ende] %s %s beendet: i2c_zeilen=%lu tx_frames=%lu "
               "rx_frames=%lu wiederholungen=%lu verworfene_rahmen=%lu "
               "letzter_status=%s\n",
               s_prog, s_ver, (unsigned long)s_zeilen,
               (unsigned long)m->tx_frames, (unsigned long)m->rx_frames,
               (unsigned long)m->retries, (unsigned long)s_verworfen,
               s_letzter_status);
    } else {
        v4_dbg("[ende] %s %s beendet: i2c_zeilen=%lu verworfene_rahmen=%lu "
               "letzter_status=%s\n",
               s_prog, s_ver, (unsigned long)s_zeilen,
               (unsigned long)s_verworfen, s_letzter_status);
    }
}
