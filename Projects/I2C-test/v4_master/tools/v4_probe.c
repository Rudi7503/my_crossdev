/*
 * v4_probe.c -- Stufenprobe fuer die Vampire V4 (Hardware-Inbetriebnahme §14.2).
 *
 * Zweck: den Start des Masters in einzelne, nummerierte Schritte zerlegen.
 * Jeder Schritt gibt genau eine Zeile auf die Konsole und flusht sofort.
 * Bleibt die Ausgabe nach Schritt N stehen, sitzt der Absturz in Schritt N+1
 * -- ohne Debugger, ohne Raterei.
 *
 * Schritt 3 (serieller Anschluss) ist nur mit `-s` dabei: ein zweites Oeffnen
 * von "ser:" kann einer Shell, die selbst auf dem seriellen Anschluss laeuft,
 * die Konsole wegziehen. Das Hauptprogramm benutzt ser: nicht mehr -- seine
 * Vorgabe ist die Logdatei Programs:test/v4_console.log.
 *
 * Aufruf auf der V4:
 *     ram:v4_probe          # Schritte 1,2,4..8 (kein ser:)
 *     ram:v4_probe -s       # zusaetzlich Schritt 3 (ser: oeffnen/schreiben)
 */

#ifndef V4_PLAT_AMIGA
#error "v4_probe.c ist ein Amiga-Programm: mit -DV4_PLAT_AMIGA=1 uebersetzen"
#endif

#include "v4_master.h"

#include <exec/types.h>
#include <exec/tasks.h>
#include <exec/io.h>
#include <devices/timer.h>
#include <dos/dos.h>

#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/timer.h>
#include <proto/i2c.h>

#include <stdarg.h>
#include <stdio.h>

/* Jede Zeile sofort raus -- bei einem Absturz darf nichts im Puffer bleiben. */
static void step(const char *fmt, ...)
{
    char    buf[256];
    va_list ap;
    int     n;

    va_start(ap, fmt);
    n = vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    if (n > 0) {
        fputs(buf, stdout);
        fflush(stdout);
    }
}

/* Wie die Konsole, nur knapp: eine Zeile je Transaktionsschritt. */
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

