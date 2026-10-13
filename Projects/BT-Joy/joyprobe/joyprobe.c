/*
 * joyprobe.c - liest die Joystick-Anschluesse der V4 DIREKT aus den Registern.
 *
 * SPDX-License-Identifier: CC0-1.0
 *
 * WOZU: statt die Pegel ueber I2C vom ESP32 zu erfragen (rund 114 ms je
 * Transaktion, siehe SERIELL.md), koennte die V4 ihre Anschluesse selbst lesen.
 * Dann waere die Anzeige sofort aktuell, und die I2C-Leitung braeuchte nur noch
 * die Bluetooth-Steuerung.
 *
 * Welche Register auf der V4 die beiden DB9-Buchsen abbilden, ist nicht
 * offensichtlich: der Apollo-Kern benutzt $DFF00A/$DFF00B als
 * APOLLO_MOUSE_GET_X/_Y (siehe ApolloLib/ApolloCrossDev_Defines.h). Deshalb
 * liest dieses Programm ALLE in Frage kommenden Quellen und zeigt sie im
 * Sekundentakt an. Drueckt man am Controller, sieht man, welche sich aendert -
 * das ist die belastbarere Auskunft als jede Vermutung.
 *
 * Ausgabe auf Bildschirm UND Debug-UART (115200).
 *
 * Aufruf:  joyprobe [sekunden]      Vorgabe 120
 */

#include <exec/types.h>
#include <dos/dos.h>
#include <proto/exec.h>
#include <proto/dos.h>

#include "ApolloCrossDev_Debug.h"

/* Klassische Amiga-Adressen. */
#define REG_JOY0DAT   ((volatile UWORD *)0xDFF00A)   /* Apollo: MOUSE_GET_X */
#define REG_JOY1DAT   ((volatile UWORD *)0xDFF00C)   /* Apollo: MOUSE_GET_Y */
#define REG_POTINP    ((volatile UWORD *)0xDFF016)
#define REG_POTGOR    ((volatile UWORD *)0xDFF016)
#define REG_CIAAPRA   ((volatile UBYTE *)0xBFE001)

static void aus_text(const char *t)
{
    ApolloDebugPutStr(t);
    while (*t != '\0') { (void)Write(Output(), (APTR)t, 1); t++; }
}

static void aus_zahl(unsigned w)
{
    char p[12];
    int i = 11;
    p[i] = '\0';
    do { p[--i] = (char)('0' + (w % 10u)); w /= 10u; } while (w != 0u);
    aus_text(&p[i]);
}

static void aus_hex(const char *name, unsigned v)
{
    static const char h[] = "0123456789ABCDEF";
    char t[40];
    int i = 0;
    while (*name) t[i++] = *name++;
    t[i++] = '=';
    t[i++] = '$';
    t[i++] = h[(v >> 12) & 0xF];
    t[i++] = h[(v >> 8) & 0xF];
    t[i++] = h[(v >> 4) & 0xF];
    t[i++] = h[v & 0xF];
    t[i++] = ' ';
    t[i] = '\0';
    aus_text(t);
}

int main(int argc, char **argv)
{
    unsigned sekunden = 120;
    unsigned s;

    if (argc > 1 && argv[1] != NULL) {
        unsigned w = 0; const char *p = argv[1];
        while (*p >= '0' && *p <= '9') { w = w * 10u + (unsigned)(*p - '0'); p++; }
        if (w > 0u && w <= 3600u) sekunden = w;
    }

    ApolloDebugInit();

    aus_text("JOYPROBE: Register der Joystick-Anschluesse, ");
    aus_zahl(sekunden);
    aus_text(" Sekunden\n");
    aus_text("  Bitte am Controller druecken - die Aenderung zeigt das Register.\n");

    for (s = 1; s <= sekunden; s++) {
        aus_text("J ");
        aus_zahl(s);
        aus_text("  ");
        aus_hex("CIAAPRA", (unsigned)*REG_CIAAPRA);
        aus_hex("JOY0DAT", (unsigned)*REG_JOY0DAT);
        aus_hex("JOY1DAT", (unsigned)*REG_JOY1DAT);
        aus_hex("POTINP",  (unsigned)*REG_POTINP);

        /*
         * Dekodierung nach der Hardware-Referenz:
         *   RECHTS  = Bit 1 des X-Zaehlers (direkt)
         *   LINKS   = Bit 1 des Y-Zaehlers (direkt)
         *   VOR     = (Y1 ^ Y0), also ((j >> 1) ^ j) & 0x0100
         *   ZURUECK = (Y1 ^ Y0) der anderen Flanke, ((j << 1) ^ j) & 0x0100
         */
        {
            UWORD j0 = *REG_JOY0DAT;
            UWORD j1 = *REG_JOY1DAT;
            aus_text("  Buchse0:");
            if (j0 & 0x0002) aus_text(" RECHTS");
            if (j0 & 0x0200) aus_text(" LINKS");
            if (((j0 >> 1) ^ j0) & 0x0100) aus_text(" VOR");
            if (((j0 << 1) ^ j0) & 0x0100) aus_text(" ZURUECK");
            aus_text(" | Buchse1:");
            if (j1 & 0x0002) aus_text(" RECHTS");
            if (j1 & 0x0200) aus_text(" LINKS");
            if (((j1 >> 1) ^ j1) & 0x0100) aus_text(" VOR");
            if (((j1 << 1) ^ j1) & 0x0100) aus_text(" ZURUECK");
        }
        aus_text("\n");
        Delay(50);
    }

    aus_text("JOYPROBE ENDE\n");
    return 0;
}
