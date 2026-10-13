/*
 * v4_console.c -- interaktives Konsolenprogramm fuer den I2C-Master des
 * Bluetooth-Joystick-Adapters.
 *
 * Dasselbe Programm laeuft auf drei Wegen:
 *   Demo/Test : gelinkt mit v4_mock.c      -> keine Hardware noetig
 *   Linux     : gelinkt mit v4_linux_i2c.c -> argv[1] bzw. -d /dev/i2c-N
 *   Vampire V4: gelinkt mit v4_amiga_i2c.c -> -d wird ignoriert
 *
 * I2C DIREKT IM HAUPTFADEN: i2c.library hat keinen Timeout -- SendI2C/
 * ReceiveI2C blockieren, bis die Transaktion durch ist. Genau so ist diese
 * Konsole auf der V4 gelaufen, und genau so laeuft sie wieder: JEDER Buszugriff
 * steht in dieser Datei und wird VON HIER aus aufgerufen. Kein CreateNewProc,
 * kein zweiter Faden, kein Nachrichtenport, kein Forbid().
 *
 * Der Preis dafuer ist bekannt und wird nicht beschoenigt: haengt der Bus,
 * steht die Konsole. Das ist die Konstruktion, die am Geraet funktioniert hat;
 * die Fassung mit eigener Aufgabe kam auf der V4 nicht einmal durch den Start.
 * Wer spaeter sperren zu muessen glaubt, nimmt eine SignalSemaphore (der
 * Wartende schlaeft, das System laeuft weiter) -- NIEMALS Forbid().
 *
 * Alles, was die Gegenstelle zuletzt gueltig gemeldet hat, liegt in
 * v4_state_t; der DebugUART bekommt jede Meldung ueber v4_debug.c.
 *
 * Die Oberflaeche ist bewusst eine Textkonsole: sie ist der Notfallweg, wenn
 * MUI (v4_gui.c) nicht startet, und sie ist das Programm, das die Testmatrix
 * ohne Hardware durchlaeuft.
 *
 * Was die Anzeige zeigt:
 *   Zug    = was die Firmware nach GND zieht (GET_INPUT, Bitmaske je Buchse).
 *            Bit gesetzt = Leitung aktiv (Taste/Richtung).
 *   Pegel  = was TATSAECHLICH an der Leitung anliegt (GET_INPUT ab der
 *            12-Byte-Fassung; Bit gesetzt = High, Ruhe 0x7F).
 *
 * Beides sind getrennte Felder und koennen auseinandergehen. Genau dann wird
 * gewarnt: "losgelassen, liest aber 0" heisst fehlender Pull-up,
 * "gezogen, liest aber High" kann elektrisch nicht sein. Antwortet die
 * Gegenstelle noch mit der 10-Byte-Fassung, sind die Pegel UNBEKANNT -- dann
 * steht dort ein Hinweis und keine Zahl (auch keine abgeleitete).
 */

#include "v4_master.h"
#include "v4_state.h"
#include "v4_debug.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CONSOLE_VERSION "0.3"
#define CONSOLE_DATUM   "13.10.2026"

/* ------------------------------------------------------------------ */
/* Schalter und Logdatei                                               */
/* ------------------------------------------------------------------ */

static int         g_use_log  = 1;      /* 0 mit -n */
static int         g_settle   = 0;      /* -t <ticks> */
static int         g_wait_us  = 0;      /* -w <us>, 0 = Vorgabe (R3) */
static int         g_try_max  = 0;      /* -r <zahl>, 0 = Vorgabe */
static const char *g_log_path = NULL;   /* -o <pfad> */
static int         g_trace    = 0;      /* -a */
static int         g_hex      = 0;      /* -x */
static int         g_quiet    = 0;      /* -quiet */

/*
 * Der Master und der zuletzt gueltige Zustand. Beides ortsfest (`static`): der
 * Zustandsblock ist ein paar Kilobyte gross und gehoert auf dem Amiga nicht auf
 * den Stapel (ein CLI-Prozess hat dort rund 4 KB).
 */
static v4_state_t  S;
static v4_master_t M;

/*
 * Alle Meldungen der Konsole laufen hier durch. Sie gehen ueber v4_dbg_zeile
 * an dieselben drei Senken wie die Debugausgabe (stdout, Debug-UART, Logdatei)
 * -- so steht in Mitschnitt und Logdatei wirklich derselbe Text.
 */
static void v4_msg(const char *fmt, ...)
{
    /* Absichtlich statisch: 512 Byte weniger Stack pro Aufruf. Das Programm ist
     * einprozessig und v4_msg wird nie verschachtelt gerufen. */
    static char buf[512];
    va_list ap;
    int     n;

    va_start(ap, fmt);
    n = vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    if (n < 0) {
        return;
    }
    if ((size_t)n > sizeof(buf) - 1u) {
        n = (int)(sizeof(buf) - 1u);    /* gekuerzt, aber terminiert */
    }
    v4_dbg_zeile(buf);
    (void)n;
}

/* ------------------------------------------------------------------ */
/* Trace                                                               */
/* ------------------------------------------------------------------ */

