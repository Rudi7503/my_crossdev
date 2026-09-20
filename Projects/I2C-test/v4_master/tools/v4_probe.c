/*
 * v4_probe.c -- Stufenprobe fuer die Vampire V4 (Hardware-Inbetriebnahme §14.2).
 *
 * Zweck: den Start des Masters in einzelne, nummerierte Schritte zerlegen.
 * Jeder Schritt gibt genau eine Zeile auf die Konsole UND in eine Logdatei,
 * beide werden sofort geflusht. Bleibt die Ausgabe nach Schritt N stehen, sitzt
 * der Absturz (oder die Haengerei) in Schritt N+1 -- ohne Debugger.
 *
 * Die Logdatei ist der eigentliche Trick: nach einem Absturz kann die Konsole
 * weg sein, die Datei nicht. Sie wird im Repository mit
 *
 *     make log LOG_NAME=v4_probe.log
 *
 * per acp von der V4 geholt -- niemand muss vom Amiga-Bildschirm abtippen.
 *
 * Aufruf auf der V4:
 *     Programs:test/v4_probe          # alles, Logdatei Programs:test/v4_probe.log
 *     Programs:test/v4_probe -n       # ohne Logdatei
 *     Programs:test/v4_probe -o ram:p.log
 *     Programs:test/v4_probe -s       # zusaetzlich ser: in Schritt 3
 */

#ifndef V4_PLAT_AMIGA
#error "v4_probe.c ist ein Amiga-Programm: mit -DV4_PLAT_AMIGA=1 uebersetzen"
#endif

#include "v4_master.h"

#include <exec/types.h>
#include <exec/tasks.h>
#include <exec/io.h>
#include <exec/libraries.h>
#include <devices/timer.h>
#include <dos/dos.h>
#include <libraries/i2c.h>

#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/timer.h>
#include <proto/i2c.h>

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

/* Eigener Dateiname, damit die Probe das Log des Konsolenprogramms nicht
 * ueberschreibt. */
#define PROBE_LOG_DEFAULT "Programs:test/v4_probe.log"

static int         g_log_on   = 1;      /* mit -n abschaltbar */
static const char *g_log_path = NULL;   /* NULL = PROBE_LOG_DEFAULT */

static struct timerequest s_timer;
static int                s_timer_open = 0;

/* Jede Zeile sofort auf beide Kanaele -- bei einem Absturz darf nichts im
 * Puffer bleiben. Der Puffer ist statisch (weniger Stack). */
static void step(const char *fmt, ...)
{
    static char buf[256];
    va_list ap;
    int     n;

    va_start(ap, fmt);
    n = vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    if (n <= 0) {
        return;
    }
    fputs(buf, stdout);
    fflush(stdout);
    if (g_log_on) {
        (void)v4_plat_log_write(buf, (unsigned long)n);
    }
}

/* Millisekunden seit dem Systemstart -- fuer die Laufzeitmessung des PING. */
static ULONG ms_now(void)
{
    if (!s_timer_open) {
        return 0ul;
    }
    s_timer.tr_node.io_Command = TR_GETSYSTIME;
    if (DoIO((struct IORequest *)&s_timer) != 0) {
        return 0ul;
    }
    return (ULONG)s_timer.tr_time.tv_secs * 1000ul
         + (ULONG)s_timer.tr_time.tv_micro / 1000ul;
}

/* Knapper Trace: eine Zeile je Transaktionsschritt. */
static void probe_trace(const v4_trace_t *ev, void *ctx)
{
    (void)ctx;

    switch (ev->event) {
    case V4_TR_TX_BEGIN:
        step("        TX  cmd=0x%02X seq=%u, %u Byte\n", (unsigned)ev->cmd,
             (unsigned)ev->seq, (unsigned)ev->len);
        break;
    case V4_TR_TX_END:
        step("        TX  rc=%d\n", ev->rc);
        break;
    case V4_TR_WAIT:
        step("        Warten %lu us\n", (unsigned long)ev->us);
        break;
    case V4_TR_RX_BEGIN:
        step("        RX  lese %u Byte\n", (unsigned)ev->len);
        break;
    case V4_TR_RX_END:
        step("        RX  rc=%d\n", ev->rc);
        break;
    case V4_TR_CHECK:
    case V4_TR_RETRY:
    case V4_TR_BUSY:
    case V4_TR_DONE:
        step("        %s\n", v4_check_str(ev->rc));
        break;
    default:
        break;
    }
}

/* Rohe Hardware lesen, die die Apollo-Variante von i2c.library benutzt:
 *   $BFD000  CIA-B PRA -- die Library liest das Register als Verzoegerung
 *   $DE0080  Apollo-I2C-Datenregister (Bit0 = SDA, Bit1 = SCL)
 *   $DE0081  dasselbe +1 -- von dort liest die Library das SDA-Bit
 * Nur lesend, byteweise. Wenn schon der Zugriff faul ist, steht das als
 * letzte Zeile im Log, bevor es knallt. */
