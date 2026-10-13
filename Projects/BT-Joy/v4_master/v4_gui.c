/*
 * v4_gui.c -- MUI-Oberflaeche fuer den Bluetooth-Joystick-Adapter an der
 * Vampire V4.
 *
 * Zweites Frontend neben v4_console.c: dieselbe Protokollschicht
 * (v4_master.c / v4_proto.c), dieselbe Plattform (v4_amiga_i2c.c). Die Konsole
 * bleibt als Notfallweg erhalten.
 *
 * Was die Oberflaeche zeigt:
 *   - Kopfzeile: Protokoll- und Firmware-Version, Faehigkeiten, Zustand
 *   - Controller-Zeile: Name, VID/PID, Art, Transport, gezaehlte Reports
 *   - zwei Portgruppen (Buchse 0 und 1) mit Betriebsart als Cycle und den
 *     sieben Leitungen UP, DOWN, LEFT, RIGHT, BTN1 (Pin 6), BTN2 (Pin 9),
 *     BTN3 (Pin 5) -- je Leitung "Zug" und "Pegel"
 *   - VERGLEICH direkt <-> I2C: je Buchse das, was die V4 an ihren EIGENEN
 *     Registern liest (JOY0DAT/JOY1DAT/CIAAPRA/POTINP), neben dem, was der
 *     ESP32 ueber GET_INPUT meldet -- mit OK/ABWEICHUNG und den Rohwerten der
 *     Feuerregister
 *   - Knoepfe: Neu suchen, Geraeteliste/Verbinden, Trennen, Vergessen,
 *     Port 0/1 Modus weiterschalten, Buchsen tauschen
 *   - Logfeld mit allen Meldungen
 *
 * DIREKT GELESEN WIRD IM SELBEN TAKT UND IM SELBEN FADEN:
 * Die Registerabfrage (v4_joyport.c) kostet nichts und braucht keinen Bus. Sie
 * laeuft deshalb in jedem Takt -- aber NACH den beiden I2C-Abfragen, damit im
 * Mitschnitt die erste Buszeile vor der ersten Zeile der Direktabfrage steht
 * (siehe gui_poll). Der Vergleich geht NICHT mehr als eigene Zeile je Takt auf
 * den DebugUART, sondern (a) in die EINE Herzzeile je Sekunde und (b) sofort,
 * wenn sich Richtung, Feuerknopf oder Urteil aendern. Genau das ist der Zweck:
 * am PC mitlesen, ob beide Quellen dasselbe sagen. Die Stabilitaet der Anzeige
 * kommt daher, dass jede Buchse aus ZWEI Abtastungen bewertet wird (siehe
 * v4_joyport.h).
 *
 * DIE AUSGABEMENGE IST TEIL DER KONSTRUKTION:
 * Auf der V4 ist der DebugUART das einzige Fenster in das Programm -- aber
 * jedes Zeichen kostet Wartezeit. Deshalb:
 *
 *   - im Ruhezustand GENAU EINE Zeile je Sekunde: das Lebenszeichen [herz] mit
 *     Zaehlern, letztem Status, verworfenen Rahmen, Wiederholungen UND dem
 *     Vergleich direkt/I2C samt Rohwerten von CIAAPRA/POTINP,
 *   - SOFORT eine Zeile bei Aenderung: Richtung, Feuerknopf, Buchsenmodus,
 *     Sitzvergabe, Fehler, Verbindungsverlust und -erholung, Abweichung
 *     direkt/I2C,
 *   - Transaktionszeilen (eine je I2C-Befehl) NUR mit -trace. Ohne -trace
 *     entstehen bei 200 ms Takt keine Zeilen.
 *
 * Jede Meldung ist EINE fertige Zeile und geht mit EINEM ApolloDebugPutStr
 * heraus (v4_debug.c); kein Zeichen fuer Zeichen, kein Delay() in der Naehe des
 * Buszugs.
 *
 * "ZUG" UND "PEGEL" SIND ZWEI VERSCHIEDENE DINGE:
 * "Zug" ist die Zug-Maske aus GET_INPUT -- was die Firmware nach GND zieht
 * (Bit gesetzt = Taste/Richtung aktiv). "Pegel" ist der GEMESSENE Pegel, den
 * die Firmware seit der 12-Byte-Fassung im selben Befehl mitschickt (Bit
 * gesetzt = High, Ruhe 0x7F). Beides kann auseinandergehen, und dann ist der
 * Widerspruch die eigentliche Aussage: "losgelassen, liest aber 0" heisst, dass
 * der Pull-up der Vampire fehlt; "gezogen, liest High" kann elektrisch nicht
 * sein. Beides wird je Leitung in der Zeile benannt und einmalig ins Log
 * geschrieben.
 *
 * Antwortet die Gegenstelle noch mit der aelteren 10-Byte-Fassung, sind die
 * Pegel UNBEKANNT: dann steht in der Spalte ein Hinweis ("neu flashen") und
 * KEINE Zahl. Eine abgeleitete Zahl waere von einer Messung nicht zu
 * unterscheiden -- genau diese Verwechslung darf es nicht geben.
 *
 * I2C DIREKT IM HAUPTFADEN (MUI-Task):
 * Die Oberflaeche ruft v4_master -- und damit v4_plat_* und i2c.library --
 * SELBST auf, aus ihrem Ereignisablauf heraus. Kein CreateNewProc, kein
 * zweiter Faden, kein Nachrichtenport, kein Forbid(). Genau diese Konstruktion
 * ist auf der V4 gelaufen; die Fassung mit eigener I2C-Aufgabe kam am
 * 13.10.2026 nicht einmal durch ihren Start (die Oberflaeche blieb nach dem
 * Banner stehen), und ihr Forbid() hat zusaetzlich die Workbench eingefroren.
 *
 * Der Takt (200 ms) fragt GET_PORT_MODE und GET_INPUT ab, jeder fuenfte Takt
 * zusaetzlich GET_STATUS und GET_HID -- jeweils direkt, synchron, im
 * MUI-Task. i2c.library hat keinen Timeout: Antwortet der ESP32 nicht, steht
 * das ausdruecklich in der obersten Zeile ("ESP32 antwortet nicht ..."), die
 * zuletzt gueltigen Werte bleiben stehen, und die Abfrage wiederholt sich mit
 * wachsendem Abstand (0,5 s -> 1 s -> 2 s -> 5 s). Dass ein wirklich
 * HAENGENDER Buszugriff damit auch das Fenster anhaelt, ist der bekannte und
 * bewusst in Kauf genommene Preis dieser Bauart (siehe README, "Offene
 * Punkte").
 *
 * Alles, was die Gegenstelle zuletzt gueltig gemeldet hat, liegt in
 * v4_state_t; die Ausgabe (Logfeld, stdout, Debug-UART, Logdatei) laeuft
 * ueber v4_debug.c.
 *
 * AUFBAU UND FENSTER:
 * Fenster 640x400 mit SizeGadget; -w/-h aendern die Groesse, mit der Maus geht
 * es ueber das Groessensymbol. Passt die gewuenschte Groesse nicht auf den
 * Schirm (die Workbench dieser V4 laeuft auf 640x360), wird sie begrenzt und
 * das ausdruecklich gemeldet -- ein Fenster, das nicht passt, ist unbrauchbar,
 * und MUI rechnet beim Oeffnen mit SizeGadget mit einem Groessenbereich, den es
 * auf so einem Schirm nicht geben kann.
 *
 * MUI-Objekte werden ERST angefasst, wenn das Fenster wirklich offen ist: der
 * Zeilen-Haken fuer die Logliste wird erst danach scharf geschaltet
 * (s_gui_up), und das Ergebnis von MUIA_Window_Open wird geprueft. Vorher stand
 * s_gui_up direkt hinter dem Aufbau des Objektbaums.
 *
 * Aufruf:  BT Gamecontroller GUI [-quiet] [-nodebug] [-trace] [-nolog]
 *                                [-l <logdatei>] [-d <i2c-device>]
 *                                [-w <px>] [-h <px>]
 */

#include "v4_master.h"
#include "v4_proto.h"
#include "v4_state.h"
#include "v4_joyport.h"
#include "v4_debug.h"

#include "ApolloCrossDev_Debug.h"   /* Debug-UART der V4, siehe Projects/ApolloLib */
#include <libraries/mui.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/graphics.h>
#include <proto/muimaster.h>
#include <clib/alib_protos.h>

#include <intuition/screens.h>      /* struct Screen: Schirmgroesse fuer -w/-h */

#include <devices/timer.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#define MAKE_ID(a, b, c, d) ((ULONG)(a) << 24 | (ULONG)(b) << 16 | \
                             (ULONG)(c) << 8 | (ULONG)(d))
#ifndef IPTR
#define IPTR ULONG
#endif

#define GUI_VERSION   "0.2"
#define GUI_DATUM     "13.10.2026"
#define GUI_VER_STR   "$VER: BT Gamecontroller GUI " GUI_VERSION " (" GUI_DATUM ")"

/* Logfeld: hoechstens so viele Zeilen, dann wird die aelteste entfernt
 * (kein unbegrenztes Wachstum). */
#define GUI_LOG_MAX   200

/* Geraeteliste: begrenzt, damit ein voller Scan den Speicher nicht fuellt. */
#define GUI_DEV_MAX   32

/* Takt der Live-Aktualisierung. 200 ms ist schnell genug, um beim Druecken
 * zuzusehen, und laesst dem MUI-Task genug Zeit (2 Befehle je Takt). */
#define GUI_TICK_MS   200u

/* Jeder fuenfte Takt holt zusaetzlich Status und HID-Info (rund 1 s) -- und
 * schreibt das Lebenszeichen. */
#define GUI_SLOW_EVERY 5u

/*
 * Rand, den ein Fenster auf dem Schirm braucht (Rahmen und Titelleiste). Die
 * Vorgabe 640x400 wird damit verglichen: passt sie nicht, wird sie auf den
 * Schirm begrenzt (siehe gui_fenster_an_schirm).
 */
#define GUI_SCHIRM_RAND_W 16
#define GUI_SCHIRM_RAND_H 24

/* Wie lange auf Scanergebnisse bzw. CONNECTED gewartet wird, in Takten. */
#define GUI_SCAN_TICKS    40u     /* 8 s  */
#define GUI_CONNECT_TICKS 30u     /* 6 s  */

/* Sichtbare Breite der Portzeilen. Feste Breite, weil MUI die Mindestgroesse
 * eines Textes aus dem Inhalt rechnet: ungleiche Zeilenbreiten liessen die
 * Portgruppen bei jeder Aktualisierung zappeln. */
#define GUI_LINE_W 44u

/* ReturnIDs der MUI-Notifies */
#define ID_QUIT       1
#define ID_SCAN       2
#define ID_CONNECT    3
#define ID_DISCONN    4
#define ID_FORGET     5
#define ID_MODE0      6
#define ID_MODE1      7
#define ID_SWAP       8
#define ID_REFRESH    9

struct IntuitionBase *IntuitionBase;
struct GfxBase       *GfxBase;
struct Library       *MUIMasterBase;

/*
 * Der Master und der zuletzt gueltige Zustand. Beides ortsfest (`static`): der
 * Zustandsblock ist ein paar Kilobyte gross. Auf dem Amiga hat ein CLI-Prozess
 * nur rund 4 KB Stapel -- ein solcher Block auf dem Stapel waere ein sicherer
 * Absturz.
 *
 * Beides gehoert DIESEM Faden (dem MUI-Task); es gibt keinen zweiten mehr.
 */
static v4_state_t  S;
static v4_master_t M;

/*
 * Die DIREKT gelesenen Joystick-Anschluesse (v4_joyport.c).
 *
 * Sie stehen NEBEN dem Zustandsblock: S haelt, was der ESP32 ueber I2C
 * gemeldet hat, J haelt, was die V4 an ihren eigenen Registern sieht. Beides
 * wird im Takt gelesen und in der Anzeige gegenuebergestellt -- stimmen die
 * Quellen ueberein, ist die Kette ESP32 -> DB9 -> V4 in Ordnung.
 *
 * Auch J gehoert DIESEM Faden; es gibt keinen zweiten.
 */
static v4_joy_t    J;

/* 1 = die Werte in S.input stammen aus DIESEM Takt (GET_INPUT hat
 * geantwortet). Nur dann gibt es ein Urteil "OK"/"ABWEICHUNG": gegen eine
 * ALTE Antwort zu vergleichen waere keine Messung. */
static int         s_input_frisch;

/*
 * Eigene Logdatei der Oberflaeche.
 *
 * WARUM NICHT DIE VORGABE DER PLATTFORM: die ist "Programs:test/v4_joy.log" und
 * wird auch von der Konsole benutzt. Wer zuerst laeuft, ueberschreibt den
 * anderen - bei einem Startfehler der Oberflaeche sucht man den Grund dann in
 * einer Datei, in der schon das naechste Programm steht. Genau das ist passiert.
 */
#define GUI_LOG_VORGABE  "Programs:test/v4_gui.log"

static const char *s_log_path = GUI_LOG_VORGABE;

/*
 * Fenstergroesse. Bewusst kleiner als der Bildschirm einer V4 und jederzeit
 * einstellbar:
 *
 *   -w <breite>   -h <hoehe>      beim Aufruf
 *   mit der Maus                  ueber das Groessensymbol (SizeGadget)
 *
 * WARUM NICHT FEST: 800x520 war zu gross - auf einem 640x480-Schirm passt das
 * Fenster nicht mehr vollstaendig auf den Schirm, und der Logbereich waechst
 * mit dem Text mit. Jetzt ist die Vorgabe kleiner, und wer mehr Platz will,
 * zieht das Fenster auf oder gibt die Groesse beim Aufruf an.
 */
