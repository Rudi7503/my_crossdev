/*
 * v4_debug.h -- die gemeinsame Debugausgabe beider Frontends.
 *
 * WOZU
 * ----
 * Die Vampire V4 hat einen Hardware-Debug-UART (SERPER, 115200 Baud), der am
 * PC ueber eine CH340 mitgelesen wird (siehe Projects/BT-Joy/tools/SERIELL.md).
 * Auf der V4 gibt es keinen Debugger: was hier nicht steht, ist nicht zu sehen.
 * Deshalb geht JEDE Meldung des Programms drei Wege -- mit demselben Text:
 *
 *   1. Standardausgabe (Shell, aus der das Programm gestartet wird),
 *   2. Hardware-Debug-UART der V4 (ApolloDebugInit/ApolloDebugPutStr),
 *   3. Logdatei der Plattform (v4_plat_log_write).
 *
 * Punkt 2 und 3 laufen also PARALLEL: nach dem Lauf lassen sich Mitschnitt und
 * Logdatei Zeile fuer Zeile vergleichen.
 *
 * WAS AUSGEGEBEN WIRD
 * -------------------
 *   - Kopfdaten beim Start: Name, Version, Datum, Uhrzeit, Logdatei, Adresse
 *   - vor/nach dem Oeffnen der Plattform (macht das jeweilige Frontend)
 *   - GENAU EINE Zeile je I2C-Transaktion: Befehl, Sequenznummer, Status,
 *     Nutzlastlaenge, Dauer in Mikrosekunden -- und in Kurzform den Inhalt der
 *     Antwort (z. B. `GET_INPUT: state0=0x00 state1=0x7F level0=...`)
 *   - jede Aenderung der Buchsenmodi, jede Sitzvergabe (CONNECT/Index),
 *     jeder Tastendruck-Wechsel
 *   - Fehler ausdruecklich und im Klartext, mit Rueckgabewert
 *   - taktmaessig ein Lebenszeichen (Herzschlag) mit Zaehlern, letztem Status
 *     und den verworfenen Rahmen
 *   - eine Schlusszeile beim Beenden
 *
 * WARUM DAS DEN BUS NICHT BREMSEN SOLL
 * ------------------------------------
 * Die serielle Hardware der V4 hat EIN Byte Empfangspuffer; wer zwischen zwei
 * Lesevorgaengen ausgibt, verliert Zeichen (tools/SERIELL.md). Dieses Programm
 * EMPFAENGT auf dem Debug-UART nichts -- es sendet nur, damit ist die Regel hier
 * nicht verletzt. Trotzdem gilt: Ausgaben werden ZUSAMMENGEFASST. Je Meldung
 * entsteht eine fertige Zeile im Puffer, und der UART bekommt sie mit EINEM
 * ApolloDebugPutStr. Ein Zeichen je Aufruf (frueher: ApolloDebugPutChar in einer
 * Schleife) waere der falsche Weg. Und: kein Delay() in der Naehe des
 * Buszugriffs.
 *
 * SCHALTER
 * --------
 *   Vorgabe: Start, Aenderungen, Fehler und EIN Lebenszeichen je Sekunde.
 *   -quiet   : Transaktionszeilen aus (die Vorgabe der Oberflaeche).
 *   -trace   : zusaetzlich jeden Transaktionsschritt (tx/rx/pruefen/...) UND
 *              die Zeile je Transaktion.
 *   -x       : mit -trace zusaetzlich die Rahmenbytes als Hexdump.
 *
 * Die Vorgabe "Transaktionszeilen an" gilt fuer die KONSOLE (dort ist die Zeile
 * je Transaktion die Arbeitsgrundlage). Die MUI-Oberflaeche setzt die Vorgabe
 * auf "aus": bei 200 ms Takt waeren das zehn Zeilen je Sekunde, und jedes
 * Zeichen kostet am DebugUART Wartezeit. Dort kommen sie nur mit -trace.
 *
 * EIN LEBENSZEICHEN, KEINE ZWEI
 * -----------------------------
 * v4_dbg_herz() schreibt die Herzzeile. Wer zusaetzlichen Text in DIESELBE
 * Zeile bringen will (die Oberflaeche haengt dort den Vergleich direkt <-> I2C
 * an), ruft vorher v4_dbg_herz_zusatz(). So bleibt es bei einer Zeile je
 * Sekunde, statt eine zweite aufzumachen.
 */

#ifndef V4_DEBUG_H
#define V4_DEBUG_H

