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
        /*
         * Zaehler statt "letzter Rahmenpruefung": `M.last_check` ist ein
         * haftenbleibender letzter Wert und sah nach EINEM Fehler wie ein
         * Dauerzustand aus. Die Zaehler (geprueft/fehlerhaft/fehlerserie)
         * sagen, ob ein einzelner Aussetzer vorliegt oder ein Dauerfehler --
         * dieselbe Schreibweise wie in Herz- und Fehlerzeile der Ausgabe.
         */
        char rahmen[80];

        v4_dbg_rahmen_text(rahmen, sizeof(rahmen));
        v4_msg("  Versuche=%lu, Wiederholungen=%lu, %s\n",
               (unsigned long)M.rx_frames, (unsigned long)M.retries, rahmen);
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
        { V4P_CAP_BATTERY,   "Batterie" },
        /* fw_ver 4/5: Eingangsverteilung, Geraetezuordnung, Maus-Einstellungen.
         * Sie stehen hier genauso wie in v4_debug.c -- wer sie liest, soll
         * nicht zwei Schreibweisen lernen muessen. */
        { V4P_CAP_PORT_ROUTING, "Verteilung" },
        { V4P_CAP_DEVICE_PORT,  "Geraetezuordnung" },
        { V4P_CAP_MOUSE_CFG,    "Maus-Einstellungen" }
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
/* Eingangsverteilung: der Befehl "cfg"                                */
/*                                                                     */
/* Dieselben Moeglichkeiten wie die Auswahlfelder der MUI-Oberflaeche,  */
/* nur als Text -- damit sich die neuen Kommandos OHNE Oberflaeche      */
/* bedienen und pruefen lassen (und damit die Testmatrix sie fahren     */
/* kann):                                                              */
/*                                                                     */
/*   cfg                                alles anzeigen (beide Buchsen) */
/*   cfg <0|1>                          eine Buchse anzeigen           */
/*   cfg <0|1> mode auto|aus|joystick|maus                             */
/*   cfg <0|1> dir alle|keine|stick_l|stick_r|dpad[+...]|0xNN          */
/*   cfg <0|1> beitrag analog|digital|beide                            */
/*                                      Bedienhilfe: setzt dieselben   */
/*                                      Routing-Werte (SET_PORT_ROUTING)*/
/*   cfg <0|1> mouse speed <1..10>      SET_MOUSE_CFG (fw_ver 5)       */
/*   cfg <0|1> mouse totzone <0..100>   Prozent, Umrechnung in der      */
/*                                      Firmware                       */
/*   cfg <0|1> btn1|btn2|btn3 <quelle>  Quelle: keine,a,b,x,y,l1,r1,   */
/*                                      l2,r2,l3,r3,start,select,home, */
/*                                      gruppe1..3 oder 0..16          */
/*   cfg assign <index|BDA> <0|1>       Geraet einer Buchse zuordnen   */
/*   cfg duo                            duo-Zustand anzeigen (fw_ver 6)*/
/*   cfg duo aus|0|port0|1|port1|2      duo setzen: EIN Controller auf */
/*                                      BEIDEN Buchsen (die eine Maus, */
/*                                      die andere Joystick). Das ist   */
/*                                      KEINE Geraetezuordnung.          */
/*                                                                     */
/* Nach JEDER Aenderung wird GET_PORT_CONFIG neu gelesen und angezeigt. */
/* Ein abgelehnter Wert (BAD_ARG, z.B. doppelte Knopfquelle) aendert     */
/* nichts, wird ausdruecklich gemeldet und die Anzeige bleibt auf dem    */
/* echten Zustand -- genau die Zusage der Oberflaeche.                   */
/* ------------------------------------------------------------------ */

/* Sind die neuen Befehle da? Verbindlich sind die Faehigkeitsbits -- NICHT die
 * Firmware-Version: fw_ver 4 benutzt denselben Rahmen und dieselbe proto_ver 3
 * und wird von v4_check_info() angenommen. */
static int cfg_verfuegbar(void)
{
    return (S.info_valid != 0
            && v4_info_has_caps(&S.info, V4P_CAP_PORT_ROUTING) != 0) ? 1 : 0;
}

static int devport_verfuegbar(void)
{
    return (S.info_valid != 0
            && v4_info_has_caps(&S.info, V4P_CAP_DEVICE_PORT) != 0) ? 1 : 0;
}

/* Maus-Einstellungen (fw_ver 5): verbindlich ist auch hier das Faehigkeitsbit,
 * nicht die Firmware-Version. */
static int mousecfg_verfuegbar(void)
{
    return (S.info_valid != 0
            && v4_info_has_caps(&S.info, V4P_CAP_MOUSE_CFG) != 0) ? 1 : 0;
}

/* GET_MOUSE_CFG lesen und in den Zustandsblock uebernehmen. Liefert den
 * Protokollstatus; bei fehlendem Faehigkeitsbit wird NICHTS gesendet. */
static uint8_t mousecfg_lesen(void)
{
    v4p_mousecfg_t c;
    uint8_t        rc;

    if (!mousecfg_verfuegbar()) {
        return V4P_ST_BAD_CMD;
    }
    rc = v4_get_mouse_cfg(&M, &c);
    (void)v4_state_note(&S, rc);
    if (rc != V4P_ST_OK) {
        print_failure("GET_MOUSE_CFG", rc);
        return rc;
    }
    v4_state_put_mousecfg(&S, &c);
    return V4P_ST_OK;
}