static int s_fenster_w = 640;
static int s_fenster_h = 400;
static int s_schirm_w;                 /* 0 = nicht ermittelt */
static int s_schirm_h;
static int s_fenster_geklemmt;         /* 1 = auf den Schirm begrenzt */
static const char *s_dev_path;        /* NULL = Vorgabe der Plattform  */
static int     s_debug_uart = 1;      /* -nodebug schaltet den Debug-UART ab */
static int     s_trace = 0;           /* -trace: jeden Rahmen mitschreiben */
static int     s_use_log = 1;         /* -nolog: keine Logdatei */
static int     s_quiet = 0;           /* -quiet: keine Zeile je Transaktion */

static Object *s_app;
static Object *s_win;
static Object *s_link;                /* Verbindungszeile (Ausfallhinweis) */
static Object *s_info;                /* Kopfzeile                        */
static Object *s_ctrl;                /* Controller-Zeile                 */
static Object *s_state;               /* Zustandszeile                    */
static Object *s_port0;               /* Portgruppe 0                     */
static Object *s_port1;               /* Portgruppe 1                     */
static Object *s_vergleich;           /* direkt <-> I2C, beide Buchsen    */
static Object *s_mode0;               /* Cycle Betriebsart Buchse 0       */
static Object *s_mode1;               /* Cycle Betriebsart Buchse 1       */

/*
 * Die Eintraege der beiden Cycle-Knoepfe hinter "Buchse 0:" / "Buchse 1:".
 *
 * MUI erwartet hier ein NULL-ABGESCHLOSSENES ARRAY von Zeichenketten
 * (STRPTR *), KEINE mit '|' getrennte Zeichenkette. Belege:
 *
 *   Compilers/GCC-6.50-Latest/m68k-amigaos/include/libraries/mui.h:3098
 *       #define MUIA_Cycle_Entries  0x80420629   -- Typ laut Kommentar dort:
 *       "V4  is. STRPTR *"
 *   Compilers/GCC-6.50-Latest/m68k-amigaos/doc/MUI_Cycle.doc,
 *   MUIA_Cycle_Entries: "You must supply a pointer to a string array,
 *   containing one entry for each item and terminated with a NULL."
 *
 * Stand dort eine Zeichenkette ("Aus|Joystick|Maus"), las MUI die ersten vier
 * Bytes als Zeiger auf ein Array: es kamen keine Eintraege heraus. Das Feld
 * war LEER und -- weil MUI seine Breite aus den Eintraegen rechnet -- ueber
 * die ganze Spaltenbreite gezogen. Am Geraet gesehen am 13.10.2026.
 *
 * Die Reihenfolge ist die der Betriebsarten (V4P_PORT_OFF = 0,
 * V4P_PORT_JOY = 1, V4P_PORT_MOUSE = 2) und damit genau der Wert, den
 * GET_PORT_MODE liefert und den der Knopf ueber SET_PORT_MODE setzt. Die
 * Namen sind dieselben wie in v4p_port_mode_name().
 *
 * Das Feld ist ABSICHTLICH ortsfest (static): MUI merkt sich den Zeiger auf
 * das Array. Ein Puffer auf dem 4-KB-Stapel waere hier die klassische Falle.
 */
static const char *const gui_modus_eintraege[] = {
    "aus", "Joystick", "Maus", NULL
};

/*
 * Sicherung zur UEBERSETZUNGSZEIT: das Array muss mit NULL enden -- MUI liest
 * die Eintraege bis NULL. Wer den Abschluss entfernt, bekommt hier einen
 * Uebersetzungsfehler statt eines leeren Auswahlfelds auf der V4.
 * (V4P_PORT_MODE_MAX ist die letzte Betriebsart "Maus"; dazu kommen die
 * Betriebsart 0 "aus" und der NULL-Abschluss.)
 */
typedef char gui_modus_eintraege_mit_null[
    (sizeof(gui_modus_eintraege) / sizeof(gui_modus_eintraege[0])
     == (V4P_PORT_MODE_MAX + 2u)) ? 1 : -1];

static Object *s_devs;                /* Listview-Liste der Geraete       */
static Object *s_devs_lv;             /* deren Listview (Doppelklick)     */
static Object *s_logl;                /* Listview-Liste der Meldungen     */
static Object *s_live;                /* Checkmark Live-Aktualisierung    */
static int     s_gui_up;              /* 1 = MUI-Objekte existieren       */

static char   *s_loglines[GUI_LOG_MAX];
static int     s_logcount;

/* Eigene Kopien je Listenzeile: MUI behaelt nur den ZEIGER auf den Eintrag.
 * Mit einem lokalen Puffer zeigten alle Zeilen auf denselben Speicher. */
static char   *s_devlines[GUI_DEV_MAX];
static v4p_dev_t s_devtab[GUI_DEV_MAX];
static int     s_devn;

/* ------------------------------------------------------------------ */
/* Laufende Vorgaenge                                                  */
/*                                                                     */
/* Der Scan laeuft auf dem ESP32 asynchron, und CONNECT ist nur         */
/* ANGENOMMEN -- beides wird ueber mehrere Takte verfolgt. Es gibt aber  */
/* keine Auftragsnummern und keine Warteschlange mehr: der Takt ruft    */
/* v4_master direkt.                                                    */
/* ------------------------------------------------------------------ */

static unsigned s_scan_left;        /* verbleibende Takte fuer den Scan   */
static unsigned s_connect_left;     /* verbleibende Takte fuer CONNECTED  */
static unsigned s_tick;             /* Taktzaehler                        */
static unsigned s_seit_versuch;     /* Takte seit dem letzten Buszugriff  */
static int      s_live_on = 1;      /* Live-Aktualisierung an            */

/*
 * 1 = die Geraeteliste wurde fuer die LAUFENDE Verbindung schon automatisch
 * geholt. Eine Verbindung, die der ESP32 von selbst aufbaut (BluePad32
 * verbindet, sobald der Controller werbend auftritt), tauchte sonst in der
 * Liste nie auf: die wurde nur vom Knopf "Neu suchen" gefuellt. Genau das war
 * der zweite Teil des Befunds "die Auswahl-/Anzeigefelder bleiben leer".
 */
static int      s_liste_auto;

static int      s_link_down_shown;  /* 0/1: was zuletzt im Log stand      */
static int      s_sd_warned;        /* Audio-Firmware-Warnung einmalig    */

/* Zuletzt gemeldeter Widerspruch zwischen Zug und Pegel, je Maske. Damit die
 * Logliste nicht in jedem Takt dieselbe Warnung bekommt. 0xFF ist der
 * Sonderwert "Firmware liefert gar keine Pegel" -- die wird genau einmal
 * gemeldet. */
static uint8_t  s_level_warn_missing;
static uint8_t  s_level_warn_impossible;

/* Zuletzt GEMELDETER Stand des Vergleichs direkt <-> I2C: je Buchse das
 * Abweichungsurteil und die direkte Maske. Nur wenn sich einer der beiden Werte
 * aendert, geht sofort eine Zeile heraus (gui_joy_aenderungen). */
static uint8_t  s_joy_abw_alt[V4P_PORT_COUNT];
static uint8_t  s_joy_dir_alt[V4P_PORT_COUNT];
static int      s_joy_stand_gueltig;

/* ------------------------------------------------------------------ */
/* Ausgabe                                                            */
/* ------------------------------------------------------------------ */

/* Eine Zeile in die MUI-Logliste (Kopie, damit MUI den Zeiger behalten darf). */
static void gui_loglist_add(const char *s)
{
    char *copy;

    if (!s_gui_up || s_logl == NULL) {
        return;
    }
    if (s_logcount >= GUI_LOG_MAX) {
        /* Nur die aelteste Zeile entfernen: MUIM_List_Clear wuerde die
         * Scrollposition verwerfen, danach laesst sich die Liste nicht mehr
         * rollen (Befund aus dem Audio-Projekt). */
        set(s_logl, MUIA_List_Quiet, TRUE);
        DoMethod(s_logl, MUIM_List_Remove, 0);
        set(s_logl, MUIA_List_Quiet, FALSE);
        free(s_loglines[0]);
        memmove(&s_loglines[0], &s_loglines[1],
                sizeof(s_loglines[0]) * (size_t)(s_logcount - 1));
        s_logcount--;
    }
    copy = (char *)malloc(strlen(s) + 1u);
    if (copy == NULL) {
        return;
    }
    (void)strcpy(copy, s);
    s_loglines[s_logcount++] = copy;
    set(s_logl, MUIA_List_Quiet, TRUE);
    DoMethod(s_logl, MUIM_List_InsertSingle, copy, MUIV_List_Insert_Bottom);
    set(s_logl, MUIA_List_Quiet, FALSE);
}

/* Alle Meldungen des Programms laufen hier durch.
 *
 * Sie gehen ueber v4_dbg_zeile() an dieselben drei Senken wie die Debugausgabe
 * (stdout, Debug-UART der V4, Logdatei). Die MUI-Logliste bekommt dieselbe
 * Zeile ueber den Zeilen-Haken von v4_debug.c -- so gibt es genau EINEN Weg
 * und keinen doppelten Eintrag. */
static void gui_msg(const char *fmt, ...)
{
    static char buf[240];
    va_list ap;
    int     n;

    va_start(ap, fmt);
    n = vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    if (n <= 0) {
        return;
    }
    if (n > (int)sizeof(buf) - 1) {
        n = (int)sizeof(buf) - 1;
    }
    v4_dbg_zeile(buf);
}

/* Der Trace-Haken ist v4_dbg_trace: er laeuft in DIESEM Faden (die
 * Transaktionen laufen ja hier), also darf er auch die MUI-Liste fuellen --
 * genau das tut der Zeilen-Haken oben. */

/* ------------------------------------------------------------------ */
/* Textbausteine fuer die Anzeige                                      */
/* ------------------------------------------------------------------ */

/* Kleiner Puffer fuer mehrzeilige MUI-Texte. */
typedef struct {
    char  *buf;
    size_t cap;
    size_t len;
} gui_tb_t;

static void tb_init(gui_tb_t *t, char *buf, size_t cap)
{
    t->buf = buf;
    t->cap = cap;
    t->len = 0u;
    if (cap > 0u) {
        buf[0] = '\0';
    }
}

static void tb_add(gui_tb_t *t, const char *s)
{
    size_t n;

    if (t->buf == NULL || t->cap == 0u) {
        return;
    }
    n = strlen(s);
    if (t->len + n + 1u > t->cap) {
        return;                     /* lieber nichts als ein halber Text */
    }
    memmove(t->buf + t->len, s, n);
    t->len += n;
    t->buf[t->len] = '\0';
}

/* Eine Zeile: sichtbarer Text wird auf feste Breite aufgefuellt und bekommt
 * den Stil vorangestellt. */
static void tb_line(gui_tb_t *t, int bold, const char *fmt, ...)
{
    char    tmp[GUI_LINE_W + 24u];
    va_list ap;
    int     n;
    size_t  fill;

    va_start(ap, fmt);
    n = vsnprintf(tmp, sizeof(tmp), fmt, ap);
    va_end(ap);
    if (n < 0) {
        return;
    }
    if ((size_t)n > sizeof(tmp) - 1u) {
        n = (int)sizeof(tmp) - 1u;
    }
    /* Auf feste Sichtbreite auffuellen: MUI rechnet die Mindestgroesse des
     * Textes aus dem Inhalt, ungleiche Zeilen liessen die Gruppe zappeln. */
    fill = (size_t)n;
    while (fill < (size_t)GUI_LINE_W && fill + 1u < sizeof(tmp)) {
        tmp[fill++] = ' ';
    }
    tmp[fill] = '\0';

    tb_add(t, bold ? "\33b" : "\33n");
    tb_add(t, tmp);
    tb_add(t, "\n");
}

static void caps_text(uint16_t caps, char *out, size_t cap)
{
    static const struct {
        uint16_t    bit;
        const char *name;
    } tab[] = {
        { V4P_CAP_PORT_MODE, "Port-Modus" },
        { V4P_CAP_GET_INPUT, "Eingaenge" },
        { V4P_CAP_GET_HID,   "HID-Info" },
        { V4P_CAP_HID_BT,    "BT" },
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
        const char *keine = "(keine)";

        for (n = 0u; keine[n] != '\0' && n + 1u < cap; n++) {
            out[n] = keine[n];
        }
    }
    out[n] = '\0';
}

/* ------------------------------------------------------------------ */
/* Anzeige aktualisieren                                               */
/*                                                                     */
/* Alle Funktionen hier LESEN NUR den Zustandsblock -- kein I2C, kein    */
/* Warten. Ist ein Wert (noch) nicht gueltig, bleibt die alte Anzeige    */
/* stehen: genau das ist die Zusage "die letzten gueltigen Werte         */
/* bleiben sichtbar".                                                    */
/* ------------------------------------------------------------------ */

/* Verbindungszeile: die Aussage, die beim Ausfall zuerst dastehen muss.
 *
 * Sie liest NUR den Zustandsblock -- kein I2C. Die Abfrage selbst macht der
 * Takt (gui_poll). "down" heisst: der letzte Buszugriff hat keine gueltige
 * Antwort gebracht. */
