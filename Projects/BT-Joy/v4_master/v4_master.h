/*
 * v4_master.h -- I2C-Master der Vampire V4 fuer den ESP32-Slave
 * (Bluetooth-Joystick-Adapter).
 *
 * Gegenstueck zum Audio-Projekt (Projects/I2C-test/v4_master): derselbe
 * Transaktionskern -- SEQ, Wiederholung, BUSY, BAD_CRC, Warte-Backoff -- aber
 * nur die Befehle, die dieser ESP32 kennt (siehe v4_proto.h). Kein SD-, Datei-,
 * Audio- oder EQ-Weg, kein BULK-Rahmen: was hier fehlt, kann die Firmware auch
 * nicht.
 *
 * Die Bibliothek ist plattformunabhaengig. Sie ruft ausschliesslich die Haken
 * unten auf; die Plattformschicht liefert sie:
 *
 *   v4_linux_i2c.c  Linux-Harness / Demo  (/dev/i2c-N, i2c-dev)
 *   v4_amiga_i2c.c  Zielsystem Vampire V4 (i2c.library + timer.device)
 *   v4_mock.c       ESP32-Emulator fuer Tests ohne Hardware
 *
 * GERUFEN WIRD DAS DIREKT AUS DEM HAUPTFADEN des jeweiligen Frontends
 * (v4_gui.c bzw. v4_console.c): kein CreateNewProc, kein zweiter Faden, kein
 * Nachrichtenport. Genau diese Konstruktion ist auf der V4 gelaufen.
 *
 * i2c.library hat keinen Timeout: SendI2C/ReceiveI2C blockieren, bis die
 * Transaktion durch ist. Ein haengender Bus haelt damit auch den
 * MUI-Ereignisablauf an -- das ist bewusst so und im README vermerkt. Eine
 * eigene Aufgabe hat das Problem am 13.10.2026 NICHT geloest, sondern den Start
 * des Programms verhindert. Deshalb gilt hier ausserdem: NIE Forbid().
 *
 * Es wird immer GENAU EIN Paar (Schreiben 32 Byte, Lesen 128 Byte) gefahren und
 * danach die vorgeschriebene Pause eingehalten: der ESP32 kann als I2C-Slave
 * kein Clock-Stretching (§1.3), deshalb gelten R1/R2/R3.
 *
 * Adresse: 7 Bit 0x5E. ACHTUNG -- das Audio-Projekt spricht denselben Adapter
 * unter 0x50 an. Wer beide Programme gleichzeitig laufen laesst, sucht den
 * Fehler sonst in der Firmware statt in der Adresse.
 */

#ifndef V4_MASTER_H
#define V4_MASTER_H

#include "v4_proto.h"

/* ------------------------------------------------------------------ */
/* Zeit- und Zaehlparameter -- §1.4                                    */
/* ------------------------------------------------------------------ */

#define V4_T_WAIT_US        2000u   /* zwischen Schreiben und Lesen        */
#define V4_T_BUSY_US        2000u   /* vor dem erneuten Senden bei BUSY    */
#define V4_RETRIES          4       /* Versuche bei Framing-/Link-Fehler   */
/* Obergrenze fuer die verlaengerte Wartezeit im Wiederholungsfall: liefert der
 * Slave einen unbrauchbaren Rahmen, bekommt er beim naechsten Versuch mehr Zeit
 * (Verdopplung ab t_wait). R3 nennt eine Untergrenze -- laenger warten ist
 * erlaubt, und im Feld brauchte der ESP32 deutlich mehr als 2000 us. */
#define V4_T_WAIT_RETRY_MAX_US 100000u

/* Verdoppelt die Wartezeit bis zur Obergrenze (siehe oben) und merkt sie als
 * neue Grundwartezeit. Ohne dieses Lernen startet jede Transaktion wieder bei
 * t_wait und laeuft in dieselben Framing-Fehler. */
#define V4_WAIT_BACKOFF(m, w)                                                \
    do {                                                                     \
        if ((w) < V4_T_WAIT_RETRY_MAX_US) {                                  \
            (w) *= 2u;                                                       \
            if ((w) > V4_T_WAIT_RETRY_MAX_US) {                              \
                (w) = V4_T_WAIT_RETRY_MAX_US;                                \
            }                                                                \
            if ((w) > (m)->t_wait_us) {                                      \
                (m)->t_wait_us = (w);        /* gelernt: naechste Mal sofort */\
            }                                                                \
        }                                                                    \
    } while (0)