/*
 * Groesste Zeile, die v4_debug.c baut. Oeffentlich, damit Tests und Aufrufer
 * pruefen koennen, dass eine Zeile WIRKLICH vollstaendig hineinpasst: wird sie
 * abgeschnitten, fehlt am Ende genau die Aussage, auf die es ankommt (bei der
 * Herzzeile also der Rohwert CIAAPRA/POTINP).
 */
#define V4_DBG_ZEILE_MAX 420u

#include <stddef.h>

#include "v4_master.h"
#include "v4_proto.h"

typedef struct {
    const char *programm;   /* z.B. "BT Gamecontroller GUI"             */
    const char *version;    /* z.B. "0.2"                               */
    const char *datum;      /* z.B. "13.10.2026"                        */
    const char *logdatei;   /* NULL = keine bzw. Vorgabe der Plattform  */
    unsigned    addr7;      /* I2C-Adresse (7 Bit)                      */
    int         uart;       /* 1 = Debug-UART der V4 benutzen           */
    int         stdout_an;  /* 1 = Standardausgabe benutzen             */
    int         quiet;      /* 1 = keine Zeile je Transaktion           */
} v4_dbg_cfg_t;

/* Setzt die Senken. Vor dem ersten v4_dbg() rufen. Ohne Aufruf ist die
 * Ausgabe AUS (wichtig fuer die Testsuite, die den Trace nicht setzt). */
void v4_dbg_init(const v4_dbg_cfg_t *cfg);

/* -trace: jeden Transaktionsschritt einzeln zeigen. */
void v4_dbg_set_detail(int an);
/* -x: mit -trace zusaetzlich die Rahmenbytes. */
void v4_dbg_set_hex(int an);

/* Zusaetzliche Senke je Zeile (ohne Umbruch) -- die MUI-Oberflaeche haengt
 * hier ihr Logfeld ein. NULL = keine. */
void v4_dbg_set_line_hook(void (*hook)(const char *zeile));

/* Die eine Ausgabefunktion: formatieren, dann EINE Zeile in alle Senken. */
void v4_dbg(const char *fmt, ...);

/* Eine bereits fertige Zeile (mit Umbruch) in alle Senken geben. Fuer
 * Meldungen, die das Frontend selbst formatiert hat -- damit Konsole,
 * Debug-UART und Logdatei wirklich denselben Text bekommen. */
void v4_dbg_zeile(const char *zeile);

/* Datum und Uhrzeit der Plattform als Text (fuers Startbanner). */
void v4_dbg_zeit(char *out, size_t cap);

/* Startbanner: Name, Version, Datum, Uhrzeit, Logdatei, I2C-Adresse. */
void v4_dbg_kopf(void);

/* Trace-Haken fuer v4_master_t.trace -- hier entsteht die Zeile je
 * Transaktion. */
void v4_dbg_trace(const v4_trace_t *ev, void *ctx);

/* Klartext zu Befehl und Trace-Ereignis (auch fuer die Frontends). */
const char *v4_dbg_cmd_name(uint8_t cmd);

/* ---- Kurzformen der Antworten ------------------------------------ */

void v4_dbg_info  (const v4p_info_t *info);
void v4_dbg_status(const v4p_status_t *st);   /* nur bei Aenderung */
void v4_dbg_sitzung(const v4p_status_t *st);  /* Sitzvergabe, nur bei Wechsel */
void v4_dbg_hid   (const v4p_hid_t *hid);
void v4_dbg_input (const v4p_input_t *in);    /* Tastendruck-Wechsel */
void v4_dbg_modes (const uint8_t mode[V4P_PORT_COUNT], const char *warum);
void v4_dbg_devs  (const v4p_dev_t *tab, uint8_t n);

/* Fehler im Klartext, mit Rueckgabewert und der Diagnose des Masters. */
void v4_dbg_fehler(const char *was, uint8_t rc, const v4_master_t *m);

/* Lebenszeichen: Zaehler, letzter Status, verworfene Rahmen. */
void v4_dbg_herz(const v4_master_t *m);

/* Zusatz fuer die NAECHSTE Herzzeile (z.B. der Vergleich direkt <-> I2C).
 * Wird als " | <text>" angehaengt, damit es bei EINER Zeile je Sekunde bleibt.
 * NULL oder "" schaltet den Zusatz wieder ab. Der Text wird kopiert. */
void v4_dbg_herz_zusatz(const char *text);

/* Schlusszeile. */
void v4_dbg_ende(const v4_master_t *m);

#endif /* V4_DEBUG_H */