int main(int argc, char **argv)
{
    int with_ser = 0;
    int i;

    for (i = 1; i < argc; i++) {
        if (argv[i][0] == '-' && argv[i][1] == 's') {
            with_ser = 1;
        }
    }

    /* ---- 1: laeuft das Programm ueberhaupt an? ---------------------- */
    step("[probe] 1/8 Start erreicht (argc=%d)\n", argc);

    {
        struct Task *me = FindTask(NULL);
        ULONG        upper = (ULONG)me->tc_SPUpper;
        ULONG        lower = (ULONG)me->tc_SPLower;

        step("[probe] 1/8 Stack: %lu Byte frei/gesamt (SPUpper=%08lx)\n",
             (unsigned long)(upper - lower), (unsigned long)upper);
    }

    /* ---- 2: der DOS-Zugriff, auf dem jedes printf steht ------------- */
    step("[probe] 2/8 DOSBase=%08lx Output=%08lx\n", (unsigned long)DOSBase,
         (unsigned long)Output());
    if (DOSBase == NULL) {
        step("  ABBRUCH: DOSBase ist NULL -- ohne dos.library geht keine Ausgabe.\n");
        return 1;
    }

    /* ---- 3: serielle Ausgabe (nur mit -s) --------------------------- */
    if (with_ser) {
        static const char msg[] = "[probe] Text ueber ser:\n";
        BPTR  fh;
        long  w;
        int   rc;

        /* Erst nur lesend oeffnen: das ist der harmlose Weg. MODE_NEWFILE
         * kommt danach getrennt, damit ein Absturz eindeutig zuzuordnen ist. */
        SetIoErr(0);
        step("[probe] 3/8 oeffne ser: mit MODE_OLDFILE ...\n");
        fh = Open((CONST_STRPTR)"ser:", MODE_OLDFILE);
        step("[probe] 3/8 ser: MODE_OLDFILE -> %08lx (DOS-Fehler %ld)\n",
             (unsigned long)fh, (long)IoErr());
        if (fh != (BPTR)0) {
            Close(fh);
        }

        step("[probe] 3/8 oeffne ser: mit MODE_NEWFILE ...\n");
        SetIoErr(0);
        rc = v4_plat_log_open("ser:");   /* ausdruecklich, nicht die Vorgabe */
        step("[probe] 3/8 ser: MODE_NEWFILE -> %d (DOS-Fehler %ld)\n", rc,
             (long)IoErr());
        if (rc == 0) {
            w = v4_plat_log_write(msg, (unsigned long)(sizeof(msg) - 1u));
            step("[probe] 3/8 ser: schreiben -> %ld (muss %lu sein)\n", w,
                 (unsigned long)(sizeof(msg) - 1u));
            v4_plat_log_close();
            step("[probe] 3/8 ser: geschlossen\n");
        }
    } else {
        step("[probe] 3/8 ser: uebersprungen (nur mit -s)\n");
    }

    /* ---- 4: i2c.library -------------------------------------------- */
    {
        struct Library *lib = OpenLibrary((CONST_STRPTR)"i2c.library", 39);

        step("[probe] 4/8 i2c.library -> %08lx\n", (unsigned long)lib);
        if (lib == NULL) {
            step("  ABBRUCH: i2c.library fehlt oder ist aelter als V39.\n");
            return 1;
        }
        CloseLibrary(lib);
        step("[probe] 4/8 i2c.library wieder geschlossen\n");
    }

    /* ---- 5: timer.device (Traeger von t_wait) ---------------------- */
    {
        struct timerequest tr;

        step("[probe] 5/8 oeffne timer.device ...\n");
        tr.tr_node.io_Command = TR_ADDREQUEST;
        if (OpenDevice((CONST_STRPTR)TIMERNAME, UNIT_MICROHZ,
                       (struct IORequest *)&tr, 0) != 0) {
            step("  ABBRUCH: timer.device UNIT_MICROHZ laesst sich nicht oeffnen.\n");
            return 1;
        }
        step("[probe] 5/8 timer.device ok (io_Device=%08lx)\n",
             (unsigned long)tr.tr_node.io_Device);
        CloseDevice((struct IORequest *)&tr);
    }

    /* ---- 6: Byte-Order-Selbsttest ---------------------------------- */
    step("[probe] 6/8 byteorder-selbsttest -> %d (muss 1 sein)\n",
         v4p_selftest_byteorder());

    /* ---- 7: Bus oeffnen und PING (der erste echte Busverkehr) ------ */
    {
        v4_master_t m;
        v4p_info_t  info;
        uint8_t     rc;

        if (v4_open(&m, NULL) != 0) {
            step("[probe] 7/8 ABBRUCH: v4_open fehlgeschlagen (%s)\n",
                 v4_plat_error_text());
            return 1;
        }
        m.trace     = probe_trace;
        m.trace_ctx = NULL;
        step("[probe] 7/8 v4_open ok, sende PING ...\n");

        rc = v4_ping(&m, &info);
        step("[probe] 7/8 PING -> %s\n", v4_strerror(rc));
        if (rc != V4P_ST_OK) {
            step("  Busfehler 0x%08lX: %s\n", v4_plat_last_error(),
                 v4_plat_error_text());
            step("  -> Slave nicht erreichbar, falsche Adresse oder Pegel.\n");
        } else {
            step("[probe] 7/8 proto=%u fw=%u write=%u read=%u chunk=%u\n",
                 (unsigned)info.proto_ver, (unsigned)info.fw_ver,
                 (unsigned)info.write_frame_len, (unsigned)info.read_frame_len,
                 (unsigned)info.chunk);
        }

        /* ---- 8: sauber schliessen ---------------------------------- */
        v4_close();
        step("[probe] 8/8 v4_close ok -- Programmende\n");
        return (rc == V4P_ST_OK) ? 0 : 10;
    }
}