#define V4_BUSY_TRIES       40      /* ≈ 80 ms pro Befehl                  */
#define V4_BADCRC_TRIES     4       /* Klarstellung, siehe README des
                                     * Audio-Projekts, Abschnitt "Abweichungen
                                     * und Klaerungen", Punkt 2 */
/* CONNECT laeuft auf dem ESP32 asynchron: der Befehl ist angenommen, die
 * Verbindung steht erst danach. Der Warteweg dafuer ist v4_wait_state() -- hier
 * steht nur das BUSY-Budget fuer den Befehl selbst. */
#define V4_POLL_WAIT_US     50000u  /* §13: Poll-Abstand fuer v4_wait_state */

/* ------------------------------------------------------------------ */
/* Adressierung -- §1.1                                                */
/* ------------------------------------------------------------------ */

#define V4_I2C_ADDR7        0x5Eu   /* 7-Bit-Adresse des ESP32             */
#define V4_I2C_ADDR8_W      0xBCu   /* addr7 << 1 | W                      */

/* ------------------------------------------------------------------ */
/* Rueckgabewerte des Masters                                          */
/*                                                                     */
/* Der Master gibt den Protokoll-Status (0x00..0x0C) direkt zurueck,    */
/* wenn eine gueltige Antwort vorlag. Transport- und Aufruffehler liegen */
/* in einem Band, das mit keinem Status kollidiert.                      */
/* ------------------------------------------------------------------ */

#define V4_ERR_LINK         0xE0u   /* alle Versuche erfolglos (Link tot)  */
#define V4_ERR_BUSY         0xE1u   /* BUSY-Budget erschoepft              */
#define V4_ERR_ARG          0xE2u   /* Argument lokal unzulaessig          */
#define V4_ERR_FRAME        0xE3u   /* Antwort gueltig, aber Nutzlast falsch */
#define V4_ERR_NOSPACE      0xE4u   /* Puffer des Aufrufers zu klein       */
#define V4_ERR_TIMEOUT      0xE6u   /* Zielzustand nicht erreicht          */
#define V4_ERR_PROTO        0xE7u   /* Firmware meldet ein anderes Protokoll */

/* Flags fuer v4_transact_n */
#define V4_X_BUSY_RETRY     0x01u   /* BUSY intern wiederholen (Regelfall) */

/* ------------------------------------------------------------------ */
/* Plattformhaken -- vom Aufrufer zu liefern                           */
/* ------------------------------------------------------------------ */

int  v4_plat_open(const char *dev);     /* dev darf NULL sein (Mock/Amiga) */
void v4_plat_close(void);
int  v4_plat_i2c_write(uint8_t addr7, const uint8_t *data, uint16_t len);
int  v4_plat_i2c_read (uint8_t addr7, uint8_t *data, uint16_t len);
void v4_plat_delay_us(uint32_t us);

/* Monotone Mikrosekunden-Uhr der Plattform, nur fuer die Messung der Dauer
 * einer Transaktion (DebugUART). 0 = keine Uhr verfuegbar.
 *
 * WICHTIG: immer als uint32_t-DIFFERENZ benutzen, nie als Absolutwert. Die Uhr
 * darf und wird ueberlaufen (Amiga: Sekunden seit dem Start mal 1e6, also nach
 * rund 71 Minuten). Eine Differenz modulo 2^32 bleibt dabei korrekt, solange
 * zwischen den beiden Abfragen weniger als 71 Minuten liegen. */
uint32_t v4_plat_now_us(void);

/* Roher Fehlercode der letzten Busoperation -- fuer die Fehlersuche auf
 * Hardware, wo kein Debugger zur Verfuegung steht:
 *   Amiga: Rueckgabewert von SendI2C/ReceiveI2C
 *   Linux: errno der letzten fehlgeschlagenen Operation
 *   Mock:  0 (der Mock scheitert nur absichtlich) */
unsigned long v4_plat_last_error(void);

/* Klartext zum letzten Busfehler (Amiga: I2CErrText der Library, Linux:
 * strerror(errno), Mock: "Mock"). Nie NULL. */
const char *v4_plat_error_text(void);

