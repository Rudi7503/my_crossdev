/*
 * v4_master.h -- I2C-Master der Vampire V4 fuer den ESP32-Slave.
 * Implementierung von PROTOCOL_V4_MASTER.md §6 und §11.
 *
 * Die Bibliothek ist plattformunabhaengig. Sie ruft ausschliesslich die fuenf
 * Haken unten auf; die Plattformschicht liefert sie:
 *
 *   v4_linux_i2c.c  Linux-Harness / Demo  (/dev/i2c-N, i2c-dev)
 *   v4_amiga_i2c.c  Zielsystem Vampire V4 (i2c.library + timer.device)
 *   v4_mock.c       ESP32-Emulator fuer Tests ohne Hardware
 *
 * Es wird immer GENAU EIN Paar (Schreiben 32 Byte, Lesen 64 bzw. 12+chunk)
 * gefahren und danach die vorgeschriebene Pause eingehalten: der ESP32 kann
 * als I2C-Slave kein Clock-Stretching (§1.3), deshalb gelten R1/R2/R3.
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
#define V4_BUSY_TRIES       40      /* ≈ 80 ms pro Befehl                  */
#define V4_BADCRC_TRIES     4       /* Klarstellung, siehe README, Abschnitt
                                     * "Abweichungen und Klaerungen", Punkt 2 */
/* PLAY_FILE/STOP_PLAY bauen die Audioquelle um (§8.3). Die Spezifikation nennt
 * dafuer ein eigenes Budget von ~1 s ("etwa 500 Runden a 2 ms"), deutlich mehr
 * als die 40 Runden des allgemeinen Budgets -- ein zu knappes Budget wuerde
 * einen funktionierenden Umbau als Timeout melden. Der ESP32 bleibt nie
 * dauerhaft BUSY: der Umbau endet mit OK oder einem Fehlerstatus (§8.3). */
#define V4_PLAY_BUSY_TRIES  500
#define V4_MOUNT_WAIT_US    200000u /* §8.1: ~200 ms nach SD_MOUNT(force=1) */
#define V4_POLL_WAIT_US     50000u  /* §13: Poll-Abstand fuer *_wait_state */

/* ------------------------------------------------------------------ */
/* Adressierung -- §1.1                                                */
/* ------------------------------------------------------------------ */

#define V4_I2C_ADDR7        0x50u   /* 7-Bit-Adresse des ESP32             */
#define V4_I2C_ADDR8_W      0xA0u   /* addr7 << 1 | W                      */

/* ------------------------------------------------------------------ */
/* Rueckgabewerte des Masters                                          */
/*                                                                     */
/* Der Master gibt den Protokoll-Status (0x00..0x0C) direkt zurueck,    */
/* wenn eine gueltige Antwort vorlag -- so wie in §14.1 gefordert       */
/* ("Rueckgabewert = 0x07" fuer NOT_FOUND). Transport- und              */
/* Aufruffehler liegen in einem Band, das mit keinem Status kollidiert. */
/* ------------------------------------------------------------------ */

#define V4_ERR_LINK         0xE0u   /* 4 Versuche erfolglos (Link tot)     */
#define V4_ERR_BUSY         0xE1u   /* 40x BUSY, Zeitbudget erschoepft     */
#define V4_ERR_ARG          0xE2u   /* Argument lokal unzulaessig          */
#define V4_ERR_FRAME        0xE3u   /* Antwort gueltig, aber Nutzlast zu kurz */
#define V4_ERR_NOSPACE      0xE4u   /* Puffer des Aufrufers zu klein       */
#define V4_ERR_NOMEM        0xE5u   /* Allokation fehlgeschlagen           */
#define V4_ERR_TIMEOUT      0xE6u   /* Zielzustand nicht erreicht          */
#define V4_ERR_PROTO        0xE7u   /* Firmware meldet ein anderes Protokoll */

/* Flags fuer v4_transact_ex -- §6 gegen §8.1 abgrenzen */
#define V4_X_BUSY_RETRY     0x01u   /* BUSY intern wiederholen (Regelfall) */

