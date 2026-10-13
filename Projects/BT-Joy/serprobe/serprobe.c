/*
 * serprobe.c - sendet ueber den direkten Debug-UART (SERPER) und empfaengt,
 * OHNE Lesepausen.
 *
 * SPDX-License-Identifier: CC0-1.0
 *
 * DER FEHLER, DER HIER BEHOBEN WIRD
 * ---------------------------------
 * Am Board kam vom PC nur das ERSTE Zeichen jeder Nachricht an ("P", "PP"). Die
 * serielle Hardware der V4 hat genau EIN Byte Puffer. Das vorige Programm las
 * zwar in Schueben, legte dazwischen aber Delay(10) ein - rund 200 ms, in denen
 * niemand liest. Eine zwoelf Zeichen lange Nachricht ist bei 115200 Baud in etwa
 * einer Millisekunde durch: das erste Zeichen bleibt im Puffer stehen, alle
 * weiteren verwirft die Hardware. Uebrig blieb "P".
 *
 * Deshalb wird hier GAR NICHT mehr geschlafen: die Schleife fragt SERDATR
 * staendig ab, und der Sekundentakt kommt aus der Uhr (ReadEClock). Damit kann
 * zwischen zwei Abfragen kein Zeichen mehr verlorengehen.
 *
 * WAS DAS PROGRAMM TUT
 *   - es sendet jede Sekunde eine Zeile "SERPROBE <n>"
 *   - es zeigt Empfangenes SOFORT Zeichen fuer Zeichen (Einzelzeichen tragen,
 *     mehrzeichenige Puffer kamen am Board nicht an)
 *   - es schickt jede empfangene Zeile als "V4-HAT-EMPFANGEN: ..." zurueck,
 *     damit der Rundlauf in einem Fenster sichtbar ist
 *
 * Aufruf:  serprobe [sekunden]      Vorgabe 60
 */

#include <exec/types.h>
#include <dos/dos.h>
#include <proto/exec.h>
#include <proto/dos.h>

#include "ApolloCrossDev_Debug.h"

#define WARTE_GRENZE  1000000UL
#define EMPFANG_MAX   200

static ULONG s_gesendet;
static ULONG s_warten_gegriffen;
static ULONG s_empfangen;
static char  s_empfang[EMPFANG_MAX + 1];
static int   s_empfang_len;
static ULONG s_leerlauf;      /* Abfragen ohne Ergebnis - erkennt das Ende einer Nachricht */

/* ------------------------------------------------------------------ */
/* Ausgabe auf den Bildschirm - zeichenweise                          */
/* ------------------------------------------------------------------ */

static void bild_text(const char *text)
{
    while (*text != '\0') {
        (void)Write(Output(), (APTR)text, 1);
        text++;
    }
}

static void bild_zahl(unsigned wert)
{
    char puffer[12];
    int  i = 11;

    puffer[i] = '\0';

    do {
        puffer[--i] = (char)('0' + (wert % 10u));
        wert /= 10u;
    } while (wert != 0u);

    bild_text(&puffer[i]);
}

/* ------------------------------------------------------------------ */
/* Senden                                                             */
/* ------------------------------------------------------------------ */

static int sende(int chr)
{
    ULONG warten = 0;

    if (chr == '\n') {
        (void)sende('\r');
    }

    while ((reg_r(SERDATR) & SERDATR_TBE) == 0) {
        if (++warten > WARTE_GRENZE) {
            s_warten_gegriffen++;
            return 0;
        }
    }

    reg_w(INTREQ, INTF_TBE);
    reg_w(SERDAT, SERDAT_STP8 | SERDAT_DB8(chr));
    s_gesendet++;

    return 1;
}

static int sende_text(const char *text)
{
    while (*text != '\0') {
        if (sende((int)(unsigned char)*text) == 0) {
            return 0;
        }
        text++;
    }
    return 1;
}

static void sende_zahl(unsigned wert)
{
    char puffer[12];
    int  i = 11;

    puffer[i] = '\0';

    do {
        puffer[--i] = (char)('0' + (wert % 10u));
        wert /= 10u;
    } while (wert != 0u);

    (void)sende_text(&puffer[i]);
}

/* ------------------------------------------------------------------ */
/* Empfang                                                            */
/* ------------------------------------------------------------------ */

static void empfang_zeigen(void)
{
    unsigned i;

    if (s_empfang_len <= 0) {
        return;
    }

    s_empfang[s_empfang_len] = '\0';
    s_leerlauf = 0;

    bild_text("EMPFANGEN (");
    bild_zahl((unsigned)s_empfang_len);
    bild_text("): ");
    for (i = 0; i < (unsigned)s_empfang_len; i++) {
        static const char hex[] = "0123456789ABCDEF";
        char paar[4];

        paar[0] = hex[((unsigned char)s_empfang[i] >> 4) & 0x0F];
        paar[1] = hex[(unsigned char)s_empfang[i] & 0x0F];
        paar[2] = ' ';
        paar[3] = '\0';
        bild_text(paar);
    }
    bild_text(" = ");
    bild_text(s_empfang);
    bild_text("\n");

    /* Und zur Kontrolle zum PC zurueck. */
    (void)sende_text("V4-HAT-EMPFANGEN: ");
    (void)sende_text(s_empfang);
    (void)sende('\n');

    s_empfang_len = 0;
}