/* Der Trace laeuft ueber v4_debug.c -- dort entsteht genau EINE Zeile je
 * Transaktion (Vorgabe an, mit -quiet aus). Hier bleibt nur der Klartext zum
 * Befehl, den die Konsole an ein paar Stellen selbst braucht. */

/* ------------------------------------------------------------------ */
/* Fehlerausgabe                                                       */
/*                                                                     */
/* Der Master liegt jetzt in DIESEM Faden -- die Diagnose kommt direkt  */
/* aus ihm und aus der Plattform, nicht mehr aus einem Zustandsblock.   */
/* ------------------------------------------------------------------ */

static void print_failure(const char *what, uint8_t rc)
{
    v4_msg("%s: %s (0x%02X)\n", what, v4_strerror(rc), (unsigned)rc);
    if (rc == V4_ERR_LINK) {
        v4_msg("  Versuche=%lu, Wiederholungen=%lu, letzte Rahmenpruefung: %s\n",
               (unsigned long)M.rx_frames, (unsigned long)M.retries,
               v4_check_str(M.last_check));
        v4_msg("  Busfehler: Code 0x%08lX (%s)\n",
               v4_plat_last_error(), v4_plat_error_text());
    }
}

/* ------------------------------------------------------------------ */
/* Anzeigen                                                            */
/* ------------------------------------------------------------------ */

static void caps_text(uint16_t caps, char *out, size_t cap)
{
    /* Bewusst als Text und nicht als Zahl: 0x00FF sagt niemandem etwas,
     * "Port-Modus,Eingaenge,HID,..." schon. */
    static const struct {
        uint16_t    bit;
        const char *name;
    } tab[] = {
        { V4P_CAP_PORT_MODE, "Port-Modus" },
        { V4P_CAP_GET_INPUT, "Eingaenge" },
        { V4P_CAP_GET_HID,   "HID-Info" },
        { V4P_CAP_HID_BT,    "BT/BR-EDR" },
        { V4P_CAP_HID_BLE,   "BLE" },
        { V4P_CAP_MOUSE,     "Maus" },
        { V4P_CAP_PAD,       "Pad" },
        { V4P_CAP_BATTERY,   "Batterie" }
    };
    size_t   n     = 0u;
    int      first = 1;
    unsigned i;

    if (out == NULL || cap == 0u) {
        return;
    }
    out[0] = '\0';
    for (i = 0u; i < (unsigned)(sizeof(tab) / sizeof(tab[0])); i++) {
        const char *name;
        size_t      len;
        size_t      k;

        if ((caps & tab[i].bit) == 0u) {
            continue;
        }
        name = tab[i].name;
        len  = strlen(name);
        if (n + (first ? 0u : 1u) + len + 1u > cap) {
            break;
        }
        if (!first) {
            out[n++] = ',';
        }
        for (k = 0u; k < len; k++) {
            out[n++] = name[k];
        }
        first = 0;
    }
    if (first) {
        const char *keine = "(keine gemeldet)";

        for (n = 0u; keine[n] != '\0' && n + 1u < cap; n++) {
            out[n] = keine[n];
        }
    }
    out[n] = '\0';
}

static void print_info(const v4p_info_t *info)
{
    char caps[160];

    caps_text(info->caps, caps, sizeof(caps));
    v4_msg("PING ok: proto=%u fw=%u write=%u read=%u chunk=%u\n",
           (unsigned)info->proto_ver, (unsigned)info->fw_ver,
           (unsigned)info->write_frame_len, (unsigned)info->read_frame_len,
           (unsigned)info->chunk);
    v4_msg("  Faehigkeiten: 0x%04X (%s)\n", (unsigned)info->caps, caps);
    v4_msg("  Bulk-Weg: %s (bulk_payload_max=%u), Geraete max. %u\n",
           (info->bulk_payload_max == 0u) ? "nein (kein Dateizugriff)"
                                          : "ja",
           (unsigned)info->bulk_payload_max, (unsigned)info->max_devices);
}

static void print_status(const v4p_status_t *st)
{
    v4_msg("Zustand %u (%s), Verbindung %s, Scan %s, Geraete %u\n",
           (unsigned)st->state, v4p_bt_state_name(st->state),
           (st->conn_index == V4P_NO_INDEX)
               ? "keine"
               : "aktiv",
           st->scan_active ? "laeuft" : "aus",
           (unsigned)st->dev_count);
    /* Die Reserviertfelder muessen 0 sein. Sind sie es nicht, antwortet eine
     * Firmware aus dem Audio-Projekt -- das gehoert gemeldet und nicht
     * ueberspielt. */
    if (st->sd_mounted != 0u || st->audio_flags != 0u
        || st->sd_card_present != 0u || st->sd_free_kb != 0u) {
        v4_msg("  WARNUNG: Antwort enthaelt SD-/Audiofelder "
               "(mounted=%u audio=0x%02X card=%u frei=%lu KB).\n"
               "  Das ist die Firmware des Audio-Projekts, nicht der "
               "Joystick-Adapter.\n",
               (unsigned)st->sd_mounted, (unsigned)st->audio_flags,
               (unsigned)st->sd_card_present, (unsigned long)st->sd_free_kb);
    }
}

