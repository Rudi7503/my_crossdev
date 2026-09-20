/*
 * v4_mock.h -- ESP32-Emulator fuer den Linux-Harness (§14.1).
 *
 * Der Mock ist die ZWEITE Implementierung der Plattformhaken aus v4_master.h:
 * er benutzt dieselben Aufbau- und Prueffunktionen aus v4_proto.c, damit kein
 * Format-Drift zwischen den beiden Seiten entstehen kann (§14.1).
 *
 * Nachgebildet werden die Verhaltensweisen, die das Protokoll tragen:
 *   - R1: genau eine Antwort pro Befehl; ohne Befehl liefert der Bus 0xFF
 *   - §5.1  Statuscodes inkl. BAD_CMD/BAD_ARG/BAD_STATE/NO_SD/NOT_FOUND/...
 *   - §7.2  DIR_NEXT mit Eintragscache und Indexdisziplin
 *   - §8.1  SD_MOUNT: force=1 -> BUSY, ohne force nur das letzte Ergebnis
 *   - §8.2  FILE_READ: erster Zugriff auf (handle,block) -> BUSY, dann Daten;
 *           hinter dem Dateiende -> BUSY, dann END
 *   - §9    SET_CHUNK verwirft den Blockcache
 *   - §10   Pfadregeln: ".." abgewiesen, 128-Byte-Pfadpuffer, TOO_LONG
 *   - §14.3 Handle-Grenzen: 2 Verzeichnisse, 4 Dateien
 * Dazu Stoerknoepfe fuer die Testmatrix aus §14.1.
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
    int bad_frame_len_every;    /* jeder n-te Read meldet len = 58         */
    int busy_count;             /* die naechsten n Antworten BUSY          */
    int busy_every;             /* jeder n-te Befehl BUSY                  */
    int never_ready;            /* immer BUSY                              */
    int force_status;           /* >= 0: einmalig erzwungener Status       */
    int bulk_wrong_handle;      /* einmalig falsches handle im Bulk-Frame  */
    int sd_present;             /* 1 = Karte vorhanden/Mount klappt        */
    int sd_card_present;        /* Sockelschalter (§11 GET_STATUS +16)     */
    int connect_rounds;         /* CONNECTING-Runden vor CONNECTED         */
    int play_rounds;            /* BUSY-Runden beim Umbau der Audio-Pipeline */
    int play_fail;              /* Umbau schlaegt fehl -> IO_ERR           */

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

/* Leerer Zustand: keine Geraete, leeres Dateisystem, Karte eingesteckt,
 * chunk = 128, alle Stoerknoepfe aus. */
void v4_mock_reset(void);

/* Standardszenario fuer den Sitzungsablauf (§13). */
void v4_mock_seed_default(void);

/* Dateisystem */
int v4_mock_add_dir (const char *path);
int v4_mock_add_file(const char *path, const uint8_t *data, uint32_t size);

/* Geraetetabelle */
int v4_mock_dev_add(const char *name, const uint8_t bda[6]);

/* Karte und Zustand */
void v4_mock_set_sd(int mounted, uint32_t total_kb, uint32_t free_kb);
void v4_mock_set_audio(int streaming);        /* Bit A2DP_STREAMING       */
void v4_mock_set_sd_playback(int playing);    /* Bit SD_PLAYBACK          */
/* Simuliert das regulare Ende einer SD-Wiedergabe: der ESP32 schaltet
 * selbsttaetig auf den I2S-Eingang zurueck (§11). */
void v4_mock_end_playback(void);
void v4_mock_bump_scan_gen(void);

/* Simuliert einen Bus-Reset: halbe Antwort verwerfen, FIFO leeren.
 * Danach muss der naechste Befehl wieder saubere Daten liefern (§14.3). */
void v4_mock_bus_reset(void);

#endif /* V4_MOCK_H */