static void empfang_poll(void)
{
    int c;
    BOOL etwas = FALSE;

    /*
     * NUR SAMMELN - hier darf NICHTS ausgegeben werden.
     *
     * Am Board zeigte sich: es kamen immer nur die ersten ein bis zwei Zeichen
     * an ("SI" von "SINGLE A"). Ursache war nicht die Leitung, sondern dieses
     * Programm: fuer jedes empfangene Zeichen wurde sofort Write(Output(), ...)
     * gerufen. Das ist ein Systemaufruf von rund 100 us - und waehrend dieser
     * Zeit liest niemand SERDATR. Der UART der V4 hat ein Byte Puffer; alles,
     * was waehrend der Ausgabe eintrifft, ist verloren.
     *
     * Deshalb wird jetzt erst gesammelt und die Zeile ausgegeben, wenn eine
     * kurze Ruhe zeigt, dass die Nachricht durch ist.
     */
    while ((c = ApolloDebugMayGetChar()) >= 0) {
        s_empfangen++;
        s_leerlauf = 0;
        etwas = TRUE;

        if (c == '\r') {
            continue;
        }
        if (c == '\n') {
            empfang_zeigen();
            continue;
        }

        if (s_empfang_len < EMPFANG_MAX) {
            s_empfang[s_empfang_len++] = (char)c;
        } else {
            empfang_zeigen();
        }
    }

    if (etwas) {
        return;
    }

    /* Kein Zeichen: wenn laengere Zeit nichts kam und etwas im Puffer liegt,
     * war das eine ganze Nachricht ohne Zeilenende. */
    s_leerlauf++;
    if (s_leerlauf > 400 && s_empfang_len > 0) {
        empfang_zeigen();
        s_leerlauf = 0;
    }
}

/* ------------------------------------------------------------------ */
/* main                                                               */
/* ------------------------------------------------------------------ */

int main(int argc, char **argv)
{
    unsigned sekunden = 60;
    unsigned gesendet_bis = 0;
    struct DateStamp   uhr_start;
    struct DateStamp   uhr_jetzt;

    if (argc > 1 && argv[1] != NULL) {
        unsigned w = 0;
        const char *p = argv[1];

        while (*p >= '0' && *p <= '9') {
            w = w * 10u + (unsigned)(*p - '0');
            p++;
        }
        if (w > 0u && w <= 3600u) {
            sekunden = w;
        }
    }

    /*
     * Taktgeber: DateStamp() aus dos.library. Der Zeitzaehler laeuft mit 50 Hz,
     * eine Sekunde sind also 50 Ticks. Wichtig ist nur, dass die Schleife
     * NICHT schlaeft - frueher stand hier Delay(), und waehrend dieser Pausen
     * gingen die empfangenen Zeichen verloren.
     */
    DateStamp(&uhr_start);

    ApolloDebugInit();

    bild_text("SERPROBE: ");
    bild_zahl(sekunden);
    bild_text(" Sekunden, Lesen OHNE Pausen\n");

    for (;;) {
        ULONG vergangen;

        /* 1. Immer wieder lesen - das ist der Kern der Korrektur. */
        empfang_poll();

        /* 2. Sekunde bestimmen: 50 Ticks je Sekunde, 3000 je Minute */
        DateStamp(&uhr_jetzt);
        vergangen = (ULONG)(((uhr_jetzt.ds_Minute - uhr_start.ds_Minute) * 3000L)
                          + (uhr_jetzt.ds_Tick - uhr_start.ds_Tick)) / 50u;

        /* 3. Jede Sekunde eine Zeile senden */
        if (vergangen > gesendet_bis) {
            gesendet_bis = vergangen;

            (void)sende_text("SERPROBE ");
            sende_zahl(gesendet_bis);
            (void)sende('\n');

            if ((gesendet_bis % 10u) == 0u) {
                bild_text("  laeuft: ");
                bild_zahl(gesendet_bis);
                bild_text(" s, gesendet ");
                bild_zahl((unsigned)s_gesendet);
                bild_text(", empfangen ");
                bild_zahl((unsigned)s_empfangen);
                bild_text("\n");
            }
        }

        if (gesendet_bis >= sekunden) {
            break;
        }
    }

    empfang_zeigen();

    bild_text("ENDE: gesendet ");
    bild_zahl((unsigned)s_gesendet);
    bild_text(" Zeichen, empfangen ");
    bild_zahl((unsigned)s_empfangen);
    bild_text(" Zeichen, Wartegrenze ");
    bild_zahl((unsigned)s_warten_gegriffen);
    bild_text(" mal\n");

    if (s_empfangen > 0) {
        bild_text("  -> Die Richtung PC -> V4 TRAEGT.\n");
    } else {
        bild_text("  -> Aus der Richtung PC -> V4 kam nichts an.\n");
    }

    return 0;
}