static void print_hid(const v4p_hid_t *hid)
{
    if (hid->kind == V4P_HID_KIND_NONE) {
        v4_msg("Controller: keiner verbunden\n");
        return;
    }
    v4_msg("Controller: %s\n", (hid->name[0] != '\0') ? hid->name
                                                      : "(ohne Namen)");
    v4_msg("  Art %s, Transport %s, VID:PID %04X:%04X, Reports %u\n",
           v4p_hid_kind_name(hid->kind), v4p_transport_name(hid->transport),
           (unsigned)hid->vid, (unsigned)hid->pid, (unsigned)hid->reports);
    v4_msg("  Report-ID %u, Reportlaenge %u Byte, Batterie %s\n",
           (unsigned)hid->report_id, (unsigned)hid->report_len,
           (hid->battery == V4P_HID_BATTERY_UNKNOWN)
               ? "unbekannt"
               : "gemeldet");
    if (hid->battery != V4P_HID_BATTERY_UNKNOWN) {
        v4_msg("  Batteriestand %u %%\n", (unsigned)hid->battery);
    }
}

/*
 * Die Portgruppe: Betriebsart und die sieben Leitungen mit BEIDEN Angaben.
 *
 * "Zug" ist die Zug-Maske (was die Firmware nach GND zieht), "Pegel" der
 * GEMESSENE Pegel aus der 12-Byte-Fassung von GET_INPUT. Liefert die Firmware
 * die alte 10-Byte-Fassung, sind die Pegel unbekannt -- dann steht hier ein
 * Hinweis und KEINE Zahl. Eine abgeleitete Zahl waere in dieser Spalte nicht
 * von einer Messung zu unterscheiden, und genau das darf nicht passieren.
 *
 * Der Widerspruch "losgelassen, liest aber 0" wird je Leitung markiert: das
 * ist der fehlende Pull-up und damit die elektrische Kernaussage.
 */
static void print_port_group(int port, uint8_t mode, uint8_t drive,
                             uint8_t level, int level_valid)
{
    unsigned bit;

    if (level_valid) {
        v4_msg("\nPort %d (%s)  Zug=0x%02X  Pegel=0x%02X\n",
               port, v4p_port_mode_name(mode), (unsigned)drive,
               (unsigned)level);
    } else {
        v4_msg("\nPort %d (%s)  Zug=0x%02X  Pegel: Firmware ohne Pegel "
               "-- neu flashen\n",
               port, v4p_port_mode_name(mode), (unsigned)drive);
    }
    v4_msg("  %-7s %-7s %-9s %-12s %s\n", "Leitung", "DB9", "Zug", "Pegel",
           "Bewertung");
    for (bit = 0u; bit < V4P_IN_ST_COUNT; bit++) {
        uint8_t maske = (uint8_t)(1u << bit);
        int     z     = (drive & maske) != 0u;
        int     p     = (level & maske) != 0u;
        const char *urteil;

        if (!level_valid) {
            urteil = "(Pegel unbekannt)";
        } else if (z && !p) {
            urteil = "ok (gezogen -> Low)";
        } else if (!z && p) {
            urteil = "ok (frei -> High)";
        } else if (!z && !p) {
            urteil = "WIDERSPRUCH: losgelassen, liest 0 -> kein Pull-up";
        } else {
            urteil = "WIDERSPRUCH: gezogen, liest High -> elektrisch unmoeglich";
        }

        v4_msg("  %-7s %-7s %-9s %-12s %s%s\n",
               v4p_bit_name(bit), v4p_bit_pin(bit),
               z ? "GND" : "frei",
               level_valid ? (p ? "1 (High)" : "0 (Low)") : "-",
               z ? "[aktiv] " : "",
               urteil);
    }
}

/* Die elektrischen Befunde beider Buchsen in einem Satz -- im Stil des
 * Audio-Projekts ("WARNUNG: ..."). */
static void print_level_diagnosis(const v4p_input_t *in)
{
    int port;
    int gemeldet = 0;

    if (!in->level_valid) {
        return;             /* ohne Pegel gibt es nichts zu deuten */
    }

    for (port = 0; port < (int)V4P_PORT_COUNT; port++) {
        uint8_t fehlt   = v4p_level_missing_pullup(in->state[port],
                                                  in->level[port]);
        uint8_t unmoegl = v4p_level_impossible(in->state[port],
                                               in->level[port]);

        if (unmoegl != 0u) {
            char txt[64];

            v4p_bits_text(unmoegl, txt, sizeof(txt));
            v4_msg("WARNUNG: Port %d zieht %s nach GND, liest dort aber High. "
                   "Das kann elektrisch\n"
                   "         nicht sein -- Ausgangsstufe oder Pinbelegung "
                   "pruefen.\n", port, txt);
            gemeldet = 1;
        }
        if (fehlt != 0u) {
            char txt[64];

            v4p_bits_text(fehlt, txt, sizeof(txt));
            v4_msg("WARNUNG: Port %d hat %s losgelassen, liest dort aber 0. "
                   "Es zieht niemand hoch:\n"
                   "         der Pull-up der Vampire fehlt oder die Platine "
                   "haengt nicht an dieser Buchse.\n", port, txt);
            gemeldet = 1;
        }
    }

    if (!gemeldet) {
        v4_msg("Leitungspruefung: Pull-up wirkt, gezogene Leitungen lesen Low, "
               "losgelassene High.\n");
    }
}