static void gui_show_link(void)
{
    static char buf[360];               /* ortsfest (4-KB-Stapel) */
    int         down;

    down = (S.link_ok == 0) ? 1 : 0;

    if (S.link_ok == 0) {
        (void)snprintf(buf, sizeof(buf),
                       "%s -- die zuletzt gueltigen Werte bleiben stehen. "
                       "Naechster Versuch in %lu ms.",
                       (S.err[0] != '\0') ? S.err : "ESP32 antwortet nicht",
                       S.gap_ms);
    } else if (S.info_valid && !S.info_checked) {
        /* Die Leitung steht, aber der Partner meldet andere Rahmengroessen
         * oder eine andere Protokollversion: die Werte darunter sind dann
         * NICHT zu gebrauchen. */
        (void)snprintf(buf, sizeof(buf),
                       "Firmware meldet ein anderes Protokoll "
                       "(andere Rahmengroessen) -- bitte den ESP32 auf den "
                       "passenden Stand flashen. Die Werte unten sind "
                       "unsicher.");
    } else {
        (void)snprintf(buf, sizeof(buf),
                       "Verbindung steht (Adresse 0x%02X)."
                       "%s",
                       (unsigned)V4_I2C_ADDR7,
                       (S.fail_streak > 0u)
                           ? " Vorherige Versuche sind fehlgeschlagen."
                           : "");
    }

    if (s_link != NULL) {
        set(s_link, MUIA_Text_Contents, (IPTR)buf);
    }
    /* Den Wechsel einmal ins Log -- nicht in jedem Takt. */
    if (down != s_link_down_shown) {
        s_link_down_shown = down;
        gui_msg("[verbindung] %s\n", buf);
    }
}

static void gui_show_info(void)
{
    v4p_info_t  info;
    char        caps[160];
    static char buf[420];

    if (!S.info_valid) {
        return;                 /* noch nichts empfangen: alte Anzeige bleibt */
    }
    info = S.info;
    caps_text(info.caps, caps, sizeof(caps));
    (void)snprintf(buf, sizeof(buf),
                   "Protokoll %u / Firmware %u, Rahmen %u -> %u Byte, "
                   "Adresse 0x%02X, Geraete max. %u\n"
                   "Faehigkeiten 0x%04X: %s\n"
                   "Bulk-Weg: %s",
                   (unsigned)info.proto_ver, (unsigned)info.fw_ver,
                   (unsigned)info.write_frame_len,
                   (unsigned)info.read_frame_len,
                   (unsigned)V4_I2C_ADDR7, (unsigned)info.max_devices,
                   (unsigned)info.caps, caps,
                   (info.bulk_payload_max == 0u)
                       ? "nein (kein Dateizugriff)"
                       : "ja");
    if (s_info != NULL) {
        set(s_info, MUIA_Text_Contents, (IPTR)buf);
    }
}

static void gui_show_status(void)
{
    v4p_status_t st;
    static char  buf[260];

    if (!S.status_valid) {
        return;
    }
    st = S.status;
    (void)snprintf(buf, sizeof(buf),
                   "Zustand %u (%s), Verbindung %s, Scan %s, Geraete %u, "
                   "Scan-Generation %u",
                   (unsigned)st.state, v4p_bt_state_name(st.state),
                   (st.conn_index == V4P_NO_INDEX) ? "keine" : "aktiv",
                   st.scan_active ? "laeuft" : "aus",
                   (unsigned)st.dev_count, (unsigned)st.scan_gen);
    if (s_state != NULL) {
        set(s_state, MUIA_Text_Contents, (IPTR)buf);
    }
    /* Ein Antwortrahmen mit SD-/Audiofeldern kommt von der Firmware des
     * Audio-Projekts. Das gehoert gesagt -- sonst sucht man den Fehler in
     * unserer Anzeige. Genau EINMAL: die Zeile in jedem Takt zu wiederholen
     * macht das Logfeld unbrauchbar. */
    if (s_sd_warned == 0
        && (st.sd_mounted != 0u || st.audio_flags != 0u
            || st.sd_card_present != 0u || st.sd_free_kb != 0u)) {
        s_sd_warned = 1;
        gui_msg("[warnung] Antwort enthaelt SD-/Audiofelder "
                "(mounted=%u audio=0x%02X) -- das ist die Audio-Firmware, "
                "nicht der Joystick-Adapter\n",
                (unsigned)st.sd_mounted, (unsigned)st.audio_flags);
    }
}

static void gui_show_hid(void)
{
    v4p_hid_t   hid;
    static char buf[420];
    char        bat[48];

    if (!S.hid_valid) {
        return;                 /* noch nichts empfangen: alte Anzeige bleibt */
    }
    hid = S.hid;
    if (hid.kind == V4P_HID_KIND_NONE) {
        (void)snprintf(buf, sizeof(buf),
                       "Controller: keiner verbunden\n"
                       "(Pad oder Maus am ESP32 verbinden; die Anzeige folgt "
                       "automatisch)");
        if (s_ctrl != NULL) {
            set(s_ctrl, MUIA_Text_Contents, (IPTR)buf);
        }
        return;
    }

    if (hid.battery == V4P_HID_BATTERY_UNKNOWN) {
        (void)snprintf(bat, sizeof(bat), "Batterie unbekannt");
    } else {
        (void)snprintf(bat, sizeof(bat), "Batterie %u %%",
                       (unsigned)hid.battery);
    }

    /*
     * Drei Zeilen, und die wichtigen Angaben stehen VORN: das Feld darf
     * hoechstens 640 Pixel breit sein (Fensterbreite), und Name, Art,
     * Transport und VID:PID sind die Aussage, die man beim Draufschauen
     * braucht. Die Zaehler dahinter sind Beiwerk und duerfen am Rand
     * abgeschnitten werden.
     */
    (void)snprintf(buf, sizeof(buf),
                   "Controller: %s%s\n"
                   "Art %s, Transport %s, VID:PID %04X:%04X\n"
                   "Report-ID %u, Reportlaenge %u Byte, Reports %u, %s",
                   (hid.name[0] != '\0') ? hid.name : "(ohne Namen)",
                   ((hid.flags & V4P_FLAG_NAME_TRUNCATED) != 0u)
                       ? " (Name gekuerzt)" : "",
                   v4p_hid_kind_name(hid.kind),
                   v4p_transport_name(hid.transport),
                   (unsigned)hid.vid, (unsigned)hid.pid,
                   (unsigned)hid.report_id, (unsigned)hid.report_len,
                   (unsigned)hid.reports, bat);
    if (s_ctrl != NULL) {
        set(s_ctrl, MUIA_Text_Contents, (IPTR)buf);
    }
}

/*
 * Eine Portgruppe zusammensetzen.
 *
 * `drive` ist die Zug-Maske, `level` der GEMESSENE Pegel aus GET_INPUT,
 * `level_valid` sagt, ob die Firmware ihn ueberhaupt geliefert hat. Ohne Pegel
 * (10-Byte-Fassung) steht in der Spalte ein Hinweis und KEINE Zahl -- eine
 * abgeleitete Zahl waere von einer Messung nicht zu unterscheiden.
 *
 * Der Widerspruch je Leitung ("losgelassen, liest aber 0" = kein Pull-up)
 * wird direkt in der Zeile benannt: das ist am Port die Aussage, auf die es
 * ankommt, und ohne sie sieht der Befund wie "nichts gedrueckt" aus.
 */
static void gui_build_port_text(int port, uint8_t mode, uint8_t drive,
                                uint8_t level, int level_valid,
                                char *buf, size_t cap)
{
    gui_tb_t  t;
    char      gezogen[64];
    unsigned  bit;

    v4p_bits_text(drive, gezogen, sizeof(gezogen));

    tb_init(&t, buf, cap);
    tb_line(&t, 0, "Buchse %d  Modus %s", port, v4p_port_mode_name(mode));
    tb_line(&t, 0, "Leitung DB9   Zug  Pegel  Bewertung");
    if (!level_valid) {
        tb_line(&t, 0, "Pegel: Firmware ohne Pegel -- neu flashen");
    }
    for (bit = 0u; bit < V4P_IN_ST_COUNT; bit++) {
        uint8_t maske = (uint8_t)(1u << bit);
        int     z     = (drive & maske) != 0u;
        int     p     = (level & maske) != 0u;
        const char *urteil;

        if (!level_valid) {
            urteil = "";
        } else if (z && !p) {
            urteil = "ok";
        } else if (!z && p) {
            urteil = "ok";
        } else if (!z && !p) {
            urteil = "<- kein Pull-up!";
        } else {
            urteil = "<- unmoeglich!";
        }

        tb_line(&t, z, "%-7s %-5s %-4s %-6s %s",
                v4p_bit_name(bit), v4p_bit_pin(bit),
                z ? "GND" : "frei",
                level_valid ? (p ? "High" : "Low") : "-",
                urteil);
    }
    tb_line(&t, 0, "gezogen: %s", gezogen);
    if (level_valid) {
        tb_line(&t, 0, "Pegel = gemessen (ESP32, GET_INPUT)");
    } else {
        tb_line(&t, 0, "Pegel = unbekannt (10-Byte-Fassung)");
    }
}

/*
 * Der Vergleich direkt <-> I2C in der Anzeige: eine Zeile je Buchse und die
 * Rohwerte der Feuerregister. Er steht in einem EIGENEN Textobjekt ueber die
 * volle Fensterbreite -- in den beiden Portspalten waere die Zeile zu breit und
 * wuerde das Fenster auseinanderziehen.
 *
 * Er wird IMMER gezeichnet, sobald irgendetwas vorliegt: die direkte Abfrage
 * haengt nicht am Bus und muss auch dann sichtbar sein, wenn der ESP32
 * schweigt. Fehlt eine Quelle, steht das ausdruecklich in der Zeile.
 */
static void gui_show_vergleich(void)
{
    static char buf[3u * V4_JOY_TEXT_MAX];  /* ortsfest (4-KB-Stapel!) */

    if (s_vergleich == NULL) {
        return;
    }
    (void)v4_joy_vergleich_text(&J, (S.input_valid != 0) ? &S.input : NULL,
                                s_input_frisch, buf, sizeof(buf));
    set(s_vergleich, MUIA_Text_Contents, (IPTR)buf);
}
/* Portmodi und Eingaenge aus dem Zustandsblock in die Anzeige uebernehmen. */
static void gui_show_ports(void)
{
    uint8_t     mode[V4P_PORT_COUNT];
    uint8_t     applied[V4P_PORT_COUNT];
    v4p_input_t in;
    static char buf[800];               /* ortsfest (4-KB-Stapel!) */

    /* Zuerst der Vergleich: er haengt NICHT daran, ob schon eine gueltige
     * I2C-Antwort da ist. */
    gui_show_vergleich();

    if (!S.mode_valid) {
        return;                 /* noch nichts empfangen: alte Anzeige bleibt */
    }
    if (!S.input_valid) {
        return;
    }
    mode[0]    = S.mode[0];
    mode[1]    = S.mode[1];
    applied[0] = S.applied[0];
    applied[1] = S.applied[1];
    in         = S.input;

    if (s_port0 != NULL) {
        gui_build_port_text(0, mode[0], in.state[0], in.level[0],
                            in.level_valid, buf, sizeof(buf));
        set(s_port0, MUIA_Text_Contents, (IPTR)buf);
    }
    if (s_port1 != NULL) {
        gui_build_port_text(1, mode[1], in.state[1], in.level[1],
                            in.level_valid, buf, sizeof(buf));
        set(s_port1, MUIA_Text_Contents, (IPTR)buf);
    }

    /* Den Widerspruch zusaetzlich in die Logliste -- man sieht ihn dort auch
     * dann, wenn das Fenster nicht im Blick ist. Nur bei Aenderung, sonst
     * liefe die Liste im Takt voll. */
    if (in.level_valid) {
        uint8_t fehlt   = (uint8_t)(v4p_level_missing_pullup(in.state[0],
                                                             in.level[0])
                                    | v4p_level_missing_pullup(in.state[1],
                                                               in.level[1]));
        uint8_t unmoegl = (uint8_t)(v4p_level_impossible(in.state[0],
                                                         in.level[0])
                                    | v4p_level_impossible(in.state[1],
                                                           in.level[1]));

        if (fehlt != s_level_warn_missing || unmoegl != s_level_warn_impossible) {
            char txt[64];

            s_level_warn_missing    = fehlt;
            s_level_warn_impossible = unmoegl;

            if (unmoegl != 0u) {
                v4p_bits_text(unmoegl, txt, sizeof(txt));
                gui_msg("[warnung] %s wird nach GND gezogen, liest aber High "
                        "-- elektrisch unmoeglich (Ausgangsstufe/Pinbelegung "
                        "pruefen)\n", txt);
            }
            if (fehlt != 0u) {
                v4p_bits_text(fehlt, txt, sizeof(txt));
                gui_msg("[warnung] %s losgelassen, liest aber 0 -- kein "
                        "Pull-up (Platine haengt nicht am Port?)\n", txt);
            }
            if (fehlt == 0u && unmoegl == 0u) {
                gui_msg("[pruefung] Pull-up wirkt: gezogene Leitungen Low, "
                        "losgelassene High\n");
            }
        }
    } else if (s_level_warn_missing != 0xFFu) {
        /* Einmal melden, dann nicht jeden Takt wiederholen. */
        s_level_warn_missing    = 0xFFu;
        s_level_warn_impossible = 0xFFu;
        gui_msg("[warnung] die Firmware liefert keine Pegel (10-Byte-Fassung "
                "von GET_INPUT) -- im Spaltenkopf steht deshalb ein Hinweis "
                "statt einer Zahl\n");
    }

    /* Cycle-Knoepfe auf den zuletzt BESTAETIGTEN Modus ziehen (aus dem
     * Zustandsblock, nicht aus der Anzeige) -- und nur bei echter Aenderung:
     * jedes set() loest sonst eine Notify und damit einen unnoetigen Befehl
     * aus (klassische MUI-Rueckkopplung). */
    if (s_mode0 != NULL) {
        IPTR jetzt = 0;

        (void)get(s_mode0, MUIA_Cycle_Active, &jetzt);
        if ((uint8_t)jetzt != applied[0]) {
            set(s_mode0, MUIA_Cycle_Active, (IPTR)applied[0]);
        }
    }
    if (s_mode1 != NULL) {
        IPTR jetzt = 0;

        (void)get(s_mode1, MUIA_Cycle_Active, &jetzt);
        if ((uint8_t)jetzt != applied[1]) {
            set(s_mode1, MUIA_Cycle_Active, (IPTR)applied[1]);
        }
    }
}

