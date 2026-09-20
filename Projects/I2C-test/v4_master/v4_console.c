/*
 * v4_console.c -- interaktives Konsolenprogramm fuer den I2C-Master.
 *
 * Dasselbe Programm laeuft auf beiden Plattformen:
 *   Linux-Harness : gelinkt mit v4_linux_i2c.c, argv[1] = /dev/i2c-N
 *   Vampire V4    : gelinkt mit v4_amiga_i2c.c, argv[1] wird ignoriert
 *
 * Es fuehrt den Ablauf aus PROTOCOL_V4_MASTER.md §13 vor: PING, Status,
 * Scan, Geraeteliste, Auswahl, CONNECT, SD-Karte, Verzeichnis, Datei lesen.
 */

#include "v4_master.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned long g_blocks;
static unsigned long g_bytes;

/* ------------------------------------------------------------------ */
/* Logdatei                                                           */
/*                                                                     */
/* Alle Texte gehen ueber v4_msg(): erst auf die Konsole, dann in die   */
/* Logdatei (Vorgabe: Programs:test/v4_console.log auf der V4, mit -o    */
/* ein anderer Pfad, mit -n abgeschaltet). Das ist beim Suchen eines     */
/* Absturzes entscheidend: das Fenster ist nach dem Absturz weg, die     */
/* Datei nicht. Jede Zeile wird sofort geschrieben und geflusht.         */
/* ------------------------------------------------------------------ */

static int         g_use_log    = 1;   /* 1 Vorgabe: Logdatei an, 0 mit -n,
                                        * 2 mit -o <pfad> */
static int         g_settle     = 0;   /* -t <ticks>: Wartezeit je Logzeile */
static const char *g_log_path   = NULL;

static void v4_msg(const char *fmt, ...)
{
    /* Absichtlich statisch: 512 Byte weniger Stack pro Aufruf. Das Programm ist
     * einprozessig und v4_msg wird nie verschachtelt gerufen. */
    static char buf[512];
    va_list ap;
    int     n;

    va_start(ap, fmt);
    n = vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    if (n < 0) {
        return;
    }
    if ((size_t)n > sizeof(buf) - 1u) {
        n = (int)(sizeof(buf) - 1u);    /* gekuerzt, aber terminiert */
    }

    fputs(buf, stdout);
    fflush(stdout);                     /* nichts darf im Puffer bleiben */
    if (g_use_log != 0) {
        (void)v4_plat_log_write(buf, (unsigned long)n);
    }
}

/* ------------------------------------------------------------------ */
/* Trace -- damit auf der V4 sichtbar wird, WO es knallt.              */
/*                                                                     */
/* Jede Zeile wird sofort geflusht: bei einem Absturz darf nichts im    */
/* Puffer haengenbleiben. Die letzte Zeile vor dem Absturz benennt den  */
/* Schritt, in dem es passiert ist.                                    */
/* ------------------------------------------------------------------ */

static int g_trace = 0;     /* Vorgabe: aus -- mit -a (Diagnose) an */
static int g_diag  = 0;     /* -a: Trace + Warteschleifen + Zusatzinfos  */
static int g_hex   = 0;     /* mit -x: Antwortbytes mit ausgeben */

/* Klartext zum Befehlscode -- steht im Trace hinter cmd=0x.. in Klammern. */
static const char *cmd_name(uint8_t cmd)
{
    switch (cmd) {
    case V4P_CMD_PING:        return "PING";
    case V4P_CMD_GET_STATUS:  return "GET_STATUS";
    case V4P_CMD_GET_INFO:    return "GET_INFO";
    case V4P_CMD_SCAN_START:  return "SCAN_START";
    case V4P_CMD_SCAN_STOP:   return "SCAN_STOP";
    case V4P_CMD_DEV_COUNT:   return "DEV_COUNT";
    case V4P_CMD_DEV_GET:     return "DEV_GET";
    case V4P_CMD_CONNECT:     return "CONNECT";
    case V4P_CMD_CONNECT_BDA: return "CONNECT_BDA";
    case V4P_CMD_DISCONNECT:  return "DISCONNECT";
    case V4P_CMD_FORGET:      return "FORGET";
    case V4P_CMD_SD_MOUNT:    return "SD_MOUNT";
    case V4P_CMD_SD_INFO:     return "SD_INFO";
    case V4P_CMD_SET_CHUNK:   return "SET_CHUNK";
    case V4P_CMD_PATH_CLEAR:  return "PATH_CLEAR";
    case V4P_CMD_PATH_APPEND: return "PATH_APPEND";
    case V4P_CMD_DIR_OPEN:    return "DIR_OPEN";
    case V4P_CMD_DIR_NEXT:    return "DIR_NEXT";
    case V4P_CMD_DIR_CLOSE:   return "DIR_CLOSE";
    case V4P_CMD_FILE_OPEN:   return "FILE_OPEN";
    case V4P_CMD_FILE_READ:   return "FILE_READ";
    case V4P_CMD_FILE_CLOSE:  return "FILE_CLOSE";
    case V4P_CMD_PLAY_FILE:   return "PLAY_FILE";
    case V4P_CMD_STOP_PLAY:   return "STOP_PLAY";
    case V4P_CMD_RESET:       return "RESET";
    default:                  return "?";
    }
}

