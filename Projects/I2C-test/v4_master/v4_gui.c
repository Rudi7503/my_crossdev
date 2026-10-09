/*
 * v4_gui.c -- MUI-Oberflaeche fuer die V4-Steuerung (Stufe 1).
 *
 * Zweites Frontend neben v4_console.c: dieselbe Protokollschicht
 * (v4_master.c / v4_proto.c), dieselbe Plattform (v4_amiga_i2c.c). Die
 * Konsole bleibt als Notfallweg erhalten.
 *
 * Stufe 1 kann:
 *   - verbinden / trennen (Scan, erster Fund)
 *   - Verzeichnis lesen (DIR_OPEN/DIR_NEXT), ".." hoch, aktualisieren
 *   - Datei abspielen / stoppen (PLAY_FILE/STOP_PLAY)
 *   - Statuszeile aus GET_STATUS (Zustand, SD, audio_flags, chunk, frei)
 *   - Logfeld mit allen Meldungen
 *
 * Debug-Ausgabe (Wunsch vom 08.10.2026): jede Meldung geht drei Wege
 *   1. printf auf die Standardausgabe,
 *   2. ueber den Hardware-Debug-UART der V4 - Vorlage ist
 *      Projects/ApolloLib/ApolloCrossDev_Debug.c (SERPER auf 115200 Baud,
 *      gepollt ueber SERDAT). Am PC ist das der COM-Port. Mit -nodebug aus,
 *      falls dort nichts angeschlossen ist.
 *   3. in die Logdatei der Plattform (v4_plat_log_write).
 * Beim Start wird ein Versionsstring ausgegeben.
 *
 * Aufruf:  v4_gui [-nodebug] [-trace] [-l <logdatei>] [-d <i2c-device>]
 *
 * Die Protokollaufrufe blockieren (BUSY-Wiederholungen bis rund 1 s). In Stufe 1
 * friert das Fenster dabei kurz ein - bewusst in Kauf genommen, siehe README.
 */

#include "v4_master.h"
#include "v4_proto.h"

#include "ApolloCrossDev_Debug.h"   /* Debug-UART der V4, siehe Projects/ApolloLib */
#include <libraries/mui.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/graphics.h>
#include <proto/muimaster.h>
#include <clib/alib_protos.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#define MAKE_ID(a, b, c, d) ((ULONG)(a) << 24 | (ULONG)(b) << 16 | \
                             (ULONG)(c) << 8 | (ULONG)(d))
#ifndef IPTR
#define IPTR ULONG
#endif

#define GUI_VERSION   "0.5"
#define GUI_DATUM     "08.10.2026"
#define GUI_VER_STR   "$VER: v4_gui " GUI_VERSION " (" GUI_DATUM ")"

/* Logfeld: hoechstens so viele Zeilen, dann wird geleert (kein Wachstum). */
#define GUI_LOG_MAX   200

/* ReturnIDs der MUI-Notifies */
#define ID_QUIT       1
#define ID_REFRESH    2
#define ID_UP         3
#define ID_PLAY       4
#define ID_STOP       5
#define ID_CONNECT    6
#define ID_DISCONN    7

struct IntuitionBase *IntuitionBase;
struct GfxBase       *GfxBase;
struct Library       *MUIMasterBase;

static v4_master_t M;

static const char *s_log_path;        /* NULL = Vorgabe der Plattform  */
static const char *s_dev_path;        /* NULL = Vorgabe der Plattform  */
static int     s_debug_uart = 1;      /* -nodebug schaltet den Debug-UART ab */
static int     s_trace = 0;           /* -trace: jeden Rahmen mitschreiben (wie -a der Konsole) */

static Object *s_app;
static Object *s_win;
static Object *s_status;
static Object *s_files;               /* Listview-Liste der Dateien  */
static Object *s_logl;                /* Listview-Liste der Meldungen */
static int     s_gui_up;              /* 1 = MUI-Objekte existieren  */

static char   *s_loglines[GUI_LOG_MAX];
static int     s_logcount;

/*
 * 0.5: Die Dateiliste braucht eigene Kopien. MUI speichert nur den ZEIGER auf
 * den Eintrag - vorher habe ich einen lokalen Puffer eingetragen, also zeigten
 * alle Zeilen auf denselben Speicher und damit auf den zuletzt geschriebenen
 * Namen ("beim Anklicken wird der Eintrag mit dem letzten ueberschrieben").
 */