/* Langsamer Takt: Zustand und Controller-Info stammen aus dem Zustandsblock --
 * der Auftrag dafuer (POLL_SLOW) wird im Takt abgesetzt, hier wird nur
 * gezeichnet. */
static void gui_show_slow(void)
{
    gui_show_status();
    gui_show_hid();
}

/* ------------------------------------------------------------------ */
/* Liste der gefundenen Geraete                                        */
/* ------------------------------------------------------------------ */

static void gui_devices_clear(void)
{
    int i;

    if (s_devs != NULL) {
        DoMethod(s_devs, MUIM_List_Clear);
    }
    for (i = 0; i < s_devn; i++) {
        free(s_devlines[i]);
        s_devlines[i] = NULL;
    }
    s_devn = 0;
}

/*
 * Die Geraeteliste aus dem Zustandsblock in die Listview uebernehmen.
 *
 * Die Liste hat der Takt eingesammelt (gui_scan_tick) -- hier werden nur noch
 * die Zeilenkopien erzeugt, die MUI behalten darf. Die Tabelle ist ortsfest:
 * 16 Geraete a 50 Byte Namen sind auf dem 4-KB-Stapel nichts, was man
 * riskieren muss.
 */
static void gui_devices_fill(void)
{
    static v4p_dev_t devbuf[V4P_MAX_DEVICES];
    uint8_t          n;
    uint8_t          i;

    gui_devices_clear();
    if (S.dev_n == 0u) {
        return;
    }
    n = S.dev_n;
    for (i = 0u; i < n && i < (uint8_t)V4P_MAX_DEVICES; i++) {
        devbuf[i] = S.devs[i];
    }
    for (i = 0u; i < n && s_devn < GUI_DEV_MAX; i++) {
        char    line[V4P_NAME_BUF + 32u];
        char   *copy;
        size_t  len;

        (void)snprintf(line, sizeof(line),
                       "%s%s  (%02X:%02X:%02X:%02X:%02X:%02X)",
                       devbuf[i].name,
                       ((devbuf[i].flags & V4P_FLAG_NAME_TRUNCATED) != 0u)
                           ? "..." : "",
                       devbuf[i].bda[0], devbuf[i].bda[1], devbuf[i].bda[2],
                       devbuf[i].bda[3], devbuf[i].bda[4], devbuf[i].bda[5]);
        len  = strlen(line);
        copy = (char *)malloc(len + 1u);
        if (copy == NULL) {
            break;
        }
        (void)strcpy(copy, line);
        s_devlines[s_devn] = copy;
        s_devtab[s_devn]   = devbuf[i];
        s_devn++;
        DoMethod(s_devs, MUIM_List_InsertSingle, copy, MUIV_List_Insert_Bottom);
    }
}

/* ------------------------------------------------------------------ */
/* Aktionen                                                            */
/*                                                                     */
/* Jede Aktion ruft v4_master DIREKT aus dem MUI-Task. Kein Auftrag,    */
/* kein Nachrichtenport. Antwortet der ESP32 nicht, steht das im        */
/* Zustandsblock, und die Anzeige meldet es.                           */
/* ------------------------------------------------------------------ */

/* ------------------------------------------------------------------ */
/* Direkte Abfrage der Joystick-Anschluesse                            */
/*                                                                     */
/* Die V4 liest ihre EIGENEN Register (JOY0DAT/JOY1DAT, CIAAPRA,       */
/* POTINP) ueber v4_joyport.c -- ohne Bus, ohne Betriebssystemaufruf,   */
/* im selben Faden. Das ist der Vergleichsmassstab zur I2C-Antwort:     */
/* stimmen beide ueberein, ist die Kette ESP32 -> DB9 -> V4 in Ordnung. */
/* ------------------------------------------------------------------ */

/*
 * Kurzer Abstand zwischen den beiden Abtastungen einer Buchse.
 *
 * WARUM KEIN Delay(): ein Delay() ist auf dem Amiga ein Aufgabenwechsel (und
 * hier zusaetzlich ein timer.device-DoIO) -- das waere erstens viel zu lang und
 * zweitens genau die Naehe zum Buszugriff, die v4_debug.h ausschliesst. Eine
 * Zaehlschleife auf dem 68020 sind wenige Mikrosekunden; `volatile` verhindert,
 * dass der Compiler sie wegoptimiert.
 */
static void gui_joy_pause(void)
{
    volatile unsigned i;

    for (i = 0u; i < 200u; i++) {
        /* absichtlich leer: nur Zeit zwischen den beiden Abtastungen */
    }
}

/*
 * ZWEI Abtastungen mit kurzem Abstand und daraus den Stand fortschreiben.
 *
 * Nur wenn beide Abtastungen denselben Zaehlerwert liefern, wird der Wert
 * uebernommen; sonst bleibt der letzte gueltige stehen (v4_joy_update). Der
 * Zaehler der V4 liefert beim Umschalten Zwischenwerte -- ohne dieses Tor
 * wuerde die Anzeige flackern.
 */
static void gui_joy_abtasten(void)
{
    v4_joy_roh_t a;
    v4_joy_roh_t b;

    v4_joy_lesen(&a);
    gui_joy_pause();
    v4_joy_lesen(&b);
    v4_joy_update(&J, &a, &b);
}

/*
 * Die Vergleichszeile EINMAL auf den DebugUART -- und damit ueber v4_debug.c
 * zugleich auf stdout und in die Logdatei.
 *
 * Sie geht BEWUSST auch mit -quiet heraus: sie ist der Zweck der Direktabfrage.
 * -quiet schaltet die Zeile je I2C-Transaktion ab, nicht diese.
 *
 * Sie wird NICHT mehr in jedem Takt geschrieben (das waren 5 Zeilen je
 * Sekunde), sondern nur noch, wenn sich etwas geaendert hat: Richtung,
 * Feuerknopf oder das Urteil direkt <-> I2C. Im Ruhezustand traegt die eine
 * Herzzeile je Sekunde denselben Vergleich (gui_joy_herz_zusatz).
 */
static void gui_joy_vergleich_zeile(void)
{
    static char buf[V4_JOY_TEXT_MAX + 2u];   /* ortsfest (4-KB-Stapel) */
    size_t      n;

    n = v4_joy_debug_text(&J, (S.input_valid != 0) ? &S.input : NULL,
                          s_input_frisch, buf, sizeof(buf));
    /* Eine FERTIGE Zeile, ein Aufruf je Senke: der Umbruch gehoert dazu,
     * sonst waeren es zwei Ausgaben fuer eine Meldung. */
    if (n + 1u < sizeof(buf)) {
        buf[n]     = '\n';
        buf[n + 1u] = '\0';
        v4_dbg_zeile(buf);
    }
}

/*
 * Denselben Vergleich OHNE Ausgabe an die naechste Herzzeile haengen: so bleibt
 * es bei EINER Zeile je Sekunde, und die enthaelt trotzdem beide Buchsen, das
 * Urteil und die Rohwerte von CIAAPRA/POTINP.
 */
static void gui_joy_herz_zusatz(void)
{
    static char buf[V4_JOY_TEXT_MAX];   /* ortsfest (4-KB-Stapel) */

    (void)v4_joy_herz_text(&J, (S.input_valid != 0) ? &S.input : NULL,
                           s_input_frisch, buf, sizeof(buf));
    v4_dbg_herz_zusatz(buf);
}

/*
 * SOFORT SCHREIBEN BEI AENDERUNG.
 *
 * Der Vergleich direkt <-> I2C ist der Zweck des Programms, und eine
 * Abweichung ist die Aussage, auf die es ankommt. Deshalb wird hier in JEDEM
 * Takt geprueft, ob sich der direkte Stand (Richtungen und Feuerknoepfe) oder
 * das Urteil einer Buchse geaendert hat -- und dann geht sofort EINE fertige
 * Zeile heraus, die beides nennt: den direkten Wert und den I2C-Wert samt
 * Rohwerten der Feuerregister.
 *
 * In dieselbe Zeile gehoert auch der Feuerknopf: welches Bit welche Taste ist,
 * steht nirgends fest (am Geraet bewegte sich beim Druecken nichts), es wird
 * also durch Draufschauen an den Rohwerten gefunden. Genau dafuer stehen
 * CIAAPRA und POTINP in der Zeile.
 *
 * Ohne Aenderung entsteht KEINE Zeile -- auch das ist eine Zusage: die
 * Ausgabe soll den Bus nicht bremsen.
 */
static void gui_joy_aenderungen(void)
{
    const v4p_input_t *in;
    int                port;
    int                neu = 0;

    in = (S.input_valid != 0) ? &S.input : NULL;
    for (port = 0; port < (int)V4P_PORT_COUNT; port++) {
        uint8_t abw = v4_joy_abweichung(&J, in, s_input_frisch, port);
        uint8_t dir = v4_joy_maske(&J, port);

        if (s_joy_stand_gueltig == 0
            || abw != s_joy_abw_alt[port]
            || dir != s_joy_dir_alt[port]) {
            neu = 1;
        }
        s_joy_abw_alt[port] = abw;
        s_joy_dir_alt[port] = dir;
    }
    s_joy_stand_gueltig = 1;

    if (neu != 0) {
        gui_joy_vergleich_zeile();
    }
}

/*
 * Vorwaertsdeklarationen: gui_poll (unten) meldet eine erkannte Verbindung an
 * die Geraeteliste weiter, die Funktionen selbst stehen weiter unten.
 */
static void act_scan_start(int automatisch);
static void gui_liste_auto(const v4p_status_t *st);

/*
 * Ein vollstaendiger Durchlauf: GET_PORT_MODE + GET_INPUT, auf Wunsch
 * zusaetzlich GET_STATUS + GET_HID (jeder fuenfte Takt, rund 1 s).
 *
 * Wichtig: der Verbindungszustand wird EINMAL mit dem schlechtesten
 * Rueckgabewert fortgeschrieben, nicht je Unterbefehl. Sonst zaehlte ein
 * einziger Ausfall die Fehlerserie dreifach hoch, und der Abstand waechst zu
 * schnell.
 */
static void gui_poll(int slow)
{
    uint8_t     mode[V4P_PORT_COUNT];
    v4p_input_t in;
    uint8_t     rc  = V4P_ST_OK;
    uint8_t     rc2;

    /*
     * Die EIGENE Hardware wird NICHT hier abgetastet, sondern im Takt
     * (gui_joy_abtasten) und zwar NACH diesem Aufruf. Zwei Gruende:
     *
     *   1. Der Takt kann auch dann abtasten, wenn die Live-Abfrage aus ist
     *      (die Registerabfrage kostet nichts und braucht keinen Bus).
     *   2. Im Mitschnitt steht damit die erste Buszeile VOR der ersten Zeile
     *      der neuen Direktabfrage. Stirbt das Programm im Takt, ist aus dem
     *      Log sofort zu sehen, WELCHER Teil es war.
     */
    rc2 = v4_get_port_mode(&M, mode);
    if (rc2 == V4P_ST_OK) {
        v4_state_put_modes(&S, mode);
        v4_dbg_modes(mode, "GET_PORT_MODE");
    } else {
        rc = rc2;
        v4_dbg_fehler("GET_PORT_MODE", rc2, &M);
    }

    rc2 = v4_get_input(&M, &in);
    if (rc2 == V4P_ST_OK) {
        v4_state_put_input(&S, &in);
        s_input_frisch = 1;
        v4_dbg_input(&in);
    } else {
        /* Achtung: S.input behaelt die letzten gueltigen Werte (das ist die
         * Zusage der Anzeige). "Frisch" sind sie damit NICHT -- der Vergleich
         * zeigt dann ausdruecklich keine Bewertung. */
        s_input_frisch = 0;
        if (rc == V4P_ST_OK) {
            rc = rc2;
            v4_dbg_fehler("GET_INPUT", rc2, &M);
        }
    }

    if (slow != 0) {
        v4p_status_t st;
        v4p_hid_t    hid;

        rc2 = v4_get_status(&M, &st);
        if (rc2 == V4P_ST_OK) {
            v4_state_put_status(&S, &st);
            v4_dbg_status(&st);
            v4_dbg_sitzung(&st);
            /* Steht eine Verbindung und ist die Auswahlliste noch leer, wird
             * sie genau einmal automatisch geholt (siehe gui_liste_auto). */
            gui_liste_auto(&st);
        } else if (rc == V4P_ST_OK) {
            rc = rc2;
            v4_dbg_fehler("GET_STATUS", rc2, &M);
        }

        rc2 = v4_get_hid(&M, &hid);
        if (rc2 == V4P_ST_OK) {
            v4_state_put_hid(&S, &hid);
            v4_dbg_hid(&hid);
        } else if (rc == V4P_ST_OK) {
            rc = rc2;
            v4_dbg_fehler("GET_HID", rc2, &M);
        }
    }

    /*
     * Die Zeile je Takt gibt es nicht mehr. Was jetzt kommt, ist in
     * gui_joy_aenderungen() bzw. in der Herzzeile enthalten:
     *
     *   - Aenderung von Richtung, Feuerknopf oder Urteil -> sofort EINE Zeile
     *     (gui_joy_aenderungen im Takt),
     *   - im Ruhezustand EINE Zeile je Sekunde mit demselben Vergleich
     *     (gui_joy_herz_zusatz + v4_dbg_herz im Takt).
     *
     * Damit sind es im Ruhezustand eine Zeile je Sekunde statt zwanzig.
     */

    (void)v4_state_note(&S, rc);
}