/* Klartext zum Zustand des Slaves. */
static const char *state_name(uint8_t state)
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

static const char *trace_name(uint8_t event)
{
    switch (event) {
    case V4_TR_TX_BEGIN: return "TX-Beginn";
    case V4_TR_TX_END:   return "TX-Ende";
    case V4_TR_WAIT:     return "Warten";
    case V4_TR_RX_BEGIN: return "RX-Beginn";
    case V4_TR_RX_END:   return "RX-Ende";
    case V4_TR_CHECK:    return "Pruefung";
    case V4_TR_RETRY:    return "WIEDERHOLUNG";
    case V4_TR_BUSY:     return "BUSY";
    case V4_TR_DONE:     return "Ergebnis";
    default:             return "?";
    }
}

static void dump_hex(const uint8_t *d, uint16_t len)
{
    uint16_t i;
    uint16_t n = (len > 32u) ? 32u : len;

    for (i = 0; i < n; i++) {
        v4_msg("%02X%s", (unsigned)d[i], ((i % 16u) == 15u) ? "\n            " : " ");
    }
    if (len > n) {
        v4_msg("... (%u weitere)", (unsigned)(len - n));
    }
    v4_msg("\n");
    fflush(stdout);
}

static void console_trace(const v4_trace_t *ev, void *ctx)
{
    (void)ctx;
    if (!g_trace) {
        return;
    }

    v4_msg("[trace] %-12s cmd=0x%02X (%-11s) seq=%3u ", trace_name(ev->event),
           (unsigned)ev->cmd, cmd_name(ev->cmd), (unsigned)ev->seq);

    switch (ev->event) {
    case V4_TR_TX_BEGIN:
        v4_msg("schreibe %u Byte\n", (unsigned)ev->len);
        break;
    case V4_TR_TX_END:
        v4_msg("rc=%d\n", ev->rc);
        if (ev->rc != 0) {
            v4_msg("            Busfehler 0x%08lX: %s\n", v4_plat_last_error(),
                   v4_plat_error_text());
        }
        break;
    case V4_TR_WAIT:
        v4_msg("%lu us warten\n", (unsigned long)ev->us);
        break;
    case V4_TR_RX_BEGIN:
        v4_msg("lese %u Byte ...\n", (unsigned)ev->len);
        break;
    case V4_TR_RX_END:
        v4_msg("rc=%d\n", ev->rc);
        if (ev->rc != 0) {
            v4_msg("            Busfehler 0x%08lX: %s\n", v4_plat_last_error(),
                   v4_plat_error_text());
        } else if (g_hex && ev->data != NULL) {
            v4_msg("            ");
            dump_hex(ev->data, ev->len);
        }
        break;
    case V4_TR_CHECK:
        v4_msg("%s (status=0x%02X len=%u)\n", v4_check_str(ev->rc),
               (unsigned)ev->status, (unsigned)ev->len);
        break;
    case V4_TR_RETRY:
        v4_msg("neuer Versuch, Grund: %s\n", v4_check_str(ev->rc));
        break;
    case V4_TR_BUSY:
        v4_msg("Slave sagt BUSY, neuer Versuch\n");
        break;
    case V4_TR_DONE:
        v4_msg("%s (status=0x%02X len=%u)\n",
               v4_strerror((uint8_t)ev->rc), (unsigned)ev->status,
               (unsigned)ev->len);
        break;
    default:
        v4_msg("\n");
        break;
    }
    fflush(stdout);     /* nichts darf im Puffer haengenbleiben */
}

/* Rohen Busfehler zeigen -- auf der V4 gibt es keinen Debugger, deshalb ist
 * der Code der Library die wichtigste Diagnose. */
static void print_bus_error(void)
{
    v4_msg("  letzter Busfehler: 0x%08lX (%s)\n", v4_plat_last_error(),
           v4_plat_error_text());
}

/* Statuszeilen -- wird auch nach PLAY_FILE/STOP_PLAY erneut ausgegeben, damit
 * der Wechsel in `audio_flags` sichtbar ist. */
static void print_status(const v4p_status_t *st)
{
    v4_msg("state=%u (%s) conn_index=0x%02X dev_count=%u scan_active=%u\n",
           (unsigned)st->state, state_name(st->state),
           (unsigned)st->conn_index, (unsigned)st->dev_count,
           (unsigned)st->scan_active);
    v4_msg("sd: mounted=%u card_present=%u  audio_flags=0x%02X (%s%s)\n",
           (unsigned)st->sd_mounted, (unsigned)st->sd_card_present,
           (unsigned)st->audio_flags,
           ((st->audio_flags & V4P_AUDIO_A2DP_STREAMING) != 0u) ? "A2DP " : "",
           ((st->audio_flags & V4P_AUDIO_SD_PLAYBACK) != 0u)
               ? "SD-Wiedergabe" : "");
    v4_msg("scan_gen=%u free=%lu kB chunk=%u\n",
           (unsigned)st->scan_gen, (unsigned long)st->sd_free_kb,
           (unsigned)st->chunk);
}


