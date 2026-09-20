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
static int         g_upto     = 0;      /* -x <n>: nach Phase n sauber beenden */
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

/* Phasen-Gatter: bricht VOR der naechsten Phase sauber ab. Damit laesst sich
 * der Absturzpunkt ohne Reboot-Runden eingrenzen:
 *   1 Start/Stack/DOSBase/MOVIW   2 Dateitests      3 Library+Timer+Register
 *   4 direkte I2C-Aufrufe         5 gerahmter PING  6 Ende
 * Ein harter Absturz nimmt den Logdatei-Schwanz mit; die Konsole zeigt aber
 * immer, wie weit es kam. */
static void phase_gate(int phase)
{
    if (g_upto == 0 || phase <= g_upto) {
        return;
    }
    step("[probe] Ende nach Phase %d (-x %d): alles davor ist durchgelaufen.\n",
         g_upto, g_upto);
    if (g_log_on) {
        v4_plat_log_close();
    }
    exit(0);
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

/* Auf der echten CPU pruefen, was MOVIW.L mit dem oberen Wort macht. GCC
 * erzeugt diese 68080-Form, wenn eine 16-Bit-Konstante in ein Long-Register
 * soll -- genau bei Open(pfad, MODE_NEWFILE) mit 1006. Bleibt das obere Wort
 * stehen, ist der Modus Muell. */
static ULONG moviw_test(void)
{
    ULONG r = 0xFFFFFFFFul;             /* oberes Wort absichtlich gesetzt */

    __asm__ __volatile__("moviw.l #1006,%0" : "+d"(r));
    return r;
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
        } else if (argv[i][1] == 'x') {
            if (i + 1 >= argc) {
                step("-x braucht eine Phasennummer (1..6)\n");
                return 5;
            }
            g_upto = atoi(argv[++i]);
            if (g_upto < 1 || g_upto > 6) {
                step("-x liegt zwischen 1 und 6\n");
                return 5;
            }
        }
    }

    /* ---- 0: Logdatei zuerst -- damit auch Stack und DOSBase im Log stehen --- */
    if (g_log_on) {
        const char *wunsch = (g_log_path != NULL) ? g_log_path
                                                  : PROBE_LOG_DEFAULT;
        int rc;

        step("[probe] 0/9 oeffne Logdatei \"%s\" ...\n", wunsch);
        rc = v4_plat_log_open((g_log_path != NULL) ? g_log_path
                                                  : PROBE_LOG_DEFAULT);
        step("[probe] 0/9 Logdatei -> %d (IoErr %ld)\n", rc, (long)IoErr());
        if (rc != 0) {
            step("[probe] 0/9 Hinweis: alles Weitere steht nur auf der Konsole.\n");
            g_log_on = 0;
        }
    }

    phase_gate(1);

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

    /* ---- 2c: was macht die CPU mit MOVIW? -------------------------- */
    {
        ULONG m = moviw_test();

        step("[probe] 2c/9 moviw.l #1006 aus 0xFFFFFFFF -> 0x%08lX %s\n",
             (unsigned long)m,
             (m == 0x000003EEul) ? "(oberes Wort geloescht: GCC ok)"
                                 : "(OBERES WORT BLEIBT: GCC-Falle!)");
    }

    phase_gate(2);

    /* ---- 3: Dateien oeffnen, jeden Pfad einzeln --------------------
     * Absichtlich mit stdio direkt, NICHT ueber die Log-Haken: die Haken
     * gehoeren der eigenen Logdatei, und ein zweites Oeffnen auf dieselbe
     * Datei liess das Log vorzeitig abreissen (auf der V4 passiert). */
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
            FILE *f;

            /* Marke VOR dem Oeffnen: die letzte Zeile vor einem Absturz nennt
             * damit den Pfad, der ihn ausgeloest hat. */
            step("[probe] 3/9 --- fopen \"%s\" ...\n", pfade[k]);
            f = fopen(pfade[k], "w");
            step("[probe] 3/9 fopen -> %s\n", (f != NULL) ? "ok" : "FEHLER");
            if (f == NULL) {
                continue;
            }
            step("[probe] 3/9 fwrite -> %lu\n",
                 (unsigned long)fwrite(msg, 1u, sizeof(msg) - 1u, f));
            (void)fflush(f);
            (void)fclose(f);                    /* schreibt auf die Platte */
            step("[probe] 3/9 geschlossen\n");
        }

        /* Roher DOS-Open mit dem Modus aus einer Variablen: Gegenprobe zur
         * stdio-Variante und zugleich Test, ob MODE_NEWFILE (1006) als
         * 16-Bit-Konstante (moviw.l) die Ursache war. */
        {
            static const LONG mode_new = 1006L;   /* MODE_NEWFILE, 32 Bit */
            BPTR fh;

            step("[probe] 3b/9 DOS-Open \"T:v4_probe_dos.txt\" (Modus aus Variable) ...\n");
            SetIoErr(0);
            fh = Open((CONST_STRPTR)"T:v4_probe_dos.txt", mode_new);
            step("[probe] 3b/9 DOS-Open -> %08lx (IoErr %ld)\n",
                 (unsigned long)fh, (long)IoErr());

            /* Ein BPTR ist immer geraude (Longword-Adresse). Ein ungerader
             * Wert ist Muell -- damit darf NICHT geschrieben werden, genau
             * das hat den vorigen Lauf hier abgebrochen. */
            if ((unsigned long)fh != 0ul
                && (((unsigned long)fh & 3ul) != 0ul)) {
                step("[probe] 3b/9 UNGUELTIGER BPTR -- Write uebersprungen\n");
            } else if (fh != (BPTR)0) {
                (void)Write(fh, (CONST_APTR)"x\n", 2L);
                (void)Flush(fh);
                Close(fh);
                step("[probe] 3b/9 geschrieben und geschlossen\n");
            }
        }

        if (with_ser) {
            FILE *f = fopen("ser:", "w");

            step("[probe] 3/9 fopen(\"ser:\") -> %s\n",
                 (f != NULL) ? "ok" : "FEHLER");
            if (f != NULL) {
                (void)fwrite("[probe] Text ueber ser:\n", 1u, 24u, f);
                (void)fflush(f);
                (void)fclose(f);
                step("[probe] 3/9 ser: geschrieben und geschlossen\n");
            }
        }
    }

    phase_gate(3);

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

    phase_gate(4);

    /* ---- 6d/6e: direkter Library-Test, genau wie das Programm, das auf
     * dieser V4 nachweislich funktioniert hat: erst ReceiveI2C, dann
     * SendI2C, ohne Rahmen/CRC. Damit ist die Library selbst geprueft,
     * bevor unser gerahmtes Protokoll drankommt. ---- */
    {
        struct Library *lib = OpenLibrary((CONST_STRPTR)"i2c.library", 39);

        if (lib != NULL) {
            UBYTE buf[16];
            ULONG err;

            /* Die Inline-Makros der Library benutzen die GLOBALE I2C_Base --
             * ohne diese Zuweisung liefe der Sprung durch einen Nullzeiger. */
            I2C_Base = lib;

            step("[probe] 6d/9 ReceiveI2C(0x%02X, 8) ...\n", (unsigned)0xA0);
            err = ReceiveI2C((UBYTE)0xA0, 8, buf);
            step("[probe] 6d/9 ReceiveI2C -> 0x%08lX %s\n", (unsigned long)err,
                 (const char *)I2CErrText(err));

            buf[0] = 0u;
            step("[probe] 6e/9 SendI2C(0x%02X, 1) ...\n", (unsigned)0xA0);
            err = SendI2C((UBYTE)0xA0, 1, buf);
            step("[probe] 6e/9 SendI2C -> 0x%08lX %s\n", (unsigned long)err,
                 (const char *)I2CErrText(err));

            CloseLibrary(lib);
            I2C_Base = NULL;
        }
    }

    phase_gate(5);

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

    phase_gate(6);

    /* ---- 9: Logdatei schliessen ------------------------------------ */
    step("[probe] 9/9 Programmende\n");
    if (g_log_on) {
        v4_plat_log_close();
    }
    return (probe_rc == V4P_ST_OK) ? 0 : 10;
}