#define GUI_FILE_MAX  512
static char   *s_filelines[GUI_FILE_MAX];
static int     s_filecount;

/* Pfad, den die Oberflaeche gerade zeigt ("" = Wurzel) */
static char    s_path[256];

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
        /*
         * Nur die aelteste Zeile entfernen (0.2). Vorher habe ich bei 400
         * Zeilen die ganze Liste geleert - MUIM_List_Clear verwirft dabei die
         * Scrollposition, danach laesst sich die Liste nicht mehr rollen.
         */
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
    /*
     * 0.4: MUIA_List_Quiet um das Einfuegen. Ohne das zerstoert MUI die
     * Darstellung beim Scrollen ("immer derselbe Eintrag") - die Liste wird
     * waehrend des Einfuegens nicht neu gezeichnet.
     */
    set(s_logl, MUIA_List_Quiet, TRUE);
    DoMethod(s_logl, MUIM_List_InsertSingle, copy, MUIV_List_Insert_Bottom);
    set(s_logl, MUIA_List_Quiet, FALSE);
}

static void gui_print(const char *s, unsigned len)
{
    if (len == 0u) {
        return;
    }
    unsigned i;

    /* 1. Standardausgabe (wie Projects/DebugTest). */
    (void)fwrite(s, 1u, len, stdout);
    (void)fflush(stdout);
    /* 2. Hardware-Debug-UART der V4 (ApolloCrossDev_Debug.c, 115200 Baud). */
    if (s_debug_uart != 0) {
        for (i = 0u; i < len; i++) {
            (void)ApolloDebugPutChar((int)s[i]);
        }
    }
    /* 3. Logdatei der Plattform. */
    (void)v4_plat_log_write(s, len);
}

/* Alle Meldungen des Programms laufen hier durch. */
static void gui_msg(const char *fmt, ...)
{
    char    buf[240];
    va_list ap;
    int     n;

    va_start(ap, fmt);
    n = vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    if (n < 0) {
        return;
    }
    if (n > (int)sizeof(buf) - 1) {
        n = (int)sizeof(buf) - 1;
    }
    gui_print(buf, (unsigned)n);            /* mit Umbruch: Konsole und UART */

    /* In die Liste ohne Umbruch - ein '\n' im Listeneintrag bringt MUI beim
     * Rechnen durcheinander (0.2). */
    while (n > 0 && (buf[n - 1] == '\n' || buf[n - 1] == '\r')) {
        buf[--n] = '\0';
    }
    if (n > 0) {
        gui_loglist_add(buf);
    }
}

/* Meldungen der Protokollschicht (Trace) ins Logfeld. */
static void gui_trace(const v4_trace_t *ev, void *ctx)
{
    (void)ctx;
    if (ev == NULL) {
        return;
    }
    gui_msg("[trace] ev=%u cmd=0x%02X seq=%u len=%u\n",
            (unsigned)ev->event, (unsigned)ev->cmd,
            (unsigned)ev->seq, (unsigned)ev->len);
}

/* ------------------------------------------------------------------ */
/* Status und Dateiliste                                              */
/* ------------------------------------------------------------------ */

static void gui_status_refresh(void)
{
    v4p_status_t st;
    uint8_t      rc;
    char         buf[220];

    rc = v4_get_status(&M, &st);
    if (rc != V4P_ST_OK) {
        (void)snprintf(buf, sizeof(buf), "Status nicht lesbar: %s", v4_strerror(rc));
    } else {
        (void)snprintf(buf, sizeof(buf),
                       "Zustand %u, Verbindung %s, SD %s%s, chunk %u, frei %lu KB, Ger. %u",
                       (unsigned)st.state,
                       (st.conn_index == 0xFFu) ? "keine" : "aktiv",
                       st.sd_mounted ? "eingehaengt" : "nicht eingehaengt",
                       st.sd_card_present ? "" : " (keine Karte)",
                       (unsigned)st.chunk,
                       (unsigned long)st.sd_free_kb,
                       (unsigned)st.dev_count);
    }
    if (s_status != NULL) {
        set(s_status, MUIA_Text_Contents, (IPTR)buf);
    }
    gui_msg("[status] %s\n", buf);
}