/* GET_PORT_CONFIG lesen und in den Zustandsblock uebernehmen. */
static uint8_t cfg_lesen(void)
{
    v4p_portcfg_t c;
    uint8_t       rc;

    if (!cfg_verfuegbar()) {
        v4_msg("Die Gegenstelle kann die Eingangsverteilung nicht "
               "(Faehigkeitsbit 0x%04X fehlt).\n", (unsigned)V4P_CAP_PORT_ROUTING);
        v4_msg("  Firmware %u meldet Faehigkeiten 0x%04X -- fuer "
               "GET_PORT_CONFIG/SET_PORT_ROUTING\n"
               "  wird fw_ver 4 mit Bit 0x0100 gebraucht.\n",
               S.info_valid ? (unsigned)S.info.fw_ver : 0u,
               S.info_valid ? (unsigned)S.info.caps : 0u);
        return V4P_ST_BAD_CMD;
    }
    rc = v4_get_port_config(&M, &c);
    (void)v4_state_note(&S, rc);
    if (rc != V4P_ST_OK) {
        print_failure("GET_PORT_CONFIG", rc);
        return rc;
    }
    v4_state_put_portcfg(&S, &c);
    return V4P_ST_OK;
}

/* Die gemerkte Geraetezuordnung einer Buchse als Text. Sie ist ueber die
 * Leitung NICHT lesbar -- hier steht nur, was in DIESER Sitzung per
 * SET_DEVICE_PORT gesetzt und bestaetigt wurde. Ist das Geraet in der
 * Scanliste, steht sein Name; sonst wenigstens die Adresse (eine Zuordnung
 * per BDA ist auch ohne Scan moeglich). */
static void cfg_geraete_text(int port, char *out, size_t cap)
{
    size_t  n     = 0u;
    int     first = 1;
    uint8_t i;

    if (out == NULL || cap == 0u) {
        return;
    }
    out[0] = '\0';
    for (i = 0u; i < S.devport_n; i++) {
        uint8_t     bda[V4P_BDA_LEN];
        const char *name = NULL;
        char        adresse[24];
        uint8_t     k;

        if (v4_state_devport_at(&S, i, bda) != (uint8_t)port) {
            continue;
        }
        for (k = 0u; k < S.dev_n; k++) {
            if (S.devs[k].bda[0] == bda[0] && S.devs[k].bda[1] == bda[1]
                && S.devs[k].bda[2] == bda[2] && S.devs[k].bda[3] == bda[3]
                && S.devs[k].bda[4] == bda[4] && S.devs[k].bda[5] == bda[5]) {
                name = (S.devs[k].name[0] != '\0') ? S.devs[k].name
                                                   : "(ohne Namen)";
                break;
            }
        }
        if (name == NULL) {
            (void)snprintf(adresse, sizeof(adresse), "%02X:%02X:%02X:%02X:%02X:%02X",
                           bda[0], bda[1], bda[2], bda[3], bda[4], bda[5]);
            name = adresse;
        }
        {
            size_t len = strlen(name);
            size_t j;

            if (n + (first ? 0u : 2u) + len + 1u > cap) {
                break;
            }
            if (!first) {
                out[n++] = ',';
                out[n++] = ' ';
            }
            for (j = 0u; j < len; j++) {
                out[n++] = name[j];
            }
            first = 0;
        }
    }
    if (first) {
        const char *keine = "(keines)";

        for (n = 0u; keine[n] != '\0' && n + 1u < cap; n++) {
            out[n] = keine[n];
        }
    }
    out[n] = '\0';
}

/* Eine Zeile je Buchse -- die Entsprechung zur Konfigurationszeile im Fenster.
 *
 * Seit den Varianten (Beitrag) und den Maus-Einstellungen steht am Ende der
 * Zeile, was die Oberflaeche als eigenes Feld zeigt: die abgeleitete Variante
 * (analog/digital/beide/eigene) und Tempo/Totzone DIESER Buchse. Beim Beitrag
 * "beide" kommt der Hinweis dazu, dass die Einzelschalter gegenstandslos sind --
 * im Fenster sind sie dort ausgegraut, in der Konsole gibt es sie weiter. */
static void cfg_zeile(int port, const v4p_portcfg_t *c, char *buf, size_t cap)
{
    char    dir[48];
    char    geraet[V4P_NAME_BUF + 16u];
    char    maus[48];
    uint8_t beitrag;

    v4p_dir_text(c->dir[port], dir, sizeof(dir));
    cfg_geraete_text(port, geraet, sizeof(geraet));
    beitrag = v4_beitrag_aus_dir(c->dir[port]);

    if (!mousecfg_verfuegbar()) {
        (void)snprintf(maus, sizeof(maus),
                       "Maus nicht verfuegbar (fw_ver %u fehlt)",
                       (unsigned)V4P_FW_VER_MOUSE);
    } else if (S.mousecfg_valid == 0) {
        (void)snprintf(maus, sizeof(maus), "Maus: noch keine Antwort");
    } else {
        v4p_mousecfg_text(&S.mousecfg, (uint8_t)port, maus, sizeof(maus));
    }

    (void)snprintf(buf, cap,
                   "Buchse %d: Geraet %-24s Vorgabe %-8s wirkt %-8s "
                   "Richtungen %-22s Pin6<-%s Pin9<-%s Pin5<-%s "
                   "Beitrag %-7s %s%s",
                   port, geraet,
                   v4p_port_mode_soll_name(c->mode_soll[port]),
                   v4p_port_mode_name(c->mode_ist[port]),
                   dir,
                   v4p_taste_name_short(c->btn[port][0]),
                   v4p_taste_name_short(c->btn[port][1]),
                   v4p_taste_name_short(c->btn[port][2]),
                   v4_beitrag_name(beitrag),
                   maus,
                   (beitrag == (uint8_t)V4P_BEITRAG_BEIDE)
                       ? " (Einzelschalter gegenstandslos)" : "");
}