/* ------------------------------------------------------------------ */
/* Logdatei -- die Ausgabe ueberlebt den Absturz                       */
/*                                                                     */
/* Beide Frontends (Konsole und MUI) schreiben jede Zeile ausserdem in  */
/* eine Datei: beim Absturz ist das Fenster weg, die Datei nicht.       */
/*                                                                     */
/*   v4_plat_log_open(NULL) -> Standardpfad der Plattform:             */
/*        Amiga: V4_LOG_DEFAULT_AMIGA ("Programs:test/v4_joy.log"),     */
/*               "w", also eine frische Datei je Lauf                   */
/*        Linux/Mock: kein Standardpfad, nur mit ausdruecklichem Pfad    */
/*   Rueckgabe 0 = offen, -1 = nicht verfuegbar (KEIN Fehler -- die     */
/*   Ausgabe geht dann nur auf die Konsole).                            */
/*                                                                     */
/* Die Amiga-Seite schliesst die Datei nach JEDER Zeile wieder und       */
/* oeffnet sie anhaengend neu: AmigaDOS puffert im FileHandle, und ein   */
/* harter Absturz nimmt sonst genau den Schwanz mit, auf den es          */
/* ankommt.                                                              */
/* ------------------------------------------------------------------ */

#define V4_LOG_DEFAULT_AMIGA "Programs:test/v4_joy.log"

int  v4_plat_log_open (const char *path);
long v4_plat_log_write(const char *s, unsigned long len);
void v4_plat_log_close(void);

/* Wartezeit nach jedem Schreiben, in Plattform-Ticks (Amiga: 1/50 s).
 * 0 = sofort weiter. Linux/Mock: ohne Wirkung. */
void v4_plat_log_settle(int ticks);

/* ------------------------------------------------------------------ */
/* Trace -- Debugausgabe im Transaktionskern                           */
/*                                                                     */
/* Auf Hardware ohne Debugger ist das die einzige Moeglichkeit zu       */
/* sehen, WIE WEIT eine Transaktion kommt. Der Haken wird VOR jedem     */
/* Schritt gerufen, also steht im Log als letzte Zeile der Schritt, in  */
/* dem es geknallt hat.                                                 */
/*                                                                     */
/* Wichtig: der Haken muss NACH v4_init()/v4_open() gesetzt werden --   */
/* v4_init() nullt die Struktur. NULL = kein Zusatzaufwand.             */
/* `data` gilt nur waehrend des Aufrufs.                                */
/* ------------------------------------------------------------------ */

enum {
    V4_TR_TX_BEGIN = 1,     /* vor dem Schreiben   (len = 32)           */
    V4_TR_TX_END,           /* nach dem Schreiben  (rc = 0/-1)          */
    V4_TR_WAIT,             /* vor dem Warten      (us = t_wait/t_busy) */
    V4_TR_RX_BEGIN,         /* vor dem Lesen       (len = 128)          */
    V4_TR_RX_END,           /* nach dem Lesen      (rc = 0/-1, data)    */
    V4_TR_CHECK,            /* Frame-Pruefung      (rc = V4P_CHECK_*)   */
    V4_TR_RETRY,            /* Wiederholung        (rc = Grund)         */
    V4_TR_BUSY,             /* BUSY-Antwort, neuer Versuch              */
    V4_TR_DONE              /* Befehl beendet (Status ODER Fehler)      */
};

/*
 * V4_TR_DONE kommt auf JEDEM Ende einer Transaktion, auch auf einem Fehler
 * (rc = V4_ERR_LINK / V4_ERR_BUSY / V4P_ST_BAD_CRC). `status` ist dann 0,
 * `len` die Nutzlastlaenge der letzten gueltigen Antwort (0, wenn keine kam).
 * In `us` steht die GESAMTE Dauer der Transaktion in Mikrosekunden -- auch bei
 * den anderen Ereignissen ist `us` gesetzt, dort aber als Wartezeit.
 * Genau darauf baut v4_debug.c seine eine Zeile je Transaktion.
 */

typedef struct {
    uint8_t        event;   /* V4_TR_* */
    uint8_t        cmd;
    uint8_t        seq;
    uint8_t        status;  /* Statuscode, sofern bekannt */
    uint16_t       len;     /* Laenge, sofern bekannt */
    int            rc;      /* Rueckgabewert des Schritts */
    uint32_t       us;      /* Wartezeit; bei V4_TR_DONE die Dauer    */
    const uint8_t *data;    /* Puffer, nur waehrend des Aufrufs gueltig */
} v4_trace_t;

typedef void (*v4_trace_fn)(const v4_trace_t *ev, void *ctx);