static void gui_list_clear(void)
{
    int i;

    if (s_files != NULL) {
        DoMethod(s_files, MUIM_List_Clear);
    }
    for (i = 0; i < s_filecount; i++) {
        free(s_filelines[i]);
        s_filelines[i] = NULL;
    }
    s_filecount = 0;
}

static void gui_files_refresh(void)
{
    uint8_t       handle = 0u;
    uint16_t      index;
    v4p_dirent_t  ent;
    uint8_t       rc;
    char          line[V4P_DIRNAME_MAX + 8u];
    int           shown = 0;

    gui_list_clear();
    rc = v4_dir_open(&M, s_path, &handle);
    if (rc != V4P_ST_OK) {
        gui_msg("[liste] Verzeichnis \"%s\" nicht lesbar: %s\n", s_path, v4_strerror(rc));
        return;
    }
    for (index = 0u; index < 512u; index++) {
        int versuch;

        memset(&ent, 0, sizeof(ent));
        /*
         * 0.3: Die Bruecke verzoegert jede Eintragsanfrage - der v4_work-Task
         * liest den Eintrag nach, und der Master fragt denselben Index erneut.
         * Genau das tut die Textkonsole, meine GUI tat es nicht und brach beim
         * ersten "noch nicht fertig" ab: "[liste] '': 0 Eintraege", obwohl 73
         * Eintraege da sind. Nur V4P_ST_END heisst wirklich "keine mehr".
         */
        for (versuch = 0; versuch < 12; versuch++) {
            rc = v4_dir_next(&M, handle, index, &ent);
            if (rc == V4P_ST_OK || rc == V4P_ST_END) {
                break;
            }
            v4_plat_delay_us(20000u);       /* 20 ms, wie die Konsole */
        }
        if (rc == V4P_ST_END) {
            break;
        }
        if (rc != V4P_ST_OK) {
            gui_msg("[liste] Index %u nicht lesbar (nach 12 Versuchen): %s\n",
                    (unsigned)index, v4_strerror(rc));
            break;
        }
        if (ent.name[0] == '\0') {
            continue;
        }
        /* Verzeichnisse mit Schraegstrich kennzeichnen - so weiss der
         * Anwender, was ein Enter und was ein Abspielen ausloest. */
        char *copy;

        (void)snprintf(line, sizeof(line), "%s%s", ent.name,
                       ((ent.attr & 0x10u) != 0u) ? "/" : "");
        if (s_filecount >= GUI_FILE_MAX) {
            break;
        }
        copy = (char *)malloc(strlen(line) + 1u);
        if (copy == NULL) {
            break;
        }
        (void)strcpy(copy, line);
        s_filelines[s_filecount++] = copy;          /* Kopie behaelt MUI */
        DoMethod(s_files, MUIM_List_InsertSingle, copy, MUIV_List_Insert_Bottom);
        shown++;
    }
    (void)v4_dir_close(&M, handle);
    gui_msg("[liste] \"%s\": %d Eintraege\n", s_path, shown);
}

/* Name des gerade markierten Eintrags, ohne Verzeichnis-Schraegstrich. */
static int gui_selected(char *out, unsigned out_len, int *is_dir)
{
    char *entry = NULL;

    if (s_files == NULL) {
        return -1;
    }
    if (DoMethod(s_files, MUIM_List_GetEntry, MUIV_List_GetEntry_Active,
                 &entry) == 0 || entry == NULL) {
        return -1;
    }
    (void)strncpy(out, entry, out_len - 1u);
    out[out_len - 1u] = '\0';
    *is_dir = 0;
    if (out[0] != '\0' && out[strlen(out) - 1u] == '/') {
        out[strlen(out) - 1u] = '\0';
        *is_dir = 1;
    }
    return 0;
}

/* Pfad aus s_path und Namen zusammensetzen (ohne fuehrenden Schraegstrich,
 * so wie es die Protokollseite erwartet). */
static void gui_path_join(const char *name, char *out, unsigned out_len)
{
    if (s_path[0] == '\0') {
        (void)snprintf(out, out_len, "%s", name);
    } else {
        (void)snprintf(out, out_len, "%s/%s", s_path, name);
    }
}