/* Alles (port < 0) oder eine Buchse anzeigen. Liest die Gegenstelle. */
static uint8_t act_cfg_show(int port)
{
    static char buf[512];               /* ortsfest (4-KB-Stapel) */
    uint8_t     rc = cfg_lesen();

    if (rc != V4P_ST_OK) {
        return rc;
    }
    /* Die Maus-Einstellungen gehoeren zu jeder Buchse dazu; sie kommen aus
     * einer EIGENEN Abfrage (GET_MOUSE_CFG). Ist das Faehigkeitsbit nicht
     * gesetzt, wird nichts gesendet -- die Zeile sagt das dann ausdruecklich. */
    if (mousecfg_verfuegbar()) {
        (void)mousecfg_lesen();
    }
    /*
     * duo gehoert zu KEINER Buchse, aber in die Zustandsuebersicht: es sagt,
     * ob EIN Controller beide Buchsen bedient. Fehlt das Faehigkeitsbit, wird
     * nichts gesendet und nichts behauptet.
     */
    if (S.info_valid != 0
        && v4_info_has_caps(&S.info, V4P_CAP_DUO) != 0) {
        uint8_t duo = 0u;
        uint8_t drc = v4_get_duo(&M, &duo);

        (void)v4_state_note(&S, drc);
        if (drc == V4P_ST_OK) {
            v4_state_put_duo(&S, duo);
            v4_msg("  duo = %u (%s)%s\n", (unsigned)duo, v4p_duo_name(duo),
                   (duo == V4P_DUO_OFF)
                       ? ""
                       : " -- EIN Controller auf BEIDEN Buchsen");
        }
    }
    if (port < 0 || port >= (int)V4P_PORT_COUNT) {
        int p;

        for (p = 0; p < (int)V4P_PORT_COUNT; p++) {
            cfg_zeile(p, &S.portcfg, buf, sizeof(buf));
            v4_msg("%s\n", buf);
        }
    } else {
        cfg_zeile(port, &S.portcfg, buf, sizeof(buf));
        v4_msg("%s\n", buf);
    }
    v4_msg("  Vorgabe = GET_PORT_CONFIG.mode_soll (auto = nach Geraeteart), "
           "wirkt = mode_ist\n"
           "  Beitrag = Bedienhilfe: analog (Stick links + L1/R1), digital "
           "(D-Pad + A/B/Start),\n"
           "  beide (alle Richtungen + Gruppe 1/2/3) -- sie setzt nur "
           "SET_PORT_ROUTING, kein neuer Befehl.\n"
           "  Tempo/Totzone gelten JE BUCHSE (GET/SET_MOUSE_CFG); die "
           "Umrechnung macht die Firmware.\n"
           "  Die Geraetezuordnung ist ueber die Leitung nicht lesbar: hier "
           "steht, was in dieser\n"
           "  Sitzung per SET_DEVICE_PORT gesetzt und bestaetigt wurde.\n");
    return V4P_ST_OK;
}

/* "auto"/"aus"/"joystick"/"maus" -> Vorgabewert. -1 = unbekannt. */
static int cfg_mode_parse(const char *s)
{
    if (s == NULL) {
        return -1;
    }
    if (strcmp(s, "auto") == 0 || strcmp(s, "0xFF") == 0) {
        return (int)V4P_MODE_AUTO;
    }
    if (strcmp(s, "aus") == 0 || strcmp(s, "off") == 0 || strcmp(s, "0") == 0) {
        return (int)V4P_PORT_OFF;
    }
    if (strcmp(s, "joystick") == 0 || strcmp(s, "joy") == 0
        || strcmp(s, "1") == 0) {
        return (int)V4P_PORT_JOY;
    }
    if (strcmp(s, "maus") == 0 || strcmp(s, "mouse") == 0
        || strcmp(s, "2") == 0) {
        return (int)V4P_PORT_MOUSE;
    }
    return -1;
}

/* Eine Knopfquelle: Name oder Zahl 0..16. -1 = unbekannt. */
static int cfg_taste_parse(const char *s)
{
    static const struct {
        const char *name;
        uint8_t     code;
    } tab[] = {
        { "keine",   V4P_TASTE_KEINE  }, { "a",      V4P_TASTE_A      },
        { "b",       V4P_TASTE_B      }, { "x",      V4P_TASTE_X      },
        { "y",       V4P_TASTE_Y      }, { "l1",     V4P_TASTE_L1     },
        { "r1",      V4P_TASTE_R1     }, { "l2",     V4P_TASTE_L2     },
        { "r2",      V4P_TASTE_R2     }, { "l3",     V4P_TASTE_L3     },
        { "r3",      V4P_TASTE_R3     }, { "start",  V4P_TASTE_START  },
        { "select",  V4P_TASTE_SELECT }, { "home",   V4P_TASTE_HOME   },
        { "gruppe1", V4P_TASTE_GRUPPE1 }, { "g1",    V4P_TASTE_GRUPPE1 },
        { "gruppe2", V4P_TASTE_GRUPPE2 }, { "g2",    V4P_TASTE_GRUPPE2 },
        { "gruppe3", V4P_TASTE_GRUPPE3 }, { "g3",    V4P_TASTE_GRUPPE3 }
    };
    unsigned i;
    char     *ende = NULL;
    long      zahl;

    if (s == NULL || s[0] == '\0') {
        return -1;
    }
    for (i = 0u; i < (unsigned)(sizeof(tab) / sizeof(tab[0])); i++) {
        if (strcmp(s, tab[i].name) == 0) {
            return (int)tab[i].code;
        }
    }
    zahl = strtol(s, &ende, 0);
    if (ende != NULL && *ende == '\0' && zahl >= 0
        && zahl < (long)V4P_TASTE_ANZAHL) {
        return (int)zahl;
    }
    return -1;
}