static void file_cb(uint16_t block, const uint8_t *data, uint16_t len,
                    void *ctx)
{
    (void)data;
    (void)ctx;
    g_blocks++;
    g_bytes += (unsigned long)len;
    if (block < 2u || block == 0xFFFFu || (block % 64u) == 0u) {
        v4_msg("  Block %5u: %4u Byte (gesamt %lu)\n", (unsigned)block,
               (unsigned)len, g_bytes);
    }
}

/* Pfad whatever oeffnen: kurze Pfade direkt, lange ueber PATH_CLEAR/APPEND (§10). */
static uint8_t open_file_path(v4_master_t *m, const char *path, uint8_t *handle,
                              uint32_t *size, uint8_t *attr)
{
    size_t n = strlen(path);
    size_t off;
    uint8_t rc;

    if (n <= (size_t)V4P_WRITE_PAYLOAD_MAX) {
        return v4_file_open(m, path, handle, size, attr);
    }

    rc = v4_path_clear(m);
    if (rc != V4P_ST_OK) {
        return rc;
    }
    for (off = 0u; off < n; ) {
        char   frag[V4P_WRITE_PAYLOAD_MAX + 1u];
        size_t take = n - off;

        if (take > (size_t)V4P_WRITE_PAYLOAD_MAX) {
            take = (size_t)V4P_WRITE_PAYLOAD_MAX;
        }
        memcpy(frag, path + off, take);
        frag[take] = '\0';
        rc = v4_path_append(m, frag);
        if (rc != V4P_ST_OK) {
            return rc;
        }
        off += take;
    }
    return v4_file_open(m, NULL, handle, size, attr);
}

