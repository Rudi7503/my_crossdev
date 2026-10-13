/*
 * v4_mock.h -- ESP32-Emulator fuer den Linux-Harness (§14.1).
 *
 * Der Mock ist die ZWEITE Implementierung der Plattformhaken aus v4_master.h:
 * er benutzt dieselben Aufbau- und Prueffunktionen aus v4_proto.c, damit kein
 * Format-Drift zwischen den beiden Seiten entstehen kann (§14.1).
 *
 * Nachgebildet werden die Verhaltensweisen, die das Protokoll tragen:
 *   - R1: genau eine Antwort pro Befehl; ohne Befehl liefert der Bus 0xFF
 *         (der ESP32 hat eine TX-FIFO, keine Nachrichtenwarteschlange)
 *   - 128-Byte-READ-Rahmen, damit die Rahmengroesse ueberhaupt geprueft wird
 *   - Statuscodes inkl. BAD_CMD/BAD_ARG/BAD_STATE/NOT_FOUND/BT_ERR
 *   - die zwei Buchsen: Betriebsart, Zug-Maske, GEMESSENE Pegel,
 *     Quadraturzaehler -- inklusive der Moeglichkeit, beide absichtlich
 *     auseinanderlaufen zu lassen (fehlender Pull-up)
 *   - Geraeteliste und Verbindungszustand: CONNECT setzt CONNECTING, erst
 *     weitere GET_STATUS fuehren nach CONNECTED (so wie der ESP32 arbeitet)
 *   - GET_HID mit Name, VID/PID, Reportzaehler
 *   - RESET gibt beide Buchsen frei, laesst die Verbindung aber stehen
 * Dazu Stoerknoepfe fuer die Testmatrix (Framing-Fehler, BUSY, Link-Ausfall,
 * alte 10-Byte-GET_INPUT-Antwort ohne Pegel).
 *
 * Der Mock liefert die Pegel als ZUSTAND, nicht als Rechnung: er kennt die
 * Zug-Maske und den Pegel getrennt und kann damit jede der vier Kombinationen
 * je Leitung darstellen (siehe v4p_level_missing_pullup() in v4_proto.h).
 */

#ifndef V4_MOCK_H
#define V4_MOCK_H

#include "v4_proto.h"

/* ------------------------------------------------------------------ */
/* Stoerknoepfe -- §14.1                                               */
/* ------------------------------------------------------------------ */

typedef struct {
    int fail_write;             /* jeder Schreibzugriff scheitert          */
    int fail_read;              /* jeder Lesezugriff scheitert             */
    int drop_read_every;        /* jeder n-te Read scheitert               */
    int drop_next_read;         /* der naechste Read scheitert (einmalig)  */
    int partial_read_once;      /* naechster Read liefert nur die Haelfte
                                 * (Antwort bleibt unvollstaendig im FIFO) */
    int bad_crc_every;          /* jeder n-te Read mit falscher CRC        */
    int no_seq_echo_every;      /* jeder n-te Read spiegelt seq nicht      */
    int bad_frame_len_every;    /* jeder n-te Read meldet len = 122        */
    /* Einmalige Verfaelschungen. Sie sind noetig, weil die "jeder n-te"-Regel
     * die ERSTE Antwort (Nummer 1) nie trifft -- und genau die erste Antwort
     * ist in einem Testfall die interessante. */
    int bad_crc_once;           /* der naechste Rahmen hat eine falsche CRC */
    int no_seq_once;            /* der naechste Rahmen spiegelt seq nicht   */
    int bad_frame_len_once;     /* der naechste Rahmen meldet len = 122     */
    int busy_count;             /* die naechsten n Antworten BUSY          */
    int busy_every;             /* jeder n-te Befehl BUSY                  */
    int never_ready;            /* immer BUSY                              */
    int force_status;           /* >= 0: einmalig erzwungener Status       */
    int connect_rounds;         /* GET_STATUS-Runden CONNECTING vor CONNECTED */
    int scan_delay;             /* DEV_COUNT liefert erst nach n Aufrufen
                                 * Geraete (der Scan laeuft asynchron)     */
    int bad_set_port_echo;      /* SET_PORT_MODE bestaetigt einen anderen
                                 * Modus -> der Master muss V4_ERR_FRAME
                                 * melden, statt der Bestaetigung zu glauben */
    int legacy_input10;         /* GET_INPUT antwortet mit der alten
                                 * 10-Byte-Fassung OHNE Pegel. Damit laesst
                                 * sich der Rueckfallweg des Masters pruefen:
                                 * er darf dann keinen Pegel anzeigen. */

    /* Diagnose */
    unsigned long writes;       /* verarbeitete WRITE-Frames               */
    unsigned long reads;        /* Leseaufrufe                             */
    unsigned long delay_calls;  /* Aufrufe von v4_plat_delay_us            */
    unsigned long delay_us;     /* summierte Wartezeit                     */
    int           last_cmd;     /* letzter Befehlscode (-1 = keiner)       */
    int           bad_write_len;/* Schreibzugriffe ohne 32 Byte            */
    uint8_t       seq_log[16];  /* SEQ der letzten 16 WRITE-Frames         */
    int           seq_log_n;    /* Anzahl gueltiger Eintraege in seq_log   */
} v4_mock_knobs_t;

extern v4_mock_knobs_t v4_mock;

/* ------------------------------------------------------------------ */
/* Aufbau und Abbau                                                    */
/* ------------------------------------------------------------------ */

/* Leerer Zustand: keine Geraete, beide Buchsen aus, alles auf 0,
 * alle Stoerknoepfe aus. */
void v4_mock_reset(void);

/* Standardszenario: drei Geraete, Buchse 0 auf Joystick, Buchse 1 aus,
 * Pad-Zustand "nichts gedrueckt". */
void v4_mock_seed_default(void);

/* Geraetetabelle. Liefert 0 bei Erfolg, -1 wenn voll oder Name unbrauchbar. */
int v4_mock_dev_add(const char *name, const uint8_t bda[6]);

/* Buchsen.
 *
 * set_port_state setzt die Zug-Maske UND den Pegel auf den Erwartungswert bei
 * wirkendem Pull-up (losgelassen = High). Wer den Widerspruch "losgelassen,
 * liest aber 0" nachstellen will, ruft DANACH set_port_level -- dann passen
 * Zug und Pegel absichtlich nicht mehr zusammen. */
void v4_mock_set_port_mode (int port, uint8_t mode);
void v4_mock_set_port_state(int port, uint8_t drive_bits);
void v4_mock_set_port_level(int port, uint8_t level_bits);
void v4_mock_set_steps     (uint32_t x, uint32_t y);

/* HID-Zustand, wie ihn GET_HID meldet. `name` darf NULL sein. */
void v4_mock_set_hid(uint8_t kind, uint8_t transport, uint16_t vid,
                     uint16_t pid, uint8_t battery, uint16_t report_id,
                     uint16_t report_len, uint16_t reports, const char *name);

/* Zustand von aussen setzen: "es ist schon jemand verbunden" (Test des
 * Bereits-verbunden-Zweigs in Konsole und GUI). */
void v4_mock_set_connected(int index);

/* Scan-Zustand: 1 = laeuft. Erhoeht die Scan-Generation, wie es der ESP32
 * beim Start tut. */
void v4_mock_set_scan(int active);
void v4_mock_bump_scan_gen(void);

/* Simuliert einen Bus-Reset: halbe Antwort verwerfen, FIFO leeren.
 * Danach muss der naechste Befehl wieder saubere Daten liefern (§14.3). */
void v4_mock_bus_reset(void);

#endif /* V4_MOCK_H */