/*
 * Richtungen: "alle", "keine", eine Bitfolge aus stick_l/stick_r/dpad (mit
 * '+' getrennt) oder eine Zahl 0..7. Rueckgabe 0 = erkannt.
 */
static int cfg_dir_parse(const char *s, uint8_t *out)
{
    uint8_t maske = 0u;
    char    kopie[64];
    char   *teil;
    size_t  i;

    if (s == NULL || out == NULL || s[0] == '\0') {
        return 0;
    }
    if (strcmp(s, "alle") == 0) {
        *out = (uint8_t)V4P_DIR_ALLE;
        return 1;
    }
    if (strcmp(s, "keine") == 0) {
        *out = 0u;
        return 1;
    }
    if (s[0] >= '0' && s[0] <= '9') {
        char *ende = NULL;
        long  zahl = strtol(s, &ende, 0);

        if (ende != NULL && *ende == '\0' && zahl >= 0
            && (zahl & ~(long)V4P_DIR_MASK) == 0) {
            *out = (uint8_t)zahl;
            return 1;
        }
        return 0;
    }
    /* Namen, durch '+' getrennt. */
    for (i = 0u; i + 1u < sizeof(kopie) && s[i] != '\0'; i++) {
        kopie[i] = s[i];
    }
    kopie[i] = '\0';
    for (teil = kopie; teil != NULL && *teil != '\0'; ) {
        char *plus = strchr(teil, '+');

        if (plus != NULL) {
            *plus = '\0';
        }
        if (strcmp(teil, "stick_l") == 0 || strcmp(teil, "stick-links") == 0) {
            maske = (uint8_t)(maske | V4P_DIR_STICK_L);
        } else if (strcmp(teil, "stick_r") == 0
                   || strcmp(teil, "stick-rechts") == 0) {
            maske = (uint8_t)(maske | V4P_DIR_STICK_R);
        } else if (strcmp(teil, "dpad") == 0 || strcmp(teil, "d-pad") == 0) {
            maske = (uint8_t)(maske | V4P_DIR_DPAD);
        } else {
            return 0;
        }
        teil = (plus != NULL) ? (plus + 1) : NULL;
    }
    *out = maske;
    return 1;
}

/* Eine Beitrags-Variante: "analog", "digital", "beide". -1 = unbekannt.
 * "eigene" ist KEINE waehlbare Variante (sie beschreibt nur einen vorgefundenen
 * Zustand) und wird deshalb nicht angenommen. */
static int cfg_beitrag_parse(const char *s)
{
    if (s == NULL) {
        return -1;
    }
    if (strcmp(s, "analog") == 0) {
        return (int)V4P_BEITRAG_ANALOG;
    }
    if (strcmp(s, "digital") == 0) {
        return (int)V4P_BEITRAG_DIGITAL;
    }
    if (strcmp(s, "beide") == 0 || strcmp(s, "beides") == 0) {
        return (int)V4P_BEITRAG_BEIDE;
    }
    return -1;
}

/* Eine ganze Zahl 0..1000 aus einem Argument. 0 = erkannt. */
static int cfg_zahl_parse(const char *s, int *out)
{
    char *ende = NULL;
    long  zahl;

    if (s == NULL || out == NULL || s[0] == '\0') {
        return 0;
    }
    zahl = strtol(s, &ende, 10);
    if (ende == NULL || *ende != '\0' || zahl < 0 || zahl > 1000) {
        return 0;
    }
    *out = (int)zahl;
    return 1;
}

/* Eine BDA aus "AA:BB:CC:DD:EE:FF" (auch ohne Doppelpunkte). 0 = erkannt. */
static int cfg_bda_parse(const char *s, uint8_t bda[V4P_BDA_LEN])
{
    unsigned i;
    unsigned ziffern = 0u;

    if (s == NULL || bda == NULL) {
        return 0;
    }
    for (i = 0u; i < V4P_BDA_LEN; i++) {
        unsigned wert = 0u;
        unsigned k;

        for (k = 0u; k < 2u; k++) {
            char c = *s;
            unsigned v;

            if (c >= '0' && c <= '9') {
                v = (unsigned)(c - '0');
            } else if (c >= 'a' && c <= 'f') {
                v = (unsigned)(c - 'a' + 10);
            } else if (c >= 'A' && c <= 'F') {
                v = (unsigned)(c - 'A' + 10);
            } else {
                return 0;
            }
            wert = (wert << 4) | v;
            s++;
            ziffern++;
        }
        bda[i] = (uint8_t)wert;
        if (*s == ':' || *s == '-' || *s == '.') {
            s++;
        } else if (*s != '\0') {
            return 0;
        }
    }
    return (*s == '\0' && ziffern == 12u) ? 1 : 0;
}