/* ------------------------------------------------------------------ */
/* Aktionen                                                           */
/* ------------------------------------------------------------------ */

static void act_refresh(void)
{
    gui_status_refresh();
    gui_files_refresh();
}

static void act_up(void)
{
    char *slash;

    if (s_path[0] == '\0') {
        gui_msg("[liste] schon in der Wurzel\n");
        return;
    }
    slash = strrchr(s_path, '/');
    if (slash == NULL) {
        s_path[0] = '\0';
    } else {
        *slash = '\0';
    }
    gui_files_refresh();
}

static void act_play(void)
{
    char name[V4P_DIRNAME_MAX + 4u];
    char path[sizeof(s_path) + sizeof(name)];
    int  is_dir = 0;
    uint8_t rc;

    if (gui_selected(name, sizeof(name), &is_dir) != 0) {
        gui_msg("[play] nichts markiert\n");
        return;
    }
    gui_path_join(name, path, sizeof(path));

    /*
     * 0.4: Erst versuchen, den Eintrag als Verzeichnis zu oeffnen. Gelingt das,
     * wird hineingewechselt - sonst als Datei abgespielt. Damit haengt die
     * Navigation weder am Schraegstrich noch am Attributbit (das war der Grund,
     * warum der Wechsel in Unterverzeichnisse nicht funktionierte).
     */
    {
        uint8_t probe = 0u;

        if (v4_dir_open(&M, path, &probe) == V4P_ST_OK) {
            (void)v4_dir_close(&M, probe);
            (void)snprintf(s_path, sizeof(s_path), "%s", path);
            gui_msg("[liste] wechsle nach \"%s\"\n", s_path);
            gui_files_refresh();
            return;
        }
    }
    rc = v4_play_file(&M, path);
    gui_msg("[play] %s -> %s\n", path, v4_strerror(rc));
    gui_status_refresh();
}

static void act_stop(void)
{
    uint8_t rc = v4_stop_play(&M);

    gui_msg("[stop] %s\n", v4_strerror(rc));
    gui_status_refresh();
}

static void act_connect(void)
{
    uint8_t     count = 0u;
    v4p_dev_t   dev;
    uint8_t     rc;

    rc = v4_scan_start(&M, 8u, 0);
    gui_msg("[connect] Scan: %s\n", v4_strerror(rc));

    rc = v4_dev_count(&M, &count);
    if (rc != V4P_ST_OK || count == 0u) {
        gui_msg("[connect] kein Geraet gefunden (%s, %u)\n", v4_strerror(rc), (unsigned)count);
        return;
    }
    memset(&dev, 0, sizeof(dev));
    rc = v4_dev_get(&M, 0u, &dev);
    if (rc != V4P_ST_OK) {
        gui_msg("[connect] Geraet 0 nicht lesbar: %s\n", v4_strerror(rc));
        return;
    }
    gui_msg("[connect] Geraet 0: %s\n", dev.name);
    rc = v4_connect(&M, dev.idx);
    gui_msg("[connect] %s -> %s\n", dev.name, v4_strerror(rc));
    gui_status_refresh();
}

static void act_disconnect(void)
{
    uint8_t rc = v4_disconnect(&M);

    gui_msg("[trennen] %s\n", v4_strerror(rc));
    gui_status_refresh();
}

/* ------------------------------------------------------------------ */
/* MUI                                                                */
/* ------------------------------------------------------------------ */

static BOOL open_libs(void)
{
    IntuitionBase = (struct IntuitionBase *)OpenLibrary((CONST_STRPTR)"intuition.library", 39);
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
    }
    if (GfxBase != NULL) {
        CloseLibrary((struct Library *)GfxBase);
    }
    if (IntuitionBase != NULL) {
        CloseLibrary((struct Library *)IntuitionBase);
    }
}