/* ------------------------------------------------------------------ */
/* Konvention von i2c.library V39+ -- WICHTIG, leicht zu verwechseln   */
/*                                                                     */
/* Quelle: i2c.generic.s (ReportAndFinish, I2CErrText) und die Tools   */
/* src/SendI2C.c / src/ReceiveI2C.c. Die Rueckgabe ist $00AABBCC:      */
/*                                                                     */
/*     CC = 0xFF   -> OK                                               */
/*     CC = 0x00   -> Fehler; BB = I/O-Fehler (1..8),                  */
/*                            AA = Allokationsfehler (1..6)            */
/*                                                                     */
/* Es ist also NICHT "0 = OK", sondern genau umgekehrt. Die Regel steht */
/* hier als Helfer, damit sie geprueft werden kann: sie war im          */
/* Audio-Projekt zuerst falsch herum implementiert und hat JEDE         */
/* erfolgreiche Transaktion als Fehler gemeldet (PING endete in         */
/* V4_ERR_LINK).                                                        */
/* ------------------------------------------------------------------ */
static inline int v4_i2c_err_is_ok(unsigned long err)
{
    return (err & 0xFFul) != 0ul;
}

/* ------------------------------------------------------------------ */
/* Master-Zustand                                                      */
/* ------------------------------------------------------------------ */

typedef struct {
    uint8_t  seq;                       /* naechste Sequenznummer      */
    unsigned try_max;                   /* Versuche je Transaktion bei
                                         * Framing-/Linkfehlern (Vorgabe
                                         * V4_RETRIES; im Feld erhoehen, weil
                                         * die Antwortbereitschaft des Slaves
                                         * streut -- Schalter -r). */
    uint32_t t_wait_us;                 /* Wartezeit vor dem Lesen (R3).
                                         * Vorgabe V4_T_WAIT_US; im Feld
                                         * anpassbar (Schalter -w), weil die
                                         * Bereitschaft des Slaves je nach
                                         * Firmware/BT-Zustand streut. */
    /* Diagnose -- nuetzlich fuer Tests und Feldprobleme */
    unsigned long tx_frames;    /* abgesetzte WRITE-Frames             */
    unsigned long rx_frames;    /* gelesene Antworten                  */
    unsigned long retries;      /* Wiederholungen wegen Link/Framing   */
    unsigned long busy_rounds;  /* BUSY-Antworten, intern wiederholt   */
    unsigned long badcrc_rounds;/* BAD_CRC-Antworten, Befehl wiederholt */
    unsigned long unsafe_retries;       /* Wiederholung NACH erfolgreichem
                                         * Schreiben: der Befehl kann zweimal
                                         * ausgefuehrt worden sein (§7/§6) */
    int           last_check;   /* letzter V4P_CHECK_*-Grund           */
    /* Debugausgabe -- nach v4_init()/v4_open() setzen (siehe oben) */
    v4_trace_fn   trace;
    void         *trace_ctx;
} v4_master_t;

/* Setzt seq=0, Zaehler 0, t_wait = Vorgabe. */
void v4_init(v4_master_t *m);

/* v4_plat_open + v4_init. Liefert 0 bei Erfolg. */
int  v4_open(v4_master_t *m, const char *dev);
void v4_close(void);

/* ------------------------------------------------------------------ */
/* Transaktionskern -- §6                                              */
/* ------------------------------------------------------------------ */

/* Ein Befehl, genau eine Antwort. Behandelt Wiederholungen, BUSY und
 * BAD_CRC intern; SEQ wird nur nach einer gueltigen Antwort erhoeht.
 * Rueckgabe: Protokoll-Status (0x00..0x0C) oder V4_ERR_*.
 * `out` wird nur bei gueltiger Antwort beschrieben. */
uint8_t v4_transact(v4_master_t *m, uint8_t cmd,
                    const uint8_t *payload, uint8_t len, v4p_read_t *out);

/* Wie v4_transact, aber mit steuerbarem BUSY-Verhalten und eigenem
 * BUSY-Budget (Anzahl Runden). Ohne V4_X_BUSY_RETRY wird eine BUSY-Antwort
 * an den Aufrufer durchgereicht. */
uint8_t v4_transact_n(v4_master_t *m, uint8_t cmd,
                      const uint8_t *payload, uint8_t len, v4p_read_t *out,
                      unsigned flags, unsigned busy_tries);

/* ------------------------------------------------------------------ */
/* Befehls-Wrapper -- §11                                              */
/* ------------------------------------------------------------------ */

/* 0x01 / 0x03: beide liefern dieselbe 12-Byte-Nutzlast. */
uint8_t v4_ping     (v4_master_t *m, v4p_info_t *info);
uint8_t v4_get_info (v4_master_t *m, v4p_info_t *info);