/*
 * "cfg duo [wert]" -- duo lesen und setzen (fw_ver 6).
 *
 * duo heisst: EIN Controller bedient BEIDE Buchsen, die eine als Maus, die
 * andere als Joystick. Es ist KEINE Geraetezuordnung (die bleibt je Geraet
 * genau eine Buchse) und wirkt nur, solange genau EIN Controller verbunden
 * ist. Verbindlich ist das Faehigkeitsbit V4P_CAP_DUO: fehlt es, wird nichts
 * gesendet und die Anzeige behauptet auch nichts.
 */
static uint8_t act_cfg_duo(int narg, char **arg)
{
    uint8_t duo = 0u;
    uint8_t rc;

    if (S.info_valid == 0
        || v4_info_has_caps(&S.info, V4P_CAP_DUO) == 0) {
        v4_msg("Die Gegenstelle kann duo nicht (Faehigkeitsbit 0x%04X fehlt, "
               "fw_ver %u noetig) -- nichts gesendet.\n",
               (unsigned)V4P_CAP_DUO, (unsigned)V4P_FW_VER_DUO);
        return V4P_ST_BAD_CMD;
    }

    if (narg < 2) {                     /* nur anzeigen */
        rc = v4_get_duo(&M, &duo);
        (void)v4_state_note(&S, rc);
        if (rc != V4P_ST_OK) {
            print_failure("GET_DUO", rc);
            return rc;
        }
        v4_state_put_duo(&S, duo);
        v4_msg("duo = %u (%s)%s\n", (unsigned)duo, v4p_duo_name(duo),
               (duo == V4P_DUO_OFF)
                   ? " -- jeder Controller bedient seine Buchse"
                   : " -- EIN Controller auf BEIDEN Buchsen");
        return V4P_ST_OK;
    }

    {
        const char *w = arg[1];
        uint8_t     neu;

        if (strcmp(w, "aus") == 0 || strcmp(w, "off") == 0
            || strcmp(w, "0") == 0) {
            neu = (uint8_t)V4P_DUO_OFF;
        } else if (strcmp(w, "port0") == 0 || strcmp(w, "p0") == 0
                   || strcmp(w, "1") == 0) {
            neu = (uint8_t)V4P_DUO_MOUSE_PORT0;
        } else if (strcmp(w, "port1") == 0 || strcmp(w, "p1") == 0
                   || strcmp(w, "2") == 0) {
            neu = (uint8_t)V4P_DUO_MOUSE_PORT1;
        } else {
            v4_msg("Aufruf: cfg duo [aus|0|port0|1|port1|2]\n");
            return V4P_ST_BAD_ARG;
        }

        rc = v4_set_duo(&M, neu);
        (void)v4_state_note(&S, rc);
        if (rc == V4P_ST_BAD_ARG) {
            v4_msg("SET_DUO: BAD_ARG -- erlaubt sind nur 0 (aus), 1 (Port 0 "
                   "Maus) und 2 (Port 1 Maus); die alte Einstellung bleibt.\n");
            return rc;
        }
        if (rc != V4P_ST_OK) {
            print_failure("SET_DUO", rc);
            return rc;
        }
        v4_state_put_duo(&S, neu);
    }
    /* Nach jeder Aenderung den ECHTEN Stand lesen und zeigen. */
    rc = v4_get_duo(&M, &duo);
    (void)v4_state_note(&S, rc);
    if (rc == V4P_ST_OK) {
        v4_state_put_duo(&S, duo);
    }
    v4_msg("duo = %u (%s) bestaetigt%s\n", (unsigned)duo, v4p_duo_name(duo),
           (duo == V4P_DUO_OFF) ? "" : " -- EIN Controller auf BEIDEN Buchsen");
    return V4P_ST_OK;
}

/*
 * Eine Kommandozeile nach "cfg" auswerten. `line` ist der Rest der Zeile
 * (ohne "cfg"). Die Geraeteliste kommt aus dem Zustandsblock (Befehl 'a').
 */