/* ------------------------------------------------------------------ */
/* Plattformhaken -- vom Aufrufer zu liefern                           */
/* ------------------------------------------------------------------ */

int  v4_plat_open(const char *dev);     /* dev darf NULL sein (Mock/Amiga) */
void v4_plat_close(void);
int  v4_plat_i2c_write(uint8_t addr7, const uint8_t *data, uint16_t len);
int  v4_plat_i2c_read (uint8_t addr7, uint8_t *data, uint16_t len);
void v4_plat_delay_us(uint32_t us);

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
/* Optionale serielle Diagnoseausgabe                                  */
/*                                                                     */
/* Zweck: die Ausgaben zusaetzlich auf die serielle Schnittstelle      */
/* legen, damit sie am PC mitgelesen werden koennen (V4 -> COMx).      */
/* Das ist beim Suchen eines Absturzes dem Bildschirmfenster           */
/* ueberlegen: was der UART bereits gepuffert hat, wird auch dann noch */
/* gesendet, wenn die Task unmittelbar danach stirbt.                   */
/*                                                                     */
/*   v4_plat_serial_open(NULL) -> Standardgeraet der Plattform:        */
/*        Amiga: "ser:" = serial.device Unit 0, AmigaOS-Vorgabe        */
/*               9600 Baud, 8 Datenbits, keine Paritaet, 1 Stopbit     */
/*        Linux/Mock: kein Standardgeraet, nur mit ausdruecklichem     */
/*                Pfad (Geraet oder Datei)                             */
/*   Rueckgabe 0 = offen, -1 = nicht verfuegbar (KEIN Fehler -- die    */
/*   Ausgabe geht dann nur auf die Konsole).                           */
/* ------------------------------------------------------------------ */

int  v4_plat_serial_open (const char *dev);
long v4_plat_serial_write(const char *s, unsigned long len);
void v4_plat_serial_close(void);

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
    V4_TR_TX_BEGIN = 1,     /* vor dem Schreiben   (len = 32)          */
    V4_TR_TX_END,           /* nach dem Schreiben  (rc = 0/-1)         */
    V4_TR_WAIT,             /* vor dem Warten      (us = t_wait/t_busy)*/
    V4_TR_RX_BEGIN,         /* vor dem Lesen       (len = 128/chunk)   */
    V4_TR_RX_END,           /* nach dem Lesen      (rc = 0/-1, data)   */
    V4_TR_CHECK,            /* Frame-Pruefung      (rc = V4P_CHECK_*)  */
    V4_TR_RETRY,            /* Wiederholung        (rc = Grund)        */
    V4_TR_BUSY,             /* BUSY-Antwort, neuer Versuch             */
    V4_TR_DONE              /* Befehl beendet      (rc = Status/Fehler)*/
};

typedef struct {
    uint8_t        event;   /* V4_TR_* */
    uint8_t        cmd;
    uint8_t        seq;
    uint8_t        status;  /* Statuscode, sofern bekannt */
    uint16_t       len;     /* Laenge, sofern bekannt */
    int            rc;      /* Rueckgabewert des Schritts */
    uint32_t       us;      /* Wartezeit */
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
/* Es ist also NICHT "0 = OK", sondern genau umgekehrt. Diese Auswerte- */
/* regel steht hier als Helfer, damit sie getestet werden kann: sie war */
/* zuerst falsch herum implementiert und hat JEDE erfolgreiche          */
/* Transaktion als Fehler gemeldet (PING endete in V4_ERR_LINK).        */
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
    uint16_t chunk;                     /* aktuelle Chunkgroesse       */
    uint8_t  path[V4P_PATH_MAX + 1u];   /* per PATH_* gebauter Pfad    */
    uint16_t path_len;
    uint8_t  path_valid;                /* 0 = Spiegel unbekannt: L=0 sperrt */
    /* Diagnose -- nuetzlich fuer Tests und Feldprobleme */
    unsigned long tx_frames;    /* abgesetzte WRITE-Frames             */
    unsigned long rx_frames;    /* gelesene Antworten                  */
    unsigned long retries;      /* Wiederholungen wegen Link/Framing   */
    unsigned long busy_rounds;  /* BUSY-Antworten, intern wiederholt   */
    unsigned long badcrc_rounds;/* BAD_CRC-Antworten, Befehl wiederholt */
    unsigned long unsafe_retries;       /* Wiederholung NACH erfolgreichem
                                         * Schreiben: der Befehl kann zweimal
                                         * ausgefuehrt worden sein (§7/§6) */
    unsigned long possible_handle_leaks;/* OPEN mit unsafe-Retry: auf dem
                                         * Slave ist evtl. ein zweites Handle
                                         * offen (Spec-Luecke, siehe README) */
    int           last_check;   /* letzter V4P_CHECK_*-Grund           */
    /* Debugausgabe -- nach v4_init()/v4_open() setzen (siehe oben) */
    v4_trace_fn   trace;
    void         *trace_ctx;
} v4_master_t;