static void roh_zugriffe(void)
{
    volatile UBYTE *cia  = (volatile UBYTE *)0x00BFD000ul;
    volatile UBYTE *i2cd = (volatile UBYTE *)0x00DE0080ul;

    step("[probe] 6b/9 lese $BFD000 (CIA-B PRA, Verzoegerung der Library) ...\n");
    step("[probe] 6b/9 $BFD000 = 0x%02X\n", (unsigned)(*cia));

    step("[probe] 6c/9 lese $DE0080/$DE0081 (Apollo-I2C) ...\n");
    step("[probe] 6c/9 $DE0080 = 0x%02X, $DE0081 = 0x%02X (SDA = Bit 0)\n",
         (unsigned)i2cd[0], (unsigned)i2cd[1]);
}

int main(int argc, char **argv)
{
    uint8_t probe_rc = V4P_ST_OK;
    int     with_ser  = 0;
    int     i;

    for (i = 1; i < argc; i++) {
        if (argv[i][0] != '-') {
            continue;
        }
        if (argv[i][1] == 's') {
            with_ser = 1;
        } else if (argv[i][1] == 'n') {
            g_log_on = 0;
        } else if (argv[i][1] == 'o') {
            if (i + 1 >= argc) {
                step("-o braucht einen Pfad\n");
                return 5;
            }
            g_log_path = argv[++i];
        }
    }

    /* ---- 1: laeuft das Programm ueberhaupt an? ---------------------- */
    step("[probe] 1/9 Start erreicht (argc=%d)\n", argc);
    {
        struct Task *me = FindTask(NULL);

        step("[probe] 1/9 Stack: %lu Byte (SPUpper=%08lx)\n",
             (unsigned long)((ULONG)me->tc_SPUpper - (ULONG)me->tc_SPLower),
             (unsigned long)(ULONG)me->tc_SPUpper);
    }

    /* ---- 2: der DOS-Zugriff, auf dem jedes printf steht ------------- */
    step("[probe] 2/9 DOSBase=%08lx Output=%08lx\n", (unsigned long)DOSBase,
         (unsigned long)Output());
    if (DOSBase == NULL) {
        step("  ABBRUCH: DOSBase ist NULL -- ohne dos.library geht keine Ausgabe.\n");
        return 1;
    }

    /* ---- 2b: Logdatei oeffnen (ab hier ist alles im Log) ------------ */
    if (g_log_on) {
        const char *wunsch = (g_log_path != NULL) ? g_log_path
                                                  : PROBE_LOG_DEFAULT;
        int rc;

        step("[probe] 2b/9 oeffne Logdatei \"%s\" ...\n", wunsch);
        rc = v4_plat_log_open(g_log_path);
        step("[probe] 2b/9 Logdatei -> %d (IoErr %ld)\n", rc, (long)IoErr());
        if (rc != 0) {
            step("[probe] 2b/9 Hinweis: die naechsten Zeilen stehen nur auf der Konsole.\n");
        }
    }

    /* ---- 3: Dateien oeffnen, jeden Pfad einzeln -------------------- */
    {
        static const char *pfade[] = {
            "T:v4_probe.txt",                       /* immer da, RAM-basiert */
            "ram:v4_probe.txt",                     /* RAM-Disk */
            "Programs:test/v4_probe.txt",           /* der echte Zielordner */
            V4_LOG_DEFAULT_AMIGA                    /* Vorgabe des Programms */
        };
        unsigned k;

        for (k = 0u; k < (unsigned)(sizeof(pfade) / sizeof(pfade[0])); k++) {
            static const char msg[] = "[probe] Text in die Logdatei\n";
            LONG w;
            int  rc;

            /* Marke VOR dem Oeffnen: die letzte Zeile vor einem Absturz nennt
             * damit den Pfad, der ihn ausgeloest hat. */
            step("[probe] 3/9 --- oeffne \"%s\" ...\n", pfade[k]);
            SetIoErr(0);
            rc = v4_plat_log_open(pfade[k]);
            step("[probe] 3/9 open -> %d (IoErr %ld)\n", rc, (long)IoErr());
            if (rc != 0) {
                continue;
            }
            w = v4_plat_log_write(msg, (unsigned long)(sizeof(msg) - 1u));
            step("[probe] 3/9 write+flush -> %ld (soll %lu)\n", w,
                 (unsigned long)(sizeof(msg) - 1u));
            v4_plat_log_close();
            step("[probe] 3/9 close ok\n");
        }

        if (with_ser) {
            step("[probe] 3/9 --- oeffne \"ser:\" ...\n");
            SetIoErr(0);
            if (v4_plat_log_open("ser:") == 0) {
                (void)v4_plat_log_write("[probe] Text ueber ser:\n", 24ul);
                v4_plat_log_close();
                step("[probe] 3/9 ser: geschrieben und geschlossen\n");
            } else {
                step("[probe] 3/9 ser: nicht verfuegbar (IoErr %ld)\n",
                     (long)IoErr());
            }
        }

        /* Die eigene Logdatei wieder oeffnen -- die Tests oben haben sie
         * geschlossen bzw. durch die Konsolen-Vorgabe ersetzt. */
        if (g_log_on) {
            int rc = v4_plat_log_open(g_log_path);

            step("[probe] 3/9 eigenes Log wieder offen -> %d\n", rc);
            if (rc != 0) {
                g_log_on = 0;
            }
        }
    }

    /* ---- 4: i2c.library -------------------------------------------- */
    {
        struct Library *lib = OpenLibrary((CONST_STRPTR)"i2c.library", 39);

        step("[probe] 4/9 i2c.library -> %08lx\n", (unsigned long)lib);
        if (lib == NULL) {
            step("  ABBRUCH: i2c.library fehlt oder ist aelter als V39.\n");
            return 1;
        }
        step("[probe] 4/9 Version %u.%u\n", (unsigned)lib->lib_Version,
             (unsigned)lib->lib_Revision);
        CloseLibrary(lib);
    }

    /* ---- 5: timer.device (Traeger von t_wait) ---------------------- */
    step("[probe] 5/9 oeffne timer.device ...\n");
    s_timer.tr_node.io_Command = TR_ADDREQUEST;
    if (OpenDevice((CONST_STRPTR)TIMERNAME, UNIT_MICROHZ,
                   (struct IORequest *)&s_timer, 0) != 0) {
        step("  ABBRUCH: timer.device UNIT_MICROHZ laesst sich nicht oeffnen.\n");
        return 1;
    }
    s_timer_open = 1;
    step("[probe] 5/9 timer.device ok (io_Device=%08lx)\n",
         (unsigned long)s_timer.tr_node.io_Device);

    /* ---- 6: Byte-Order-Selbsttest ---------------------------------- */
    step("[probe] 6/9 byteorder-selbsttest -> %d (muss 1 sein)\n",
         v4p_selftest_byteorder());

    /* ---- 6b/6c: rohe Register, die die Library benutzt ------------- */
    roh_zugriffe();

    /* ---- 7: Bus oeffnen und PING (der erste echte Busverkehr) ------ */
    probe_rc = V4P_ST_OK;
    {
        v4_master_t m;
        v4p_info_t  info;
        ULONG       t0;
        ULONG       t1;

        if (v4_open(&m, NULL) != 0) {
            step("[probe] 7/9 ABBRUCH: v4_open fehlgeschlagen (%s)\n",
                 v4_plat_error_text());
            return 1;
        }
        step("[probe] 7/9 v4_open ok\n");

        /* Zustand der Library abfragen, BEVOR der erste Transfer laeuft. */
        step("[probe] 7/9 AllocI2C -> %d (0 = OK)\n",
             (int)AllocI2C((UBYTE)DELAY_TIMER, (STRPTR)"v4_probe"));
        step("[probe] 7/9 SetI2CDelay(READONLY) -> %lu\n",
             (unsigned long)SetI2CDelay(I2CDELAY_READONLY));
        {
            STRPTR opp = GetI2COpponent();

            step("[probe] 7/9 GetI2COpponent -> %s\n",
                 (opp != NULL) ? (const char *)opp : "(keiner)");
        }
        {
            char *env = getenv("I2CDELAY");

            step("[probe] 7/9 I2CDELAY im Environment -> %s\n",
                 (env != NULL) ? env : "(nicht gesetzt)");
        }

        m.trace     = probe_trace;
        m.trace_ctx = NULL;

        step("[probe] 7/9 sende PING ...\n");
        t0 = ms_now();
        probe_rc = v4_ping(&m, &info);
        t1 = ms_now();
        step("[probe] 7/9 PING -> %s (%lu ms)\n", v4_strerror(probe_rc),
             (unsigned long)(t1 - t0));
        if (probe_rc != V4P_ST_OK) {
            step("  Busfehler 0x%08lX: %s\n", v4_plat_last_error(),
                 v4_plat_error_text());
            step("  -> Slave nicht erreichbar, falsche Adresse oder Pegel.\n");
        } else {
            step("[probe] 7/9 proto=%u fw=%u write=%u read=%u chunk=%u\n",
                 (unsigned)info.proto_ver, (unsigned)info.fw_ver,
                 (unsigned)info.write_frame_len, (unsigned)info.read_frame_len,
                 (unsigned)info.chunk);
        }

        /* ---- 8: sauber schliessen ---------------------------------- */
        v4_close();
        step("[probe] 8/9 v4_close ok\n");
    }

    /* ---- 9: Logdatei schliessen ------------------------------------ */
    step("[probe] 9/9 Programmende\n");
    if (g_log_on) {
        v4_plat_log_close();
    }
    return (probe_rc == V4P_ST_OK) ? 0 : 10;
}