/* Beide Portgruppen in einem Aufwasch: Modi und Eingaenge getrennt abfragen.
 * Beide Aufrufe stehen hier im Hauptfaden -- kein Auftrag, kein Warten. */
static uint8_t print_ports(void)
{
    uint8_t     mode[V4P_PORT_COUNT];
    v4p_input_t in;
    uint8_t     rc;

    rc = v4_get_port_mode(&M, mode);
    (void)v4_state_note(&S, rc);
    if (rc != V4P_ST_OK) {
        print_failure("GET_PORT_MODE", rc);
        return rc;
    }
    v4_state_put_modes(&S, mode);
    v4_dbg_modes(mode, "GET_PORT_MODE");

    rc = v4_get_input(&M, &in);
    (void)v4_state_note(&S, rc);
    if (rc != V4P_ST_OK) {
        print_failure("GET_INPUT", rc);
        return rc;
    }
    v4_state_put_input(&S, &in);
    v4_dbg_input(&in);

    print_port_group(0, mode[0], in.state[0], in.level[0], in.level_valid);
    print_port_group(1, mode[1], in.state[1], in.level[1], in.level_valid);
    v4_msg("\nQuadraturzaehler: X=%lu Y=%lu Schritte (seit dem Start des "
           "ESP32)\n",
           (unsigned long)in.steps_x, (unsigned long)in.steps_y);
    if (!in.level_valid) {
        v4_msg("Hinweis: die Gegenstelle antwortet mit der 10-Byte-Fassung von "
               "GET_INPUT (ohne Pegel).\n"
               "  Die Pegelspalte bleibt deshalb leer. Firmware des ESP32 auf "
               "den Stand mit Pegeln flashen.\n");
    }
    print_level_diagnosis(&in);
    return V4P_ST_OK;
}

/* ------------------------------------------------------------------ */
/* Aktionen                                                            */
/*                                                                     */
/* Jede Aktion ruft v4_master DIREKT. Kein Auftrag, kein Nachrichtenport. */
/* ------------------------------------------------------------------ */

static uint8_t act_status(void)
{
    v4p_status_t st;
    uint8_t      rc = v4_get_status(&M, &st);

    (void)v4_state_note(&S, rc);
    if (rc != V4P_ST_OK) {
        print_failure("GET_STATUS", rc);
        return rc;
    }
    v4_state_put_status(&S, &st);
    v4_dbg_status(&st);
    v4_dbg_sitzung(&st);
    print_status(&st);
    return V4P_ST_OK;
}

static uint8_t act_hid(void)
{
    v4p_hid_t hid;
    uint8_t   rc = v4_get_hid(&M, &hid);

    (void)v4_state_note(&S, rc);
    if (rc != V4P_ST_OK) {
        print_failure("GET_HID", rc);
        return rc;
    }
    v4_state_put_hid(&S, &hid);
    v4_dbg_hid(&hid);
    print_hid(&hid);
    return V4P_ST_OK;
}

static uint8_t act_scan(uint8_t *dev_count)
{
    static v4p_dev_t tab[V4P_MAX_DEVICES];   /* ortsfest (4-KB-Stapel) */
    uint8_t      count = 0u;
    uint8_t      n     = 0u;
    uint8_t      rc;
    int          i;
    int          warten;

    *dev_count = 0u;
    v4_msg("-- Geraeteliste --\n");
    rc = v4_scan_start(&M, 8u, 1);
    (void)v4_state_note(&S, rc);
    if (rc != V4P_ST_OK) {
        print_failure("SCAN_START", rc);
        return rc;
    }

    /*
     * Der Scan laeuft auf dem ESP32 asynchron: unmittelbar nach SCAN_START ist
     * die Liste oft noch leer. Deshalb warten -- aber begrenzt, damit ein
     * fehlender Adapter das Programm nicht fuer immer blockiert.
     */
    v4_msg("Suche Bluetooth-Geraete ");
    for (warten = 0; warten < 40; warten++) {   /* hoechstens rund 20 s */
        rc = v4_dev_count(&M, &count);
        (void)v4_state_note(&S, rc);
        if (rc != V4P_ST_OK) {
            v4_msg("\n");
            print_failure("DEV_COUNT", rc);
            return rc;
        }
        if (count > 0u) {
            break;
        }
        v4_plat_delay_us(500000u);      /* 0,5 s zwischen zwei Runden */
        v4_msg(".");
    }
    v4_msg("\n");

    if (count > (uint8_t)V4P_MAX_DEVICES) {
        v4_msg("[scan] Slave meldet %u Geraete (Obergrenze dieser Anzeige %u) "
               "- lese nur %u\n", (unsigned)count,
               (unsigned)V4P_MAX_DEVICES, (unsigned)V4P_MAX_DEVICES);
        count = (uint8_t)V4P_MAX_DEVICES;
    }

    *dev_count = count;
    v4_msg("Geraete: %u\n", (unsigned)count);
    for (i = 0; i < (int)count; i++) {
        v4p_dev_t d;

        rc = v4_dev_get(&M, (uint8_t)i, &d);
        (void)v4_state_note(&S, rc);
        if (rc != V4P_ST_OK) {
            v4_msg("  [%2d] %s\n", i, v4_strerror(rc));
            continue;
        }
        if (d.name_len == 0u || d.name[0] == '\0') {
            v4_msg("  [%2d] (ohne Namen) - uebersprungen\n", i);
            continue;
        }
        v4_msg("  [%2d] %-32s %02X:%02X:%02X:%02X:%02X:%02X%s\n", i,
               d.name, (unsigned)d.bda[0], (unsigned)d.bda[1],
               (unsigned)d.bda[2], (unsigned)d.bda[3],
               (unsigned)d.bda[4], (unsigned)d.bda[5],
               ((d.flags & V4P_FLAG_NAME_TRUNCATED) != 0u)
                   ? "  (Name gekuerzt)" : "");
        if (n < (uint8_t)V4P_MAX_DEVICES) {
            tab[n++] = d;
        }
    }
    v4_state_put_devs(&S, tab, n);
    v4_dbg_devs(tab, n);
    return V4P_ST_OK;
}