/* Setzt seq=0, chunk=128, Pfad leer, Zaehler 0 (§13 Schritt 1). */
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
 * `out` wird nur bei V4P_ST_* beschrieben. */
uint8_t v4_transact(v4_master_t *m, uint8_t cmd,
                    const uint8_t *payload, uint8_t len, v4p_read_t *out);

/* Wie v4_transact, aber mit steuerbarem BUSY-Verhalten. Ohne
 * V4_X_BUSY_RETRY wird eine BUSY-Antwort an den Aufrufer durchgereicht --
 * das braucht §8.1, wo der Aufrufer die Mount-Runden selbst faehrt. */
uint8_t v4_transact_ex(v4_master_t *m, uint8_t cmd,
                       const uint8_t *payload, uint8_t len, v4p_read_t *out,
                       unsigned flags);

/* Wie v4_transact_ex, zusaetzlich mit eigenem BUSY-Budget (Anzahl Runden). */
uint8_t v4_transact_n(v4_master_t *m, uint8_t cmd,
                      const uint8_t *payload, uint8_t len, v4p_read_t *out,
                      unsigned flags, unsigned busy_tries);

/* Wie v4_transact, aber fuer Befehle mit BULK-Antwort (FILE_READ).
 * `buf` muss mindestens 12 + m->chunk Byte fassen. handle/block werden aus
 * der Nutzlast gelesen ([0] bzw. [1..2]) und im Bulk-Frame verifiziert. */
uint8_t v4_transact_bulk(v4_master_t *m, uint8_t cmd,
                         const uint8_t *payload, uint8_t len,
                         uint8_t *buf, v4p_bulk_t *out);

/* ------------------------------------------------------------------ */
/* Befehls-Wrapper -- §11                                              */
/* ------------------------------------------------------------------ */

uint8_t v4_ping       (v4_master_t *m, v4p_info_t *info);           /* 0x01 */
uint8_t v4_get_info   (v4_master_t *m, v4p_info_t *info);           /* 0x03 */
/* Prueft eine GET_INFO-Antwort gegen diese Implementierung: proto_ver,
 * write/read_frame_len und chunk. Ein Firmware-Stand mit anderen Rahmengroessen
 * (z.B. die alte 64-Byte-Fassung) wuerde sonst stillschweigend Muell liefern. */
uint8_t v4_check_info (const v4p_info_t *info);
uint8_t v4_get_status (v4_master_t *m, v4p_status_t *st);           /* 0x02 */

uint8_t v4_scan_start (v4_master_t *m, uint8_t inq_units, int continuous); /* 0x10 */
uint8_t v4_scan_stop  (v4_master_t *m);                             /* 0x11 */
uint8_t v4_dev_count  (v4_master_t *m, uint8_t *count);             /* 0x12 */
uint8_t v4_dev_get    (v4_master_t *m, uint8_t idx, v4p_dev_t *dev);/* 0x13 */

uint8_t v4_connect    (v4_master_t *m, uint8_t idx);                /* 0x20 */
uint8_t v4_connect_bda(v4_master_t *m, const uint8_t bda[6]);       /* 0x21 */
uint8_t v4_disconnect (v4_master_t *m);                             /* 0x22 */
uint8_t v4_forget     (v4_master_t *m);                             /* 0x23 */