/* §8.2 ausgeschrieben: BUSY-Runden macht v4_file_read intern. */
static uint8_t read_file_manual(v4_master_t *m, const char *path)
{
    uint8_t   handle = 0u;
    uint32_t  size = 0u;
    uint8_t   attr = 0u;
    uint16_t  block = 0u;
    uint32_t  total = 0u;
    uint8_t  *scratch;
    size_t    need;
    uint8_t   rc;

    need = (size_t)V4P_BULK_OVERHEAD + (size_t)m->chunk;
    scratch = (uint8_t *)malloc(need);
    if (scratch == NULL) {
        return V4_ERR_NOMEM;
    }

    rc = open_file_path(m, path, &handle, &size, &attr);
    if (rc != V4P_ST_OK) {
        free(scratch);
        return rc;
    }
    v4_msg("  Datei: %lu Byte, attr 0x%02X\n", (unsigned long)size,
           (unsigned)attr);

    for (;;) {
        v4p_bulk_t b;

        rc = v4_file_read(m, handle, block, scratch, &b);
        if (rc == V4P_ST_END) {
            rc = V4P_ST_OK;
            break;
        }
        if (rc != V4P_ST_OK) {
            break;
        }
        if (b.len == 0u) {
            break;
        }
        total += (uint32_t)b.len;
        file_cb(b.block, b.data, b.len, NULL);
        if (b.len < m->chunk) {
            break;
        }
        if (block == 0xFFFFu) {
            rc = V4_ERR_ARG;
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
    free(scratch);
    v4_msg("  gelesen: %lu Byte\n", (unsigned long)total);
    return rc;
}

/* Schalter lesen. Muss VOR der ersten Ausgabe passieren, damit -s/-o schon
 * fuer die erste Zeile gilt. */
static int parse_args(int argc, char **argv, const char **dev_out)
{
    const char *dev = NULL;
    int         i;

    for (i = 1; i < argc; i++) {
        if (argv[i][0] == '-') {
            if (argv[i][1] == 'a') {
                g_diag  = 1;                /* Diagnose: alles an */
                g_trace = 1;
            } else if (argv[i][1] == 'q') {
                g_trace = 0;                /* still (ist die Vorgabe) */
            } else if (argv[i][1] == 'x') {
                g_hex = 1;                  /* Antwortbytes zeigen */
            } else if (argv[i][1] == 't') {
                if (i + 1 >= argc) {
                    v4_msg("-t braucht Ticks (1/50 s), z.B. -t 50\n");
                    return 5;
                }
                g_settle = atoi(argv[++i]); /* Wartezeit je Logzeile */
            } else if (argv[i][1] == 'n') {
                g_use_log = 0;           /* keine Logdatei, nur Konsole */
            } else if (argv[i][1] == 'o') {
                if (i + 1 >= argc) {
                    v4_msg("-o braucht einen Pfad, z.B. "
                           "-o Programs:test/v4_console.log\n");
                    return 5;
                }
                g_log_path = argv[++i];
                g_use_log  = 2;          /* ausdruecklicher Pfad */
            } else {
                v4_msg("Unbekannter Schalter '%s' (erlaubt: -a -q -x -n "
                       "-o <pfad> -t <ticks>)\n", argv[i]);
                return 5;
            }
        } else if (dev == NULL) {
            dev = argv[i];                  /* nur Linux: /dev/i2c-N */
        }
    }
    *dev_out = dev;
    return 0;
}

/* ------------------------------------------------------------------ */
/* Dateiauswahl: browsen statt Pfad eintippen                          */
/*                                                                     */
/* Der Slave liefert Verzeichniseintraege einzeln (DIR_OPEN, DIR_NEXT,  */
/* DIR_CLOSE). Fuer eine Auswahl per Nummer wird die Liste deshalb erst  */
/* vollstaendig eingesammelt, sortiert (Verzeichnisse zuerst) und        */
/* seitenweise angezeigt.                                               */
/* ------------------------------------------------------------------ */

#define BROWSE_PAGE 20u          /* Eintraege je Seite                 */
#define BROWSE_MAX  512u         /* Obergrenze je Verzeichnis (Speicher) */

typedef struct {
    char     name[V4P_NAME_BUF + 1u];
    uint32_t size;
    uint8_t  attr;
} browse_ent_t;

typedef struct {
    browse_ent_t *ent;
    unsigned      count;
    unsigned      cap;
    int           overflow;
} browse_list_t;

static void browse_add(const v4p_dirent_t *e, void *ctx)
{
    browse_list_t *l = (browse_list_t *)ctx;
    size_t         n;

    if (l->count >= l->cap) {
        unsigned      newcap = (l->cap == 0u) ? 64u : (l->cap * 2u);
        browse_ent_t *neu;

        if (newcap > BROWSE_MAX) {
            newcap = BROWSE_MAX;
        }
        if (l->count >= newcap) {
            l->overflow = 1;
            return;
        }
        neu = (browse_ent_t *)realloc(l->ent,
                                      (size_t)newcap * sizeof(browse_ent_t));
        if (neu == NULL) {
            l->overflow = 1;
            return;
        }
        l->ent = neu;
        l->cap = newcap;
    }
    n = strlen(e->name);
    if (n > (size_t)V4P_NAME_BUF) {
        n = (size_t)V4P_NAME_BUF;
    }
    memcpy(l->ent[l->count].name, e->name, n);
    l->ent[l->count].name[n] = '\0';
    l->ent[l->count].size    = e->size;
    l->ent[l->count].attr    = e->attr;
    l->count++;
}

/* Verzeichnisse zuerst, dann alphabetisch ohne Ruecksicht auf Gross/Klein. */
static int browse_cmp(const void *a, const void *b)
{
    const browse_ent_t *x = (const browse_ent_t *)a;
    const browse_ent_t *y = (const browse_ent_t *)b;
    const char         *p = x->name;
    const char         *q = y->name;
    int                 dx = ((x->attr & V4P_ATTR_DIR) != 0u) ? 0 : 1;
    int                 dy = ((y->attr & V4P_ATTR_DIR) != 0u) ? 0 : 1;

    if (dx != dy) {
        return dx - dy;
    }
    for (;;) {
        int cx = (int)(unsigned char)*p++;
        int cy = (int)(unsigned char)*q++;

        if (cx >= 'a' && cx <= 'z') {
            cx -= 'a' - 'A';
        }
        if (cy >= 'a' && cy <= 'z') {
            cy -= 'a' - 'A';
        }
        if (cx != cy) {
            return cx - cy;
        }
        if (cx == 0) {
            return 0;
        }
    }
}

/* Name an einen Pfad anhaengen ("/" dazwischen, nie fuehrend). */
static void path_append(char *buf, size_t cap, const char *name)
{
    size_t n = strlen(buf);
    size_t k;

    if (n > 0u && n + 1u < cap) {
        buf[n++] = '/';
        buf[n]   = '\0';
    }
    k = strlen(name);
    if (n + k >= cap) {
        k = (cap > n + 1u) ? (cap - n - 1u) : 0u;
    }
    memcpy(buf + n, name, k);
    buf[n + k] = '\0';
}

/* Wiedergabe starten, laufen lassen, auf Enter wieder stoppen. */
static void do_play(v4_master_t *m, const char *path)
{
    v4p_status_t st;
    uint8_t      rc = v4_play_file(m, path);
    char         line[32];

    v4_msg("PLAY_FILE: %s\n", v4_strerror(rc));

    /* Zweiter Versuch mit fuehrendem Schraegstrich: der Slave bekommt Pfade
     * relativ zum Mount, aber wenn er sie absolut erwartet, hilft diese Form.
     * Der Mitschnitt zeigt dann, welche funktioniert. */
    if (rc == V4P_ST_NOT_FOUND && path[0] != '/') {
        char alt[V4P_PATH_MAX + 2u];

        alt[0] = '/';
        if (strlen(path) < sizeof(alt) - 1u) {
            memcpy(alt + 1u, path, strlen(path) + 1u);
            v4_msg("zweiter Versuch mit fuehrendem \"/\": %s\n", alt);
            rc = v4_play_file(m, alt);
            v4_msg("PLAY_FILE: %s\n", v4_strerror(rc));
        }
    }
    if (rc != V4P_ST_OK) {
        return;
    }
    if (v4_get_status(m, &st) == V4P_ST_OK) {
        print_status(&st);
    }
    v4_msg("Wiedergabe laeuft -- Enter zum Stoppen ...");
    fflush(stdout);
    if (fgets(line, sizeof(line), stdin) == NULL) {
        v4_msg("\n(Eingabe beendet -- Wiedergabe wird gestoppt)\n");
    }
    rc = v4_stop_play(m);
    v4_msg("STOP_PLAY: %s\n", v4_strerror(rc));
    if (v4_get_status(m, &st) == V4P_ST_OK) {
        print_status(&st);
    }
}

/* Datei pruefen (oeffnen und gleich wieder schliessen). */
static uint8_t file_probe(v4_master_t *m, const char *path, uint32_t *size)
{
    uint8_t handle = 0u;
    uint8_t attr   = 0u;
    uint8_t rc     = v4_file_open(m, path, &handle, size, &attr);

    if (rc == V4P_ST_OK) {
        (void)v4_file_close(m, handle);
    }
    return rc;
}

/* Browsen bis 'q'. Enter beendet, Nummern waehlen, n/p blaettern,
 * u = eine Ebene hoch, r = Wurzel. */
static void browse_files(v4_master_t *m)
{
    char dir[V4P_PATH_MAX + 1u];

    dir[0] = '\0';                         /* Wurzel */
    for (;;) {
        browse_list_t l;
        unsigned      pages;
        unsigned      page = 0u;
        uint8_t       rc;
        char          line[32];

        memset(&l, 0, sizeof(l));
        v4_msg("\n-- Dateien --\nPfad: /%s\n", dir);
        rc = v4_list_dir(m, dir, browse_add, &l);
        if (rc != V4P_ST_OK) {
            v4_msg("DIR: %s\n", v4_strerror(rc));
            free(l.ent);
            return;
        }
        if (l.count > 1u) {
            qsort(l.ent, l.count, sizeof(browse_ent_t), browse_cmp);
        }
        pages = (l.count + BROWSE_PAGE - 1u) / BROWSE_PAGE;
        if (pages == 0u) {
            pages = 1u;
        }
        v4_msg("%u Eintraege%s\n", l.count,
               (l.overflow != 0) ? " (Liste abgeschnitten)" : "");

        for (;;) {                          /* Seiten-Schleife */
            unsigned first = page * BROWSE_PAGE;
            unsigned last  = first + BROWSE_PAGE;
            unsigned i;
            unsigned idx;

            if (last > l.count) {
                last = l.count;
            }
            v4_msg("\nSeite %u/%u\n", page + 1u, pages);
            for (i = first; i < last; i++) {
                int isdir = ((l.ent[i].attr & V4P_ATTR_DIR) != 0u);

                v4_msg("  [%3u] %-6s %10lu  %s%s\n", i,
                       isdir ? "<DIR>" : "Datei",
                       (unsigned long)l.ent[i].size, l.ent[i].name,
                       isdir ? "/" : "");
            }
            v4_msg("Nummer = auswaehlen, n = weiter, p = zurueck, u = hoch, "
                   "r = Wurzel, Enter/q = Ende: ");
            fflush(stdout);
            if (fgets(line, sizeof(line), stdin) == NULL) {
                free(l.ent);
                return;
            }
            if (line[0] == 'q' || line[0] == 'Q' || line[0] == '\n'
                || line[0] == '\r' || line[0] == '\0') {
                free(l.ent);
                return;
            }
            if (line[0] == 'r' || line[0] == 'R') {
                dir[0] = '\0';
                break;                      /* neu einlesen */
            }
            if (line[0] == 'u' || line[0] == 'U') {
                size_t n = strlen(dir);

                while (n > 0u && dir[n - 1u] != '/') {
                    n--;
                }
                if (n > 0u) {
                    n--;                    /* Schraegstrich weg */
                }
                dir[n] = '\0';
                break;                      /* neu einlesen */
            }
            if (line[0] == 'n' || line[0] == 'N') {
                if (page + 1u < pages) {
                    page++;
                }
                continue;
            }
            if (line[0] == 'p' || line[0] == 'P') {
                if (page > 0u) {
                    page--;
                }
                continue;
            }

            idx = (unsigned)atoi(line);
            if (idx >= l.count) {
                v4_msg("Kein Eintrag [%u].\n", idx);
                continue;
            }
            if ((l.ent[idx].attr & V4P_ATTR_DIR) != 0u) {
                path_append(dir, sizeof(dir), l.ent[idx].name);
                break;                      /* hinein */
            }

            /* Datei: Pfad bauen, pruefen, Aktion anbieten. */
            {
                char     fpath[V4P_PATH_MAX + 1u];
                uint32_t fsize = 0u;
                uint8_t  prc;

                fpath[0] = '\0';
                if (dir[0] != '\0') {
                    size_t n = strlen(dir);

                    if (n >= sizeof(fpath)) {
                        n = sizeof(fpath) - 1u;
                    }
                    memcpy(fpath, dir, n);
                    fpath[n] = '\0';
                }
                path_append(fpath, sizeof(fpath), l.ent[idx].name);

                v4_msg("\nGewaehlt: %s (%lu Byte laut Liste)\n", fpath,
                       (unsigned long)l.ent[idx].size);
                prc = file_probe(m, fpath, &fsize);
                if (prc == V4P_ST_OK) {
                    v4_msg("Oeffnen: OK, %lu Byte\n", (unsigned long)fsize);
                } else {
                    v4_msg("Oeffnen: %s\n", v4_strerror(prc));
                }
                v4_msg("(a)bsspielen, (l)esen/pruefen, (z)urueck, "
                       "(q)ende: ");
                fflush(stdout);
                if (fgets(line, sizeof(line), stdin) == NULL) {
                    free(l.ent);
                    return;
                }
                if (line[0] == 'q' || line[0] == 'Q') {
                    free(l.ent);
                    return;
                }
                if (line[0] == 'a' || line[0] == 'A') {
                    do_play(m, fpath);
                } else if (line[0] == 'l' || line[0] == 'L') {
                    g_blocks = 0ul;
                    g_bytes  = 0ul;
                    rc = read_file_manual(m, fpath);
                    v4_msg("FILE_READ: %s (%lu Byte in %lu Bloecken)\n",
                           v4_strerror(rc), g_bytes, g_blocks);
                    if (rc == V4_ERR_LINK) {
                        print_bus_error();
                    }
                }
                /* alles andere (auch 'z'): zurueck zur Liste */
            }
        }
        free(l.ent);
    }
}

static int console_run(const char *dev)
{
    int         i;

    v4_master_t m;
    v4p_info_t  info;
    v4p_status_t st;
    v4p_sdinfo_t si;
    uint8_t     count = 0u;
    uint8_t     rc;
    char        line[160];

    v4_msg("I2C-Master fuer den ESP32-Slave (Proto v%u)\n", V4P_PROTO_VER);
    v4_msg("========================================\n");

    if (v4p_selftest_byteorder() != 1) {
        v4_msg("Byte-Order-Selbsttest fehlgeschlagen -- Abbruch.\n");
        return 10;
    }

    v4_msg("Trace: %s%s%s\n", g_trace ? "an" : "aus",
           g_diag ? " (Diagnosemodus -a)" : " (mit -a einschaltbar)",
           g_hex ? ", Hexdump an" : "");
    v4_msg("[trace] oeffne I2C-Bus%s%s ...\n", dev ? ": " : "",
           dev ? dev : " (Standard)");
    fflush(stdout);

    if (v4_open(&m, dev) != 0) {
        v4_msg("I2C-Bus nicht verfuegbar (%s).\n", v4_plat_error_text());
        return 10;
    }
    /* Ab jetzt jeden Transaktionsschritt protokollieren. Muss NACH
     * v4_open() stehen, weil v4_init() die Struktur nullt. */
    m.trace     = console_trace;
    m.trace_ctx = NULL;

    rc = v4_ping(&m, &info);
    if (rc != V4P_ST_OK) {
        v4_msg("PING: %s\n", v4_strerror(rc));
        v4_msg("Hinweis: eine Firmware mit dem alten 64-Byte-READ-Frame\n"
               "         (proto_ver 2) liefert auf einen 128-Byte-Read keine\n"
               "         gueltige Antwort -- dann ist der Stand zu alt fuer\n"
               "         proto_ver 3. Aktuell erwartet wird proto_ver %u.\n",
               (unsigned)V4P_PROTO_VER);
        print_bus_error();
        v4_close();
        return 10;
    }
    rc = v4_check_info(&info);
    if (rc != V4P_ST_OK) {
        /* Wichtig seit der 128-Byte-Fassung: eine aeltere Firmware meldet
         * read_frame_len = 64 -- dann waere jede Antwort verschoben. */
        v4_msg("Protokoll passt nicht (%s): Slave meldet proto=%u "
               "write=%u read=%u chunk=%u; erwartet proto=%u write=%u "
               "read=%u\n", v4_strerror(rc), (unsigned)info.proto_ver,
               (unsigned)info.write_frame_len, (unsigned)info.read_frame_len,
               (unsigned)info.chunk, (unsigned)V4P_PROTO_VER,
               (unsigned)V4P_WRITE_FRAME_LEN, (unsigned)V4P_READ_FRAME_LEN);
        v4_close();
        return 10;
    }
    v4_msg("PING ok: proto=%u fw=%u write=%u read=%u bulk_max=%u chunk=%u\n",
           (unsigned)info.proto_ver, (unsigned)info.fw_ver,
           (unsigned)info.write_frame_len, (unsigned)info.read_frame_len,
           (unsigned)info.bulk_payload_max, (unsigned)info.chunk);

    v4_msg("\n-- Zustand --\n");
    rc = v4_get_status(&m, &st);
    if (rc != V4P_ST_OK) {
        v4_msg("GET_STATUS: %s\n", v4_strerror(rc));
    } else {
        print_status(&st);
    }

    v4_msg("\n-- Scan --\n");
    rc = v4_scan_start(&m, 8u, 1);
    if (rc != V4P_ST_OK) {
        v4_msg("SCAN_START: %s\n", v4_strerror(rc));
    } else if (g_diag != 0) {
        /* Nur im Diagnosemodus: die Warteschleife aus §13 vorfuehren. */
        uint16_t gen = st.scan_gen;

        v4_msg("Dauer-Scan laeuft, warte auf Aenderung der Geraeteliste");
        fflush(stdout);
        for (i = 0; i < 20; i++) {
            v4_plat_delay_us(500000u);
            rc = v4_get_status(&m, &st);
            if (rc != V4P_ST_OK) {
                v4_msg("\nGET_STATUS: %s\n", v4_strerror(rc));
                break;
            }
            if (st.scan_gen != gen) {
                break;
            }
            v4_msg(".");
            fflush(stdout);
        }
        v4_msg("\n");
    } else {
        v4_msg("Dauer-Scan gestartet (Diagnoseschritte nur mit -a).\n");
    }

    rc = v4_dev_count(&m, &count);
    if (rc == V4P_ST_OK) {
        v4_msg("Geraete: %u\n", (unsigned)count);
        for (i = 0; i < (int)count; i++) {
            v4p_dev_t d;

            rc = v4_dev_get(&m, (uint8_t)i, &d);
            if (rc == V4P_ST_OK) {
                v4_msg("  [%2d] %-32s %02X:%02X:%02X:%02X:%02X:%02X\n", i,
                       d.name, (unsigned)d.bda[0], (unsigned)d.bda[1],
                       (unsigned)d.bda[2], (unsigned)d.bda[3],
                       (unsigned)d.bda[4], (unsigned)d.bda[5]);
            } else {
                v4_msg("  [%2d] %s\n", i, v4_strerror(rc));
            }
        }
    } else {
        v4_msg("DEV_COUNT: %s\n", v4_strerror(rc));
    }

    if (count == 0u) {
        v4_msg("Keine Geraete -- Abbruch.\n");
        v4_close();
        return 0;
    }

    v4_msg("\nGeraeteindex zum Verbinden (0-%u, 'q' = Ende): ",
           (unsigned)(count - 1u));
    fflush(stdout);
    if (fgets(line, sizeof(line), stdin) == NULL) {
        v4_close();
        return 0;
    }
    if (line[0] == 'q' || line[0] == 'Q') {
        v4_close();
        return 0;
    }
    i = atoi(line);
    if (i < 0 || i >= (int)count) {
        v4_msg("Index ausserhalb des Bereichs.\n");
        v4_close();
        return 5;
    }

    v4_msg("\n-- Verbinden --\n");
    rc = v4_connect(&m, (uint8_t)i);
    if (rc != V4P_ST_OK) {
        v4_msg("CONNECT: %s\n", v4_strerror(rc));
        v4_close();
        return 10;
    }
    {
        int want = i;                   /* der eben gewaehlte Index */

        v4_msg("CONNECT gesendet, warte auf state == CONNECTED ");
        fflush(stdout);
        for (i = 0; i < 200; i++) {
            rc = v4_get_status(&m, &st);
            if (rc != V4P_ST_OK) {
                v4_msg("\nGET_STATUS: %s\n", v4_strerror(rc));
                break;
            }
            if (st.state == V4P_STATE_CONNECTED
                || st.state == V4P_STATE_IDLE) {
                break;                  /* fertig oder abgebrochen */
            }
            v4_plat_delay_us(50000u);
            v4_msg(".");
            fflush(stdout);
        }
        v4_msg("\n");

        /* Der Zustand allein sagt noch nichts: er muss zum gewaehlten Index
         * passen, und die Audio-Strecke muss stehen. Ein ausgeschaltetes
         * Headset liefert keinen A2DP-Stream. */
        if (st.state == V4P_STATE_CONNECTED && st.conn_index == (uint8_t)want) {
            v4_msg("Verbunden mit Index %u (%s).\n", (unsigned)st.conn_index,
                   state_name(st.state));
        } else if (st.state == V4P_STATE_CONNECTED) {
            v4_msg("WARNUNG: Slave meldet CONNECTED, aber mit Index %u statt "
                   "%d.\n", (unsigned)st.conn_index, want);
        } else {
            v4_msg("Verbindung NICHT bestaetigt: state=%u (%s) nach %d "
                   "Abfragen.\n", (unsigned)st.state, state_name(st.state),
                   i + 1);
        }

        if (v4_get_status(&m, &st) == V4P_ST_OK) {
            v4_msg("Status: state=%u (%s) conn_index=%u audio_flags=0x%02X "
                   "(%s%s)\n", (unsigned)st.state, state_name(st.state),
                   (unsigned)st.conn_index, (unsigned)st.audio_flags,
                   ((st.audio_flags & V4P_AUDIO_A2DP_STREAMING) != 0u)
                       ? "A2DP " : "kein A2DP",
                   ((st.audio_flags & V4P_AUDIO_SD_PLAYBACK) != 0u)
                       ? "SD-Wiedergabe" : "");
            if (st.state == V4P_STATE_CONNECTED
                && (st.audio_flags & V4P_AUDIO_A2DP_STREAMING) == 0u) {
                v4_msg("WARNUNG: verbunden, aber kein A2DP-Stream -- ist das "
                       "Headset eingeschaltet?\n");
            }
        }
    }

    v4_msg("\n-- SD-Karte --\n");
    rc = v4_sd_mount_wait(&m, 50u);
    v4_msg("SD_MOUNT: %s\n", v4_strerror(rc));
    if (rc == V4P_ST_OK) {
        if (g_diag != 0) {
            rc = v4_sd_info(&m, &si);
            if (rc == V4P_ST_OK) {
                v4_msg("total=%lu kB free=%lu kB sector=%u fat=%u\n",
                       (unsigned long)si.total_kb, (unsigned long)si.free_kb,
                       (unsigned)si.sector_size, (unsigned)si.fat_type);
            } else {
                v4_msg("SD_INFO: %s\n", v4_strerror(rc));
            }
        }

        browse_files(&m);
    }

    v4_msg("\n-- Trennen --\n");
    if (v4_get_status(&m, &st) == V4P_ST_OK && st.state == V4P_STATE_CONNECTED) {
        rc = v4_disconnect(&m);
        v4_msg("DISCONNECT: %s\n", v4_strerror(rc));
    } else {
        v4_msg("Kein DISCONNECT noetig: state=%u (%s).\n", (unsigned)st.state,
               state_name(st.state));
    }

    v4_close();
    return 0;
}


int v4_console_main(int argc, char **argv)
{
    const char *dev = NULL;
    int         rc;

    rc = parse_args(argc, argv, &dev);
    if (rc != 0) {
        return rc;
    }

    v4_plat_log_settle(g_settle);       /* 0 = sofort, 50 = eine Sekunde */

    /* Zeilen mit dem Praefix "(K) " sind Konsolen-Marken fuer die
     * Absturzlokalisierung: sie stehen VOR bzw. ZWISCHEN den DOS-Aufrufen.
     * Der Smoketest filtert sie auf beiden Seiten heraus, wenn er Konsole und
     * Logdatei vergleicht (siehe Makefile, Smoke 5). */
    if (g_use_log != 0) {
        const char *wunsch = (g_log_path != NULL) ? g_log_path
                                                  : V4_LOG_DEFAULT_AMIGA;

        v4_msg("(K) Log: Open(\"%s\") ...\n", wunsch);   /* vor dem Aufruf */
        rc = v4_plat_log_open(g_log_path);
        v4_msg("(K) Log: Open zurueck, rc=%d\n", rc);      /* nach dem Aufruf */

        if (rc == 0) {
            /* Erster Schreibvorgang: Write() + Flush(). Kommt diese Zeile noch,
             * aber die Datei bleibt leer, sitzt es in Flush(). */
            v4_msg("Logdatei: %s\n", wunsch);
            v4_msg("(K) Log: erster Schreibvorgang ok\n");
        } else {
            v4_msg("Logdatei nicht verfuegbar (%s) -- nur Konsole.\n", wunsch);
        }
    } else {
        v4_msg("(K) Logdatei: abgeschaltet (-n).\n");
    }

    rc = console_run(dev);

    /* Erst hier schliessen: so wird der Kanal auch auf jedem frueheren
     * return-Pfad sauber geschlossen. */
    v4_plat_log_close();
    return rc;
}

/* Einstiegspunkt. Fuer den Smoketest (tests/smoke_console.c) wird nur
 * v4_console_main eingebunden, damit der Mock vorher Geraete und
 * Dateisystem bestuecken kann. */
#ifndef V4_CONSOLE_NO_MAIN
int main(int argc, char **argv)
{
    return v4_console_main(argc, argv);
}
#endif