static int gui_build(void)
{
    Object *btn_refresh, *btn_up, *btn_play, *btn_stop, *btn_conn, *btn_disconn;

    s_app = ApplicationObject,
        MUIA_Application_Title,       (IPTR)"V4 Steuerung v" GUI_VERSION,
        MUIA_Application_Version,     (IPTR)GUI_VER_STR,
        MUIA_Application_Copyright,   (IPTR)" ",
        MUIA_Application_Author,      (IPTR)" ",
        MUIA_Application_Description, (IPTR)"Vampire V4 ueber ESP32 steuern",
        MUIA_Application_Base,        (IPTR)"V4GUI",

        MUIA_Application_Window, s_win = WindowObject,
            MUIA_Window_Title, "V4 Steuerung v" GUI_VERSION,
            MUIA_Window_ID,    MAKE_ID('V', '4', 'G', 'I'),
            /*
             * Feste Anfangsgroesse (0.2). Ohne sie zieht MUI das Fenster auf
             * die Inhaltsgroesse: bei 73 Dateien und hunderten Logzeilen wird
             * es breiter als der Bildschirm, und die rechte Spalte (Logfeld)
             * liegt ausserhalb - genau der gemeldete Scrollfehler. Die Listen
             * bekommen so eine feste Flaeche und eigene Rollbalken.
             */
            MUIA_Window_Width,  700,
            MUIA_Window_Height, 420,
            WindowContents, VGroup,
                Child, s_status = TextObject,
                    MUIA_Text_Contents, (IPTR)"Verbinde ...",
                    MUIA_Text_PreParse, (IPTR)"\33c",
                End,
                Child, HGroup,
                    Child, btn_refresh = MUI_MakeObject(MUIO_Button, (IPTR)"Aktualisieren", NULL),
                    Child, btn_up      = MUI_MakeObject(MUIO_Button, (IPTR)"Hoch", NULL),
                    Child, btn_play    = MUI_MakeObject(MUIO_Button, (IPTR)"Abspielen", NULL),
                    Child, btn_stop    = MUI_MakeObject(MUIO_Button, (IPTR)"Stopp", NULL),
                    Child, btn_conn    = MUI_MakeObject(MUIO_Button, (IPTR)"Verbinden", NULL),
                    Child, btn_disconn = MUI_MakeObject(MUIO_Button, (IPTR)"Trennen", NULL),
                End,
                Child, HGroup,
                    Child, ListviewObject,
                        MUIA_Listview_List, s_files = ListObject,
                        End,
                    End,
                    Child, ListviewObject,
                        MUIA_Listview_List, s_logl = ListObject,
                        End,
                    End,
                End,
            End,
        End,
    End;

    if (s_app == NULL || s_win == NULL) {
        gui_msg("MUI: Anwendung konnte nicht angelegt werden\n");
        return -1;
    }
    s_gui_up = 1;

    /* Schliessen und jeder Knopf schicken eine ReturnID in die Hauptschleife.
     * MUIA_Pressed/FALSE loest beim Loslassen aus - das uebliche MUI-Muster. */
    DoMethod(s_win, MUIM_Notify, MUIA_Window_CloseRequest, TRUE,
             s_app, 2, MUIM_Application_ReturnID, ID_QUIT);
    DoMethod(btn_refresh, MUIM_Notify, MUIA_Pressed, FALSE,
             s_app, 2, MUIM_Application_ReturnID, ID_REFRESH);
    DoMethod(btn_up, MUIM_Notify, MUIA_Pressed, FALSE,
             s_app, 2, MUIM_Application_ReturnID, ID_UP);
    DoMethod(btn_play, MUIM_Notify, MUIA_Pressed, FALSE,
             s_app, 2, MUIM_Application_ReturnID, ID_PLAY);
    DoMethod(btn_stop, MUIM_Notify, MUIA_Pressed, FALSE,
             s_app, 2, MUIM_Application_ReturnID, ID_STOP);
    DoMethod(btn_conn, MUIM_Notify, MUIA_Pressed, FALSE,
             s_app, 2, MUIM_Application_ReturnID, ID_CONNECT);
    DoMethod(btn_disconn, MUIM_Notify, MUIA_Pressed, FALSE,
             s_app, 2, MUIM_Application_ReturnID, ID_DISCONN);
    return 0;
}

/* ------------------------------------------------------------------ */
/* main                                                               */
/* ------------------------------------------------------------------ */