static uint8_t act_connect(uint8_t idx)
{
    v4p_status_t st;
    uint8_t      rc;
    unsigned     runden;

    memset(&st, 0, sizeof(st));
    rc = v4_connect(&M, idx);
    (void)v4_state_note(&S, rc);
    if (rc != V4P_ST_OK) {
        /* Der Befehl kann den Slave trotzdem erreicht haben -- die Antwort kann
         * unterwegs verlorengegangen sein. Deshalb den Zustand fragen, statt
         * sofort abzubrechen. */
        print_failure("CONNECT", rc);
    }

    /* CONNECT ist nur angenommen. Die Verbindung steht erst danach: der ESP32
     * meldet so lange CONNECTING. Warten ist Pflicht, sonst zeigt die Anzeige
     * "keine Verbindung", obwohl gerade eine entsteht. */
    v4_msg("Warte auf die Verbindung ");
    for (runden = 0u; runden < 20u; runden++) {
        rc = v4_get_status(&M, &st);
        (void)v4_state_note(&S, rc);
        if (rc != V4P_ST_OK) {
            v4_msg("\n");
            print_failure("GET_STATUS", rc);
            return rc;
        }
        v4_state_put_status(&S, &st);
        v4_dbg_sitzung(&st);
        if (st.state == V4P_STATE_CONNECTED
            || st.state == V4P_STATE_SUSPENDED) {
            break;
        }
        v4_plat_delay_us(250000u);      /* 0,25 s zwischen zwei Runden */
        v4_msg(".");
    }
    v4_msg("\n");

    if (st.state == V4P_STATE_CONNECTED) {
        v4_msg("Verbunden mit Index %u (%s)\n", (unsigned)st.conn_index,
               v4p_bt_state_name(st.state));
        if (st.conn_index != idx) {
            v4_msg("  HINWEIS: der Slave meldet Index %u, angesprochen war %u\n",
                   (unsigned)st.conn_index, (unsigned)idx);
        }
    } else {
        v4_msg("Nach %.1f s noch in %s.\n", (double)runden * 0.25,
               v4p_bt_state_name(st.state));
        v4_msg("  Der Controller ist evtl. aus oder ausser Reichweite.\n");
    }
    return V4P_ST_OK;
}

static uint8_t act_cycle_mode(int port)
{
    uint8_t mode[V4P_PORT_COUNT];
    uint8_t neu;
    uint8_t applied = 0u;
    uint8_t rc;

    rc = v4_get_port_mode(&M, mode);
    (void)v4_state_note(&S, rc);
    if (rc != V4P_ST_OK) {
        print_failure("GET_PORT_MODE", rc);
        return rc;
    }
    v4_state_put_modes(&S, mode);
    neu = v4p_port_mode_next(mode[port]);
    rc = v4_set_port_mode(&M, (uint8_t)port, neu, &applied);
    (void)v4_state_note(&S, rc);
    if (rc != V4P_ST_OK) {
        print_failure("SET_PORT_MODE", rc);
        return rc;
    }
    v4_state_put_mode(&S, (uint8_t)port, applied);
    v4_dbg_modes(S.mode, "SET_PORT_MODE");
    v4_msg("Port %d: %s -> %s (bestaetigt: %s)\n", port,
           v4p_port_mode_name(mode[port]), v4p_port_mode_name(neu),
           v4p_port_mode_name(applied));
    return V4P_ST_OK;
}