/*
 * "Neu suchen" startet nur den Scan. Das Einsammeln der Ergebnisse macht der
 * Takt (gui_scan_tick): der ESP32 scannt asynchron, und eine Warteschleife
 * wuerde das Fenster fuer Sekunden einfrieren.
 *
 * `automatisch` ist 1, wenn der Scan nicht vom Knopf kommt, sondern weil eine
 * Verbindung erkannt wurde und die Geraeteliste noch leer ist (gui_liste_auto).
 */
static void act_scan_start(int automatisch)
{
    uint8_t rc;

    gui_devices_clear();
    rc = v4_scan_start(&M, 8u, 1);
    (void)v4_state_note(&S, rc);
    if (rc != V4P_ST_OK) {
        gui_msg("[scan] SCAN_START: %s\n", v4_strerror(rc));
        return;
    }
    if (automatisch != 0) {
        gui_msg("[scan] Verbindung erkannt -- die Geraeteliste wird "
                "automatisch geholt (bis zu %u s)\n",
                (unsigned)(GUI_SCAN_TICKS * GUI_TICK_MS / 1000u));
    } else {
        gui_msg("[scan] Scan gestartet (bis zu %u s) -- die Liste fuellt "
                "sich waehrend der Live-Aktualisierung\n",
                (unsigned)(GUI_SCAN_TICKS * GUI_TICK_MS / 1000u));
    }
    s_scan_left = GUI_SCAN_TICKS;
}

/*
 * Eine erkannte Verbindung soll AUCH in der Geraeteliste stehen -- nicht nur
 * in der Controller-Zeile. Ohne das blieb die Auswahlliste leer, solange
 * niemand "Neu suchen" drueckte; im Mitschnitt vom 13.10.2026 gibt es keine
 * einzige [scan]-Zeile der Oberflaeche, obwohl sich der Controller zweimal von
 * selbst verbunden hat.
 *
 * Der Scan laeuft EINMAL je Verbindung und nur, wenn die Liste leer ist: das
 * ist derselbe Verkehr wie ein Knopfdruck und keine Dauerlast auf dem Bus.
 */
static void gui_liste_auto(const v4p_status_t *st)
{
    int verbunden;

    if (st == NULL) {
        return;
    }
    verbunden = (st->state == V4P_STATE_CONNECTED
                 || st->state == V4P_STATE_SUSPENDED) ? 1 : 0;
    if (verbunden == 0) {
        /* Fuer die naechste Verbindung wieder scharf schalten -- aber nur,
         * wenn die Liste leer blieb. Sonst loeste jedes Ab- und Anmelden
         * einen neuen Scan aus. */
        if (s_devn == 0) {
            s_liste_auto = 0;
        }
        return;
    }
    if (s_liste_auto != 0 || s_scan_left > 0u || s_devn > 0) {
        return;
    }
    s_liste_auto = 1;
    act_scan_start(1);
}

/* Eine Scan-Runde: DEV_COUNT; sind Geraete da, gleich die ganze Liste holen. */
static void gui_scan_tick(void)
{
    static v4p_dev_t tab[V4P_MAX_DEVICES];  /* ortsfest (4-KB-Stapel) */
    uint8_t count = 0u;
    uint8_t n     = 0u;
    uint8_t i;
    uint8_t rc;

    if (s_scan_left == 0u) {
        return;
    }
    s_scan_left--;

    rc = v4_dev_count(&M, &count);
    (void)v4_state_note(&S, rc);
    if (rc != V4P_ST_OK) {
        gui_msg("[scan] DEV_COUNT: %s\n", v4_strerror(rc));
        s_scan_left = 0u;
        return;
    }
    if (count == 0u) {
        if (s_scan_left == 0u) {
            gui_msg("[scan] keine Geraete gefunden. Ist der Controller "
                    "eingeschaltet und in Reichweite?\n");
        }
        return;
    }

    if (count > (uint8_t)V4P_MAX_DEVICES) {
        gui_msg("[scan] Slave meldet %u Geraete (Obergrenze dieser Anzeige "
                "%u) - lese nur %u\n", (unsigned)count,
                (unsigned)V4P_MAX_DEVICES, (unsigned)V4P_MAX_DEVICES);
        count = (uint8_t)V4P_MAX_DEVICES;
    }
    for (i = 0u; i < count; i++) {
        v4p_dev_t d;

        rc = v4_dev_get(&M, i, &d);
        (void)v4_state_note(&S, rc);
        if (rc != V4P_ST_OK) {
            continue;               /* einzelner Fehlschlag: ueberspringen */
        }
        if (d.name_len == 0u || d.name[0] == '\0') {
            gui_msg("[scan] Fund %u ohne Namen - uebersprungen\n",
                    (unsigned)i);
            continue;
        }
        tab[n++] = d;
    }
    v4_state_put_devs(&S, tab, n);
    v4_dbg_devs(tab, n);
    gui_devices_fill();
    gui_msg("[scan] %u Geraet(e) in der Liste\n", (unsigned)s_devn);
    s_scan_left = 0u;
}

/* Doppelklick in der Liste oder der Knopf: verbinden. */
static void act_connect(void)
{
    char  *sel = NULL;
    int    i;
    int    idx = -1;
    uint8_t rc;

    if (s_devs != NULL
        && DoMethod(s_devs, MUIM_List_GetEntry, MUIV_List_GetEntry_Active,
                    &sel) != 0
        && sel != NULL) {
        for (i = 0; i < s_devn; i++) {
            if (s_devlines[i] == sel) {
                idx = (int)s_devtab[i].idx;
                break;
            }
        }
    }
    if (idx < 0 && s_devn > 0) {
        idx = (int)s_devtab[0].idx;     /* nichts markiert: den ersten nehmen */
    }
    if (idx < 0) {
        gui_msg("[verbinden] keine Geraeteliste - erst \"Neu suchen\"\n");
        return;
    }
    rc = v4_connect(&M, (uint8_t)idx);
    (void)v4_state_note(&S, rc);
    if (rc != V4P_ST_OK) {
        /* Der Befehl kann den Slave trotzdem erreicht haben (Antwort
         * verloren). Deshalb weiter beobachten statt abzubrechen. */
        gui_msg("[verbinden] CONNECT: %s - beobachte den Zustand weiter\n",
                v4_strerror(rc));
    } else {
        gui_msg("[verbinden] Index %u angenommen - warte auf CONNECTED\n",
                (unsigned)idx);
    }
    s_connect_left = GUI_CONNECT_TICKS;
}

static void act_disconnect(void)
{
    uint8_t rc = v4_disconnect(&M);

    (void)v4_state_note(&S, rc);
    gui_msg("[trennen] DISCONNECT: %s\n", v4_strerror(rc));
    s_connect_left = 0u;
}

static void act_forget(void)
{
    uint8_t rc = v4_forget(&M);

    (void)v4_state_note(&S, rc);
    gui_msg("[vergessen] FORGET: %s (die laufende Verbindung bleibt)\n",
            v4_strerror(rc));
}

/*
 * Betriebsart setzen -- ausgeloest vom Cycle-Knopf oder vom Schaltknopf.
 *
 * Die Rueckkopplung: jedes set(MUIA_Cycle_Active) loest die Notify aus, auch
 * wenn die Anzeige nur den vom Slave gemeldeten Wert nachzieht. Deshalb wird
 * hier gegen den BESTAETIGTEN Modus aus dem Zustandsblock verglichen: stimmt
 * der angezeigte Wert damit ueberein, gibt es nichts zu tun.
 */
static void act_set_mode(int port)
{
    Object *cyc = (port == 0) ? s_mode0 : s_mode1;
    IPTR    v   = 0;
    uint8_t want;
    uint8_t applied;
    uint8_t bestaetigt = 0u;
    uint8_t rc;

    if (cyc == NULL) {
        return;
    }
    /* Ohne gueltige Antwort gibt es nichts zu vergleichen: der Wunsch wird
     * trotzdem geschickt, der Slave bestaetigt oder lehnt ab. */
    applied = (S.mode_valid != 0) ? S.applied[port] : (uint8_t)V4P_PORT_OFF;

    (void)get(cyc, MUIA_Cycle_Active, &v);
    want = (uint8_t)v;
    if (want > V4P_PORT_MODE_MAX) {
        want = (uint8_t)V4P_PORT_OFF;
    }
    if (want == applied) {
        return;         /* nur die Anzeige hat sich nachgezogen */
    }

    rc = v4_set_port_mode(&M, (uint8_t)port, want, &bestaetigt);
    (void)v4_state_note(&S, rc);
    if (rc != V4P_ST_OK) {
        gui_msg("[port%d] SET_PORT_MODE: %s\n", port, v4_strerror(rc));
        /* Die Anzeige auf den bestaetigten Stand zurueckziehen -- sonst zeigt
         * der Knopf eine Betriebsart, die es nicht gibt. */
        if (S.mode_valid != 0) {
            set(cyc, MUIA_Cycle_Active, (IPTR)S.applied[port]);
        }
        return;
    }
    v4_state_put_mode(&S, (uint8_t)port, bestaetigt);
    v4_dbg_modes(S.mode, "SET_PORT_MODE");
    gui_msg("[port%d] Modus %s bestaetigt\n", port,
            v4p_port_mode_name(bestaetigt));
}

static void act_swap(void)
{
    uint8_t now[V4P_PORT_COUNT];
    uint8_t rc = v4_swap_port_modes(&M, now);

    (void)v4_state_note(&S, rc);
    if (rc != V4P_ST_OK) {
        gui_msg("[tauschen] Buchsen tauschen: %s\n", v4_strerror(rc));
        return;
    }
    v4_state_put_modes(&S, now);
    v4_dbg_modes(now, "Buchsen getauscht");
    gui_msg("[tauschen] Buchse 0 = %s, Buchse 1 = %s\n",
            v4p_port_mode_name(now[0]), v4p_port_mode_name(now[1]));
}

/* "Aktualisieren": einen vollstaendigen Durchlauf sofort. Auch die EIGENE
 * Hardware wird dabei neu abgetastet -- der Knopf soll einen frischen Vergleich
 * zeigen und nicht den Stand des letzten Takts. */
static void act_refresh(void)
{
    gui_poll(1);
    gui_joy_abtasten();
    gui_joy_aenderungen();
    gui_joy_herz_zusatz();
    gui_show_link();
    gui_show_info();
    gui_show_ports();
    gui_show_slow();
}

/* ------------------------------------------------------------------ */
/* Takt                                                                */
/* ------------------------------------------------------------------ */

/*
 * Der Takt im Ereignisablauf. Er fragt den Bus SELBST ab (direkt, synchron im
 * MUI-Task) und zeichnet danach die Anzeige. Bei fehlender Verbindung wird nur
 * nach dem angekuendigten Abstand (Backoff) wieder gefragt: ein fehlender
 * Partner soll keinen Dauerlauf auf dem Bus erzeugen. Genau diese Bauart ist
 * auf der V4 gelaufen.
 *
 * DIE REIHENFOLGE IST TEIL DER AUSSAGE:
 *
 *   1. Bus abfragen           -- nur wenn die Live-Abfrage an ist und der
 *                                Abstand es zulaesst
 *   2. Scan/Verbindung schieben
 *   3. eigene Register lesen  -- NACH dem Bus (siehe gui_poll)
 *   4. Aenderungen sofort melden, Vergleich fuer die Herzzeile bauen
 *   5. Lebenszeichen          -- GENAU EINE Zeile je Sekunde, mit dem Vergleich
 *   6. Anzeige zeichnen
 *
 * Im Ruhezustand ergibt das eine Zeile je Sekunde. Jede Aenderung (Richtung,
 * Feuerknopf, Urteil, Modus, Sitzung, Fehler, Verbindungsverlust) kommt
 * sofort dazu.
 */