static void act_cfg(char *line)
{
    char  *arg[4];
    int    narg = 0;
    char  *p;

    for (p = strtok(line, " \t\r\n"); p != NULL && narg < 4;
         p = strtok(NULL, " \t\r\n")) {
        arg[narg++] = p;
    }

    /* "cfg" bzw. "cfg <port>" -- anzeigen. */
    if (narg == 0) {
        (void)act_cfg_show(-1);
        return;
    }
    /*
     * duo ist NICHT buchsenbezogen und wird deshalb vor der Buchsenauswertung
     * behandelt. Verbindlich ist das Faehigkeitsbit V4P_CAP_DUO; fehlt es,
     * wird NICHTS gesendet.
     */
    if (strcmp(arg[0], "duo") == 0) {
        (void)act_cfg_duo(narg, arg);
        return;
    }
    if (strcmp(arg[0], "assign") == 0) {
        int port;

        if (narg < 3) {
            v4_msg("Aufruf: cfg assign <index|BDA> <0|1>\n");
            return;
        }
        port = atoi(arg[2]);
        if (port < 0 || port >= (int)V4P_PORT_COUNT) {
            v4_msg("Es gibt nur Buchse 0 und Buchse 1.\n");
            return;
        }
        if (!devport_verfuegbar()) {
            v4_msg("Die Gegenstelle kann SET_DEVICE_PORT nicht "
                   "(Faehigkeitsbit 0x%04X fehlt).\n",
                   (unsigned)V4P_CAP_DEVICE_PORT);
            return;
        }
        if (arg[1][0] >= '0' && arg[1][0] <= '9'
            && strlen(arg[1]) <= 2) {
            /* Index in die Geraeteliste aus dem Zustandsblock. */
            int idx = atoi(arg[1]);

            if (idx < 0 || idx >= (int)S.dev_n) {
                v4_msg("Index %d liegt nicht in der Geraeteliste (%u "
                       "Eintraege). Erst 'a' (Scan) ausfuehren.\n",
                       idx, (unsigned)S.dev_n);
                return;
            }
            {
                uint8_t rc = v4_set_device_port(&M, S.devs[idx].bda,
                                                (uint8_t)port);

                (void)v4_state_note(&S, rc);
                if (rc != V4P_ST_OK) {
                    v4_msg("SET_DEVICE_PORT: %s -- nichts geaendert.\n",
                           v4_strerror(rc));
                    return;
                }
                v4_state_put_devport(&S, S.devs[idx].bda, (uint8_t)port);
                v4_msg("Geraet [%d] \"%s\" -> Buchse %d (bestaetigt).\n",
                       idx,
                       (S.devs[idx].name[0] != '\0') ? S.devs[idx].name
                                                     : "(ohne Namen)",
                       port);
            }
        } else {
            uint8_t bda[V4P_BDA_LEN];
            uint8_t rc;

            if (!cfg_bda_parse(arg[1], bda)) {
                v4_msg("\"%s\" ist keine Adresse (AA:BB:CC:DD:EE:FF) und kein "
                       "Index.\n", arg[1]);
                return;
            }
            rc = v4_set_device_port(&M, bda, (uint8_t)port);
            (void)v4_state_note(&S, rc);
            if (rc != V4P_ST_OK) {
                v4_msg("SET_DEVICE_PORT: %s -- nichts geaendert.\n",
                       v4_strerror(rc));
                return;
            }
            v4_state_put_devport(&S, bda, (uint8_t)port);
            v4_msg("Adresse %02X:%02X:%02X:%02X:%02X:%02X -> Buchse %d "
                   "(bestaetigt).\n",
                   bda[0], bda[1], bda[2], bda[3], bda[4], bda[5], port);
        }
        /* Wie bei jeder anderen Aenderung: den echten Zustand nachlesen und
         * die Zeile zeigen (der Geraetename steht darin). */
        (void)act_cfg_show(port);
        return;
    }

    /* Ab hier: cfg <port> ... */
    {
        int port = atoi(arg[0]);

        if (port < 0 || port >= (int)V4P_PORT_COUNT) {
            v4_msg("Es gibt nur Buchse 0 und Buchse 1 (oder 'cfg assign').\n");
            return;
        }
        if (narg == 1) {
            (void)act_cfg_show(port);
            return;
        }
        if (narg < 3) {
            v4_msg("Aufruf: cfg <0|1> mode|dir|btn1|btn2|btn3 <wert>\n"
                   "        cfg <0|1> beitrag analog|digital|beide\n"
                   "        cfg <0|1> mouse speed|totzone <wert>\n"
                   "        cfg duo [aus|0|port0|1|port1|2]\n");
            return;
        }
        if (!cfg_verfuegbar()) {
            (void)cfg_lesen();          /* gibt den Klartext aus */
            return;
        }

        if (strcmp(arg[1], "mode") == 0) {
            int     m = cfg_mode_parse(arg[2]);
            uint8_t applied = 0u;
            uint8_t rc;

            if (m < 0) {
                v4_msg("\"%s\" ist keine Betriebsart (auto, aus, Joystick, "
                       "Maus).\n", arg[2]);
                return;
            }
            rc = v4_set_port_mode(&M, (uint8_t)port, (uint8_t)m, &applied);
            (void)v4_state_note(&S, rc);
            if (rc != V4P_ST_OK) {
                v4_msg("SET_PORT_MODE: %s -- nichts geaendert.\n",
                       v4_strerror(rc));
                return;
            }
            if ((uint8_t)m == (uint8_t)V4P_MODE_AUTO) {
                v4_msg("Buchse %d: Vorgabe auto (nach Geraeteart) "
                       "bestaetigt; die wirksame Betriebsart wird beim "
                       "naechsten Verbinden bzw. von 'duo' neu abgeleitet.\n",
                       port);
            } else {
                v4_msg("Buchse %d: Betriebsart %s bestaetigt.\n", port,
                       v4p_port_mode_name(applied));
            }
        } else if (strcmp(arg[1], "dir") == 0) {
            uint8_t neu;
            uint8_t rc;

            if (!cfg_dir_parse(arg[2], &neu)) {
                v4_msg("\"%s\" ist keine Richtungsangabe (alle, keine, "
                       "stick_l, stick_r, dpad, mit '+' verbunden, oder "
                       "0x00..0x07).\n", arg[2]);
                return;
            }
            rc = cfg_lesen();
            if (rc != V4P_ST_OK) {
                return;
            }
            rc = v4_set_port_routing(&M, (uint8_t)port, neu,
                                     S.portcfg.btn[port]);
            (void)v4_state_note(&S, rc);
            if (rc != V4P_ST_OK) {
                v4_msg("SET_PORT_ROUTING: %s -- nichts geaendert.\n",
                       v4_strerror(rc));
                return;
            }
            v4_msg("Buchse %d: Richtungen gesetzt.\n", port);
        } else if (strcmp(arg[1], "beitrag") == 0) {
            /* Die drei Varianten setzen NUR die bestehenden Routing-Werte. */
            int     b = cfg_beitrag_parse(arg[2]);
            uint8_t rc;
            uint8_t btn[V4P_BTN_COUNT];
            char    dir[48];

            if (b < 0) {
                v4_msg("\"%s\" ist kein Beitrag (analog, digital, beide).\n",
                       arg[2]);
                return;
            }
            rc = v4_set_port_beitrag(&M, (uint8_t)port, (uint8_t)b);
            (void)v4_state_note(&S, rc);
            if (rc != V4P_ST_OK) {
                v4_msg("SET_PORT_ROUTING (Beitrag %s): %s -- nichts "
                       "geaendert.\n", v4_beitrag_name((uint8_t)b),
                       v4_strerror(rc));
                return;
            }
            v4_beitrag_btn((uint8_t)b, btn);
            v4p_dir_text(v4_beitrag_dir((uint8_t)b), dir, sizeof(dir));
            v4_msg("Buchse %d: Beitrag %s gesetzt -- Richtungen %s, "
                   "Pin6<-%s Pin9<-%s Pin5<-%s.\n",
                   port, v4_beitrag_name((uint8_t)b), dir,
                   v4p_taste_name(btn[0]), v4p_taste_name(btn[1]),
                   v4p_taste_name(btn[2]));
            if ((uint8_t)b == (uint8_t)V4P_BEITRAG_BEIDE) {
                v4_msg("  Hinweis: bei 'beide' sind die EINZELNEN Richtungs- "
                       "und Knopfschalter gegenstandslos;\n"
                       "  'cfg %d dir' bzw. 'cfg %d btnN' aendern die "
                       "Verteilung trotzdem.\n", port, port);
            }
        } else if (strcmp(arg[1], "mouse") == 0) {
            int     wert = 0;
            int     neu_speed;
            int     neu_totzone;
            uint8_t rc;

            if (narg < 4) {
                v4_msg("Aufruf: cfg <0|1> mouse speed|totzone <wert>\n"
                       "  speed   1..%u (1 = langsamste)\n"
                       "  totzone %u..%u Prozent\n",
                       (unsigned)V4P_MOUSE_SPEED_MAX,
                       (unsigned)V4P_MOUSE_TOTZONE_MIN,
                       (unsigned)V4P_MOUSE_TOTZONE_MAX);
                return;
            }
            if (!mousecfg_verfuegbar()) {
                v4_msg("Die Gegenstelle kann die Maus-Einstellungen nicht "
                       "(Faehigkeitsbit 0x%04X fehlt, gemeldet 0x%04X).\n"
                       "  Sie braucht fw_ver %u; es wird NICHTS gesendet.\n",
                       (unsigned)V4P_CAP_MOUSE_CFG,
                       S.info_valid ? (unsigned)S.info.caps : 0u,
                       (unsigned)V4P_FW_VER_MOUSE);
                return;
            }
            if (!cfg_zahl_parse(arg[3], &wert)) {
                v4_msg("\"%s\" ist keine Zahl.\n", arg[3]);
                return;
            }
            /* ERST den bestaetigten Stand holen: das Feld, das NICHT geaendert
             * wird, muss mitgeschickt werden (SET_MOUSE_CFG setzt Tempo UND
             * Totzone einer Buchse). Ohne diese Abfrage wuerde ein Zahlendreher
             * im anderen Feld mitgeschickt. */
            rc = mousecfg_lesen();
            if (rc != V4P_ST_OK) {
                return;
            }
            neu_speed   = (int)S.mousecfg.speed[port];
            neu_totzone = (int)S.mousecfg.totzone[port];
            if (strcmp(arg[2], "speed") == 0) {
                if (!v4p_mouse_speed_valid((uint8_t)wert)) {
                    v4_msg("Tempo %d liegt ausserhalb 1..%u.\n", wert,
                           (unsigned)V4P_MOUSE_SPEED_MAX);
                    return;
                }
                neu_speed = wert;
            } else if (strcmp(arg[2], "totzone") == 0) {
                if (!v4p_mouse_totzone_valid((uint8_t)wert)) {
                    v4_msg("Totzone %d liegt ausserhalb %u..%u Prozent.\n",
                           wert, (unsigned)V4P_MOUSE_TOTZONE_MIN,
                           (unsigned)V4P_MOUSE_TOTZONE_MAX);
                    return;
                }
                neu_totzone = wert;
            } else {
                v4_msg("\"%s\" ist keine Maus-Einstellung (speed, totzone).\n",
                       arg[2]);
                return;
            }
            rc = v4_set_mouse_cfg(&M, (uint8_t)port, (uint8_t)neu_speed,
                                  (uint8_t)neu_totzone);
            (void)v4_state_note(&S, rc);
            if (rc == V4P_ST_BAD_ARG) {
                v4_msg("SET_MOUSE_CFG: BAD_ARG -- der Wert wurde abgelehnt "
                       "(Tempo 1..%u, Totzone %u..%u Prozent); nichts "
                       "geaendert.\n",
                       (unsigned)V4P_MOUSE_SPEED_MAX,
                       (unsigned)V4P_MOUSE_TOTZONE_MIN,
                       (unsigned)V4P_MOUSE_TOTZONE_MAX);
                return;
            }
            if (rc != V4P_ST_OK) {
                v4_msg("SET_MOUSE_CFG: %s -- nichts geaendert.\n",
                       v4_strerror(rc));
                return;
            }
            /* Der Wrapper hat das Echo feldweise geprueft -- auf V4P_ST_OK
             * stehen die Werte fest. Sie werden uebernommen wie bei
             * SET_PORT_MODE; die Anzeige darunter liest ohnehin nach. */
            S.mousecfg.speed[port]   = (uint8_t)neu_speed;
            S.mousecfg.totzone[port] = (uint8_t)neu_totzone;
            S.mousecfg_valid         = 1;
            v4_msg("Buchse %d: Maus-Einstellung bestaetigt -- Tempo %d/%u, "
                   "Totzone %d%%.\n", port, neu_speed,
                   (unsigned)V4P_MOUSE_SPEED_MAX, neu_totzone);
        } else if (strcmp(arg[1], "btn1") == 0
                   || strcmp(arg[1], "btn2") == 0
                   || strcmp(arg[1], "btn3") == 0) {
            int     idx = (arg[1][3] == '1') ? 0 : ((arg[1][3] == '2') ? 1 : 2);
            int     code = cfg_taste_parse(arg[2]);
            uint8_t btn[V4P_BTN_COUNT];
            uint8_t rc;
            unsigned k;

            if (code < 0) {
                v4_msg("\"%s\" ist keine Knopfquelle (keine, a, b, x, y, l1, "
                       "r1, l2, r2, l3, r3, start, select, home, gruppe1..3 "
                       "oder 0..16).\n", arg[2]);
                return;
            }
            rc = cfg_lesen();
            if (rc != V4P_ST_OK) {
                return;
            }
            for (k = 0u; k < V4P_BTN_COUNT; k++) {
                btn[k] = S.portcfg.btn[port][k];
            }
            btn[idx] = (uint8_t)code;
            rc = v4_set_port_routing(&M, (uint8_t)port, S.portcfg.dir[port],
                                     btn);
            (void)v4_state_note(&S, rc);
            if (rc == V4P_ST_BAD_ARG) {
                /* Der haeufigste Fall: die Quelle steht schon auf einer anderen
                 * Leitung. Das gehoert im Klartext gesagt, sonst sucht man den
                 * Fehler im Kabel. */
                v4_msg("SET_PORT_ROUTING: BAD_ARG -- \"%s\" ist auf dieser "
                       "Buchse schon einer anderen Leitung zugeordnet\n"
                       "  (Pin6<-%s Pin9<-%s Pin5<-%s). Eine nicht-leere "
                       "Quelle darf nur auf EINER Leitung stehen; nichts "
                       "geaendert.\n",
                       v4p_taste_name((uint8_t)code),
                       v4p_taste_name_short(S.portcfg.btn[port][0]),
                       v4p_taste_name_short(S.portcfg.btn[port][1]),
                       v4p_taste_name_short(S.portcfg.btn[port][2]));
                return;
            }
            if (rc != V4P_ST_OK) {
                v4_msg("SET_PORT_ROUTING: %s -- nichts geaendert.\n",
                       v4_strerror(rc));
                return;
            }
            v4_msg("Buchse %d: %s <- %s bestaetigt.\n", port,
                   (idx == 0) ? "Pin 6" : ((idx == 1) ? "Pin 9" : "Pin 5"),
                   v4p_taste_name_short((uint8_t)code));
        } else {
            v4_msg("Unbekannt: 'cfg %d %s ...' (mode, dir, btn1, btn2, btn3,\n"
                   "  beitrag analog|digital|beide, mouse speed|totzone).\n",
                   port, arg[1]);
            return;
        }

        /* Nach JEDER Aenderung den echten Zustand nachlesen und zeigen. */
        (void)act_cfg_show(port);
    }
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
    /* Die Eingangsverteilung (fw_ver 4) gleich mit -- so stehen die Felder,
     * die die Oberflaeche als Auswahlfelder zeigt, auch hier von Anfang an da. */
    if (cfg_verfuegbar()) {
        (void)act_cfg_show(-1);
    }

    /*
     * Hauptmenue. Jede Zeile ist ein Befehl; das Programm beendet sich auch bei
     * geschlossener Eingabe (Pipe-Ende) -- sonst bliebe ein Vorfuehrlauf ohne
     * 'q' fuer immer stehen.
     */
    v4_msg("\n-- Menue --  s Zustand, h HID, i Eingaenge, p Portmodus, "
           "t tauschen,\n"
           "             a Scan/Liste, c verbinden, d trennen, f vergessen, "
           "r RESET, q Ende\n"
           "             cfg [<0|1> mode|dir|beitrag|mouse|btn1|btn2|btn3 "
           "<wert>] | cfg assign <index|BDA> <0|1> | cfg duo [0|1|2]\n");

    while (beenden == 0) {
        console_herz_maybe();
        v4_msg("Befehl> ");
        if (fgets(line, sizeof(line), stdin) == NULL) {
            v4_msg("\n(Eingabe zu Ende -- Programm beendet)\n");
            beenden = 1;
            continue;
        }
        /*
         * "cfg" beginnt mit 'c' -- das ist unten der Befehl "verbinden".
         * Deshalb wird es VOR dem Schalter abgefangen; nur "cfg" mit
         * Wortgrenze, damit "c" allein weiter verbindet.
         */
        if (strncmp(line, "cfg", 3) == 0
            && (line[3] == '\0' || line[3] == ' ' || line[3] == '\t'
                || line[3] == '\n' || line[3] == '\r')) {
            act_cfg(line + 3);
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