static void usage(void)
{
    gui_msg("v4_gui " GUI_VERSION " -- " GUI_VER_STR "\n");
    gui_msg("Aufruf: v4_gui [-noser] [-l <logdatei>] [-d <i2c-device>]\n");
    gui_msg("  -noser  keine Ausgabe ueber ser: (COM-Port)\n");
    gui_msg("  -l      Logdatei der Plattform\n");
    gui_msg("  -d      I2C-Geraet, Vorgabe von i2c.library\n");
}

int main(int argc, char **argv)
{
    int   i;
    ULONG signals;
    BOOL  running = TRUE;
    uint8_t rc;
    v4p_info_t info;

    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-nodebug") == 0) {
            s_debug_uart = 0;
        } else if (strcmp(argv[i], "-trace") == 0) {
            s_trace = 1;
        } else if (strcmp(argv[i], "-l") == 0 && i + 1 < argc) {
            s_log_path = argv[++i];
        } else if (strcmp(argv[i], "-d") == 0 && i + 1 < argc) {
            s_dev_path = argv[++i];
        } else {
            usage();
            return 1;
        }
    }

    /* Logdatei der Plattform oeffnen, damit auch Startfehler darin stehen. */
    (void)v4_plat_log_open(s_log_path);

    if (s_debug_uart != 0) {
        ApolloDebugInit();          /* CIAB + SERPER: 115200 Baud am COM-Port */
    }

    /* Versionsstring zuerst - auf allen Kanaelen. */
    gui_msg(GUI_VER_STR "\n");
    gui_msg("v4_gui " GUI_VERSION " (" GUI_DATUM "), Debug-UART %s\n",
            (s_debug_uart != 0) ? "an (115200 Baud)" : "aus");
    gui_msg("Ausgabe: stdout + Debug-UART + Logdatei %s\n",
            (s_log_path != NULL) ? s_log_path : "(Vorgabe der Plattform)");

    v4_init(&M);
    /*
     * Rahmen-Trace nur auf Wunsch (-trace). Vorgabe aus: jeder Befehl erzeugt
     * acht Zeilen, eine Verzeichnisliste damit Hunderte - das hatte der erste
     * Lauf auf der V4 gezeigt. Die Konsole hat dafuer ihr -a.
     */
    M.trace     = (s_trace != 0) ? gui_trace : NULL;
    M.trace_ctx = NULL;

    rc = (uint8_t)v4_plat_open(s_dev_path);
    gui_msg("Plattform oeffnen: %s\n", v4_strerror(rc));
    if (rc != V4P_ST_OK) {
        gui_msg("Ohne I2C keine Steuerung - Abbruch\n");
        return 2;
    }

    memset(&info, 0, sizeof(info));
    rc = v4_get_info(&M, &info);
    if (rc == V4P_ST_OK) {
        gui_msg("Gegenstelle: Firmware %u, Protokoll %u, Chunk %u\n",
                (unsigned)info.fw_ver, (unsigned)info.proto_ver, (unsigned)info.chunk);
    } else {
        gui_msg("GET_INFO: %s\n", v4_strerror(rc));
    }

    if (open_libs() == FALSE) {
        gui_msg("MUI/intuition nicht offen - Abbruch\n");
        return 3;
    }
    if (gui_build() != 0) {
        close_libs();
        return 4;
    }

    set(s_win, MUIA_Window_Open, TRUE);
    s_path[0] = '\0';
    act_refresh();

    while (running != FALSE) {
        ULONG id = DoMethod(s_app, MUIM_Application_Input, &signals);

        switch (id) {
        case ID_QUIT:
            running = FALSE;
            break;
        case ID_REFRESH:
            act_refresh();
            break;
        case ID_UP:
            act_up();
            break;
        case ID_PLAY:
            act_play();
            break;
        case ID_STOP:
            act_stop();
            break;
        case ID_CONNECT:
            act_connect();
            break;
        case ID_DISCONN:
            act_disconnect();
            break;
        default:
            break;
        }
        if (running != FALSE && signals != 0u) {
            (void)Wait(signals);
        }
    }

    gui_msg("v4_gui beendet\n");
    set(s_win, MUIA_Window_Open, FALSE);
    MUI_DisposeObject(s_app);
    s_gui_up = 0;
    close_libs();
    v4_plat_log_close();
    return 0;
}
