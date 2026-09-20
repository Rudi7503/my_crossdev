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
static const char *g_log_path   = NULL;

static void v4_msg(const char *fmt, ...)
{
    char    buf[512];
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

static int g_trace = 1;     /* mit -q abschaltbar */
static int g_hex   = 0;     /* mit -x: Antwortbytes mit ausgeben */

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

    v4_msg("[trace] %-12s cmd=0x%02X seq=%3u ", trace_name(ev->event),
           (unsigned)ev->cmd, (unsigned)ev->seq);

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
    v4_msg("state=%u conn_index=0x%02X dev_count=%u scan_active=%u\n",
           (unsigned)st->state, (unsigned)st->conn_index,
           (unsigned)st->dev_count, (unsigned)st->scan_active);
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

static void dir_cb(const v4p_dirent_t *ent, void *ctx)
{
    (void)ctx;
    v4_msg("  %-8s %10lu  %s\n",
           ((ent->attr & V4P_ATTR_DIR) != 0u) ? "<DIR>" : "Datei",
           (unsigned long)ent->size, ent->name);
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
            if (argv[i][1] == 'q') {
                g_trace = 0;                /* still */
            } else if (argv[i][1] == 'x') {
                g_hex = 1;                  /* Antwortbytes zeigen */
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
                v4_msg("Unbekannter Schalter '%s' "
                       "(erlaubt: -q -x -n -o <pfad>)\n", argv[i]);
                return 5;
            }
        } else if (dev == NULL) {
            dev = argv[i];                  /* nur Linux: /dev/i2c-N */
        }
    }
    *dev_out = dev;
    return 0;
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

    v4_msg("Trace: %s%s (mit -q abschaltbar)\n", g_trace ? "an" : "aus",
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
    } else {
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
    v4_msg("CONNECT gesendet, warte auf state == CONNECTED ");
    fflush(stdout);
    for (i = 0; i < 200; i++) {
        rc = v4_get_status(&m, &st);
        if (rc != V4P_ST_OK) {
            v4_msg("\nGET_STATUS: %s\n", v4_strerror(rc));
            break;
        }
        if (st.state == V4P_STATE_CONNECTED) {
            break;
        }
        v4_plat_delay_us(50000u);
        v4_msg(".");
        fflush(stdout);
    }
    v4_msg("\n");
    if (st.state == V4P_STATE_CONNECTED) {
        v4_msg("Verbunden mit Index %u.\n", (unsigned)st.conn_index);
    } else {
        v4_msg("Verbindung nicht bestaetigt (state=%u).\n", (unsigned)st.state);
    }

    v4_msg("\n-- SD-Karte --\n");
    rc = v4_sd_mount_wait(&m, 50u);
    v4_msg("SD_MOUNT: %s\n", v4_strerror(rc));
    if (rc == V4P_ST_OK) {
        rc = v4_sd_info(&m, &si);
        if (rc == V4P_ST_OK) {
            v4_msg("total=%lu kB free=%lu kB sector=%u fat=%u\n",
                   (unsigned long)si.total_kb, (unsigned long)si.free_kb,
                   (unsigned)si.sector_size, (unsigned)si.fat_type);
        } else {
            v4_msg("SD_INFO: %s\n", v4_strerror(rc));
        }

        v4_msg("\n-- Wurzelverzeichnis --\n");
        rc = v4_list_dir(&m, "/", dir_cb, NULL);
        if (rc != V4P_ST_OK) {
            v4_msg("DIR: %s\n", v4_strerror(rc));
        }

        v4_msg("\nDatei lesen (Pfad relativ zum Mount, leer = ueberspringen): ");
        fflush(stdout);
        if (fgets(line, sizeof(line), stdin) != NULL) {
            size_t n = strlen(line);

            while (n > 0u && (line[n - 1u] == '\n' || line[n - 1u] == '\r')) {
                line[--n] = '\0';
            }
            if (n > 0u) {
                g_blocks = 0ul;
                g_bytes  = 0ul;
                rc = read_file_manual(&m, line);
                v4_msg("FILE_READ: %s (%lu Byte in %lu Bloecken)\n",
                       v4_strerror(rc), g_bytes, g_blocks);
                if (rc == V4_ERR_LINK) {
                    print_bus_error();
                }
            }
        }
    }

    v4_msg("\n-- Wiedergabe von der SD-Karte (PLAY_FILE/STOP_PLAY) --\n");
    v4_msg("Datei abspielen (Pfad, leer = ueberspringen): ");
    fflush(stdout);
    if (fgets(line, sizeof(line), stdin) != NULL) {
        size_t n = strlen(line);

        while (n > 0u && (line[n - 1u] == '\n' || line[n - 1u] == '\r')) {
            line[--n] = '\0';
        }
        if (n > 0u) {
            rc = v4_play_file(&m, line);
            v4_msg("PLAY_FILE: %s\n", v4_strerror(rc));
            if (rc == V4P_ST_OK) {
                if (v4_get_status(&m, &st) == V4P_ST_OK) {
                    print_status(&st);
                }
                v4_msg("Wiedergabe laeuft -- Enter zum Stoppen ...");
                fflush(stdout);
                if (fgets(line, sizeof(line), stdin) == NULL) {
                    /* Eingabe zu Ende (Pipe/EOF): trotzdem sauber stoppen. */
                    v4_msg("\n(Eingabe beendet -- Wiedergabe wird gestoppt)\n");
                }
                rc = v4_stop_play(&m);
                v4_msg("STOP_PLAY: %s\n", v4_strerror(rc));
                if (v4_get_status(&m, &st) == V4P_ST_OK) {
                    print_status(&st);
                }
            }
        }
    }

    v4_msg("\n-- Trennen --\n");
    rc = v4_disconnect(&m);
    v4_msg("DISCONNECT: %s\n", v4_strerror(rc));

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

    if (g_use_log != 0) {
        /* Marke VOR dem Oeffnen: bleibt der Bildschirm danach stehen, sitzt der
         * Absturz in Open() und nicht weiter unten. Die Datei ist noch nicht
         * offen, die Zeile geht also nur auf die Konsole. */
        v4_msg("Oeffne Logdatei ...\n");
        if (v4_plat_log_open(g_log_path) == 0) {
            v4_msg("Logdatei: %s\n", (g_log_path != NULL) ? g_log_path
                                                         : "Standardpfad");
        } else {
            v4_msg("Logdatei nicht verfuegbar (%s) -- nur Konsole.\n",
                   (g_log_path != NULL) ? g_log_path : "Standardpfad");
        }
    } else {
        v4_msg("Logdatei: abgeschaltet (-n).\n");
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