static void gui_tick(void)
{
    int slow;

    if (!s_gui_up) {
        return;
    }
    s_tick++;
    /*
     * Der ERSTE Takt ist immer ein VOLLSTAENDIGER Takt.
     *
     * Die Konsole holt und zeigt Status, Controller und Buchsen sofort beim
     * Start (ihr "-- Anfangszustand --" mit act_status/act_hid/print_ports);
     * die Oberflaeche tat das nicht: `slow' wurde allein aus
     * s_tick % GUI_SLOW_EVERY gebildet, und s_tick startet bei 1 -- der
     * erste langsame Takt kam also erst nach fuenf Takten. Solange standen in
     * den Feldern nur die Anfangstexte (" " fuer den Zustand, "Controller:
     * (wird gelesen)"), und genau das sieht wie ein leeres Anzeigefeld aus,
     * obwohl ein Controller verbunden ist. Ist der ESP32 in dieser Zeit
     * beschaeftigt (BluePad32 sucht fortlaufend), liegen zwischen Fenster und
     * erster Statuszeile mehrere Sekunden -- im Mitschnitt vom 13.10.2026
     * 25 s (17:37:49 -> 17:38:14).
     */
    slow = ((s_tick % GUI_SLOW_EVERY) == 0u || s_connect_left > 0u
            || s_tick == 1u) ? 1 : 0;

    /* 1. Bus abfragen -- aber nur, wenn es der Abstand zulaesst.
     *
     * Die Transaktionszeilen dazu gibt es nur mit -trace. Ohne -trace
     * entstehen hier im Ruhezustand KEINE Zeilen. */
    if (s_live_on != 0) {
        if (S.link_ok != 0 || (unsigned long)s_seit_versuch * GUI_TICK_MS
                                  >= S.gap_ms) {
            gui_poll(slow);
            s_seit_versuch = 0u;
        } else {
            s_seit_versuch++;
        }
    }

    /* 2. Laufenden Scan weiterschieben. */
    if (s_scan_left > 0u) {
        gui_scan_tick();
    }

    /* 3. Auf CONNECTED warten: CONNECT ist nur ANGENOMMEN. */
    if (s_connect_left > 0u) {
        v4p_status_t st;
        uint8_t      rc = v4_get_status(&M, &st);

        (void)v4_state_note(&S, rc);
        if (rc == V4P_ST_OK) {
            v4_state_put_status(&S, &st);
            v4_dbg_sitzung(&st);
        }
        s_connect_left--;
        if (rc == V4P_ST_OK
            && (st.state == V4P_STATE_CONNECTED
                || st.state == V4P_STATE_SUSPENDED)) {
            gui_msg("[verbinden] verbunden: Index %u (%s)\n",
                    (unsigned)st.conn_index, v4p_bt_state_name(st.state));
            s_connect_left = 0u;
        } else if (s_connect_left == 0u) {
            gui_msg("[verbinden] kein CONNECTED nach %u s (Zustand %s). "
                    "Controller eingeschaltet?\n",
                    (unsigned)(GUI_CONNECT_TICKS * GUI_TICK_MS / 1000u),
                    (rc == V4P_ST_OK) ? v4p_bt_state_name(st.state)
                                      : v4_strerror(rc));
        }
    }

    /* 4. Die EIGENE Hardware: kostet nichts, braucht keinen Bus -- deshalb
     *    auch dann, wenn die Live-Abfrage aus ist. */
    gui_joy_abtasten();

    /* 5. Sofort melden, was sich geaendert hat (eine fertige Zeile), und den
     *    Vergleich fuer die Herzzeile bereitlegen. */
    gui_joy_aenderungen();
    gui_joy_herz_zusatz();

    /* 6. Lebenszeichen: EINE Zeile je Sekunde. Sie traegt Zaehler, letzten
     *    Status, verworfene Rahmen, Wiederholungen UND den Vergleich. */
    if (slow != 0) {
        v4_dbg_herz(&M);
    }

    /* 7. Anzeige zeichnen. Ist ein Wert (noch) nicht gueltig, bleibt die alte
     *    Anzeige stehen -- genau das ist die Zusage "die letzten gueltigen
     *    Werte bleiben sichtbar". */
    gui_show_link();
    gui_show_info();
    gui_show_ports();
    gui_show_slow();
}

/* ------------------------------------------------------------------ */
/* Timer (timer.device), in das Wait() der MUI-Schleife eingemischt     */
/* ------------------------------------------------------------------ */

static struct MsgPort     *s_tport = NULL;
static struct timerequest *s_treq = NULL;
static int                 s_timer_up = 0;
static int                 s_timer_pending = 0;

static void timer_arm(unsigned ms)
{
    if (!s_timer_up || s_timer_pending) {
        return;
    }
    s_treq->tr_node.io_Command = TR_ADDREQUEST;
    s_treq->tr_time.tv_secs    = (ULONG)(ms / 1000u);
    s_treq->tr_time.tv_micro   = (ULONG)((ms % 1000u) * 1000u);
    SendIO((struct IORequest *)s_treq);
    s_timer_pending = 1;
}

/* 1 = der Timer ist abgelaufen (und wurde abgeraeumt). */
static int timer_fired(void)
{
    if (!s_timer_up || !s_timer_pending) {
        return 0;
    }
    if (CheckIO((struct IORequest *)s_treq) == NULL) {
        return 0;                       /* laeuft noch */
    }
    WaitIO((struct IORequest *)s_treq);
    s_timer_pending = 0;
    return 1;
}

static int timer_open(void)
{
    s_tport = CreateMsgPort();
    if (s_tport == NULL) {
        return -1;
    }
    s_treq = (struct timerequest *)CreateIORequest(s_tport,
                                                  sizeof(struct timerequest));
    if (s_treq == NULL) {
        DeleteMsgPort(s_tport);
        s_tport = NULL;
        return -1;
    }
    if (OpenDevice((CONST_STRPTR)TIMERNAME, UNIT_MICROHZ,
                   (struct IORequest *)s_treq, 0) != 0) {
        DeleteIORequest((struct IORequest *)s_treq);
        s_treq = NULL;
        DeleteMsgPort(s_tport);
        s_tport = NULL;
        return -1;
    }
    s_timer_up = 1;
    return 0;
}

static void timer_close(void)
{
    if (s_timer_up) {
        if (!CheckIO((struct IORequest *)s_treq)) {
            AbortIO((struct IORequest *)s_treq);
        }
        WaitIO((struct IORequest *)s_treq);
        CloseDevice((struct IORequest *)s_treq);
        s_timer_up = 0;
        s_timer_pending = 0;
    }
    if (s_treq != NULL) {
        DeleteIORequest((struct IORequest *)s_treq);
        s_treq = NULL;
    }
    if (s_tport != NULL) {
        DeleteMsgPort(s_tport);
        s_tport = NULL;
    }
}

/* ------------------------------------------------------------------ */
/* MUI                                                                */
/* ------------------------------------------------------------------ */

static BOOL open_libs(void)
{
    IntuitionBase = (struct IntuitionBase *)OpenLibrary(
        (CONST_STRPTR)"intuition.library", 39);
    if (IntuitionBase == NULL) {
        return FALSE;
    }
    GfxBase = (struct GfxBase *)OpenLibrary((CONST_STRPTR)"graphics.library", 0);
    if (GfxBase == NULL) {
        return FALSE;
    }
    MUIMasterBase = OpenLibrary((CONST_STRPTR)MUIMASTER_NAME, 19);
    if (MUIMasterBase == NULL) {
        return FALSE;
    }
    return TRUE;
}

static void close_libs(void)
{
    if (MUIMasterBase != NULL) {
        CloseLibrary((struct Library *)MUIMasterBase);
        MUIMasterBase = NULL;
    }
    if (GfxBase != NULL) {
        CloseLibrary((struct Library *)GfxBase);
        GfxBase = NULL;
    }
    if (IntuitionBase != NULL) {
        CloseLibrary((struct Library *)IntuitionBase);
        IntuitionBase = NULL;
    }
}

/*
 * Die Fenstergroesse an den Schirm anpassen.
 *
 * WOZU: die Vorgabe ist 640x400 (mit -w/-h aenderbar). Laeuft die Workbench
 * aber auf einem kleineren Schirm -- auf dieser V4 ist 640x360 eingestellt --
 * dann ist das Fenster HOEHER als der Schirm. Das ist nicht nur unbrauchbar
 * (die unterste Zeile, das Logfeld, ist nie zu sehen): MUI rechnet beim
 * Oeffnen eines Fensters mit SizeGadget mit einem Groessenbereich, den es auf
 * diesem Schirm nicht geben kann. Deshalb wird die gewuenschte Groesse auf den
 * Schirm begrenzt -- und das ausdruecklich gemeldet, damit niemand raetselt,
 * warum das Fenster kleiner ist.
 *
 * Passt 640x400 (oder was -w/-h vorgibt), aendert sich NICHTS. Ohne
 * oeffentlichen Schirm ebenfalls nicht, dann gilt die Vorgabe unveraendert.
 */
static void gui_fenster_an_schirm(void)
{
    struct Screen *schirm;

    schirm = LockPubScreen(NULL);
    if (schirm == NULL) {
        return;
    }
    s_schirm_w = (int)schirm->Width;
    s_schirm_h = (int)schirm->Height;
    UnlockPubScreen(NULL, schirm);

    if (s_schirm_w > 0 && s_fenster_w > s_schirm_w - GUI_SCHIRM_RAND_W) {
        s_fenster_w        = s_schirm_w - GUI_SCHIRM_RAND_W;
        s_fenster_geklemmt = 1;
    }
    if (s_schirm_h > 0 && s_fenster_h > s_schirm_h - GUI_SCHIRM_RAND_H) {
        s_fenster_h        = s_schirm_h - GUI_SCHIRM_RAND_H;
        s_fenster_geklemmt = 1;
    }
    /* Dieselben Untergrenzen wie -w/-h beim Aufruf. */
    if (s_fenster_w < 320) {
        s_fenster_w = 320;
    }
    if (s_fenster_h < 240) {
        s_fenster_h = 240;
    }
}