static uint8_t act_swap(void)
{
    uint8_t now[V4P_PORT_COUNT];
    uint8_t rc = v4_swap_port_modes(&M, now);

    (void)v4_state_note(&S, rc);
    if (rc != V4P_ST_OK) {
        print_failure("Buchsen tauschen", rc);
        return rc;
    }
    v4_state_put_modes(&S, now);
    v4_dbg_modes(now, "Buchsen getauscht");
    v4_msg("Buchsen getauscht: Port 0 = %s, Port 1 = %s\n",
           v4p_port_mode_name(now[0]), v4p_port_mode_name(now[1]));
    return V4P_ST_OK;
}

/* ------------------------------------------------------------------ */
/* Schalter                                                            */
/* ------------------------------------------------------------------ */

static void usage(void)
{
    v4_msg("Aufruf: v4_console [schalter] [geraet]\n");
    v4_msg("  geraet       I2C-Geraet (Linux, z.B. /dev/i2c-1); auf der V4 "
           "wirkungslos\n");
    v4_msg("  -d <geraet>  dasselbe als Schalter\n");
    v4_msg("  -o <pfad>    Logdatei (ohne Angabe: Vorgabe der Plattform)\n");
    v4_msg("  -n           keine Logdatei\n");
    v4_msg("  -t <ticks>   Wartezeit je Logzeile in 1/50 s (V4)\n");
    v4_msg("  -w <us>      Wartezeit vor jedem Lesen (R3), Vorgabe 2000\n");
    v4_msg("  -r <zahl>    Versuche je Transaktion, Vorgabe 4\n");
    v4_msg("  -a           Trace an (jeden Transaktionsschritt zeigen)\n");
    v4_msg("  -x           mit -a zusaetzlich die Rahmenbytes\n");
    v4_msg("  -quiet       keine Zeile je I2C-Transaktion (Vorgabe: an)\n");
    v4_msg("Die Ausgabe geht immer zugleich auf stdout, auf den Debug-UART der\n"
           "V4 (115200 Baud) und in die Logdatei.\n");
}

/* Liefert 0 = weiter, 1 = Ende, -1 = Schalterfehler. */
static int parse_args(int argc, char **argv, const char **dev_out)
{
    int i;

    *dev_out = NULL;
    for (i = 1; i < argc; i++) {
        const char *a = argv[i];

        if (a[0] != '-') {
            *dev_out = a;               /* Geraet ohne Schalter */
            continue;
        }
        if (strcmp(a, "-n") == 0) {
            g_use_log = 0;
        } else if (strcmp(a, "-quiet") == 0 || strcmp(a, "-q") == 0) {
            g_quiet = 1;
        } else if (strcmp(a, "-a") == 0) {
            g_trace = 1;
        } else if (strcmp(a, "-x") == 0) {
            g_hex   = 1;
            g_trace = 1;                /* ein Hexdump ohne Trace waere sinnlos */
        } else if (strcmp(a, "-d") == 0 && i + 1 < argc) {
            *dev_out = argv[++i];
        } else if (strcmp(a, "-o") == 0) {
            if (i + 1 >= argc) {
                v4_msg("FEHLER: -o braucht einen Pfad\n");
                return -1;
            }
            g_log_path = argv[++i];
        } else if (strcmp(a, "-t") == 0 && i + 1 < argc) {
            g_settle = atoi(argv[++i]);
        } else if (strcmp(a, "-w") == 0 && i + 1 < argc) {
            g_wait_us = atoi(argv[++i]);
        } else if (strcmp(a, "-r") == 0 && i + 1 < argc) {
            g_try_max = atoi(argv[++i]);
        } else {
            v4_msg("Unbekannter Schalter: %s\n", a);
            usage();
            return -1;
        }
    }
    return 0;
}

/* ------------------------------------------------------------------ */
/* Hauptablauf                                                         */
/* ------------------------------------------------------------------ */

/*
 * Lebenszeichen. Die Konsole wartet auf Eingabe; einen eigenen Takt gibt es
 * hier nicht (und soll es nicht geben -- sie fragt nur auf Befehl ab). Damit
 * trotzdem mindestens einmal je Sekunde eine Herzschlagzeile entsteht, wird
 * hier geprueft, ob seit der letzten eine Sekunde vergangen ist.
 */
static unsigned long s_herz_us = 0ul;

static void console_herz_maybe(void)
{
    uint32_t jetzt = v4_plat_now_us();

    if (jetzt == 0u) {
        return;                     /* Plattform ohne Uhr */
    }
    if (s_herz_us == 0ul) {
        s_herz_us = (unsigned long)jetzt;
        return;
    }
    if ((uint32_t)(jetzt - (uint32_t)s_herz_us) >= 1000000u) {
        s_herz_us = (unsigned long)jetzt;
        v4_dbg_herz(&M);
    }
}