/* Prueft eine GET_INFO-Antwort gegen diese Implementierung: proto_ver,
 * write/read_frame_len. Ein Firmware-Stand mit anderen Rahmengroessen (z.B.
 * die alte 64-Byte-Fassung) wuerde sonst stillschweigend Muell liefern --
 * deshalb ist das kein "nice to have", sondern die erste Pruefung nach PING.
 * `bulk_payload_max` wird NICHT verlangt: 0 ist hier die Aussage "kein
 * Dateiweg", ein spaeterer Stand darf dort etwas anderes melden. */
uint8_t v4_check_info(const v4p_info_t *info);

uint8_t v4_get_status(v4_master_t *m, v4p_status_t *st);            /* 0x02 */

uint8_t v4_set_port_mode(v4_master_t *m, uint8_t port, uint8_t mode,
                         uint8_t *applied);                         /* 0x04 */
uint8_t v4_get_port_mode(v4_master_t *m, uint8_t mode[V4P_PORT_COUNT]); /* 0x05 */
/* Liest Zug-Masken, Quadraturzaehler und -- falls die Firmware sie liefert --
 * die GEMESSENEN Pegel. Beide Nutzlastlaengen werden angenommen: 12 Byte (mit
 * Pegeln, `out->level_valid = 1`) und 10 Byte (aeltere Fassung, Pegel
 * UNBEKANNT, `out->level_valid = 0`). Eine Laenge dazwischen ist ein Fehler
 * und ergibt V4_ERR_FRAME: sie waere ein Hinweis auf einen verschobenen Strom
 * und darf nicht als "keine Pegel" durchgehen. */
uint8_t v4_get_input (v4_master_t *m, v4p_input_t *in);             /* 0x06 */
uint8_t v4_get_hid   (v4_master_t *m, v4p_hid_t *hid);              /* 0x07 */

uint8_t v4_scan_start(v4_master_t *m, uint8_t inq_units, int continuous); /* 0x10 */
uint8_t v4_scan_stop (v4_master_t *m);                              /* 0x11 */
uint8_t v4_dev_count (v4_master_t *m, uint8_t *count);              /* 0x12 */
uint8_t v4_dev_get   (v4_master_t *m, uint8_t idx, v4p_dev_t *dev); /* 0x13 */

uint8_t v4_connect    (v4_master_t *m, uint8_t idx);                /* 0x20 */
uint8_t v4_connect_bda(v4_master_t *m, const uint8_t bda[6]);       /* 0x21 */
uint8_t v4_disconnect (v4_master_t *m);                             /* 0x22 */
uint8_t v4_forget     (v4_master_t *m);                             /* 0x23 */

uint8_t v4_reset      (v4_master_t *m);                             /* 0x7E */

/* ------------------------------------------------------------------ */
/* Ablaufhilfen -- §13                                                 */
/* ------------------------------------------------------------------ */

/* GET_STATUS pollen, bis state == want (oder Versuche erschoepft).
 * Gebraucht nach CONNECT: der Befehl ist angenommen, die Verbindung steht
 * erst danach (CONNECTING -> CONNECTED). */
uint8_t v4_wait_state(v4_master_t *m, uint8_t want, unsigned tries,
                      v4p_status_t *last);

/*
 * "Buchsen tauschen": die Betriebsarten beider Buchsen vertauschen.
 *
 * Der ESP32 hat dafuer bewusst KEINEN Befehl -- er wuerde nur eine zweite
 * Schreibweise fuer SET_PORT_MODE schaffen, und zwei Wege zu einer Sache sind
 * ein Weg zu viel. Der Tausch ist deshalb hier gebaut: erst beide Modi lesen,
 * dann zweimal SET_PORT_MODE in der getauschten Reihenfolge.
 *
 * ACHTUNG Reihenfolge: es wird NICHT "0 <- 1, dann 1 <- 0" geschrieben. Nach
 * dem ersten Schreiben stuende der alte Modus von Buchse 0 nicht mehr zur
 * Verfuegung. Die gelesenen Werte werden deshalb vorher gemerkt.
 *
 * `now` (darf NULL sein) erhaelt die Modi NACH dem Tausch.
 * Rueckgabe: V4P_ST_OK nur, wenn BEIDE Schreibvorgaenge bestaetigt wurden.
 */
uint8_t v4_swap_port_modes(v4_master_t *m, uint8_t now[V4P_PORT_COUNT]);

/* ------------------------------------------------------------------ */
/* Diagnose                                                            */
/* ------------------------------------------------------------------ */

/* Text zu Protokoll-Status und V4_ERR_*. */
const char *v4_strerror(uint8_t code);

/* Text zu einem V4P_CHECK_*-Wert (fuer Fehlersuche im Transaktionskern). */
const char *v4_check_str(int rc);

#endif /* V4_MASTER_H */