static int gui_build(void)
{
    Object *btn_scan, *btn_conn, *btn_disconn, *btn_forget;
    Object *btn_mode0, *btn_mode1, *btn_swap, *btn_refresh;

    s_app = ApplicationObject,
        MUIA_Application_Title,       (IPTR)"BT Gamecontroller GUI v" GUI_VERSION,
        MUIA_Application_Version,     (IPTR)GUI_VER_STR,
        MUIA_Application_Copyright,   (IPTR)" ",
        MUIA_Application_Author,      (IPTR)" ",
        MUIA_Application_Description, (IPTR)"Vampire V4: Bluetooth-Joystick "
                                             "ueber den ESP32 steuern",
        MUIA_Application_Base,        (IPTR)"V4JOY",

        MUIA_Application_Window, s_win = WindowObject,
            MUIA_Window_Title, "BT Gamecontroller GUI v" GUI_VERSION,
            MUIA_Window_ID,    MAKE_ID('V', '4', 'J', 'Y'),
            /*
             * Feste Anfangsgroesse, aber kleiner als frueher: ohne feste
             * Groesse zieht MUI das Fenster auf die Inhaltsgroesse, und ein
             * langer Logtext macht es breiter als den Bildschirm. Die Listen
             * bekommen so eine feste Flaeche mit eigenen Rollbalken.
             *
             * SizeGadget: die Groesse laesst sich mit der Maus aendern - wer
             * mehr Platz braucht, zieht einfach auf.
             */
            MUIA_Window_Width,      s_fenster_w,
            MUIA_Window_Height,     s_fenster_h,
            MUIA_Window_SizeGadget, TRUE,
            WindowContents, VGroup,
                /*
                 * WARUM DIESE VIER FELDER GEWICHT 0 HABEN (und die Buchsen-
                 * gruppen unten ebenfalls):
                 *
                 * MUI gibt jedem Objekt ohne MUIA_Weight das Vorgabegewicht
                 * 100 und verteilt den Platz danach. Reicht der Platz im
                 * Fenster nicht -- 640x400 ist fest, und der Inhalt ist mit
                 * zwei Buchsengruppen, Vergleichsfeld, zwei Knopfreihen und
                 * zwei Listen lang --, dann verkleinert MUI die Kinder nach
                 * ihrem Gewicht. Ein Textobjekt mit Gewicht 100 kann dabei
                 * unter seine Mindesthoehe gedrueckt werden und sieht dann
                 * LEER aus, obwohl sein Inhalt laengst gesetzt ist.
                 *
                 * Die Autodokumentation der Area-Klasse sagt dazu eindeutig:
                 * "An object with a weight of 0 will always stay at its
                 * minimum size." Genau das ist hier gewollt: die Anzeige-
                 * felder behalten immer ihre Mindestgroesse; den fehlenden
                 * Platz bekommen die beiden Listen (Gewicht 30/40), die
                 * scrollen koennen.
                 *
                 * MUIA_Text_SetMin, FALSE gehoert dazu: die Vorgabe TRUE
                 * macht die Textbreite zur Mindestbreite des Fensterinhalts.
                 * Die Controller-Zeile ist rund 100 Zeichen lang -- das waere
                 * breiter als die zugesagten 640 Pixel und wuerde MUI das
                 * Fenster aufziehen. Mit FALSE bleibt das Fenster 640 breit
                 * und der Text wird am Rand abgeschnitten (die wichtigen
                 * Angaben stehen am Zeilenanfang).
                 */
                Child, s_link = TextObject,
                    MUIA_Text_Contents, (IPTR)"Oeffne i2c.library ...",
                    MUIA_Text_PreParse, (IPTR)"\33b",
                    MUIA_Text_SetMin, FALSE,
                    MUIA_Weight, 0,
                End,
                Child, s_info = TextObject,
                    MUIA_Text_Contents, (IPTR)"Verbinde mit dem ESP32 ...",
                    MUIA_Text_PreParse, (IPTR)"\33l",
                    MUIA_Text_SetMin, FALSE,
                    MUIA_Weight, 0,
                End,
                Child, s_state = TextObject,
                    MUIA_Text_Contents, (IPTR)" ",
                    MUIA_Text_PreParse, (IPTR)"\33l",
                    MUIA_Text_SetMin, FALSE,
                    MUIA_Weight, 0,
                End,
                Child, s_ctrl = TextObject,
                    MUIA_Text_Contents, (IPTR)"Controller: (wird gelesen)",
                    MUIA_Text_PreParse, (IPTR)"\33l",
                    MUIA_Text_SetMin, FALSE,
                    MUIA_Weight, 0,
                End,
                Child, HGroup,
                    MUIA_Weight, 0,
                    Child, VGroup,
                        Child, HGroup,
                            Child, TextObject,
                                MUIA_Text_Contents, (IPTR)"Buchse 0:",
                                /* Auch die Beschriftung bleibt auf ihrer
                                 * Mindestbreite. Sonst nimmt sie den Platz
                                 * der Zeile fuer sich und schiebt den
                                 * kompakten Auswahlknopf an den rechten
                                 * Rand. */
                                MUIA_Weight, 0,
                            End,
                            Child, s_mode0 = CycleObject,
                                MUIA_Cycle_Entries,
                                    (IPTR)gui_modus_eintraege,
                                /* Gewicht 0: der Knopf bleibt so breit wie
                                 * sein laengster Eintrag ("Joystick") und
                                 * wird nicht auf die Spaltenbreite gezogen
                                 * (Area-Doku: "An object with a weight of 0
                                 * will always stay at its minimum size"). */
                                MUIA_Weight, 0,
                            End,
                        End,
                        Child, s_port0 = TextObject,
                            MUIA_Text_Contents, (IPTR)" ",
                            MUIA_Text_PreParse, (IPTR)"\33l",
                            MUIA_Weight, 0,
                        End,
                    End,
                    Child, VGroup,
                        Child, HGroup,
                            Child, TextObject,
                                MUIA_Text_Contents, (IPTR)"Buchse 1:",
                                MUIA_Weight, 0,
                            End,
                            Child, s_mode1 = CycleObject,
                                MUIA_Cycle_Entries,
                                    (IPTR)gui_modus_eintraege,
                                MUIA_Weight, 0,
                            End,
                        End,
                        Child, s_port1 = TextObject,
                            MUIA_Text_Contents, (IPTR)" ",
                            MUIA_Text_PreParse, (IPTR)"\33l",
                            MUIA_Weight, 0,
                        End,
                    End,
                End,
                /*
                 * Der Vergleich direkt <-> I2C, volle Fensterbreite: eine Zeile
                 * je Buchse und darunter die Rohwerte der Feuerregister. Die
                 * direkte Abfrage laeuft neben der I2C-Antwort -- stimmen beide
                 * ueberein, ist die Kette ESP32 -> DB9 -> V4 in Ordnung.
                 */
                Child, s_vergleich = TextObject,
                    MUIA_Text_Contents, (IPTR)"Vergleich direkt/I2C ...",
                    MUIA_Text_PreParse, (IPTR)"\33l",
                    MUIA_Weight, 0,
                End,
                Child, HGroup,
                    MUIA_Weight, 0,
                    Child, btn_scan    = MUI_MakeObject(MUIO_Button,
                                          (IPTR)"Neu suchen", NULL),
                    Child, btn_conn    = MUI_MakeObject(MUIO_Button,
                                          (IPTR)"Verbinden", NULL),
                    Child, btn_disconn = MUI_MakeObject(MUIO_Button,
                                          (IPTR)"Trennen", NULL),
                    Child, btn_forget  = MUI_MakeObject(MUIO_Button,
                                          (IPTR)"Vergessen", NULL),
                    Child, btn_refresh = MUI_MakeObject(MUIO_Button,
                                          (IPTR)"Aktualisieren", NULL),
                    Child, s_live      = MUI_MakeObject(MUIO_Checkmark,
                                          (IPTR)"Live (0,2 s)", NULL),
                End,
                Child, HGroup,
                    MUIA_Weight, 0,
                    Child, btn_mode0 = MUI_MakeObject(MUIO_Button,
                                          (IPTR)"Port 0 Modus >", NULL),
                    Child, btn_mode1 = MUI_MakeObject(MUIO_Button,
                                          (IPTR)"Port 1 Modus >", NULL),
                    Child, btn_swap  = MUI_MakeObject(MUIO_Button,
                                          (IPTR)"Buchsen tauschen", NULL),
                    Child, TextObject,
                        MUIA_Text_Contents, (IPTR)"\"Zug\" = nach GND gezogen "
                            "(Firmware), \"Pegel\" = am ESP32 gemessen",
                    End,
                End,
                /*
                 * Die beiden Listen sind die EINZIGEN Kinder mit Gewicht: sie
                 * bekommen den Platz, der nach den festen Anzeigefeldern
                 * uebrig bleibt (Gewicht 30 fuer die Geraeteliste, 40 fuer das
                 * Logfeld). MUIA_MinHeight haelt sie auch dann sichtbar, wenn
                 * das Fenster kleiner gezogen wird -- sonst blieben von einer
                 * Liste nur ein paar Pixel uebrig und sie saehe leer aus.
                 */
                Child, s_devs_lv = ListviewObject,
                    MUIA_Listview_List, s_devs = ListObject,
                    End,
                    MUIA_Weight, 30,
                    MUIA_MinHeight, 36,
                End,
                Child, ListviewObject,
                    MUIA_Listview_List, s_logl = ListObject,
                    End,
                    MUIA_Weight, 40,
                    MUIA_MinHeight, 36,
                End,
            End,
        End,
    End;

    if (s_app == NULL || s_win == NULL) {
        gui_msg("MUI: Anwendung konnte nicht angelegt werden\n");
        return -1;
    }
    /*
     * WICHTIG: s_gui_up wird hier NICHT gesetzt.
     *
     * s_gui_up schaltet den Zeilen-Haken (gui_loglist_add) scharf, und der
     * greift auf die MUI-Logliste zu. Solange das Fenster nicht offen ist,
     * darf kein MUI-Objekt angefasst werden -- das Setzen gehoert deshalb in
     * main() HINTER set(s_win, MUIA_Window_Open, TRUE).
     */
    if (s_vergleich == NULL || s_port0 == NULL || s_port1 == NULL
        || s_logl == NULL) {
        /* Ein fehlendes Objekt ist kein Grund weiterzumachen: die Anzeige
         * waere unvollstaendig, und ein set() darauf wuerde MUI treffen. */
        gui_msg("MUI: Objektbaum unvollstaendig (vergleich=%d port0=%d "
                "port1=%d log=%d) -- Abbruch\n",
                (s_vergleich != NULL), (s_port0 != NULL), (s_port1 != NULL),
                (s_logl != NULL));
        return -1;
    }

    /* Schliessen und jeder Knopf schicken eine ReturnID in die Hauptschleife.
     * MUIA_Pressed/FALSE loest beim Loslassen aus - das uebliche MUI-Muster. */
    DoMethod(s_win, MUIM_Notify, MUIA_Window_CloseRequest, TRUE,
             s_app, 2, MUIM_Application_ReturnID, ID_QUIT);
    DoMethod(btn_scan, MUIM_Notify, MUIA_Pressed, FALSE,
             s_app, 2, MUIM_Application_ReturnID, ID_SCAN);
    DoMethod(btn_conn, MUIM_Notify, MUIA_Pressed, FALSE,
             s_app, 2, MUIM_Application_ReturnID, ID_CONNECT);
    DoMethod(btn_disconn, MUIM_Notify, MUIA_Pressed, FALSE,
             s_app, 2, MUIM_Application_ReturnID, ID_DISCONN);
    DoMethod(btn_forget, MUIM_Notify, MUIA_Pressed, FALSE,
             s_app, 2, MUIM_Application_ReturnID, ID_FORGET);
    DoMethod(btn_refresh, MUIM_Notify, MUIA_Pressed, FALSE,
             s_app, 2, MUIM_Application_ReturnID, ID_REFRESH);
    DoMethod(btn_mode0, MUIM_Notify, MUIA_Pressed, FALSE,
             s_app, 2, MUIM_Application_ReturnID, ID_MODE0);
    DoMethod(btn_mode1, MUIM_Notify, MUIA_Pressed, FALSE,
             s_app, 2, MUIM_Application_ReturnID, ID_MODE1);
    DoMethod(btn_swap, MUIM_Notify, MUIA_Pressed, FALSE,
             s_app, 2, MUIM_Application_ReturnID, ID_SWAP);
    /* Doppelklick auf ein gefundenes Geraet verbindet es. */
    DoMethod(s_devs_lv, MUIM_Notify, MUIA_Listview_DoubleClick, TRUE,
             s_app, 2, MUIM_Application_ReturnID, ID_CONNECT);
    /* Modus direkt am Cycle waehlen. */
    DoMethod(s_mode0, MUIM_Notify, MUIA_Cycle_Active, MUIV_EveryTime,
             s_app, 2, MUIM_Application_ReturnID, ID_MODE0);
    DoMethod(s_mode1, MUIM_Notify, MUIA_Cycle_Active, MUIV_EveryTime,
             s_app, 2, MUIM_Application_ReturnID, ID_MODE1);
    /* Live-Aktualisierung abschaltbar: wer nur zusehen will, kann sie
     * laufen lassen; wer am Bus stoeren will, schaltet sie ab. */
    DoMethod(s_live, MUIM_Notify, MUIA_Selected, MUIV_EveryTime,
             s_app, 2, MUIM_Application_ReturnID, ID_REFRESH);
    return 0;
}

/* ------------------------------------------------------------------ */
/* main                                                               */
/* ------------------------------------------------------------------ */

static void usage(void)
{
    /* Bewusst mit fprintf: die Aufrufhilfe wird schon beim Auswerten der
     * Schalter gebraucht, also BEVOR die Debugausgabe aufgesetzt ist. */
    (void)fprintf(stdout,
                  "Aufruf: BT Gamecontroller GUI [-quiet] [-nodebug] [-trace] "
                  "[-nolog] [-l <logdatei>]\n"
                  "        [-d <i2c-device>] [-w <px>] [-h <px>]\n"
                  "  -quiet    Transaktionszeilen aus (das ist die Vorgabe)\n"
                  "  -nodebug  keine Ausgabe ueber den Debug-UART der V4\n"
                  "  -trace    zusaetzlich jede Transaktion und jeden Schritt\n"
                  "            (sehr ausfuehrlich; hat Vorrang vor -quiet)\n"
                  "  -nolog    keine Logdatei\n"
                  "  -l        Logdatei (Vorgabe Programs:test/v4_gui.log)\n"
                  "  -d        I2C-Geraet (auf der V4 wirkungslos)\n"
                  "  -w <px>   Fensterbreite (Vorgabe %d, mit der Maus "
                  "aenderbar)\n"
                  "  -h <px>   Fensterhoehe  (Vorgabe %d, mit der Maus "
                  "aenderbar)\n"
                  "Ohne -trace: im Ruhezustand EINE Zeile je Sekunde "
                  "(Lebenszeichen mit\n"
                  "dem Vergleich direkt/I2C); jede Aenderung sofort.\n",
                  s_fenster_w, s_fenster_h);
    (void)fflush(stdout);
}