static int console_run(const char *dev)
{
    v4p_info_t  info;
    uint8_t     dev_count = 0u;
    char        line[160];
    int         beenden = 0;
    uint8_t     rc;

    v4_msg("V4-I2C-Master fuer den Bluetooth-Joystick-Adapter (Proto v%u, "
           "Adresse 0x%02X)\n", (unsigned)V4P_PROTO_VER,
           (unsigned)V4_I2C_ADDR7);
    v4_msg("=========================================================\n");

    if (v4p_selftest_byteorder() != 1) {
        v4_msg("Byte-Order-Selbsttest fehlgeschlagen -- Abbruch.\n");
        return 10;
    }

    v4_msg("Trace %s, t_wait=%lu us, Versuche=%u\n",
           g_trace ? "an" : "aus (mit -a einschaltbar)",
           (unsigned long)((g_wait_us > 0) ? g_wait_us : (int)V4_T_WAIT_US),
           (g_try_max > 0) ? (unsigned)g_try_max : (unsigned)V4_RETRIES);

    /*
     * Ab hier laeuft alles in DIESEM Faden: v4_init, v4_plat_open und danach
     * der erste PING. Genau die Konstruktion, die auf der V4 gelaufen ist --
     * kein CreateNewProc, kein zweiter Faden, kein Nachrichtenport.
     */
    v4_state_init(&S);
    v4_init(&M);                    /* nullt den Master: erst danach setzen */
    if (g_wait_us > 0) {
        M.t_wait_us = (uint32_t)g_wait_us;
    }
    if (g_try_max > 0) {
        M.try_max = (unsigned)g_try_max;
    }
    M.trace     = v4_dbg_trace;     /* immer: daraus entsteht die Zeile je
                                     * Transaktion (Vorgabe an, -quiet aus) */
    M.trace_ctx = NULL;

    v4_msg("Oeffne I2C-Bus%s%s ...\n", dev ? ": " : " (Standard)",
           dev ? dev : "");
    {
        int orc = v4_plat_open(dev);

        v4_msg("[plattform] v4_plat_open -> %d (%s)\n", orc,
               (orc == 0) ? "OK" : v4_plat_error_text());
        if (orc != 0) {
            v4_msg("I2C-Bus nicht verfuegbar -- Abbruch.\n");
            return 10;
        }
    }
    v4_msg("[plattform] i2c.library und timer.device sind offen\n");

    /* Erster PING: er fuellt die Kopfdaten der Gegenstelle. */
    v4_msg("[ping] erste Abfrage der Gegenstelle ...\n");
    rc = v4_ping(&M, &info);
    (void)v4_state_note(&S, rc);
    if (rc != V4P_ST_OK) {
        print_failure("PING", rc);
        v4_msg("Erwartet wird ein 128-Byte-READ-Rahmen (proto_ver %u).\n",
               (unsigned)V4P_PROTO_VER);
        v4_close();
        return 10;
    }
    v4_state_put_info(&S, &info, 1);
    v4_dbg_info(&info);
    if (v4_check_info(&info) != V4P_ST_OK) {
        /* Wichtig seit der 128-Byte-Fassung: eine aeltere Firmware meldet
         * read_frame_len = 64 -- dann waere jede Antwort verschoben. */
        v4_msg("Protokoll passt nicht (%s): Slave meldet proto=%u write=%u "
               "read=%u; erwartet proto=%u write=%u read=%u\n",
               v4_strerror(v4_check_info(&info)), (unsigned)info.proto_ver,
               (unsigned)info.write_frame_len, (unsigned)info.read_frame_len,
               (unsigned)V4P_PROTO_VER, (unsigned)V4P_WRITE_FRAME_LEN,
               (unsigned)V4P_READ_FRAME_LEN);
        v4_close();
        return 10;
    }
    print_info(&info);

    v4_msg("\n-- Anfangszustand --\n");
    (void)act_status();
    (void)act_hid();
    (void)print_ports();

    /*
     * Hauptmenue. Jede Zeile ist ein Befehl; das Programm beendet sich auch bei
     * geschlossener Eingabe (Pipe-Ende) -- sonst bliebe ein Vorfuehrlauf ohne
     * 'q' fuer immer stehen.
     */
    v4_msg("\n-- Menue --  s Zustand, h HID, i Eingaenge, p Portmodus, "
           "t tauschen,\n"
           "             a Scan/Liste, c verbinden, d trennen, f vergessen, "
           "r RESET, q Ende\n");

    while (beenden == 0) {
        console_herz_maybe();
        v4_msg("Befehl> ");
        if (fgets(line, sizeof(line), stdin) == NULL) {
            v4_msg("\n(Eingabe zu Ende -- Programm beendet)\n");
            beenden = 1;
            continue;
        }
        switch (line[0]) {
        case '\0':
        case '\n':
            break;                          /* leere Zeile: nichts tun */
        case 'q':
        case 'Q':
            beenden = 1;
            break;
        case 's':
        case 'S':
            (void)act_status();
            break;
        case 'h':
        case 'H':
            (void)act_hid();
            break;
        case 'i':
        case 'I':
            (void)print_ports();
            break;
        case 'p':
        case 'P': {
            int port = -1;

            v4_msg("Port (0/1): ");
            if (fgets(line, sizeof(line), stdin) == NULL) {
                beenden = 1;
                break;
            }
            port = atoi(line);
            if (port < 0 || port >= (int)V4P_PORT_COUNT) {
                v4_msg("Es gibt nur Port 0 und Port 1.\n");
                break;
            }
            (void)act_cycle_mode(port);
            break;
        }
        case 't':
        case 'T':
            (void)act_swap();
            break;
        case 'a':
        case 'A':
            (void)act_scan(&dev_count);
            break;
        case 'c':
        case 'C': {
            int idx;

            if (dev_count == 0u) {
                v4_msg("Keine Geraeteliste -- erst 'a' (Scan) ausfuehren.\n");
                break;
            }
            v4_msg("Index (0-%u): ", (unsigned)(dev_count - 1u));
            if (fgets(line, sizeof(line), stdin) == NULL) {
                beenden = 1;
                break;
            }
            idx = atoi(line);
            if (idx < 0 || idx >= (int)dev_count) {
                v4_msg("Index %d liegt nicht zwischen 0 und %u.\n", idx,
                       (unsigned)(dev_count - 1u));
                break;
            }
            (void)act_connect((uint8_t)idx);
            break;
        }
        case 'd':
        case 'D': {
            uint8_t rc2 = v4_disconnect(&M);

            (void)v4_state_note(&S, rc2);
            v4_msg("DISCONNECT: %s\n", v4_strerror(rc2));
            break;
        }
        case 'f':
        case 'F': {
            uint8_t rc2 = v4_forget(&M);

            (void)v4_state_note(&S, rc2);
            v4_msg("FORGET: %s\n", v4_strerror(rc2));
            break;
        }
        case 'r':
        case 'R': {
            uint8_t rc2 = v4_reset(&M);

            (void)v4_state_note(&S, rc2);
            v4_msg("RESET: %s (beide Buchsen aus, Verbindung bleibt)\n",
                   v4_strerror(rc2));
            break;
        }
        default:
            v4_msg("Unbekannt: '%c' (s h i p t a c d f r q)\n", line[0]);
            break;
        }
    }

    v4_msg("Ende.\n");
    v4_dbg_ende(&M);
    v4_msg("[plattform] schliesse i2c.library und timer.device\n");
    v4_close();
    return 0;
}