uint8_t v4_sd_mount   (v4_master_t *m, int force);                  /* 0x30 */
/* Wie v4_sd_mount, aber BUSY wird NICHT intern wiederholt (§8.1). */
uint8_t v4_sd_mount_probe(v4_master_t *m, int force);               /* 0x30 */
uint8_t v4_sd_info    (v4_master_t *m, v4p_sdinfo_t *si);           /* 0x31 */
uint8_t v4_set_chunk  (v4_master_t *m, uint16_t chunk);             /* 0x32 */

uint8_t v4_path_clear (v4_master_t *m);                             /* 0x38 */
uint8_t v4_path_append(v4_master_t *m, const char *fragment);       /* 0x39 */

/* path == NULL bedeutet: den per PATH_* gebauten Pfad benutzen (len = 0). */
uint8_t v4_dir_open   (v4_master_t *m, const char *path, uint8_t *handle);   /* 0x40 */
uint8_t v4_dir_next   (v4_master_t *m, uint8_t handle, uint16_t index,
                       v4p_dirent_t *ent);                                   /* 0x41 */
uint8_t v4_dir_close  (v4_master_t *m, uint8_t handle);                      /* 0x42 */

uint8_t v4_file_open  (v4_master_t *m, const char *path, uint8_t *handle,
                       uint32_t *size, uint8_t *attr);                       /* 0x50 */
/* `buf` muss 12 + m->chunk Byte fassen. out->data zeigt in `buf`. */
uint8_t v4_file_read  (v4_master_t *m, uint8_t handle, uint16_t block,
                       uint8_t *buf, v4p_bulk_t *out);                       /* 0x51 */
uint8_t v4_file_close (v4_master_t *m, uint8_t handle);                      /* 0x52 */

uint8_t v4_reset      (v4_master_t *m);                             /* 0x7E */

/* path == NULL: den per PATH_* gebauten Pfad benutzen (L = 0). */
uint8_t v4_play_file  (v4_master_t *m, const char *path);           /* 0x60 */
uint8_t v4_stop_play  (v4_master_t *m);                             /* 0x61 */

/* ------------------------------------------------------------------ */
/* Ablaufhilfen -- §7.2, §8.1, §8.2, §13                               */
/* ------------------------------------------------------------------ */

/* §8.1: SD_MOUNT(force=1) -> BUSY abwarten, dann Zustand abfragen. */
uint8_t v4_sd_mount_wait(v4_master_t *m, unsigned tries);

/* §13: GET_STATUS pollen, bis state == want (oder Versuche erschoepft). */
uint8_t v4_wait_state(v4_master_t *m, uint8_t want, unsigned tries,
                      v4p_status_t *last);

/* §7.2: vollstaendige Verzeichnis-Iteration inkl. Index-Disziplin. */
typedef void (*v4_dirent_cb)(const v4p_dirent_t *ent, void *ctx);
uint8_t v4_list_dir(v4_master_t *m, const char *path,
                    v4_dirent_cb cb, void *ctx);

/* §8.2: Datei blockweise lesen, BUSY-Runden inklusive.
 * `scratch` darf NULL sein, dann wird passend zu m->chunk allokiert.
 * Der Callback sieht genau die gueltigen Bytes jedes Blocks. */
typedef void (*v4_file_cb)(uint16_t block, const uint8_t *data, uint16_t len,
                           void *ctx);
uint8_t v4_read_file(v4_master_t *m, const char *path,
                     uint8_t *scratch, size_t scratch_cap,
                     v4_file_cb cb, void *ctx, uint32_t *out_bytes);

/* ------------------------------------------------------------------ */
/* Diagnose                                                            */
/* ------------------------------------------------------------------ */

/* Text zu Protokoll-Status und V4_ERR_*. */
const char *v4_strerror(uint8_t code);

/* Text zu einem V4P_CHECK_*-Wert (fuer Fehlersuche im Transaktionskern). */
const char *v4_check_str(int rc);

#endif /* V4_MASTER_H */