int main(int argc, char **argv)
{
    int         i;
    ULONG       signals;
    ULONG       tmask;
    BOOL        running = TRUE;

    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-nodebug") == 0) {
            s_debug_uart = 0;
        } else if (strcmp(argv[i], "-quiet") == 0
                   || strcmp(argv[i], "-q") == 0) {
            s_quiet = 1;
        } else if (strcmp(argv[i], "-trace") == 0) {
            s_trace = 1;
        } else if (strcmp(argv[i], "-nolog") == 0) {
            s_use_log = 0;
        } else if (strcmp(argv[i], "-l") == 0 && i + 1 < argc) {
            s_log_path = argv[++i];
        } else if (strcmp(argv[i], "-d") == 0 && i + 1 < argc) {
            s_dev_path = argv[++i];
        } else if ((strcmp(argv[i], "-w") == 0 || strcmp(argv[i], "-h") == 0)
                   && i + 1 < argc) {
            const int ist_breite = (argv[i][1] == 'w');
            const int wert = atoi(argv[++i]);

            if (wert < 320 || wert > 4000) {
                (void)fprintf(stdout,
                              "%s: %d ist unbrauchbar (erlaubt 320..4000)\n",
                              ist_breite ? "-w" : "-h", wert);
                (void)fflush(stdout);
                return 1;
            }
            if (ist_breite) {
                s_fenster_w = wert;
            } else {
                s_fenster_h = wert;
            }
        } else {
            usage();
            return 1;
        }
    }

    /* Logdatei der Plattform oeffnen, damit auch Startfehler darin stehen. */
    if (s_use_log != 0) {
        if (v4_plat_log_open(s_log_path) != 0) {
            (void)fprintf(stdout, "Hinweis: keine Logdatei (%s)\n",
                          (s_log_path != NULL) ? s_log_path
                                               : "Plattform ohne Vorgabepfad");
            (void)fflush(stdout);
        }
    }

    /*
     * Debugausgabe aufsetzen: stdout + Debug-UART der V4 + Logdatei, ueberall
     * derselbe Text. Das MUI-Logfeld bekommt jede Zeile ueber den Zeilen-Haken
     * -- deshalb gibt es keinen doppelten Eintrag.
     *
     * TRANSAKTIONSZEILEN: die Vorgabe der Oberflaeche ist AUS.
     *
     * Bei 200 ms Takt und zwei Befehlen je Takt waeren das zehn Zeilen je
     * Sekunde, jede 150 bis 300 Zeichen lang -- und jedes Zeichen kostet am
     * DebugUART Wartezeit. Sie kommen deshalb nur noch mit -trace; -quiet
     * schaltet sie ausdruecklich ab und ist damit dieselbe Vorgabe.
     */
    {
        v4_dbg_cfg_t cfg;

        memset(&cfg, 0, sizeof(cfg));
        cfg.programm  = "BT Gamecontroller GUI";
        cfg.version   = GUI_VERSION;
        cfg.datum     = GUI_DATUM;
        cfg.logdatei  = (s_use_log != 0) ? s_log_path : NULL;
        cfg.addr7     = (unsigned)V4_I2C_ADDR7;
        cfg.uart      = s_debug_uart;
        cfg.stdout_an = 1;
        cfg.quiet     = (s_trace != 0) ? 0 : 1;
        s_quiet       = cfg.quiet;
        v4_dbg_init(&cfg);
        v4_dbg_set_detail(s_trace);
        v4_dbg_set_line_hook(gui_loglist_add);
    }
    v4_dbg_kopf();
    gui_msg("[schalter] Transaktionszeilen %s, Schritt-Trace %s\n",
            (s_quiet != 0) ? "aus (Vorgabe)" : "an",
            (s_trace != 0) ? "an" : "aus");

    if (v4p_selftest_byteorder() != 1) {
        gui_msg("Byte-Order-Selbsttest fehlgeschlagen -- Abbruch\n");
        return 2;
    }

    /*
     * Ab hier laeuft der Busverkehr in DIESEM Faden: Master initialisieren,
     * Plattform oeffnen, erster PING. Kein CreateNewProc, kein zweiter Faden,
     * kein Nachrichtenport.
     *
     * Scheitert das OEFFNEN DER PLATTFORM (kein i2c.library / timer.device),
     * wird abgebrochen -- ohne Bus gibt es keine Steuerung. Ein fehlender ESP32
     * ist dagegen KEIN Abbruchgrund: das steht dann als Zeile im Fenster.
     */
    v4_state_init(&S);
    v4_init(&M);                        /* nullt den Master: erst danach setzen */
    /* Der Trace-Haken haengt IMMER am Master (er zaehlt auch die verworfenen
     * Rahmen und die Wiederholungen fuer das Lebenszeichen). Was er AUSGIBT,
     * entscheidet die Ausgabeschicht: die Zeile je Transaktion nur mit
     * -trace, die Meldung "Rahmen verworfen" auch ohne. */
    M.trace     = v4_dbg_trace;
    M.trace_ctx = NULL;

    /*
     * Die direkte Abfrage der Joystick-Anschluesse aufsetzen und den
     * GRUNDZUSTAND der Feuerleitungen JETZT merken -- vor dem ersten Takt und
     * bevor der Anwender etwas druecken kann.
     *
     * Warum der Grundzustand und nicht eine feste Polaritaet: am Geraet hat
     * sich beim Druecken an CIAAPRA/POTINP nichts bewegt (siehe v4_joyport.h).
     * Welches Bit welche Taste ist, wird deshalb NICHT geraten: als gedrueckt
     * gilt, was sich gegen diesen Startwert UNTERSCHEIDET. Die Rohwerte stehen
     * daneben, damit die Belegung durch Draufschauen zu finden ist.
     */
    v4_joy_init(&J);
    {
        v4_joy_roh_t roh;

        v4_joy_lesen(&roh);
        v4_joy_basis(&J, &roh);
        gui_msg("[direkt] Register beim Start: JOY0DAT=0x%04X JOY1DAT=0x%04X "
                "CIAAPRA=0x%04X POTINP=0x%04X\n",
                (unsigned)roh.joy0dat, (unsigned)roh.joy1dat,
                (unsigned)roh.ciaapra, (unsigned)roh.potinp);
    }

    gui_msg("Oeffne I2C-Bus%s%s ...\n",
            (s_dev_path != NULL) ? ": " : " (Standard)",
            (s_dev_path != NULL) ? s_dev_path : "");
    {
        int orc = v4_plat_open(s_dev_path);

        gui_msg("[plattform] v4_plat_open -> %d (%s)\n", orc,
                (orc == 0) ? "OK" : v4_plat_error_text());
        if (orc != 0) {
            gui_msg("i2c.library/timer.device nicht verfuegbar -- Abbruch\n");
            if (s_use_log != 0) {
                v4_plat_log_close();
            }
            return 2;
        }
    }
    gui_msg("[plattform] i2c.library und timer.device sind offen\n");

    /* Erster PING: er fuellt die Kopfdaten der Gegenstelle. */
    {
        v4p_info_t info;
        uint8_t    rc;

        memset(&info, 0, sizeof(info));
        gui_msg("[ping] erste Abfrage der Gegenstelle ...\n");
        rc = v4_ping(&M, &info);
        (void)v4_state_note(&S, rc);
        if (rc == V4P_ST_OK) {
            uint8_t check = v4_check_info(&info);

            v4_state_put_info(&S, &info, (check == V4P_ST_OK) ? 1 : 0);
            v4_dbg_info(&info);
            if (check != V4P_ST_OK) {
                gui_msg("[ping] %s: Slave meldet proto=%u write=%u read=%u; "
                        "erwartet proto=%u write=%u read=%u\n",
                        v4_strerror(check), (unsigned)info.proto_ver,
                        (unsigned)info.write_frame_len,
                        (unsigned)info.read_frame_len,
                        (unsigned)V4P_PROTO_VER,
                        (unsigned)V4P_WRITE_FRAME_LEN,
                        (unsigned)V4P_READ_FRAME_LEN);
            } else {
                gui_msg("[ping] OK: proto=%u fw=%u write=%u read=%u "
                        "caps=0x%04X\n",
                        (unsigned)info.proto_ver, (unsigned)info.fw_ver,
                        (unsigned)info.write_frame_len,
                        (unsigned)info.read_frame_len, (unsigned)info.caps);
            }
        } else {
            gui_msg("[ping] %s\n", v4_strerror(rc));
            v4_dbg_fehler("PING", rc, &M);
        }
    }

    if (open_libs() == FALSE) {
        gui_msg("MUI/intuition nicht offen -- Abbruch\n");
        v4_close();
        if (s_use_log != 0) {
            v4_plat_log_close();
        }
        return 3;
    }

    /*
     * Fensteraufbau. Die Reihenfolge hier ist Absicht:
     *
     *   1. Groesse an den Schirm anpassen (nur wenn sie nicht passt),
     *   2. Objektbaum bauen,
     *   3. Fenster OEFFNEN und das Ergebnis pruefen,
     *   4. ERST JETZT s_gui_up setzen und MUI-Objekte anfassen.
     *
     * Vorher stand s_gui_up direkt nach dem Bauen -- also bevor das Fenster
     * offen war. Jede Meldung dazwischen haette ueber den Zeilen-Haken in die
     * MUI-Logliste geschrieben, und die gehoert zu einem Fenster, das es noch
     * nicht gibt.
     */
    {
        const int wunsch_w = s_fenster_w;
        const int wunsch_h = s_fenster_h;

        gui_fenster_an_schirm();
        if (s_fenster_geklemmt != 0) {
            gui_msg("[fenster] %dx%d passt nicht auf den Schirm (%dx%d) -- auf "
                    "%dx%d begrenzt. Mit -w/-h laesst sich eine andere Groesse "
                    "vorgeben.\n",
                    wunsch_w, wunsch_h, s_schirm_w, s_schirm_h,
                    s_fenster_w, s_fenster_h);
        } else if (s_schirm_w > 0) {
            gui_msg("[fenster] %dx%d, Schirm %dx%d\n",
                    s_fenster_w, s_fenster_h, s_schirm_w, s_schirm_h);
        }
    }

    if (gui_build() != 0) {
        close_libs();
        v4_close();
        if (s_use_log != 0) {
            v4_plat_log_close();
        }
        return 4;
    }
    gui_msg("[mui] Objektbaum steht, oeffne das Fenster (%dx%d) ...\n",
            s_fenster_w, s_fenster_h);
    set(s_win, MUIA_Window_Open, TRUE);
    {
        IPTR offen = 0;

        (void)get(s_win, MUIA_Window_Open, &offen);
        if (offen == 0) {
            gui_msg("[mui] FEHLER: das Fenster liess sich nicht oeffnen "
                    "(%dx%d). Mit -w/-h eine andere Groesse versuchen.\n",
                    s_fenster_w, s_fenster_h);
            MUI_DisposeObject(s_app);
            s_app = NULL;
            close_libs();
            v4_close();
            if (s_use_log != 0) {
                v4_plat_log_close();
            }
            return 5;
        }
    }
    gui_msg("[mui] Fenster offen (%dx%d). Ab hier werden MUI-Objekte "
            "angefasst.\n", s_fenster_w, s_fenster_h);

    /* ERST JETZT: der Zeilen-Haken darf in die Logliste schreiben, und die
     * Cycle-Knoepfe duerfen nachgezogen werden. */
    s_gui_up = 1;

    if (timer_open() != 0) {
        /* Ohne Timer keine Live-Anzeige. Das ist kein Grund abzubrechen: die
         * Knoepfe aktualisieren weiterhin, nur eben von Hand. */
        gui_msg("WARNUNG: timer.device nicht verfuegbar - keine "
                "Live-Aktualisierung, bitte \"Aktualisieren\" benutzen\n");
    }

    set(s_live, MUIA_Selected, TRUE);       /* Live-Anzeige ist die Vorgabe */

    /* Erste Anzeige. Die Cycle-Knoepfe werden im Takt aus dem zuletzt
     * bestaetigten Modus nachgezogen -- ein sofortiges set() hier wuerde sofort
     * eine Notify ausloesen und damit einen unnoetigen SET_PORT_MODE schicken. */
    gui_msg("[takt] erste Abfrage ...\n");
    gui_tick();

    tmask = (s_tport != NULL) ? (1UL << s_tport->mp_SigBit) : 0UL;
    if (tmask != 0UL) {
        timer_arm(GUI_TICK_MS);
    }

    /*
     * Die dokumentierte MUI-Hauptschleife. MUIM_Application_NewInput erwartet
     * das Ergebnis des vorigen Wait() (oder 0) und wartet selbst nicht -- genau
     * deshalb laesst sich hier der Timersignal-Bit in das Wait() einmischen.
     */
    signals = 0u;
    while (running != FALSE) {
        ULONG id = DoMethod(s_app, MUIM_Application_NewInput, &signals);

        switch (id) {
        case ID_QUIT:
            running = FALSE;
            break;
        case ID_SCAN:
            act_scan_start(0);      /* vom Knopf: kein Automatik-Hinweis */
            break;
        case ID_CONNECT:
            act_connect();
            break;
        case ID_DISCONN:
            act_disconnect();
            break;
        case ID_FORGET:
            act_forget();
            break;
        case ID_MODE0:
            act_set_mode(0);
            break;
        case ID_MODE1:
            act_set_mode(1);
            break;
        case ID_SWAP:
            act_swap();
            break;
        case ID_REFRESH: {
            IPTR live = 0;

            /* Der Refresh kommt auch vom Live-Schalter. */
            (void)get(s_live, MUIA_Selected, &live);
            s_live_on = (live != 0) ? 1 : 0;
            act_refresh();
            break;
        }
        default:
            break;
        }

        if (running == FALSE) {
            break;
        }
        signals = Wait(signals | tmask | SIGBREAKF_CTRL_C);
        if ((signals & SIGBREAKF_CTRL_C) != 0UL) {
            gui_msg("Strg-C -- Ende\n");
            running = FALSE;
            break;
        }
        if (tmask != 0UL && (signals & tmask) != 0UL) {
            signals = (ULONG)(signals & ~tmask);
            if (timer_fired()) {
                gui_tick();
                timer_arm(GUI_TICK_MS);
            }
        } else if (tmask == 0UL) {
            /*
             * Ohne timer.device gibt es keinen Takt. Damit die Anzeige
             * trotzdem folgt (die Auftraege laufen ja weiter), wird hier nach
             * jedem Ereignis einmal gezeichnet -- das sind nur die Ereignisse,
             * die der Anwender ausloest.
             */
            gui_tick();
        }
    }

    /* Schlusszeile: erst der Vergleich direkt/I2C in seiner letzten Fassung,
     * dann die verworfenen Abtastungen der Direktseite -- das Gegenstueck zu
     * "verworfene Rahmen" auf der I2C-Seite. Dann die gemeinsame Schlusszeile. */
    gui_joy_vergleich_zeile();
    gui_msg("v4_gui beendet (direkt verworfen: Zaehler %lu, Feuer %lu)\n",
            J.verworfen, J.verworfen_feuer);
    v4_dbg_ende(&M);

    /* Erst das Fenster schliessen, dann den Bus. */
    set(s_win, MUIA_Window_Open, FALSE);

    /* Die Plattform gehoert diesem Faden -- kein zweiter Faden, der noch
     * darauf zugreifen koennte. */
    v4_close();

    MUI_DisposeObject(s_app);
    s_app  = NULL;
    s_gui_up = 0;
    timer_close();
    close_libs();
    if (s_use_log != 0) {
        v4_plat_log_close();
    }
    return 0;
}