/*
 * Einstieg. Der eigentliche Ablauf steckt in v4_console_main(), damit ein
 * Vorfuehr-/Testrahmen (tests/demo_main.c) den Mock vorher bestuecken kann --
 * dafuer wird diese Datei mit -DV4_CONSOLE_NO_MAIN uebersetzt.
 */
int v4_console_main(int argc, char **argv)
{
    const char *dev = NULL;
    int         r;

    /*
     * Vorschaltung: Ausgabe auf stdout, BEVOR die Schalter ausgewertet werden.
     * Sonst verschluckt ein unbekannter Schalter seine eigene Fehlermeldung --
     * die Ausgabe wird ja erst danach aufgesetzt. Hier ist der Debug-UART noch
     * aus und -quiet noch an; das endgueltige Aufsetzen folgt unten.
     */
    {
        v4_dbg_cfg_t cfg;

        memset(&cfg, 0, sizeof(cfg));
        cfg.programm  = "v4_console";
        cfg.version   = CONSOLE_VERSION;
        cfg.datum     = CONSOLE_DATUM;
        cfg.addr7     = (unsigned)V4_I2C_ADDR7;
        cfg.stdout_an = 1;
        cfg.quiet     = 1;
        v4_dbg_init(&cfg);
    }

    r = parse_args(argc, argv, &dev);
    if (r != 0) {
        return 1;
    }

    /* Logdatei der Plattform oeffnen, damit auch Startfehler darin stehen.
     * Ohne Vorgabepfad (Linux/Mock) ist das kein Fehler -- dann geht die
     * Ausgabe nur auf Konsole und Debug-UART. */
    if (g_use_log != 0) {
        if (v4_plat_log_open(g_log_path) == 0) {
            v4_plat_log_settle(g_settle);
        } else {
            (void)fprintf(stdout,
                          "Hinweis: keine Logdatei (%s); mit -o <pfad> "
                          "einschalten.\n",
                          (g_log_path != NULL) ? g_log_path
                                               : "Plattform ohne Vorgabepfad");
            (void)fflush(stdout);
        }
    }

    /* Debugausgabe endgueltig aufsetzen: stdout + Debug-UART der V4 + Logdatei,
     * derselbe Text. Ab hier laeuft jede Meldung ueber v4_dbg_zeile()/v4_dbg(). */
    {
        v4_dbg_cfg_t cfg;

        memset(&cfg, 0, sizeof(cfg));
        cfg.programm  = "v4_console";
        cfg.version   = CONSOLE_VERSION;
        cfg.datum     = CONSOLE_DATUM;
        cfg.logdatei  = (g_use_log != 0) ? g_log_path : NULL;
        cfg.addr7     = (unsigned)V4_I2C_ADDR7;
        cfg.uart      = 1;              /* auf dem Wirt ohne Wirkung */
        cfg.stdout_an = 1;
        cfg.quiet     = g_quiet;
        v4_dbg_init(&cfg);
        v4_dbg_set_detail(g_trace);
        v4_dbg_set_hex(g_hex);
    }
    v4_dbg_kopf();

    return console_run(dev);
}

#ifndef V4_CONSOLE_NO_MAIN
int main(int argc, char **argv)
{
    return v4_console_main(argc, argv);
}
#endif
