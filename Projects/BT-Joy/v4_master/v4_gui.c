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
 *   - Umschalter "Seite" mit ZWEI Seiten (MUIA_Group_PageMode):
 *       Seite "Anzeige"        die beiden Buchsen -- NUR die direkte
 *                              Registerabfrage (JOY0DAT/JOY1DAT fuer die
 *                              Richtungen, CIAAPRA/POTINP fuer die Knoepfe)
 *                              samt Rohwerten und der WIRKSAMEN Betriebsart
 *                              daneben. Controller-Eingabewerte kommen
 *                              AUSSCHLIESSLICH von hier; GET_INPUT wird nicht
 *                              mehr abgefragt, und es gibt keinen Vergleich
 *                              "direkt <-> I2C" -- weder im Fenster noch im
 *                              Protokoll.
 *       Seite "Konfiguration"  die Eingangsverteilung (fw_ver 4/5): je Buchse
 *                              Geraet (SET_DEVICE_PORT), Beitrag
 *                              analog/digital/beide (Bedienhilfe, setzt
 *                              SET_PORT_ROUTING), Richtungen (drei Schalter),
 *                              die drei Knopfquellen Pin 6/Pin 9/Pin 5 sowie
 *                              Maus-Tempo und Maus-Totzone
 *                              (GET/SET_MOUSE_CFG), dazu der Klartext beider
 *                              Buchsen
 *   - Knoepfe: Neu suchen, Geraeteliste/Verbinden, Trennen, Vergessen,
 *     Port 0/1 Vorgabe weiterschalten, Buchsen tauschen
 *   - Logfeld mit allen Meldungen
 *
 * WARUM DIE SEITEN IN EINER SCROLLGRUPPE LIEGEN (15.10.2026): die Mindesthoehe
 * einer Seitengruppe ist das MAXIMUM ueber alle Seiten (MUI_Group.doc). Ist
 * eine Seite hoeher als das Fenster, drueckt MUI die Felder zusammen oder
 * schneidet sie ab -- am Geraet war die Konfigurationsseite dann erst nach dem
 * Vergroessern des Fensters bedienbar. Scrollgroup + Virtgroup (TryFit FALSE)
 * nehmen den Seiten die Macht, das Fenster zu vergroessern; gescrollt wird nur,
 * wenn es nicht passt (AutoBars). Der Umschalter "Seite:" steht AUSSERHALB und
 * ist damit immer sichtbar. Nach dem Oeffnen und bei jedem Seitenwechsel
 * schreibt die Oberflaeche die von MUI BERECHNETEN Mindestmasse ins Protokoll
 * ([fenster] Mindestgroesse ...), damit "passt nicht" belegbar ist.
 *
 * WARUM ZWEI SEITEN UND NICHT EINE LAENGERE LISTE: mit fw_ver 4 ist der Inhalt
 * so gewachsen, dass er nicht mehr untereinander passt. MUI_Group.doc sagt zur Seitengruppe: "the minimum
 * width/height of a page group is the maximum minimum width/height of all its
 * children". Die Anzeigeseite ist durch den Wegfall der Eingangsabfrage KLEINER
 * geworden; die Konfigurationsseite bleibt durch die neue Zeile je Einstellung
 * in derselben Groessenordnung. Die Mindestgroesse des Fensters aendert sich
 * damit nicht nach oben, und nichts wird unter seine Mindesthoehe gedrueckt
 * (das war der Befund "leere Felder").
 *
 * DIE EINGANGSVERTEILUNG GEHT SOFORT ZUM ESP32: jede Aenderung an einem
 * Auswahlfeld oder Schalter ruft direkt SET_DEVICE_PORT, SET_PORT_MODE oder
 * SET_PORT_ROUTING -- im Hauptfaden, wie alles andere. Danach wird
 * GET_PORT_CONFIG gelesen und die Felder werden auf den ECHTEN Zustand
 * gestellt. Lehnt der ESP32 ab (BAD_ARG, z.B. eine doppelt vergebene
 * Knopfquelle), steht der Grund im Klartext im Fenster und im Protokoll, und
 * die Anzeige bleibt auf dem letzten bestaetigten Stand.
 *
 * WAS DIE GEGENSTELLE NICHT HERGIBT: welche BDA welcher Buchse zugeordnet ist,
 * ist ueber die Leitung NICHT lesbar (DEV_GET liefert nur Name und Adresse,
 * GET_PORT_CONFIG nur die Buchsenfelder). Die Oberflaeche zeigt deshalb die
 * Zuordnungen, die in DIESER Sitzung per SET_DEVICE_PORT gesetzt und
 * bestaetigt wurden -- die Zuordnung selbst liegt im NVS des ESP32 und bleibt
 * dort. Ein "Loesen" gibt es nicht: der ESP32 kennt nur BDA -> Buchse.
 *
 * DIREKT GELESEN WIRD IM SELBEN TAKT UND IM SELBEN FADEN:
 * Die Registerabfrage (v4_joyport.c) kostet nichts und braucht keinen Bus. Sie
 * laeuft deshalb in JEDEM Takt -- und sie ist seit dem 14.10.2026 die EINZIGE
 * Quelle fuer das, was der Controller tut. I2C wird nur noch im LANGSAMEN Takt
 * gefragt (Status, HID, Verteilung, Maus), und zwar in gui_poll. Die
 * Registerwerte gehen (a) in die EINE Herzzeile je Sekunde und (b) sofort als
 * eigene Zeile, wenn sich Richtung oder Feuerknopf aendern. Einen Vergleich mit
 * der ESP32-Sicht gibt es nicht mehr: GET_INPUT wird nicht mehr abgefragt.
 * Die Stabilitaet der Anzeige kommt daher, dass jede Buchse aus ZWEI Abtastungen
 * bewertet wird (siehe v4_joyport.h).
 *
 * DIE AUSGABEMENGE IST TEIL DER KONSTRUKTION:
 * Auf der V4 ist der DebugUART das einzige Fenster in das Programm -- aber
 * jedes Zeichen kostet Wartezeit. Deshalb:
 *
 *   - im Ruhezustand GENAU EINE Zeile je Sekunde: das Lebenszeichen [herz] mit
 *     Zaehlern, letztem Status, verworfenen Rahmen, Wiederholungen UND den
 *     direkten Registerwerten beider Buchsen samt Rohwerten von CIAAPRA/POTINP,
 *   - SOFORT eine Zeile bei Aenderung: Richtung, Feuerknopf, Buchsenmodus,
 *     Sitzvergabe, Fehler, Verbindungsverlust und -erholung,
 *   - Transaktionszeilen (eine je I2C-Befehl) NUR mit -trace. Ohne -trace
 *     entstehen bei 200 ms Takt keine Zeilen.
 *
 * Jede Meldung ist EINE fertige Zeile und geht mit EINEM ApolloDebugPutStr
 * heraus (v4_debug.c); kein Zeichen fuer Zeichen, kein Delay() in der Naehe des
 * Buszugs.
 *
 * "ZUG" UND "PEGEL" GIBT ES NUR NOCH IN DER KONSOLE:
 * Die MUI-Oberflaeche liest GET_INPUT nicht mehr und kennt deshalb weder
 * state0/state1 noch level0/level1. Was sie zeigt, sind die direkt gelesenen
 * Registerwerte. Die Konsole als Notfallweg und Diagnosewerkzeug behaelt
 * GET_INPUT und damit die Zug-/Pegel-Spalten samt Widerspruchspruefung.
 *
 * I2C DIREKT IM HAUPTFADEN (MUI-Task):
 * Die Oberflaeche ruft v4_master -- und damit v4_plat_* und i2c.library --
 * SELBST auf, aus ihrem Ereignisablauf heraus. Kein CreateNewProc, kein
 * zweiter Faden, kein Nachrichtenport, kein Forbid(). Genau diese Konstruktion
 * ist auf der V4 gelaufen; die Fassung mit eigener I2C-Aufgabe kam am
 * 13.10.2026 nicht einmal durch ihren Start (die Oberflaeche blieb nach dem
 * Banner stehen), und ihr Forbid() hat zusaetzlich die Workbench eingefroren.
 *
 * Der Takt (200 ms) liest die Register der V4. I2C wird nur im LANGSAMEN Takt
 * gefragt (jeder fuenfte, rund 1 s): GET_STATUS, GET_PORT_MODE, GET_HID,
 * GET_PORT_CONFIG, GET_MOUSE_CFG -- alles Einstellungen, Verbindungszustand
 * oder Geraeteinfos, KEINE Controller-Eingabewerte. Waehrend einer Suche kommt
 * alle drei Takte DEV_COUNT dazu. Alles laeuft direkt, synchron, im MUI-Task.
 * i2c.library hat keinen Timeout: Antwortet der ESP32 nicht, steht
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
 * Fenster 700x480 (Vorgabe) mit SizeGadget; -w/-h aendern die Groesse, mit der Maus geht
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
 * Vorgabe 700x480 wird damit verglichen: passt sie nicht, wird sie auf den
 * Schirm begrenzt (siehe gui_fenster_an_schirm).
 */
#define GUI_SCHIRM_RAND_W 16
#define GUI_SCHIRM_RAND_H 24

/*
 * Wie lange auf Scanergebnisse bzw. CONNECTED gewartet wird, in Takten.
 *
 * WARUM DER SCAN 30 s BEKOMMT (vorher 8 s): eine BluePad32-Suche laeuft in
 * eigenen Fenstern, und im Mitschnitt vom 13.10.2026 lagen zwischen SCAN_START
 * und der ersten brauchbaren Statusantwort 25 s. Mit 40 Takten (8 s nominal)
 * gab die Oberflaeche auf, bevor die Gegenstelle ueberhaupt Ergebnisse haben
 * konnte -- die Meldung "keine Geraete gefunden" war dann keine Aussage ueber
 * den Controller, sondern ueber die zu kurze Frist.
 *
 * GUI_SCAN_MAX_TICKS ist die HARTE Obergrenze: solange die Gegenstelle laut
 * GET_STATUS noch sucht, wird nachgefasst, aber nie laenger als das.
 * GUI_SCAN_POLL_EVERY haelt den Bus ruhig: DEV_COUNT wird nicht in jedem Takt
 * gefragt, sondern nur alle drei (rund 1,7-mal je Sekunde statt 5-mal).
 */
#define GUI_SCAN_TICKS     150u   /* 30 s Regelwartezeit */
#define GUI_SCAN_MAX_TICKS 300u   /* 60 s harte Obergrenze */
#define GUI_SCAN_POLL_EVERY 3u    /* DEV_COUNT nur alle drei Takte */
#define GUI_CONNECT_TICKS  30u    /* 6 s  */

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
/* Eingangsverteilung (fw_ver 4): je Buchse ein Ausloeser fuer Geraet,
 * Richtungen und Knopfquellen. Die drei Richtungsschalter bzw. die drei
 * Knopf-Auswahlfelder einer Buchse werden im Handler gemeinsam gelesen --
 * ein ReturnID je EINZELNEM Feld waere nur mehr Buchhaltung. */
#define ID_DEV0      10
#define ID_DEV1      11
#define ID_BTN0      14
#define ID_BTN1      15
/* Die beiden Knoepfe "Port N Modus >": sie schalten die VORGABE EINEN Schritt
 * weiter. Das ist ein eigener ReturnID, weil das Auswahlfeld selbst schon auf
 * ID_MODE0/ID_MODE1 hoert (dort wird der Wert direkt gesendet). */
#define ID_MODE0_NEXT 16
#define ID_MODE1_NEXT 17
/* Der Umschalter "Seite" (Anzeige / Konfiguration). */
#define ID_SEITE      18
/* Die Beitrags-Auswahl je Buchse (analog / digital / beide / eigene). */
#define ID_BEITRAG0   19
#define ID_BEITRAG1   20
/* Die Maus-Einstellungen je Buchse: Tempo-Auswahl und Totzone -/+ . */
#define ID_MSPEED0    21
#define ID_MSPEED1    22
#define ID_MTOT0M     23
#define ID_MTOT0P     24
#define ID_MTOT1M     25
#define ID_MTOT1P     26
/* duo (fw_ver 6): EIN Controller auf beiden Buchsen. */
#define ID_DUO        27

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
 * SIE SIND DIE EINZIGE QUELLE FUER DAS, WAS DER CONTROLLER TUT. Die
 * Oberflaeche fragt seit dem 14.10.2026 kein GET_INPUT mehr ab: was auf den
 * DB9-Leitungen ankommt, steht in den Registern der V4 (JOY0DAT/JOY1DAT,
 * CIAAPRA/POTINP). S haelt daneben nur noch, was EINSTELLUNGEN und
 * Verbindungszustand sind (Status, HID-Info, Buchsenverteilung, Maus-Werte).
 *
 * Auch J gehoert DIESEM Faden; es gibt keinen zweiten.
 */
static v4_joy_t    J;

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
 * WARUM 700x480 (16.10.2026): mit 640x400 stand die Seite "Ports" genau auf
 * der Kante -- der Benutzer musste das Fenster von Hand leicht vergroessern,
 * danach war es in Ordnung. Der Schirm dieser V4 ist 1280x720, es ist also
 * Platz. 700x480 laesst beiden Spalten und allen Seiten Luft, ohne den
 * Kopfbereich oder die Listen zu druecken. Wer mehr oder weniger will, zieht
 * das Fenster auf oder gibt die Groesse beim Aufruf an; auf einem KLEINEREN
 * Schirm wird weiterhin auf den Schirm begrenzt (gui_fenster_an_schirm).
 */
static int s_fenster_w = 700;
static int s_fenster_h = 480;
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
static Object *s_root;                /* Fensterinhalt (oberste VGroup)   */
static Object *s_link;                /* Verbindungszeile (Ausfallhinweis) */
static Object *s_info;                /* Kopfzeile                        */
static Object *s_ctrl;                /* Controller-Zeile                 */
static Object *s_state;               /* Zustandszeile                    */
static Object *s_port0;               /* Portgruppe 0                     */
static Object *s_port1;               /* Portgruppe 1                     */
static Object *s_regtext;             /* Rohregister (direkt), volle Breite */
static Object *s_mode0;               /* Cycle Vorgabe Betriebsart Port 0 */
static Object *s_mode1;               /* Cycle Vorgabe Betriebsart Port 1 */

/*
 * Die Eingangsverteilung (fw_ver 4) in der Oberflaeche -- ein Satz Objekte je
 * Buchse:
 *
 *   s_dev0/s_dev1        Geraet aus der Scanliste (SET_DEVICE_PORT)
 *   s_rdir0/s_rdir1      Richtungen als KLARTEXT -- nur Anzeige, kein Schalter
 *                        (die Werte setzt der Beitrag, siehe unten)
 *   s_b0/s_b1            Pin 6, Pin 9, Pin 5 als Auswahlfeld
 *   s_wirk0/s_wirk1      "wirkt: <Betriebsart>" neben dem Auswahlfeld
 *   s_cfgtext            Kopf, Zusammenfassung und Meldungen im Klartext
 */
static Object *s_dev0, *s_dev1;
static Object *s_rdir0, *s_rdir1;     /* Richtungen: NUR Anzeige (Klartext) */
static Object *s_b0[3], *s_b1[3];
static Object *s_wirk0, *s_wirk1;
static Object *s_cfgtext;
/*
 * Beitrag je Buchse (Bedienhilfe analog/digital/beide) und die
 * Maus-Einstellungen je Buchse (Tempo als Cycle 1..10, Totzone als Text mit
 * "-"/"+"-Knoepfen). s_cfghint ist der dynamische Hinweis unter der Tabelle:
 * er nennt die Beitrags-Bedeutung, den "gegenstandslos"-Fall und -- wenn das
 * Faehigkeitsbit fehlt -- dass die Maus-Regler nicht verfuegbar sind.
 */
static Object *s_beitrag0, *s_beitrag1;
static Object *s_mspeed0, *s_mspeed1;
static Object *s_mtotal0, *s_mtotal1;
static Object *s_mtot0m, *s_mtot0p, *s_mtot1m, *s_mtot1p;
static Object *s_duo;                 /* duo-Auswahlfeld (Seite "Ports")   */
static Object *s_duotext;             /* duo: Klartext daneben             */
static Object *s_cfghint;             /* Hinweis auf der Seite "Ports"     */
static Object *s_maushint;            /* Hinweis auf der Seite "Maus"      */
/*
 * Seitenumschalter: s_seite ist das Auswahlfeld ("Anzeige"/"Konfiguration"),
 * s_seiten die Seitengruppe (MUIA_Group_PageMode) mit den beiden Seiten.
 *
 * s_scroll/s_virt sind die Scrollgruppe und die virtuelle Gruppe um die
 * Seitengruppe: sie halten das Fenster bei jeder Groesse bedienbar, auch wenn eine
 * Seite hoeher ist als der Platz (siehe der Aufbau unten).
 */
static Object *s_seite;
static Object *s_seiten;
static Object *s_scroll;              /* Scrollgroup um die Seitengruppe   */
static Object *s_virt;                /* Virtgroup (Inhalt der Scrollgroup)*/
static Object *s_seite_anzeige;       /* Seite 0 (Anzeige)                 */
static Object *s_seite_ports;         /* Seite 1 (Ports: Geraet/Beitrag/..)*/
static Object *s_seite_maus;          /* Seite 2 (Maus: Tempo/Totzone)     */

/*
 * Die Eintraege der beiden Cycle-Knoepfe hinter "Betriebsart".
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
 * Der ERSTE Eintrag ist "auto" (V4P_MODE_AUTO, 0xFF): das ist die VORGABE
 * "nach Geraeteart" aus GET_PORT_CONFIG.mode_soll. Der Index im Auswahlfeld
 * ist deshalb NICHT der Wert -- die Umrechnung machen gui_mode_index() und
 * gui_mode_aus_index(). Die folgenden drei Eintraege sind die Betriebsarten
 * 0/1/2 in der Reihenfolge von V4P_PORT_*.
 *
 * Das Feld ist ABSICHTLICH ortsfest (static): MUI merkt sich den Zeiger auf
 * das Array. Ein Puffer auf dem 4-KB-Stapel waere hier die klassische Falle.
 */
static const char *const gui_modus_eintraege[] = {
    "auto", "aus", "Joystick", "Maus", NULL
};

/*
 * Sicherung zur UEBERSETZUNGSZEIT: das Array muss mit NULL enden -- MUI liest
 * die Eintraege bis NULL. Wer den Abschluss entfernt, bekommt hier einen
 * Uebersetzungsfehler statt eines leeren Auswahlfelds auf der V4.
 * (V4P_PORT_MODE_MAX ist die letzte Betriebsart "Maus"; dazu kommen "auto",
 * die Betriebsart 0 "aus" und der NULL-Abschluss.)
 */
typedef char gui_modus_eintraege_mit_null[
    (sizeof(gui_modus_eintraege) / sizeof(gui_modus_eintraege[0])
     == (V4P_PORT_MODE_MAX + 3u)) ? 1 : -1];

/*
 * Die Knopfquellen fuer die drei Leitungen Pin 6, Pin 9, Pin 5. Die Reihenfolge
 * ist genau der Wert (V4P_TASTE_*): der Index im Auswahlfeld IST der Code.
 *
 * Kurzfassungen der drei Gruppen ("Gruppe 1" statt "Gruppe 1 (A,L1,L2,L3)")
 * waeren ohne Erklaerung nicht zu verstehen; die Erklaerung steht als Hinweis
 * unter den Zeilen und im Klartext in der Zusammenfassung (gui_show_portcfg).
 * Das haelt die drei Auswahlfelder schmal genug fuer zwei Buchsen nebeneinander.
 *
 * "Home" statt "Home/Capture": MUI rechnet die Breite eines Auswahlfelds aus
 * dem LAENGSTEN Eintrag, und drei Felder nebeneinander muessen in eine Spalte
 * von rund 250 Pixeln passen. Der volle Name steht in der Zusammenfassung und
 * im Hinweistext darunter.
 */
/*
 * duo (fw_ver 6). Der INDEX ist der WERT (V4P_DUO_*): 0 aus, 1 Port 0 Maus,
 * 2 Port 1 Maus. Die Eintraege sind KURZ (die volle Bedeutung steht im
 * Klartext daneben), weil MUI die Breite aus dem laengsten Eintrag rechnet.
 */
static const char *const gui_duo_eintraege[] = {
    "aus", "Port 0 Maus", "Port 1 Maus", NULL
};

typedef char gui_duo_eintraege_mit_null[
    (sizeof(gui_duo_eintraege) / sizeof(gui_duo_eintraege[0]) == 4u) ? 1 : -1];

static const char *const gui_taste_eintraege[] = {
    "keine", "A", "B", "X", "Y", "L1", "R1", "L2", "R2", "L3", "R3",
    "Start", "Select", "Home", "Gruppe 1", "Gruppe 2", "Gruppe 3", NULL
};

/* Dieselbe Sicherung wie oben: 17 Codes + NULL. */
typedef char gui_taste_eintraege_mit_null[
    (sizeof(gui_taste_eintraege) / sizeof(gui_taste_eintraege[0])
     == (V4P_TASTE_ANZAHL + 1u)) ? 1 : -1];

/*
 * DIESELBEN Codes in KURZ -- fuer die EINE Zeile "Knopf 6/9/5", in der drei
 * Auswahlfelder nebeneinander in eine Spalte von 250 px passen muessen. MUI
 * rechnet die Breite eines Cycles aus dem LAENGSTEN Eintrag; "Gruppe 1" und
 * "Select" waren dafuer zu breit. Die vollen Namen stehen unveraendert in der
 * Klartextzeile (v4p_taste_name) und im Protokoll.
 */
static const char *const gui_taste_kurz_eintraege[] = {
    "keine", "A", "B", "X", "Y", "L1", "R1", "L2", "R2", "L3", "R3",
    "Start", "Sel", "Home", "Grp1", "Grp2", "Grp3", NULL
};

typedef char gui_taste_kurz_eintraege_mit_null[
    (sizeof(gui_taste_kurz_eintraege) / sizeof(gui_taste_kurz_eintraege[0])
     == (V4P_TASTE_ANZAHL + 1u)) ? 1 : -1];

/*
 * Der "Beitrag" je Buchse -- die drei Varianten und der Anzeigezustand
 * "eigene".
 *
 * Der Index IST der Wert (V4P_BEITRAG_*): 0 analog, 1 digital, 2 beide,
 * 3 eigene. "eigene" ist KEINE waehlbare Variante, sondern die ehrliche
 * Anzeige fuer eine Verteilung, die zu keiner der drei passt (z.B. keine
 * Richtung oder eine Handbelegung) -- ohne diesen Eintrag muesste das Feld
 * eine Variante behaupten, die es nicht gibt.
 *
 * Wie bei gui_modus_eintraege gilt: MUI erwartet ein NULL-ABGESCHLOSSENES
 * Zeigerarray (STRPTR *), keine mit '|' getrennte Zeichenkette. Das Feld ist
 * ortsfest, weil MUI den Zeiger behaelt.
 */
static const char *const gui_beitrag_eintraege[] = {
    "analog", "digital", "beides", "eigene", NULL
};

/* Uebersetzungszeit-Sicherung: 3 waehlbare Varianten + "eigene" + NULL. */
typedef char gui_beitrag_eintraege_mit_null[
    (sizeof(gui_beitrag_eintraege) / sizeof(gui_beitrag_eintraege[0])
     == (V4P_BEITRAG_ANZAHL + 2u)) ? 1 : -1];

/*
 * Der Klartext im FENSTER. Die Variante heisst dort "beides" (so hat es der
 * Benutzer genannt); Konsole und Protokoll behalten "beide" aus v4_master.c.
 * Beide Schreibweisen bezeichnen denselben Wert -- der Unterschied ist reine
 * Beschriftung.
 */
static const char *gui_beitrag_name(uint8_t beitrag)
{
    if (beitrag == (uint8_t)V4P_BEITRAG_BEIDE) {
        return "beides";
    }
    return v4_beitrag_name(beitrag);
}

/*
 * Das Maus-Tempo als Stufe ("1".."10"). Der Index im Auswahlfeld ist
 * Stufe-1, weil es keine Stufe 0 gibt. Auch hier: ortsfest und
 * NULL-abgeschlossen.
 */
static const char *const gui_speed_eintraege[] = {
    "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", NULL
};

typedef char gui_speed_eintraege_mit_null[
    (sizeof(gui_speed_eintraege) / sizeof(gui_speed_eintraege[0])
     == ((V4P_MOUSE_SPEED_MAX - V4P_MOUSE_SPEED_MIN) + 2u)) ? 1 : -1];

/*
 * Die DREI Seiten: "Anzeige" (Portdaten), "Ports" (Geraet, Beitrag,
 * Richtungen als Klartext, Knopfquellen) und "Maus" (Tempo/Totzone je Port).
 * Der Inhalt ist mit fw_ver 4/5 so gewachsen, dass er nicht mehr untereinander
 * passt; eine Seitengruppe (MUIA_Group_PageMode) zeigt immer nur eine Seite.
 * Ihre Mindestgroesse ist laut MUI_Group.doc "the maximum minimum width/height
 * of all its children" -- deshalb liegt sie in einer Scrollgruppe, und deshalb
 * sind die Einstellungen auf ZWEI Seiten verteilt: so bleibt jede Seite unter
 * der Fensterhoehe und muss NICHT gescrollt werden.
 */
static const char *const gui_seiten_eintraege[] = {
    "Anzeige", "Ports", "Maus", NULL
};

/*
 * Die Geraeteauswahl je Buchse. Die Eintraege stehen NICHT fest: sie kommen aus
 * der Scanliste (DEV_COUNT/DEV_GET) und werden bei jeder gefuellten Liste neu
 * gebaut (gui_dev_eintraege_bauen). Der Zeiger muss deshalb ebenfalls ortsfest
 * sein -- MUI behaelt ihn, ein Stapelpuffer waere hier ein Absturz.
 *
 * Index 0 = "(keines)": der ESP32 kennt nur "BDA -> Buchse", es gibt KEINEN
 * Befehl zum Loesen einer Zuordnung. Die Auswahl von "(keines)" wird deshalb
 * ausdruecklich abgefangen und als "nicht moeglich" gemeldet, statt eine
 * Zuordnung vorzutaeuschen, die es nicht gibt.
 */
static const char *s_dev_eintraege[GUI_DEV_MAX + 2u] = { "(keines)", NULL };
/*
 * Gekuerzte Namen fuer die Auswahlfelder. Ein Name aus DEV_GET darf bis zu 49
 * Zeichen lang sein; das waere (zweimal nebeneinander) breiter als das Fenster
 * und wuerde die Spalten auseinanderziehen. Der VOLLE Name steht in der
 * Zusammenfassung (gui_show_portcfg) und in der Geraeteliste.
 */
static char        s_dev_kurz[GUI_DEV_MAX + 2u][24];

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

/*
 * 1 = die Gegenstelle kann die Eingangsverteilung (Faehigkeitsbit
 * V4P_CAP_PORT_ROUTING). Das ist die VERBINDLICHE Auskunft -- nicht die
 * Firmware-Version: fw_ver 4 benutzt denselben Rahmen und dieselbe proto_ver 3
 * und wird von v4_check_info() angenommen.
 *
 * Ist das Bit nicht gesetzt (aeltere Firmware), bleiben GET_PORT_CONFIG und
 * SET_PORT_ROUTING aus; die Auswahlfelder zeigen dann ausdruecklich einen
 * Hinweis statt erfundener Werte, und "auto" ist nicht waehlbar (SET_PORT_MODE
 * mit 0xFF kennt nur fw_ver 4).
 */
static int     s_cfg_ok;
/* 1 = die Gegenstelle kann SET_DEVICE_PORT (Faehigkeitsbit
 * V4P_CAP_DEVICE_PORT). Die beiden Bits sind unabhaengig voneinander -- die
 * Doku nennt sie getrennt, deshalb werden sie auch getrennt geprueft. */
static int     s_devport_ok;
/* 1 = die Gegenstelle kann die Maus-Einstellungen (Faehigkeitsbit
 * V4P_CAP_MOUSE_CFG, GET/SET_MOUSE_CFG 0x0B/0x0C, fw_ver 5). Auch das ist eine
 * EIGENE Faehigkeit -- fehlt sie, bleiben die Regler sichtbar gesperrt, statt
 * sinnlos zu senden. */
static int     s_mousecfg_ok;
/* 1 = die Gegenstelle kann duo (Faehigkeitsbit V4P_CAP_DUO, GET_DUO 0x0D /
 * SET_DUO 0x0E, fw_ver 6): EIN Controller auf BEIDEN Buchsen. Fehlt das Bit,
 * ist der Schalter sichtbar gesperrt und es wird nichts gesendet. */
static int     s_duo_ok;
static int     s_cfg_warned;          /* Hinweis nur EINMAL ins Log         */
static int     s_cfg_sperre;          /* 1 = gerade Anzeige nachziehen:
                                       * Notifies nicht als Befehl deuten   */
/* Anzahl der zuletzt in s_dev_eintraege gebauten Geraete (ohne NULL). */
static int     s_dev_eintraege_n = -1;

/* ------------------------------------------------------------------ */
/* Laufende Vorgaenge                                                  */
/*                                                                     */
/* Der Scan laeuft auf dem ESP32 asynchron, und CONNECT ist nur         */
/* ANGENOMMEN -- beides wird ueber mehrere Takte verfolgt. Es gibt aber  */
/* keine Auftragsnummern und keine Warteschlange mehr: der Takt ruft    */
/* v4_master direkt.                                                    */
/* ------------------------------------------------------------------ */

static unsigned s_scan_left;        /* Takte bis zur naechsten Entscheidung */
static unsigned s_scan_hard;        /* harte Obergrenze des Scans, in Takten */
static unsigned s_scan_poll;        /* Taktzaehler fuer DEV_COUNT          */
static int      s_scan_nachgefasst; /* 1 = "fasse nach" schon gemeldet    */
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
/*
 * 1 = die Gegenstelle sucht SELBST (ihr Scan laeuft schon); die Oberflaeche
 * will nur die Fundtabelle LESEN. Dann darf sie weder SCAN_START noch
 * SCAN_STOP senden -- sonst braeche sie die Suche ab, die sie gar nicht
 * gestartet hat.
 */
static int      s_scan_nur_lesen;
/*
 * Scan-Generation, deren Fundtabelle schon in der Liste steht. Damit wird die
 * Liste JE SUCHLAUF genau einmal geholt -- ohne Knopfdruck und ohne
 * Dauerlast.
 */
static unsigned s_liste_gen = 0xFFFFFFFFu;

static int      s_link_down_shown;  /* 0/1: was zuletzt im Log stand      */
static int      s_sd_warned;        /* Audio-Firmware-Warnung einmalig    */

/* Zuletzt GEMELDETER direkter Stand: je Buchse die Maske aus den Registern.
 * Nur wenn sie sich aendert, geht sofort eine Zeile heraus
 * (gui_joy_aenderungen). Einen I2C-Vergleich gibt es nicht mehr. */
static uint8_t  s_joy_dir_alt[V4P_PORT_COUNT];
static int      s_joy_stand_gueltig;

/*
 * Zuletzt GEMELDETE Maus-Einstellungen. GET/SET_MOUSE_CFG bekommen eine
 * EIGENE, immer eingeschaltete Protokollzeile (auch ohne -trace): nur so ist
 * nachvollziehbar, ob eine Tempo-Aenderung beim ESP32 ankommt und was er
 * zurueckliest. Eine Zeile entsteht nur bei einer AENDERUNG (nicht im Takt).
 */
static v4p_mousecfg_t s_maus_alt;
static int            s_maus_alt_gueltig;

/* Zuletzt GEMELDETER duo-Zustand -- dieselbe Regel: eine Zeile nur bei einer
 * Aenderung, nicht im Takt. */
static uint8_t s_duo_alt;
static int     s_duo_alt_gueltig;

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
/* Klartext der Faehigkeiten (fuer die Kopfzeile)                      */
/* ------------------------------------------------------------------ */

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
        { V4P_CAP_BATTERY,   "Batterie" },
        { V4P_CAP_PORT_ROUTING, "Verteilung" },
        { V4P_CAP_DEVICE_PORT,  "Geraetezuordnung" },
        { V4P_CAP_MOUSE_CFG,    "Maus-Einstellungen" },
        { V4P_CAP_DUO,          "duo" },
        { V4P_CAP_DEV_STATE,    "Geraetezustand" }
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
    /*
     * ERKLAERUNG STATT LEERER ZUSTAND: BluePad32 sucht nicht weiter, solange
     * ein Controller verbunden ist (der Benutzer hat genau das beobachtet:
     * "wenn verbunden laeuft kein scan mehr"). Ohne diesen Zusatz sieht die
     * Zeile aus, als sei die Suche kaputt. Gesucht wird erst wieder, wenn die
     * Verbindung endet -- das steht hier ausdruecklich.
     */
    {
        int verbunden = (st.state == V4P_STATE_CONNECTED
                         || st.state == V4P_STATE_SUSPENDED) ? 1 : 0;
        const char *suchzusatz =
            (verbunden != 0 && st.scan_active == 0u)
                ? " (Verbindung aktiv -- Suche pausiert)"
                : "";

        (void)snprintf(buf, sizeof(buf),
                       "Zustand %u (%s), Verbindung %s, Scan %s%s, Geraete %u, "
                       "Scan-Generation %u",
                       (unsigned)st.state, v4p_bt_state_name(st.state),
                       (st.conn_index == V4P_NO_INDEX) ? "keine" : "aktiv",
                       st.scan_active ? "laeuft" : "aus", suchzusatz,
                       (unsigned)st.dev_count, (unsigned)st.scan_gen);
    }
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
     * hoechstens so breit wie das Fenster sein, und Name, Art,
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
 * Eine Portgruppe zusammensetzen -- AUSSCHLIESSLICH aus den eigenen Registern.
 *
 * WAS HIER NICHT MEHR STEHT: die Sicht des ESP32 (state0/state1, level0/level1
 * aus GET_INPUT) und ein Vergleich. GET_INPUT wird nicht mehr abgefragt; das
 * Fenster zeigt, was an den Anschlussleitungen ankommt -- das ist die direkte
 * Abfrage (JOY0DAT/JOY1DAT fuer die Richtungen, CIAAPRA/POTINP fuer die
 * Knoepfe), siehe v4_joy_direkt_text(). Ins Protokoll gehen dieselben
 * Registerwerte (v4_joy_direkt_protokoll_text), kein Urteil.
 *
 * Die wirksame BETRIEBSART kommt weiterhin vom ESP32 (GET_PORT_MODE bzw.
 * GET_PORT_CONFIG.mode_ist): sie beschreibt, was der ESP32 an den Leitungen
 * TUT, und gehoert deshalb zur Anzeige -- ausdruecklich als "(ESP32)"
 * gekennzeichnet, damit die Herkunft klar bleibt.
 *
 * Gezeichnet wird AUCH OHNE gueltige I2C-Antwort: die Registerabfrage haengt
 * nicht am Bus. Fehlt die Betriebsart, steht "Modus ?" da -- kein alter Wert.
 */
static void gui_build_port_text(int port, uint8_t mode, int mode_gueltig,
                                char *buf, size_t cap)
{
    char direkt[V4_JOY_DIREKT_MAX];
    char kopf[64];

    if (mode_gueltig != 0) {
        (void)snprintf(kopf, sizeof(kopf), "Port %d  Modus %s (ESP32)",
                       port, v4p_port_mode_name(mode));
    } else {
        (void)snprintf(kopf, sizeof(kopf),
                       "Port %d  Modus ? (ESP32 antwortet nicht)", port);
    }
    (void)v4_joy_direkt_text(&J, port, direkt, sizeof(direkt));
    (void)snprintf(buf, cap, "%s\n%s", kopf, direkt);
}

/*
 * Die Rohwerte der Feuerregister ueber die volle Fensterbreite. Das steht in
 * einem EIGENEN Textobjekt, weil es in den beiden Portspalten zu breit waere.
 *
 * Er wird IMMER gezeichnet: die direkte Abfrage haengt nicht am Bus und muss
 * auch dann sichtbar sein, wenn der ESP32 schweigt.
 *
 * Es gibt KEINEN Vergleich mehr -- weder hier noch im Protokoll. Die
 * Oberflaeche liest GET_INPUT nicht mehr, die Registerwerte stehen fuer sich.
 */
static void gui_show_regtext(void)
{
    static char buf[V4_JOY_TEXT_MAX + 128u];    /* ortsfest (4-KB-Stapel!) */
    size_t      n;

    if (s_regtext == NULL) {
        return;
    }
    n = v4_joy_reg_text(&J, buf, sizeof(buf));
    if (n + 90u < sizeof(buf)) {
        (void)snprintf(buf + n, sizeof(buf) - n,
                       "\nDirekt aus den Registern der V4 (JOY0DAT/JOY1DAT, "
                       "CIAAPRA/POTINP) -- die einzige Eingangsquelle.");
    }
    set(s_regtext, MUIA_Text_Contents, (IPTR)buf);
}
/* Vorwaertsdeklarationen: gui_show_ports (unten) zieht die Betriebsart-Knoepfe
 * auf die VORGABE aus GET_PORT_CONFIG; die beiden Helfer stehen im naechsten
 * Abschnitt bei den uebrigen Funktionen der Eingangsverteilung. */
static IPTR    gui_mode_index(uint8_t soll);
static uint8_t gui_mode_soll(int port);

/*
 * Die Buchsen-Anzeige: NUR die eigenen Register.
 *
 * Das ist seit dem 14.10.2026 die EINZIGE Quelle fuer das, was der Controller
 * tut. Die Oberflaeche fragt GET_INPUT nicht mehr ab -- weder fuer die Anzeige
 * noch fuer einen Vergleich. Es gibt deshalb auch kein Abweichungsurteil
 * mehr (weder im Fenster noch im Protokoll): ohne I2C-Eingang gibt es nichts zu
 * vergleichen, die Registerwerte stehen fuer sich.
 *
 * Gezeichnet wird auch dann, wenn der ESP32 schweigt: die Registerabfrage haengt
 * nicht am Bus. Die wirksame Betriebsart kommt weiterhin vom ESP32
 * (GET_PORT_MODE/GET_PORT_CONFIG) und wird als "(ESP32)" gekennzeichnet; fehlt
 * sie, steht "Modus ?".
 */
static void gui_show_ports(void)
{
    uint8_t     mode[V4P_PORT_COUNT];
    static char buf[900];               /* ortsfest (4-KB-Stapel!) */

    /* Zuerst die Rohregister: sie haengen NICHT daran, ob eine gueltige
     * I2C-Antwort da ist. */
    gui_show_regtext();

    mode[0] = S.mode_valid ? S.mode[0] : 0u;
    mode[1] = S.mode_valid ? S.mode[1] : 0u;

    if (s_port0 != NULL) {
        gui_build_port_text(0, mode[0], S.mode_valid, buf, sizeof(buf));
        set(s_port0, MUIA_Text_Contents, (IPTR)buf);
    }
    if (s_port1 != NULL) {
        gui_build_port_text(1, mode[1], S.mode_valid, buf, sizeof(buf));
        set(s_port1, MUIA_Text_Contents, (IPTR)buf);
    }

    /* Cycle-Knoepfe auf die zuletzt BESTAETIGTE VORGABE ziehen (aus dem
     * Zustandsblock, nicht aus der Anzeige) -- und nur bei echter Aenderung:
     * jedes set() loest sonst eine Notify und damit einen unnoetigen Befehl
     * aus (klassische MUI-Rueckkopplung).
     *
     * Seit fw_ver 4 zeigt der Knopf die VORGABE aus GET_PORT_CONFIG
     * (mode_soll, mit "auto"); die wirksame Betriebsart steht daneben in
     * s_wirk0/s_wirk1. Kann die Gegenstelle die Verteilung nicht, gibt es
     * kein mode_soll -- dann gilt weiter der bestaetigte Modus. */
    if (s_mode0 != NULL) {
        IPTR jetzt = 0;
        IPTR soll  = gui_mode_index(gui_mode_soll(0));

        (void)get(s_mode0, MUIA_Cycle_Active, &jetzt);
        if (jetzt != soll) {
            set(s_mode0, MUIA_Cycle_Active, soll);
        }
    }
    if (s_mode1 != NULL) {
        IPTR jetzt = 0;
        IPTR soll  = gui_mode_index(gui_mode_soll(1));

        (void)get(s_mode1, MUIA_Cycle_Active, &jetzt);
        if (jetzt != soll) {
            set(s_mode1, MUIA_Cycle_Active, soll);
        }
    }
}

/* ------------------------------------------------------------------ */
/* Eingangsverteilung in der Oberflaeche (fw_ver 4)                    */
/* ------------------------------------------------------------------ */

/* Der Index im Betriebsart-Auswahlfeld: 0 = "auto", 1..3 = Betriebsart 0..2. */
static IPTR gui_mode_index(uint8_t soll)
{
    return (soll == (uint8_t)V4P_MODE_AUTO) ? (IPTR)0 : (IPTR)(soll + 1u);
}

/* Umgekehrt: Auswahlfeld -> Wert fuer SET_PORT_MODE. */
static uint8_t gui_mode_aus_index(IPTR idx)
{
    return (idx == 0) ? (uint8_t)V4P_MODE_AUTO : (uint8_t)(idx - 1u);
}

/* Die VORGABE einer Buchse: aus GET_PORT_CONFIG, sonst der bestaetigte Modus
 * (aeltere Firmware ohne die neuen Befehle). */
static uint8_t gui_mode_soll(int port)
{
    if (S.portcfg_valid != 0) {
        return S.portcfg.mode_soll[port];
    }
    return (S.mode_valid != 0) ? S.applied[port] : (uint8_t)V4P_PORT_OFF;
}

static int gui_bda_gleich(const uint8_t a[V4P_BDA_LEN],
                          const uint8_t b[V4P_BDA_LEN])
{
    unsigned i;

    for (i = 0u; i < V4P_BDA_LEN; i++) {
        if (a[i] != b[i]) {
            return 0;
        }
    }
    return 1;
}

/*
 * GET_PORT_CONFIG lesen und in den Zustandsblock uebernehmen.
 *
 * Das ist die eine Quelle fuer die Auswahlfelder: Vorgabe UND wirksame
 * Betriebsart, Richtungsquellen und die drei Knopfquellen beider Buchsen. Sie
 * wird beim Start (erster Takt), bei jeder Aenderung und dann im langsamen Takt
 * (rund einmal je Sekunde) gelesen -- so aendert sich die Anzeige auch, wenn
 * der ESP32 von sich aus etwas umstellt (z.B. "duo").
 *
 * Ein Fehlschlag laesst die Anzeige auf dem LETZTEN gueltigen Stand stehen --
 * das ist dieselbe Zusage wie bei den uebrigen Feldern.
 *
 * `note` = 1: den Rueckgabewert selbst in die Verbindungslage eintragen
 * (Einzelaufruf aus einer Aktion). `note` = 0: das macht der Aufrufer
 * (gui_poll) EINMAL mit dem schlechtesten Wert der Runde, damit ein einziger
 * Ausfall die Fehlerserie nicht mehrfach hochzaehlt.
 */
static uint8_t gui_cfg_read(int note)
{
    v4p_portcfg_t c;
    uint8_t       rc;

    if (s_cfg_ok == 0) {
        return V4P_ST_BAD_CMD;
    }
    rc = v4_get_port_config(&M, &c);
    if (note != 0) {
        (void)v4_state_note(&S, rc);
    }
    if (rc != V4P_ST_OK) {
        v4_dbg_fehler("GET_PORT_CONFIG", rc, &M);
        gui_msg("[cfg] GET_PORT_CONFIG: %s -- die Felder bleiben auf dem "
                "letzten gueltigen Stand\n", v4_strerror(rc));
        return rc;
    }
    v4_state_put_portcfg(&S, &c);
    return rc;
}

/*
 * GET_MOUSE_CFG lesen und in den Zustandsblock uebernehmen (fw_ver 5).
 *
 * Im langsamen Takt (rund einmal je Sekunde) sowie nach jeder Aenderung. Die
 * Werte aendern sich nur, wenn jemand sie aendert -- haeufiger waere reiner
 * Verkehr ohne neue Aussage. gui_show_portcfg zeichnet nur aus dem
 * Zustandsblock und macht ausdruecklich KEIN I2C.
 *
 * `note` wie bei gui_cfg_read.
 *
 * Ist das Faehigkeitsbit nicht gesetzt, wird NICHTS gesendet; die Regler
 * bleiben gesperrt und der Hinweis darunter nennt den Grund.
 */
/* Beide Maus-Einstellungen in EINER Zeile -- fuer die immer eingeschalteten
 * [maus]-Protokollzeilen. Nur so ist zu sehen, ob eine Tempo-Aenderung den
 * RICHTIGEN Port getroffen hat (oder ob beide wandern). */
static void gui_maus_text(const v4p_mousecfg_t *c, char *out, size_t cap)
{
    if (out == NULL || cap == 0u) {
        return;
    }
    if (c == NULL) {
        (void)snprintf(out, cap, "(keine Maus-Einstellungen gelesen)");
        return;
    }
    (void)snprintf(out, cap,
                   "port0 Tempo %u/%u Totzone %u%%, "
                   "port1 Tempo %u/%u Totzone %u%%",
                   (unsigned)c->speed[0], (unsigned)V4P_MOUSE_SPEED_MAX,
                   (unsigned)c->totzone[0],
                   (unsigned)c->speed[1], (unsigned)V4P_MOUSE_SPEED_MAX,
                   (unsigned)c->totzone[1]);
}

/* 1 = die Werte unterscheiden sich vom zuletzt gemeldeten Stand. */
static int gui_maus_geaendert(const v4p_mousecfg_t *c)
{
    int i;

    if (c == NULL) {
        return 0;
    }
    if (s_maus_alt_gueltig == 0) {
        return 1;
    }
    for (i = 0; i < (int)V4P_PORT_COUNT; i++) {
        if (s_maus_alt.speed[i] != c->speed[i]
            || s_maus_alt.totzone[i] != c->totzone[i]) {
            return 1;
        }
    }
    return 0;
}

static uint8_t gui_mouse_read(int note)
{
    v4p_mousecfg_t c;
    uint8_t        rc;

    if (s_mousecfg_ok == 0) {
        return V4P_ST_BAD_CMD;
    }
    rc = v4_get_mouse_cfg(&M, &c);
    if (note != 0) {
        (void)v4_state_note(&S, rc);
    }
    if (rc != V4P_ST_OK) {
        v4_dbg_fehler("GET_MOUSE_CFG", rc, &M);
        gui_msg("[maus] GET_MOUSE_CFG: %s -- die Regler bleiben auf dem letzten "
                "gueltigen Stand\n", v4_strerror(rc));
        return rc;
    }
    v4_state_put_mousecfg(&S, &c);
    /* Aenderung (auch die erste Antwort) EINMAL melden -- nicht in jedem
     * Takt. Das ist die Zeile, die zeigt, was der ESP32 wirklich liefert. */
    if (gui_maus_geaendert(&c) != 0) {
        char txt[128];

        gui_maus_text(&c, txt, sizeof(txt));
        gui_msg("[maus] GET_MOUSE_CFG: %s\n", txt);
        s_maus_alt         = c;
        s_maus_alt_gueltig = 1;
    }
    return rc;
}

/*
 * duo lesen (GET_DUO, fw_ver 6).
 *
 * Wie bei den Maus-Einstellungen: eine Zeile nur bei einer AENDERUNG (auch
 * fuer die erste Antwort), nicht in jedem Takt.
 */
static uint8_t gui_duo_read(int note)
{
    uint8_t duo = 0u;
    uint8_t rc;

    if (s_duo_ok == 0) {
        return V4P_ST_BAD_CMD;
    }
    rc = v4_get_duo(&M, &duo);
    if (note != 0) {
        (void)v4_state_note(&S, rc);
    }
    if (rc != V4P_ST_OK) {
        v4_dbg_fehler("GET_DUO", rc, &M);
        gui_msg("[duo] GET_DUO: %s -- der Schalter bleibt auf dem letzten "
                "gueltigen Stand\n", v4_strerror(rc));
        return rc;
    }
    v4_state_put_duo(&S, duo);
    if (s_duo_alt_gueltig == 0 || s_duo_alt != duo) {
        gui_msg("[duo] GET_DUO: %u (%s)%s\n", (unsigned)duo,
                v4p_duo_name(duo),
                (duo == V4P_DUO_OFF) ? ""
                                     : " -- EIN Controller auf BEIDEN Buchsen");
        s_duo_alt         = duo;
        s_duo_alt_gueltig = 1;
    }
    return rc;
}

/*
 * Welcher Port ist bei aktivem duo der BEGLEITER?
 *
 * duo setzt nur die Betriebsarten; die Geraetezuordnung bleibt, wie sie ist
 * (ein Geraet = ein Port). Der Begleiter ist deshalb der Port, der NICHT die
 * gemerkte Buchse des einen Controllers ist -- dort braucht es keine eigene
 * Zuordnung, und genau dort wird die Geraeteauswahl gesperrt.
 *
 * Ist keine (eindeutige) Zuordnung bekannt, gilt der Port mit der zweiten
 * Rolle: bei duo = 1 (Port 0 Maus) ist das Port 1. So ist die Anzeige auch
 * direkt nach dem Einschalten eindeutig und behauptet nichts Falsches.
 *
 * Rueckgabe: 0/1 = Begleiter, -1 = duo aus oder unklar.
 */
static int gui_duo_begleiter(void)
{
    uint8_t i;
    uint8_t bda[V4P_BDA_LEN];
    int     belegt0 = 0;
    int     belegt1 = 0;

    if (s_duo_ok == 0 || S.duo_valid == 0
        || S.duo == (uint8_t)V4P_DUO_OFF) {
        return -1;
    }
    for (i = 0u; i < S.devport_n; i++) {
        uint8_t port = v4_state_devport_at(&S, i, bda);

        if (port == 0u) {
            belegt0 = 1;
        } else if (port == 1u) {
            belegt1 = 1;
        }
    }
    if (belegt0 != 0 && belegt1 == 0) {
        return 1;
    }
    if (belegt1 != 0 && belegt0 == 0) {
        return 0;
    }
    return (S.duo == (uint8_t)V4P_DUO_MOUSE_PORT0) ? 1 : 0;
}

/*
 * Welche Rolle hat `port`, wenn duo laeuft? "Maus" oder "Joystick"; NULL,
 * wenn duo aus ist. duo = 1: Port 0 Maus / Port 1 Joystick, duo = 2 umgekehrt.
 */
static const char *gui_duo_rolle(int port)
{
    if (S.duo_valid == 0 || S.duo == (uint8_t)V4P_DUO_OFF) {
        return NULL;
    }
    if (S.duo == (uint8_t)V4P_DUO_MOUSE_PORT0) {
        return (port == 0) ? "Maus" : "Joystick";
    }
    return (port == 0) ? "Joystick" : "Maus";
}

/* Die Geraeteliste in die Eintraege der beiden Auswahlfelder uebernehmen.
 * Index 0 ist "(keines)" (siehe s_dev_eintraege). Die Namen werden auf 14
 * Zeichen gekuerzt: die Zelle teilt sich seit dem 15.10.2026 mit dem
 * Beitragsfeld, und MUI rechnet die Cycle-Breite aus dem laengsten Eintrag.
 * Der VOLLE Name steht in der Zusammenfassung und in der Geraeteliste. */
static void gui_dev_eintraege_bauen(void)
{
    int i;

    s_dev_eintraege[0] = "(keines)";
    for (i = 0; i < s_devn && i < GUI_DEV_MAX; i++) {
        const char *name = s_devtab[i].name;
        size_t      n    = strlen(name);

        if (n > 14u) {
            size_t k;

            for (k = 0u; k < 14u; k++) {
                s_dev_kurz[i + 1][k] = name[k];
            }
            s_dev_kurz[i + 1][14] = '.';
            s_dev_kurz[i + 1][15] = '.';
            s_dev_kurz[i + 1][16] = '.';
            s_dev_kurz[i + 1][17] = '\0';
        } else {
            size_t k;

            for (k = 0u; k < n; k++) {
                s_dev_kurz[i + 1][k] = name[k];
            }
            s_dev_kurz[i + 1][n] = '\0';
        }
        s_dev_eintraege[i + 1] = s_dev_kurz[i + 1];
    }
    s_dev_eintraege[i + 1] = NULL;
    s_dev_eintraege_n      = s_devn;

    if (s_dev0 != NULL) {
        set(s_dev0, MUIA_Cycle_Entries, (IPTR)s_dev_eintraege);
    }
    if (s_dev1 != NULL) {
        set(s_dev1, MUIA_Cycle_Entries, (IPTR)s_dev_eintraege);
    }
}

/* Index im Geraete-Auswahlfeld der Buchse: 0 = "(keines)", sonst Geraet+1.
 * Quelle ist die in DIESER Sitzung gemerkte Zuordnung -- ueber die Leitung ist
 * sie nicht lesbar (siehe v4_state.h). */
static IPTR gui_dev_active(int port)
{
    uint8_t i;
    int     k;

    for (i = 0u; i < S.devport_n; i++) {
        uint8_t bda[V4P_BDA_LEN];

        if (v4_state_devport_at(&S, i, bda) != (uint8_t)port) {
            continue;
        }
        for (k = 0; k < s_devn; k++) {
            if (gui_bda_gleich(bda, s_devtab[k].bda) != 0) {
                return (IPTR)(k + 1);
            }
        }
        /* Zugeordnet, aber (noch) nicht in der Liste: das Feld kann das nicht
         * darstellen -- die Zusammenfassung nennt die Adresse. */
    }
    return 0;
}

/* Ein Auswahlfeld auf einen Wert ziehen -- nur bei echter Aenderung. */
static void gui_cycle_set(Object *o, IPTR soll)
{
    IPTR jetzt = 0;

    if (o == NULL) {
        return;
    }
    (void)get(o, MUIA_Cycle_Active, &jetzt);
    if (jetzt != soll) {
        set(o, MUIA_Cycle_Active, soll);
    }
}

/* Ein Auswahlfeld lesen. */
static IPTR gui_cycle_get(Object *o)
{
    IPTR v = 0;

    if (o == NULL) {
        return 0;
    }
    (void)get(o, MUIA_Cycle_Active, &v);
    return v;
}

/*
 * Ein Objekt sperren oder freigeben -- nur bei echter Aenderung.
 *
 * MUIA_Disabled ist ein Area-Attribut (ISG) und damit an jedem Gadget
 * erlaubt; MUI zeichnet ein gesperrtes Gadget mit einem Raster ("ghost
 * pattern") und nimmt keine Eingaben mehr an. Genau das ist die Kennzeichnung
 * "gegenstandslos" fuer die Einzelschalter bei Beitrag "beide" und fuer die
 * Maus-Regler ohne Faehigkeitsbit -- sichtbar UND nicht bedienbar, statt eine
 * Schalterstellung zu zeigen, die nichts bewirkt.
 */
static void gui_disabled_set(Object *o, int aus)
{
    IPTR v = 0;

    if (o == NULL) {
        return;
    }
    (void)get(o, MUIA_Disabled, &v);
    if ((v != 0) != (aus != 0)) {
        set(o, MUIA_Disabled, (IPTR)((aus != 0) ? TRUE : FALSE));
    }
}

/*
 * Die Geraetezuordnung einer Buchse in Klartext: Name, wenn das Geraet in der
 * Scanliste steht, sonst die Adresse. Quelle ist die gemerkte Zuordnung.
 */
static void gui_dev_text(int port, char *out, size_t cap)
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
        int         k;

        if (v4_state_devport_at(&S, i, bda) != (uint8_t)port) {
            continue;
        }
        for (k = 0; k < s_devn; k++) {
            if (gui_bda_gleich(bda, s_devtab[k].bda) != 0) {
                name = (s_devtab[k].name[0] != '\0') ? s_devtab[k].name
                                                     : "(ohne Namen)";
                break;
            }
        }
        if (name == NULL) {
            (void)snprintf(adresse, sizeof(adresse),
                           "%02X:%02X:%02X:%02X:%02X:%02X",
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

/*
 * Die Felder der Eingangsverteilung aus dem Zustandsblock stellen und die
 * Zusammenfassung schreiben. Das ist die Entsprechung zu gui_show_ports: LESEN
 * und ANZEIGEN, kein I2C, kein Warten.
 *
 * Die Zusammenfassung nennt je Buchse genau das, was der Auftrag sehen will:
 * Geraet, Vorgabe, wirksame Betriebsart, Richtungen und die drei Quellen --
 * Letztere mit dem VOLLEN Namen ("Gruppe 1 (A,L1,L2,L3)"), damit die
 * Kurzfassung im Auswahlfeld nicht im Unklaren laesst. Dazu kommen der
 * abgeleitete Beitrag und -- sofern vorhanden -- Tempo/Totzone der Buchse.
 *
 * DER BEITRAG "beide" SPERRT DIE EINZELSCHALTER: sind alle Richtungsquellen
 * aktiv, haben die drei Richtungsschalter und die drei Knopfquellen keine
 * eigene Aussage mehr. Sie werden deshalb gesperrt (MUIA_Disabled) -- MUI
 * zeichnet sie dann gerastert -- UND im Hinweistext benannt. Beim Wechsel auf
 * "analog"/"digital" werden sie wieder bedienbar. Damit gibt es keine
 * Schalterstellung, die einer anderen Anzeige widerspricht.
 */
static void gui_show_portcfg(void)
{
    static char buf[900];               /* ortsfest (4-KB-Stapel!) */
    static char hint[640];              /* ortsfest (4-KB-Stapel!) */
    int         port;
    int         duo_begleiter;
    size_t      n = 0u;

    if (s_cfgtext == NULL) {
        return;
    }

    if (s_cfg_ok == 0) {
        (void)snprintf(buf, sizeof(buf),
                       "Konfiguration: die Gegenstelle meldet das "
                       "Faehigkeitsbit 0x%04X nicht -- sie kann die "
                       "Eingangsverteilung nicht.\n"
                       "  GET_PORT_CONFIG/SET_PORT_ROUTING/SET_DEVICE_PORT "
                       "bleiben aus. Firmware des ESP32 auf fw_ver %u "
                       "bringen.",
                       (unsigned)V4P_CAP_PORT_ROUTING,
                       (unsigned)V4P_FW_VER_ROUTING);
        set(s_cfgtext, MUIA_Text_Contents, (IPTR)buf);
        /* Ohne Verteilung sind auch die Beitrags-Auswahl und die Maus-Regler
         * ohne Wirkung -- sie werden gesperrt gezeichnet, statt Befehle
         * anzubieten, die der Slave mit BAD_CMD abweist. */
        gui_disabled_set(s_beitrag0, 1);
        gui_disabled_set(s_beitrag1, 1);
        /* duo ist eine eigene Faehigkeit; ohne Verteilung bleibt der Schalter
         * trotzdem gesperrt, weil er ohne Betriebsarten nichts bewirkt. */
        gui_disabled_set(s_duo, 1);
        if (s_duotext != NULL) {
            set(s_duotext, MUIA_Text_Contents,
                (IPTR)"untaetig (keine Eingangsverteilung)");
        }
        gui_disabled_set(s_mspeed0, 1);
        gui_disabled_set(s_mspeed1, 1);
        gui_disabled_set(s_mtotal0, 1);
        gui_disabled_set(s_mtotal1, 1);
        gui_disabled_set(s_mtot0m, 1);
        gui_disabled_set(s_mtot0p, 1);
        gui_disabled_set(s_mtot1m, 1);
        gui_disabled_set(s_mtot1p, 1);
        if (s_cfghint != NULL) {
            set(s_cfghint, MUIA_Text_Contents,
                (IPTR)"Geraet, Beitrag und Knopfquellen bleiben untaetig, bis "
                      "die Gegenstelle die Eingangsverteilung kann.");
        }
        if (s_maushint != NULL) {
            set(s_maushint, MUIA_Text_Contents,
                (IPTR)"Ohne Eingangsverteilung (Faehigkeitsbit 0x0400/0x0100) "
                      "sind die Maus-Regler untaetig.");
        }
        if (s_cfg_warned == 0) {
            s_cfg_warned = 1;
            gui_msg("[cfg] die Gegenstelle kann die Eingangsverteilung nicht "
                    "(Faehigkeitsbit 0x%04X fehlt, gemeldet 0x%04X) -- die "
                    "Felder Geraet/Beitrag/Knopfquellen bleiben untaetig\n",
                    (unsigned)V4P_CAP_PORT_ROUTING,
                    S.info_valid ? (unsigned)S.info.caps : 0u);
        }
        return;
    }
    if (S.portcfg_valid == 0) {
        return;                 /* noch nichts empfangen: alte Anzeige bleibt */
    }

    /* Die Geraeteliste kann sich waehrend des Laufs fuellen. */
    if (s_dev_eintraege_n != s_devn) {
        gui_dev_eintraege_bauen();
    }

    /* Wer ist bei aktivem duo der Begleiter-Port? (Siehe gui_duo_begleiter.)
     * EINMAL bestimmt -- beide Schleifen dieser Funktion brauchen denselben
     * Wert, sonst zeigte die Klartextzeile etwas anderes als die Felder. */
    duo_begleiter = gui_duo_begleiter();

    /* ZWEI Zeilen -- eine je Port. Eine Titelzeile waere Hoehe ohne Aussage;
     * was die Zeile bedeutet, sagt der Hinweis unter der Tabelle. */
    n = 0u;
    buf[0] = '\0';
    for (port = 0; port < (int)V4P_PORT_COUNT; port++) {
        char    dir[48];
        char    geraet[V4P_NAME_BUF + 16u];
        char    maus[48];
        uint8_t beitrag = v4_beitrag_aus_dir(S.portcfg.dir[port]);

        v4p_dir_text(S.portcfg.dir[port], dir, sizeof(dir));
        if (duo_begleiter == port) {
            const char *rolle = gui_duo_rolle(port);

            (void)snprintf(geraet, sizeof(geraet), "duo: %s",
                           (rolle != NULL) ? rolle : "ein Controller");
        } else {
            gui_dev_text(port, geraet, sizeof(geraet));
        }
        if (s_mousecfg_ok != 0 && S.mousecfg_valid != 0) {
            v4p_mousecfg_text(&S.mousecfg, (uint8_t)port, maus, sizeof(maus));
        } else if (s_mousecfg_ok == 0) {
            (void)snprintf(maus, sizeof(maus), "Maus nicht verfuegbar");
        } else {
            (void)snprintf(maus, sizeof(maus), "Maus: ?");
        }
        if (n + 260u >= sizeof(buf)) {
            break;
        }
        n += (size_t)snprintf(buf + n, sizeof(buf) - n,
                              "%sPort %d: %s -- Vorgabe %s, wirkt %s, "
                              "Richtungen %s, Pin6<-%s, Pin9<-%s, Pin5<-%s, "
                              "Beitrag %s, %s",
                              (port == 0) ? "" : "\n", port, geraet,
                              v4p_port_mode_soll_name(S.portcfg.mode_soll[port]),
                              v4p_port_mode_name(S.portcfg.mode_ist[port]),
                              dir,
                              v4p_taste_name(S.portcfg.btn[port][0]),
                              v4p_taste_name(S.portcfg.btn[port][1]),
                              v4p_taste_name(S.portcfg.btn[port][2]),
                              gui_beitrag_name(beitrag), maus);
    }
    set(s_cfgtext, MUIA_Text_Contents, (IPTR)buf);

    /* Auswahlfelder und Schalter auf den BESTAETIGTEN Stand ziehen.
     * s_cfg_sperre verhindert, dass das Nachziehen als Anwenderwunsch
     * missdeutet wird -- die Handler vergleichen ohnehin gegen den
     * Zustandsblock, aber so bleibt es auch bei einem Renntakt eindeutig. */
    s_cfg_sperre = 1;

    /*
     * duo stellen. Ohne Faehigkeitsbit gesperrt (kein Senden), ohne gueltige
     * Antwort "?" statt "aus" -- eine fehlende Antwort ist keine Aussage.
     */
    if (s_duo != NULL) {
        gui_disabled_set(s_duo, (s_duo_ok == 0) ? 1 : 0);
        gui_cycle_set(s_duo, (IPTR)((S.duo_valid != 0) ? S.duo : 0u));
    }
    if (s_duotext != NULL) {
        char t[96];

        if (s_duo_ok == 0) {
            (void)snprintf(t, sizeof(t), "nicht verfuegbar (fw %u)",
                           (unsigned)V4P_FW_VER_DUO);
        } else if (S.duo_valid == 0) {
            (void)snprintf(t, sizeof(t), "?");
        } else if (S.duo == (uint8_t)V4P_DUO_OFF) {
            (void)snprintf(t, sizeof(t),
                           "aus (jeder Controller seine Buchse)");
        } else {
            /* Passt in die 250-px-Zelle und nennt BEIDE Rollen. */
            (void)snprintf(t, sizeof(t), "%s", v4p_duo_name(S.duo));
        }
        set(s_duotext, MUIA_Text_Contents, (IPTR)t);
    }

    for (port = 0; port < (int)V4P_PORT_COUNT; port++) {
        Object **b3    = (port == 0) ? s_b0 : s_b1;
        Object  *rdir  = (port == 0) ? s_rdir0 : s_rdir1;
        Object  *dev   = (port == 0) ? s_dev0 : s_dev1;
        Object  *mcyc  = (port == 0) ? s_mode0 : s_mode1;
        Object  *wirk  = (port == 0) ? s_wirk0 : s_wirk1;
        Object  *bcyc  = (port == 0) ? s_beitrag0 : s_beitrag1;
        Object  *speed = (port == 0) ? s_mspeed0 : s_mspeed1;
        Object  *tot   = (port == 0) ? s_mtotal0 : s_mtotal1;
        Object  *tmin  = (port == 0) ? s_mtot0m : s_mtot1m;
        Object  *tplus = (port == 0) ? s_mtot0p : s_mtot1p;
        uint8_t  beitrag = v4_beitrag_aus_dir(S.portcfg.dir[port]);
        int      maus_aus;
        unsigned k;

        /*
         * RICHTUNGEN NUR ANZEIGEN (seit 16.10.2026): es gibt keine
         * Richtungsschalter mehr. Welche Quellen gelten, legt der Beitrag
         * fest (analog -> Stick links, digital -> D-Pad, beides -> alle);
         * drei Checkboxen daneben waren eine zweite, widerspruechliche
         * Stelle. Hier steht die WIRKSAME Maske als Klartext.
         */
        if (rdir != NULL) {
            char r[48];

            v4p_dir_text(S.portcfg.dir[port], r, sizeof(r));
            set(rdir, MUIA_Text_Contents, (IPTR)r);
        }

        /*
         * Die drei Knopfquellen bleiben bedienbar -- auch bei "beides": sie
         * sagen, WELCHE Taste auf Pin 6/9/5 liegt, und das ist eine echte
         * Wahl. Nur die Richtungen sind durch den Beitrag festgelegt.
         */
        for (k = 0u; k < V4P_BTN_COUNT; k++) {
            uint8_t code = S.portcfg.btn[port][k];

            gui_cycle_set(b3[k],
                          (code < (uint8_t)V4P_TASTE_ANZAHL) ? (IPTR)code
                                                             : (IPTR)0);
        }
        gui_cycle_set(dev, gui_dev_active(port));
        /*
         * BEI AKTIVEM duo GEHOERT DER BEGLEITER-PORT duo -- dort waere eine
         * Geraeteauswahl eine Luege (es bedient ihn derselbe Controller).
         * Deshalb sichtbar gesperrt, mit dem Grund im Klartext daneben.
         */
        gui_disabled_set(dev, (duo_begleiter == port) ? 1 : 0);
        gui_cycle_set(mcyc, gui_mode_index(S.portcfg.mode_soll[port]));
        gui_cycle_set(bcyc, (IPTR)beitrag);

        if (wirk != NULL) {
            char w[48];

            (void)snprintf(w, sizeof(w), "wirkt: %s",
                           v4p_port_mode_name(S.portcfg.mode_ist[port]));
            set(wirk, MUIA_Text_Contents, (IPTR)w);
        }

        /* Maus-Regler: ohne Faehigkeitsbit (oder bevor eine gueltige Antwort
         * da ist) gesperrt; sonst auf den BESTAETIGTEN Stand gezogen. */
        maus_aus = (s_mousecfg_ok == 0 || S.mousecfg_valid == 0) ? 1 : 0;
        gui_disabled_set(speed, maus_aus);
        gui_disabled_set(tot, maus_aus);
        gui_disabled_set(tmin, maus_aus);
        gui_disabled_set(tplus, maus_aus);
        if (S.mousecfg_valid != 0) {
            unsigned sp = S.mousecfg.speed[port];
            unsigned tz = S.mousecfg.totzone[port];

            gui_cycle_set(speed,
                          (sp >= (unsigned)V4P_MOUSE_SPEED_MIN
                           && sp <= (unsigned)V4P_MOUSE_SPEED_MAX)
                              ? (IPTR)(sp - (unsigned)V4P_MOUSE_SPEED_MIN)
                              : (IPTR)0);
            if (tot != NULL) {
                char mm[24];

                (void)snprintf(mm, sizeof(mm), "%u %%", tz);
                set(tot, MUIA_Text_Contents, (IPTR)mm);
            }
        }
    }

    s_cfg_sperre = 0;

    /* Hinweis auf der Seite "Ports": was der Beitrag bedeutet und dass ein
     * Geraet im ESP32 genau EINEN Port hat (duo ist der Weg fuer beide). */
    if (s_cfghint != NULL) {
        (void)snprintf(hint, sizeof(hint),
                       "Beitrag: analog = Stick links + L1/R1, digital = "
                       "D-Pad + A/B/Start,\n"
                       "beides = alle Richtungen + Gruppe 1/2/3, eigene = "
                       "passt zu keiner Variante.\n"
                       "Geraet: genau EIN Port je Geraet; fuer BEIDE Ports ist "
                       "duo da (Seite Maus).");
        set(s_cfghint, MUIA_Text_Contents, (IPTR)hint);
    }

    /* Hinweis auf der Seite "Maus". */
    if (s_maushint != NULL) {
        if (s_mousecfg_ok == 0) {
            (void)snprintf(hint, sizeof(hint),
                           "Tempo und Totzone sind NICHT verfuegbar: die "
                           "Gegenstelle meldet das Faehigkeitsbit 0x%04X "
                           "nicht\n(fw_ver %u noetig); es wird nichts "
                           "gesendet.",
                           (unsigned)V4P_CAP_MOUSE_CFG,
                           (unsigned)V4P_FW_VER_MOUSE);
        } else {
            (void)snprintf(hint, sizeof(hint),
                           "Tempo und Totzone gelten JE PORT. Klick schaltet "
                           "weiter, die rechte Maustaste oeffnet die Liste.\n"
                           "duo = EIN Controller auf beiden Ports (die eine "
                           "Maus, die andere Joystick); es setzt nur die\n"
                           "Betriebsarten und sperrt die Geraeteauswahl des "
                           "duo-Ports (kein zweiter Controller).");
        }
        set(s_maushint, MUIA_Text_Contents, (IPTR)hint);
    }
}

/*
 * Die von MUI BERECHNETEN Mindestgroessen ins Protokoll schreiben.
 *
 * WARUM: MUI rechnet die Mindestgroesse eines Fensters aus dem Inhalt; ob eine
 * Seite in das Fenster passt, war bisher nur zu raten. (Der erste Versuch mit
 * MUIM_AskMinMax lieferte am Geraet durchgehend 0 -- siehe unten.)
 * die Methode, die das Layout dafuer benutzt -- der Aufruf hier ist reine
 * Diagnose und aendert nichts. Die Seitengruppe meldet das MAXIMUM ueber alle
 * Seiten (MUI_Group.doc); die beiden Seiten werden zusaetzlich einzeln
 * gemeldet, damit ein "zu hoch" eindeutig einer Seite zuzuordnen ist.
 *
 * Wird einmal nach dem Oeffnen und danach bei jedem Seitenwechsel gerufen.
 */
static void gui_mindestmasse_melden(void)
{
    Object     *obj[3];
    const char *name[3];
    int         i;

    /*
     * WARUM NICHT MUIM_AskMinMax: der Aufruf von aussen lieferte am Geraet
     * durchgehend "0x0" (Mitschnitt 16.10.2026, 13:35:30) -- die Methode ist
     * fuer das Layout INNERHALB einer Klasse gedacht. Gemessen wird deshalb mit
     * den dokumentierten LESENDEN Attributen MUIA_Width/Height/Left/Top;
     * die sind nach dem Oeffnen gueltig (MUI_Area.doc: "valid between
     * MUIM_Show/Hide").
     */
    obj[0] = s_root;   name[0] = "Fensterinhalt";
    obj[1] = s_scroll; name[1] = "Seitenbereich (sichtbar)";
    obj[2] = s_seiten; name[2] = "Seitengruppe (alle Seiten)";

    for (i = 0; i < 3; i++) {
        IPTR w = 0, h = 0, l = 0, t = 0;

        if (obj[i] == NULL) {
            continue;
        }
        (void)get(obj[i], MUIA_Width,  &w);
        (void)get(obj[i], MUIA_Height, &h);
        (void)get(obj[i], MUIA_LeftEdge, &l);
        (void)get(obj[i], MUIA_TopEdge,  &t);
        gui_msg("[fenster] %s: %ldx%ld px bei %ld,%ld (Fenster %dx%d)\n",
                name[i], (long)w, (long)h, (long)l, (long)t,
                s_fenster_w, s_fenster_h);
    }

    /*
     * Und die SICHTBARE Seite: nur ihre Groesse ist gueltig (verdeckte Seiten
     * sind nicht aufgebaut, MUI liefert dort 0). Ist sie hoeher als der
     * Seitenbereich, muss gescrollt werden -- genau das sagt diese Zeile.
     */
    {
        IPTR    v = 0;
        Object *p = NULL;
        const char *pn = "?";

        if (s_seite != NULL) {
            (void)get(s_seite, MUIA_Cycle_Active, &v);
        }
        if (v == 0) {
            p = s_seite_anzeige; pn = "Seite Anzeige";
        } else if (v == 1) {
            p = s_seite_ports;   pn = "Seite Ports";
        } else {
            p = s_seite_maus;    pn = "Seite Maus";
        }
        if (p != NULL) {
            IPTR w = 0, h = 0;

            (void)get(p, MUIA_Width,  &w);
            (void)get(p, MUIA_Height, &h);
            gui_msg("[fenster] %s (sichtbar): %ldx%ld px\n", pn,
                    (long)w, (long)h);
        }
    }
}

/* Vorwaertsdeklaration: gui_show_slow (hier) zieht die Geraeteliste nach, die
 * Funktion selbst steht bei der Liste weiter unten. */
static void gui_liste_zustand_nachziehen(void);

/* Langsamer Takt: Zustand und Controller-Info stammen aus dem Zustandsblock --
 * der Auftrag dafuer (POLL_SLOW) wird im Takt abgesetzt, hier wird nur
 * gezeichnet. */
static void gui_show_slow(void)
{
    gui_show_status();
    gui_show_hid();
    /* "verbunden" in der Geraeteliste nachziehen, wenn sich der Zustand
     * geaendert hat (die Liste selbst wird nur beim Scan gefuellt). */
    gui_liste_zustand_nachziehen();
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
/*
 * Ist das Geraet mit dem Tabellenindex `idx` JETZT verbunden?
 *
 * QUELLE: GET_STATUS.conn_index -- der Index des verbundenen Geraets in
 * GENAU der Tabelle, aus der DEV_GET liest (dieselbe Reihenfolge). Damit
 * laesst sich der Zustand ohne neuen Befehl anzeigen.
 *
 * OHNE gueltige Statusantwort steht dort AUSDRUECKLICH "unbekannt" und nicht
 * "nicht verbunden": eine fehlende Antwort ist keine Aussage. Sobald der
 * Firmware-Agent das Feld in DEV_GET nachreicht (Auftrag vom 16.10.2026),
 * kann die Anzeige darauf umgestellt werden -- bis dahin ist das hier die
 * belastbare Quelle.
 */
static void gui_dev_verbunden_text(uint8_t idx, char *out, size_t cap)
{
    int verbunden;

    if (out == NULL || cap == 0u) {
        return;
    }
    if (S.status_valid == 0) {
        (void)snprintf(out, cap, "unbekannt");
        return;
    }
    verbunden = (S.status.state == V4P_STATE_CONNECTED
                 || S.status.state == V4P_STATE_SUSPENDED) ? 1 : 0;
    if (verbunden != 0 && S.status.conn_index == idx) {
        (void)snprintf(out, cap, "verbunden");
    } else {
        (void)snprintf(out, cap, "nicht verbunden");
    }
}

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
        char    line[V4P_NAME_BUF + 64u];
        char    zustand[24];
        char   *copy;
        size_t  len;

        /* Hinter der MAC steht, ob das Geraet JETZT verbunden ist. */
        gui_dev_verbunden_text(devbuf[i].idx, zustand, sizeof(zustand));
        (void)snprintf(line, sizeof(line),
                       "%s%s  (%02X:%02X:%02X:%02X:%02X:%02X)  - %s",
                       devbuf[i].name,
                       ((devbuf[i].flags & V4P_FLAG_NAME_TRUNCATED) != 0u)
                           ? "..." : "",
                       devbuf[i].bda[0], devbuf[i].bda[1], devbuf[i].bda[2],
                       devbuf[i].bda[3], devbuf[i].bda[4], devbuf[i].bda[5],
                       zustand);
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

/*
 * Den Verbindungszustand in die Geraeteliste nachziehen.
 *
 * Die Liste wird nur beim Scan gefuellt; "verbunden" aendert sich aber
 * jederzeit (BluePad32 verbindet von selbst). Deshalb wird hier verglichen,
 * was zuletzt in der Liste stand, und nur bei einer AENDERUNG neu gefuellt --
 * sonst flackerte die Auswahl im Sekundentakt. Der aktive Eintrag bleibt
 * erhalten.
 */
static void gui_liste_zustand_nachziehen(void)
{
    static int alt_idx   = -2;
    static int alt_state = -1;
    int        jetzt_idx;
    int        jetzt_state;

    if (S.status_valid == 0) {
        return;
    }
    jetzt_idx   = (S.status.conn_index == V4P_NO_INDEX)
                      ? -1 : (int)S.status.conn_index;
    jetzt_state = (int)S.status.state;
    if (jetzt_idx == alt_idx && jetzt_state == alt_state) {
        return;
    }
    alt_idx   = jetzt_idx;
    alt_state = jetzt_state;
    if (s_devn == 0 || s_devs == NULL || S.dev_n == 0u) {
        return;                 /* nichts in der Liste / nichts nachzuziehen */
    }
    {
        IPTR aktiv = 0;

        (void)get(s_devs, MUIA_List_Active, &aktiv);
        gui_devices_fill();
        if ((int)aktiv < (int)s_devn) {
            set(s_devs, MUIA_List_Active, aktiv);
        }
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
/* im selben Faden. Sie sind seit dem 14.10.2026 die EINZIGE Quelle     */
/* fuer das, was der Controller tut -- GET_INPUT wird nicht mehr gefragt.*/
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
 * Die Direktzeile EINMAL auf den DebugUART -- und damit ueber v4_debug.c
 * zugleich auf stdout und in die Logdatei.
 *
 * Sie geht BEWUSST auch mit -quiet heraus: sie ist der Zweck der Direktabfrage.
 * -quiet schaltet die Zeile je I2C-Transaktion ab, nicht diese.
 *
 * Sie wird NICHT in jedem Takt geschrieben, sondern nur, wenn sich der direkte
 * Stand (Richtung/Feuerknopf) geaendert hat. Im Ruhezustand traegt die eine
 * Herzzeile je Sekunde dieselben Registerwerte (gui_joy_herz_zusatz).
 *
 * ES GIBT KEINEN I2C-VERGLEICH MEHR: die Oberflaeche liest GET_INPUT nicht
 * mehr ab, es gibt also nichts zu vergleichen und kein Abweichungsurteil.
 */
static void gui_direkt_zeile(void)
{
    static char buf[V4_JOY_TEXT_MAX + 8u];   /* ortsfest (4-KB-Stapel) */
    size_t      n;

    n = v4_joy_direkt_protokoll_text(&J, buf, sizeof(buf));
    /* Eine FERTIGE Zeile, ein Aufruf je Senke: der Umbruch gehoert dazu,
     * sonst waeren es zwei Ausgaben fuer eine Meldung. */
    if (n + 1u < sizeof(buf)) {
        buf[n]      = '\n';
        buf[n + 1u] = '\0';
        v4_dbg_zeile(buf);
    }
}

/*
 * Dieselben direkten Werte OHNE Ausgabe an die naechste Herzzeile haengen: so
 * bleibt es bei EINER Zeile je Sekunde, und die enthaelt trotzdem beide
 * Buchsen, die dekodierten Richtungen/Feuerknoepfe und die Rohwerte von
 * CIAAPRA/POTINP.
 */
static void gui_joy_herz_zusatz(void)
{
    static char buf[V4_JOY_TEXT_MAX];   /* ortsfest (4-KB-Stapel) */

    (void)v4_joy_direkt_protokoll_text(&J, buf, sizeof(buf));
    v4_dbg_herz_zusatz(buf);
}

/*
 * SOFORT SCHREIBEN BEI AENDERUNG.
 *
 * Geprueft wird in JEDEM Takt, ob sich der DIREKTE Stand (Richtungen und
 * Feuerknoepfe aus den Registern der V4) einer Buchse geaendert hat -- und
 * dann geht sofort EINE fertige Zeile heraus. Ohne Aenderung entsteht KEINE
 * Zeile: die Ausgabe soll den Bus nicht bremsen.
 *
 * Einen I2C-Vergleich gibt es nicht mehr; die Zeile traegt ausschliesslich die
 * Registerwerte (v4_joy_direkt_protokoll_text).
 */
static void gui_joy_aenderungen(void)
{
    int port;
    int neu = 0;

    for (port = 0; port < (int)V4P_PORT_COUNT; port++) {
        uint8_t dir = v4_joy_maske(&J, port);

        if (s_joy_stand_gueltig == 0 || dir != s_joy_dir_alt[port]) {
            neu = 1;
        }
        s_joy_dir_alt[port] = dir;
    }
    s_joy_stand_gueltig = 1;

    if (neu != 0) {
        gui_direkt_zeile();
    }
}

/*
 * Vorwaertsdeklarationen: gui_poll (unten) meldet eine erkannte Verbindung an
 * die Geraeteliste weiter, die Funktionen selbst stehen weiter unten.
 */
static void act_scan_start(int automatisch);
static void gui_liste_auto(const v4p_status_t *st);

/*
 * Der I2C-Teil des Takts -- NUR im langsamen Takt (jeder fuenfte, rund 1 s).
 *
 * WAS HIER NICHT MEHR STEHT: GET_INPUT. Die Oberflaeche holt keine
 * Controller-Eingabewerte mehr ueber I2C; die Quelle dafuer sind die
 * Joyport-Register der V4 (gui_joy_abtasten, in JEDEM Takt, ohne Bus).
 *
 * WAS BLEIBT (alles Einstellungen, Verbindungszustand oder Geraeteinfos --
 * keine Eingabewerte):
 *   GET_STATUS    Verbindung/Scan  -- sonst weiss die Oberflaeche nicht, ob
 *                                      ein Controller da ist
 *   GET_PORT_MODE wirksame Betriebsart
 *   GET_HID       Name/VID/PID/Batterie des Controllers
 *   GET_PORT_CONFIG Verteilung (Geraet, Richtungen, Knopfquellen)
 *   GET_MOUSE_CFG Tempo/Totzone je Buchse
 * Dazu kommen ausserhalb dieses Takts SCAN/DEV_COUNT/DEV_GET (nur waehrend
 * einer Suche) und alle SETZENDEN Befehle (auf Knopfdruck).
 *
 * WARUM ALLES IN DEN LANGSAMEN TAKT: der Bus soll ruhig bleiben. Vorher liefen
 * GET_PORT_MODE UND GET_INPUT in JEDEM Takt (200 ms), zusammen rund zehn
 * Transaktionen je Sekunde -- GET_INPUT war davon die haeufigste. Ohne die
 * Eingangsabfrage und mit allen Abfragen im Sekundentakt sind es noch rund
 * fuenf je Sekunde, und keine davon traegt Controller-Werte.
 *
 * Wichtig: der Verbindungszustand wird EINMAL mit dem schlechtesten
 * Rueckgabewert fortgeschrieben, nicht je Unterbefehl. Sonst zaehlte ein
 * einziger Ausfall die Fehlerserie mehrfach hoch, und der Abstand waechst zu
 * schnell.
 */
static void gui_poll(int slow)
{
    uint8_t rc  = V4P_ST_OK;
    uint8_t rc2;

    if (slow == 0) {
        /* Kein I2C im schnellen Takt. Die Registerabfrage macht der Takt
         * selbst (gui_joy_abtasten), sie braucht keinen Bus. */
        return;
    }

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
    {
        v4p_status_t st;

        rc2 = v4_get_status(&M, &st);
        if (rc2 == V4P_ST_OK) {
            v4_state_put_status(&S, &st);
            v4_dbg_status(&st);
            v4_dbg_sitzung(&st);
            /* Steht eine Verbindung und ist die Auswahlliste noch leer, wird
             * sie genau einmal automatisch geholt (siehe gui_liste_auto). */
            gui_liste_auto(&st);
        } else {
            rc = rc2;
            v4_dbg_fehler("GET_STATUS", rc2, &M);
        }
    }
    {
        uint8_t mode[V4P_PORT_COUNT];

        rc2 = v4_get_port_mode(&M, mode);
        if (rc2 == V4P_ST_OK) {
            v4_state_put_modes(&S, mode);
            v4_dbg_modes(mode, "GET_PORT_MODE");
        } else if (rc == V4P_ST_OK) {
            rc = rc2;
            v4_dbg_fehler("GET_PORT_MODE", rc2, &M);
        }
    }
    {
        v4p_hid_t hid;

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
     * Eingangsverteilung (fw_ver 4) und Maus-Einstellungen (fw_ver 5): Vorgabe
     * und wirksame Betriebsart, Richtungen, Knopfquellen, Tempo/Totzone.
     * Einmal je Sekunde genuegt -- sie aendern sich nur, wenn jemand etwas
     * einstellt (oder der ESP32 von sich aus, z.B. durch "duo"). Die beiden
     * Helfer notieren den Zustand hier NICHT selbst (note = 0); das macht
     * dieser Durchlauf EINMAL mit dem schlechtesten Wert.
     */
    rc2 = gui_cfg_read(0);
    if (rc2 != V4P_ST_OK && rc == V4P_ST_OK) {
        rc = rc2;
    }
    rc2 = gui_mouse_read(0);
    if (rc2 != V4P_ST_OK && rc == V4P_ST_OK) {
        rc = rc2;
    }
    /* duo (fw_ver 6). Fehlt das Faehigkeitsbit, kostet das KEINEN Rahmen --
     * gui_duo_read kehrt sofort zurueck. */
    rc2 = gui_duo_read(0);
    if (rc2 != V4P_ST_OK && rc == V4P_ST_OK) {
        rc = rc2;
    }

    (void)v4_state_note(&S, rc);
}

/*
 * "Neu suchen" startet nur den Scan. Das Einsammeln der Ergebnisse macht der
 * Takt (gui_scan_tick): der ESP32 scannt asynchron, und eine Warteschleife
 * wuerde das Fenster fuer Sekunden einfrieren.
 *
 * EIN Fenster, KEIN Dauerbetrieb: `continuous = 0`. Mit Dauerbetrieb scannte
 * der ESP32 nach einem Knopfdruck endlos weiter (Funk und Strom), und
 * GET_STATUS.scan_active taugte nicht mehr als "fertig"-Signal. Die Fensterlaenge
 * wird aus der Wartezeit gerechnet: der Master schickt sie in 1,28-s-Einheiten.
 *
 * `automatisch` ist 1, wenn der Scan nicht vom Knopf kommt, sondern weil eine
 * Verbindung erkannt wurde und die Geraeteliste noch leer ist (gui_liste_auto).
 */
static void act_scan_start(int automatisch)
{
    uint8_t  rc;
    unsigned sek   = (unsigned)(GUI_SCAN_TICKS * GUI_TICK_MS / 1000u);
    uint8_t  units = (uint8_t)((sek * 100u + 127u) / 128u);    /* aufrunden */

    gui_devices_clear();
    rc = v4_scan_start(&M, units, 0);
    (void)v4_state_note(&S, rc);
    if (rc != V4P_ST_OK) {
        gui_msg("[scan] SCAN_START: %s\n", v4_strerror(rc));
        return;
    }
    if (automatisch != 0) {
        gui_msg("[scan] Verbindung erkannt -- die Geraeteliste wird "
                "automatisch geholt (Fenster %u s)\n", sek);
    } else {
        gui_msg("[scan] Scan gestartet (Fenster %u s) -- die Liste fuellt "
                "sich waehrend der Live-Aktualisierung\n", sek);
    }
    s_scan_left        = GUI_SCAN_TICKS;
    s_scan_hard        = GUI_SCAN_MAX_TICKS;
    s_scan_poll        = 0u;
    s_scan_nachgefasst = 0;
    s_scan_nur_lesen   = 0;         /* wir haben die Suche gestartet */
}

/*
 * NUR LESEN: die Gegenstelle sucht selbst (GET_STATUS.scan_active), und ihre
 * Fundtabelle soll in die Liste -- OHNE eigenen SCAN_START/SCAN_STOP.
 */
static void gui_liste_holen(void)
{
    s_scan_left        = GUI_SCAN_TICKS;
    s_scan_hard        = GUI_SCAN_MAX_TICKS;
    s_scan_poll        = 0u;
    s_scan_nachgefasst = 0;
    s_scan_nur_lesen   = 1;
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
    /*
     * DER WICHTIGSTE FALL (Auftrag 16.10.2026): die Gegenstelle hat Geraete
     * GEFUNDEN (dev_count > 0), ohne dass die Oberflaeche gesucht hat -- z.B.
     * weil der ESP32 nach dem Einschalten selbst scannt. Dann wird die Tabelle
     * JE SCAN-GENERATION genau einmal gelesen. Vorher blieb die Liste leer, bis
     * jemand "Neu suchen" drueckte; im Mitschnitt vom 16.10.2026 stand
     * "Geraete 1" in der Zustandszeile, aber keine einzige [scan]-Zeile.
     */
    if (st->dev_count > 0u && st->scan_gen != s_liste_gen
        && s_scan_left == 0u && s_scan_hard == 0u) {
        s_liste_gen = st->scan_gen;
        /*
         * Diese Zeile ist der Nachweis im Mitschnitt: sie erscheint OHNE
         * Knopfdruck, sobald die Gegenstelle Funde meldet. Danach folgen
         * DEV_COUNT/DEV_GET und "[scan] N Geraet(e) in der Liste".
         */
        gui_msg("[scan] Gegenstelle meldet %u Fund(e) (Scan-Generation %u) -- "
                "die Liste wird automatisch gelesen\n",
                (unsigned)st->dev_count, (unsigned)st->scan_gen);
        gui_liste_holen();
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
    if (s_liste_auto != 0 || s_scan_left > 0u || s_scan_hard > 0u
        || s_devn > 0) {
        return;
    }
    s_liste_auto = 1;
    act_scan_start(1);
}

/*
 * Sucht die Gegenstelle laut GET_STATUS noch? Dann darf die Oberflaeche nicht
 * "keine Geraete" behaupten, sondern muss nachfassen (Auftrag vom 14.10.2026,
 * Punkt 4).
 */
static int gui_scan_laeuft(void)
{
    return (S.status_valid != 0 && S.status.scan_active != 0u) ? 1 : 0;
}

/* Eine Scan-Runde: DEV_COUNT; sind Geraete da, gleich die ganze Liste holen.
 *
 * DEV_COUNT wird NICHT in jedem Takt gefragt (GUI_SCAN_POLL_EVERY): der Bus
 * soll auch waehrend einer Suche ruhig bleiben.
 *
 * Ist das Fenster abgelaufen und die Liste noch leer, wird EINMAL nachgefasst,
 * solange die Gegenstelle laut GET_STATUS noch sucht -- hoechstens bis
 * GUI_SCAN_MAX_TICKS. Erst danach steht "keine Geraete gefunden", und zwar mit
 * dem Stand der Gegenstelle (Scan beendet? Generation?) in der Zeile: damit ist
 * aus dem Mitschnitt sofort zu sehen, ob wirklich nichts gefunden wurde oder
 * die Gegenstelle noch mitten in der Suche war.
 */
static void gui_scan_tick(void)
{
    static v4p_dev_t tab[V4P_MAX_DEVICES];  /* ortsfest (4-KB-Stapel) */
    uint8_t count = 0u;
    uint8_t n     = 0u;
    uint8_t i;
    uint8_t rc;

    if (s_scan_left == 0u && s_scan_hard == 0u) {
        return;
    }
    if (s_scan_left > 0u) {
        s_scan_left--;
    }
    if (s_scan_hard > 0u) {
        s_scan_hard--;
    }

    s_scan_poll++;
    if (s_scan_poll < GUI_SCAN_POLL_EVERY) {
        return;
    }
    s_scan_poll = 0u;

    /*
     * Das DEV_GET-Layout haengt am Faehigkeitsbit V4P_CAP_DEV_STATE (fw_ver 6:
     * Verbunden-Byte VOR name_len). Ist noch kein GET_INFO gelaufen, waere das
     * Layout unbekannt und die Namen kaemen als Muell heraus -- genau die
     * Falle vom 16.10.2026. Deshalb hier EINMAL nachfragen.
     */
    if (S.info_valid == 0) {
        v4p_info_t info;

        if (v4_get_info(&M, &info) == V4P_ST_OK) {
            v4_state_put_info(&S, &info, v4_check_info(&info));
        }
    }

    rc = v4_dev_count(&M, &count);
    (void)v4_state_note(&S, rc);
    if (rc != V4P_ST_OK) {
        gui_msg("[scan] DEV_COUNT: %s\n", v4_strerror(rc));
        s_scan_left      = 0u;
        s_scan_hard      = 0u;
        s_scan_nur_lesen = 0;
        return;
    }
    if (count == 0u) {
        if (s_scan_left > 0u) {
            return;                 /* das Fenster laeuft noch */
        }
        if (s_scan_hard > 0u && gui_scan_laeuft() != 0) {
            /* Nachfassen: die Gegenstelle sucht noch, ihre Antwort ist noch
             * nicht die letzte. */
            if (s_scan_nachgefasst == 0) {
                s_scan_nachgefasst = 1;
                gui_msg("[scan] noch keine Geraete, aber die Gegenstelle sucht "
                        "weiter (Scan-Generation %u) -- es wird nachgefasst\n",
                        S.status_valid ? (unsigned)S.status.scan_gen : 0u);
            }
            s_scan_left = GUI_SCAN_TICKS;
            return;
        }
        gui_msg("[scan] keine Geraete gefunden (Scan der Gegenstelle %s, "
                "Generation %u). Ist der Controller eingeschaltet und in "
                "Reichweite?\n",
                (gui_scan_laeuft() != 0) ? "laeuft noch" : "beendet",
                S.status_valid ? (unsigned)S.status.scan_gen : 0u);
        /*
         * Den Scan der Gegenstelle NUR beenden, wenn WIR ihn gestartet haben.
         * Laeuft er auf der ESP32-Seite (Einschaltscan), gehoert er ihr --
         * ein SCAN_STOP von hier waere ein Eingriff ohne Auftrag.
         */
        if (s_scan_nur_lesen == 0) {
            (void)v4_scan_stop(&M);
        }
        s_scan_left      = 0u;
        s_scan_hard      = 0u;
        s_scan_nur_lesen = 0;
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
    /* Diese Scan-Generation steht jetzt in der Liste -- gleich welcher Weg sie
     * geholt hat (Knopf oder automatisch). */
    if (S.status_valid != 0) {
        s_liste_gen = S.status.scan_gen;
    }
    gui_msg("[scan] %u Geraet(e) in der Liste\n", (unsigned)s_devn);
    s_scan_left      = 0u;
    s_scan_hard      = 0u;
    s_scan_nur_lesen = 0;
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
 * Betriebsart-VORGABE setzen -- ausgeloest vom Auswahlfeld oder vom
 * Schaltknopf "Port N Modus >".
 *
 * Seit fw_ver 4 hat das Feld vier Eintraege (auto/aus/Joystick/Maus). "auto"
 * heisst V4P_MODE_AUTO (0xFF): die Vorgabe wird geloescht, die WIRKSAME
 * Betriebsart bleibt zunaechst stehen -- genau das sagt der ESP32 zu. Danach
 * wird GET_PORT_CONFIG nachgelesen, damit die Anzeige den echten Zustand
 * zeigt (mode_soll UND mode_ist).
 *
 * Die Rueckkopplung: jedes set(MUIA_Cycle_Active) loest die Notify aus, auch
 * wenn die Anzeige nur den vom Slave gemeldeten Wert nachzieht. Deshalb wird
 * hier gegen den BESTAETIGTEN Stand aus dem Zustandsblock verglichen: stimmt
 * der angezeigte Wert damit ueberein, gibt es nichts zu tun. Zusaetzlich
 * schuetzt s_cfg_sperre die Zeit, in der gui_show_portcfg die Felder stellt.
 */
static void act_set_mode(int port)
{
    Object *cyc = (port == 0) ? s_mode0 : s_mode1;
    IPTR    v   = 0;
    uint8_t want;
    uint8_t soll;
    uint8_t bestaetigt = 0u;
    uint8_t rc;

    if (cyc == NULL) {
        return;
    }
    (void)get(cyc, MUIA_Cycle_Active, &v);
    want = gui_mode_aus_index(v);
    soll = gui_mode_soll(port);

    if (want == soll) {
        return;         /* nur die Anzeige hat sich nachgezogen */
    }
    if (want == (uint8_t)V4P_MODE_AUTO && s_cfg_ok == 0) {
        /* SET_PORT_MODE mit 0xFF kennt nur fw_ver 4. Ohne das Faehigkeitsbit
         * wird der Wert gar nicht erst geschickt -- und die Anzeige auf den
         * bestaetigten Stand zurueckgezogen. */
        gui_msg("[cfg] \"auto\" braucht fw_ver %u (Faehigkeitsbit 0x%04X); "
                "die Gegenstelle meldet 0x%04X -- nichts geaendert\n",
                (unsigned)V4P_FW_VER_ROUTING, (unsigned)V4P_CAP_PORT_ROUTING,
                S.info_valid ? (unsigned)S.info.caps : 0u);
        gui_cycle_set(cyc, gui_mode_index(soll));
        return;
    }

    rc = v4_set_port_mode(&M, (uint8_t)port, want, &bestaetigt);
    (void)v4_state_note(&S, rc);
    if (rc != V4P_ST_OK) {
        gui_msg("[port%d] SET_PORT_MODE: %s\n", port, v4_strerror(rc));
        /* Die Anzeige auf den bestaetigten Stand zurueckziehen -- sonst zeigt
         * der Knopf eine Betriebsart, die es nicht gibt. */
        gui_cycle_set(cyc, gui_mode_index(soll));
        return;
    }
    if (want == (uint8_t)V4P_MODE_AUTO) {
        gui_msg("[port%d] Vorgabe auto (nach Geraeteart) bestaetigt -- die "
                "wirksame Betriebsart wird beim naechsten Verbinden bzw. von "
                "\"duo\" neu abgeleitet\n", port);
    } else {
        gui_msg("[port%d] Vorgabe %s bestaetigt\n", port,
                v4p_port_mode_name(bestaetigt));
    }
    /* Den echten Zustand nachlesen (die Vorgabe steht, die wirksame kann
     * zunaechst eine andere bleiben) und die Felder danach stellen. */
    if (s_cfg_ok != 0) {
        gui_cfg_read(1);
        gui_show_portcfg();
    } else {
        v4_state_put_mode(&S, (uint8_t)port, bestaetigt);
        v4_dbg_modes(S.mode, "SET_PORT_MODE");
    }
}

/*
 * Der Knopf "Port N Modus >": die VORGABE einen Schritt weiterschalten.
 *
 * Der Kreislauf ist auto -> aus -> Joystick -> Maus -> auto (die Reihenfolge
 * des Auswahlfeldes). Das Auswahlfeld wird dabei MITGEZOGEN -- sein set()
 * loest zwar dieselbe Notify aus, aber der anschliessende direkte Aufruf
 * macht die Wirkung unabhaengig davon sofort.
 */
static void act_mode_weiter(int port)
{
    Object *cyc  = (port == 0) ? s_mode0 : s_mode1;
    uint8_t soll = gui_mode_aus_index(gui_cycle_get(cyc));
    uint8_t next;

    if (soll == (uint8_t)V4P_MODE_AUTO) {
        next = (uint8_t)V4P_PORT_OFF;
    } else if (soll >= (uint8_t)V4P_PORT_MODE_MAX) {
        next = (uint8_t)V4P_MODE_AUTO;
    } else {
        next = (uint8_t)(soll + 1u);
    }
    gui_cycle_set(cyc, gui_mode_index(next));
    act_set_mode(port);
}

/*
 * Geraet (BDA aus der Scanliste) einer Buchse zuordnen -- SET_DEVICE_PORT.
 *
 * "(keines)" ist NICHT setzbar: der ESP32 kennt nur "BDA -> Buchse", es gibt
 * keinen Befehl zum Loesen. Statt eine Zuordnung vorzutaeuschen, wird die
 * Auswahl zurueckgezogen und das ausdruecklich gemeldet.
 */
static void act_set_dev(int port)
{
    Object *cyc = (port == 0) ? s_dev0 : s_dev1;
    IPTR    v;
    int     idx;

    if (cyc == NULL) {
        return;
    }
    if (s_devport_ok == 0) {
        gui_cycle_set(cyc, gui_dev_active(port));
        gui_msg("[cfg] SET_DEVICE_PORT braucht fw_ver %u (Faehigkeitsbit "
                "0x%04X; gemeldet 0x%04X) -- nichts gesendet\n",
                (unsigned)V4P_FW_VER_ROUTING,
                (unsigned)V4P_CAP_DEVICE_PORT,
                S.info_valid ? (unsigned)S.info.caps : 0u);
        return;
    }
    if (s_cfg_sperre != 0) {
        return;
    }
    v = gui_cycle_get(cyc);
    if (v == (IPTR)gui_dev_active(port)) {
        return;                     /* nur die Anzeige hat sich nachgezogen */
    }
    if (v == 0) {
        gui_cycle_set(cyc, gui_dev_active(port));
        gui_msg("[cfg] Eine Zuordnung LAESST sich ueber I2C nicht loesen "
                "(der ESP32 kennt nur BDA -> Buchse). Die Anzeige bleibt auf "
                "dem bestaetigten Stand\n");
        return;
    }
    idx = (int)v - 1;
    if (idx < 0 || idx >= s_devn) {
        gui_cycle_set(cyc, gui_dev_active(port));
        return;
    }
    {
        uint8_t vorher = v4_state_devport(&S, s_devtab[idx].bda);
        uint8_t rc = v4_set_device_port(&M, s_devtab[idx].bda, (uint8_t)port);

        (void)v4_state_note(&S, rc);
        if (rc != V4P_ST_OK) {
            gui_msg("[cfg] SET_DEVICE_PORT (\"%s\" -> Port %d): %s -- die "
                    "Anzeige bleibt auf dem bestaetigten Stand\n",
                    (s_devtab[idx].name[0] != '\0') ? s_devtab[idx].name
                                                    : "(ohne Namen)",
                    port, v4_strerror(rc));
            gui_cycle_set(cyc, gui_dev_active(port));
            return;
        }
        v4_state_put_devport(&S, s_devtab[idx].bda, (uint8_t)port);
        gui_msg("[cfg] Port %d: Geraet \"%s\" (%02X:%02X:%02X:%02X:%02X:%02X) "
                "zugeordnet -- beim naechsten Verbinden, oder sofort wenn der "
                "Port frei ist\n",
                port,
                (s_devtab[idx].name[0] != '\0') ? s_devtab[idx].name
                                                : "(ohne Namen)",
                s_devtab[idx].bda[0], s_devtab[idx].bda[1],
                s_devtab[idx].bda[2], s_devtab[idx].bda[3],
                s_devtab[idx].bda[4], s_devtab[idx].bda[5]);
        /*
         * WARUM DAS HIER STEHT: der ESP32 kennt nur EINE Buchse je Geraet
         * (NVS-Feld "port" der Geraetetabelle). Wer dasselbe Geraet fuer den
         * anderen Port waehlt, VERSCHIEBT die Zuordnung -- der erste Port
         * verliert sie. Das ist kein Fehler der Oberflaeche, sondern die
         * Datenhaltung der Gegenstelle. Fuer EINEN Controller auf BEIDEN
         * Buchsen ist der duo-Betrieb des ESP32 zustaendig; er braucht (und
         * kennt) keine doppelte Zuordnung. Ohne diese Zeile sucht man den
         * Fehler in der Oberflaeche.
         */
        if (vorher != 0xFFu && vorher != (uint8_t)port) {
            gui_msg("[cfg] Hinweis: \"%s\" war Port %u zugeordnet und ist "
                    "jetzt Port %u -- ein Geraet hat im ESP32 immer genau EINE "
                    "Buchse. Fuer EINEN Controller auf beiden Buchsen ist der "
                    "duo-Betrieb des ESP32 zustaendig (nicht ueber I2C "
                    "einstellbar).\n",
                    (s_devtab[idx].name[0] != '\0') ? s_devtab[idx].name
                                                    : "(ohne Namen)",
                    (unsigned)vorher, (unsigned)port);
        }
    }
    gui_cfg_read(1);
    gui_show_portcfg();
}

/* Die drei Knopfquellen-Auswahlfelder einer Buchse lesen (Index = Code). */
static void gui_btn_aus_cycles(int port, uint8_t btn[V4P_BTN_COUNT])
{
    Object **b = (port == 0) ? s_b0 : s_b1;
    unsigned k;

    for (k = 0u; k < V4P_BTN_COUNT; k++) {
        IPTR v = gui_cycle_get(b[k]);

        btn[k] = (v < (IPTR)V4P_TASTE_ANZAHL) ? (uint8_t)v
                                             : (uint8_t)V4P_TASTE_KEINE;
    }
}

/* Meldung, welche Quelle doppelt vergeben ist -- das ist der haeufigste
 * BAD_ARG-Fall und ohne Klartext kaum zu finden. Rueckgabe 1 = gemeldet. */
static int gui_btn_doppelt_melden(int port, const uint8_t btn[V4P_BTN_COUNT])
{
    unsigned i;
    unsigned k;

    for (i = 0u; i < V4P_BTN_COUNT; i++) {
        if (btn[i] == (uint8_t)V4P_TASTE_KEINE) {
            continue;
        }
        for (k = (unsigned)(i + 1u); k < V4P_BTN_COUNT; k++) {
            if (btn[k] == btn[i]) {
                gui_msg("[cfg] BAD_ARG: \"%s\" ist auf Buchse %d zweimal "
                        "vergeben (Pin 6/Pin 9/Pin 5) -- eine nicht-leere "
                        "Quelle darf nur auf EINER Leitung stehen; nichts "
                        "geaendert\n", v4p_taste_name(btn[i]), port);
                return 1;
            }
        }
    }
    return 0;
}

/*
 * Knopfquellen setzen -- SET_PORT_ROUTING mit dem ganzen Paket (Richtungen
 * bleiben, wie sie bestaetigt wurden).
 *
 * BAD_ARG ist hier der Regelfall bei einer Doppelbelegung: der ESP32 lehnt ab
 * und aendert NICHTS. Die Anzeige wird deshalb auf den echten Zustand
 * zurueckgezogen und der Grund im Fenster und im Protokoll genannt.
 */
static void act_set_btn(int port)
{
    uint8_t btn[V4P_BTN_COUNT];
    uint8_t rc;
    unsigned k;
    int gleich = 1;

    if (s_cfg_ok == 0 || S.portcfg_valid == 0) {
        gui_msg("[cfg] Knopfquellen brauchen GET_PORT_CONFIG (fw_ver %u) -- "
                "noch keine gueltige Antwort\n", (unsigned)V4P_FW_VER_ROUTING);
        return;
    }
    if (s_cfg_sperre != 0) {
        return;
    }
    gui_btn_aus_cycles(port, btn);
    for (k = 0u; k < V4P_BTN_COUNT; k++) {
        if (btn[k] != S.portcfg.btn[port][k]) {
            gleich = 0;
        }
    }
    if (gleich != 0) {
        return;                     /* nur die Anzeige hat sich nachgezogen */
    }

    rc = v4_set_port_routing(&M, (uint8_t)port, S.portcfg.dir[port], btn);
    (void)v4_state_note(&S, rc);
    if (rc == V4P_ST_BAD_ARG) {
        if (gui_btn_doppelt_melden(port, btn) == 0) {
            /* Sollte es nicht die Doppelbelegung sein (z.B. eine Maske, die
             * der Slave nicht kennt): trotzdem im Klartext melden. */
            gui_msg("[cfg] BAD_ARG: Buchse %d hat die Knopfquellen abgelehnt "
                    "(Pin6<-%s Pin9<-%s Pin5<-%s) -- nichts geaendert\n",
                    port, v4p_taste_name_short(btn[0]),
                    v4p_taste_name_short(btn[1]),
                    v4p_taste_name_short(btn[2]));
        }
        gui_cfg_read(1);
        gui_show_portcfg();
        return;
    }
    if (rc != V4P_ST_OK) {
        gui_msg("[cfg] SET_PORT_ROUTING (Buchse %d): %s -- nichts geaendert\n",
                port, v4_strerror(rc));
        gui_cfg_read(1);
        gui_show_portcfg();
        return;
    }
    gui_msg("[cfg] Buchse %d: Pin6<-%s Pin9<-%s Pin5<-%s bestaetigt\n", port,
            v4p_taste_name(btn[0]), v4p_taste_name(btn[1]),
            v4p_taste_name(btn[2]));
    gui_cfg_read(1);
    gui_show_portcfg();
}

/*
 * Eine Beitrags-Variante setzen (analog / digital / beide).
 *
 * Die Variante ist NUR eine Bedienhilfe: sie uebersetzt sich in
 * SET_PORT_ROUTING (siehe v4_set_port_beitrag in v4_master.c) und erfindet
 * keinen Befehl. Danach wird wie ueberall GET_PORT_CONFIG gelesen und die
 * Felder werden auf den bestaetigten Stand gestellt.
 *
 * "eigene" (Index 3) ist keine waehlbare Variante -- es beschreibt nur eine
 * Verteilung, die zu keiner der drei passt. Wird es dennoch angeklickt, wird
 * nichts gesendet und das Feld auf den echten Zustand zurueckgezogen.
 */
static void act_set_beitrag(int port)
{
    Object *cyc = (port == 0) ? s_beitrag0 : s_beitrag1;
    IPTR    v;
    uint8_t beitrag_ist;
    uint8_t rc;

    if (s_cfg_ok == 0 || S.portcfg_valid == 0) {
        gui_msg("[cfg] Der Beitrag braucht GET_PORT_CONFIG (fw_ver %u) -- "
                "noch keine gueltige Antwort\n", (unsigned)V4P_FW_VER_ROUTING);
        return;
    }
    if (s_cfg_sperre != 0) {
        return;
    }
    v = gui_cycle_get(cyc);
    beitrag_ist = v4_beitrag_aus_dir(S.portcfg.dir[port]);
    if (v == (IPTR)beitrag_ist) {
        return;                     /* nur die Anzeige hat sich nachgezogen */
    }
    if (v >= (IPTR)V4P_BEITRAG_ANZAHL) {
        gui_cycle_set(cyc, (IPTR)beitrag_ist);
        gui_msg("[cfg] \"eigene\" ist keine Variante, sondern die Anzeige "
                "fuer eine Verteilung, die zu keiner der drei passt -- es "
                "wird nichts gesendet\n");
        return;
    }
    rc = v4_set_port_beitrag(&M, (uint8_t)port, (uint8_t)v);
    (void)v4_state_note(&S, rc);
    if (rc != V4P_ST_OK) {
        char dir[48];

        v4p_dir_text(v4_beitrag_dir((uint8_t)v), dir, sizeof(dir));
        gui_msg("[cfg] SET_PORT_ROUTING (Buchse %d, Beitrag %s -> \"%s\"): "
                "%s -- nichts geaendert\n", port, v4_beitrag_name((uint8_t)v),
                dir, v4_strerror(rc));
        gui_cfg_read(1);
        gui_show_portcfg();
        return;
    }
    {
        char dir[48];
        uint8_t btn[V4P_BTN_COUNT];

        v4_beitrag_btn((uint8_t)v, btn);
        v4p_dir_text(v4_beitrag_dir((uint8_t)v), dir, sizeof(dir));
        gui_msg("[cfg] Buchse %d: Beitrag %s bestaetigt -- Richtungen %s, "
                "Pin6<-%s Pin9<-%s Pin5<-%s\n",
                port, v4_beitrag_name((uint8_t)v), dir,
                v4p_taste_name(btn[0]), v4p_taste_name(btn[1]),
                v4p_taste_name(btn[2]));
        if ((uint8_t)v == (uint8_t)V4P_BEITRAG_BEIDE) {
            gui_msg("[cfg] Buchse %d: analog+digital -- die Einzelschalter "
                    "(Richtungen und Knopfquellen) sind gegenstandslos und "
                    "deshalb gesperrt; der Klartext nennt die Werte\n", port);
        }
    }
    gui_cfg_read(1);
    gui_show_portcfg();
}

/*
 * Eine Maus-Einstellung setzen (fw_ver 5). `feld`:
 *   0 = Tempo (Index in s_mspeedN, 0-basiert), 1 = Totzone -1, 2 = Totzone +1.
 *
 * Das jeweils ANDERE Feld wird aus dem bestaetigten Zustand mitgeschickt --
 * SET_MOUSE_CFG setzt Tempo UND Totzone einer Buchse, es gibt keinen Befehl
 * fuer ein einzelnes Feld. Ist das Faehigkeitsbit nicht gesetzt, wird NICHTS
 * gesendet und das Feld zurueckgezogen.
 */
/*
 * duo setzen (SET_DUO, fw_ver 6).
 *
 * duo heisst: EIN Controller bedient BEIDE Buchsen -- die eine als Maus, die
 * andere als Joystick. Es ist KEINE Geraetezuordnung: die bleibt je Geraet
 * genau eine Buchse. Deshalb wird hier auch nichts an der Liste geaendert;
 * die Oberflaeche MARKIERT nur, welcher Port den duo-Part uebernimmt.
 */
static void act_set_duo(void)
{
    IPTR    v;
    uint8_t vorher = (S.duo_valid != 0) ? S.duo : (uint8_t)V4P_DUO_OFF;
    uint8_t rc;

    if (s_duo == NULL) {
        return;
    }
    if (s_duo_ok == 0) {
        gui_cycle_set(s_duo, (IPTR)vorher);
        gui_msg("[duo] Die Gegenstelle kann duo nicht (Faehigkeitsbit 0x%04X "
                "fehlt, fw_ver %u noetig) -- nichts gesendet\n",
                (unsigned)V4P_CAP_DUO, (unsigned)V4P_FW_VER_DUO);
        return;
    }
    if (s_cfg_sperre != 0) {
        return;
    }
    v = gui_cycle_get(s_duo);
    if (v > (IPTR)2) {
        v = 0;
    }
    if (S.duo_valid != 0 && (uint8_t)v == S.duo) {
        return;                     /* nur die Anzeige hat sich nachgezogen */
    }
    rc = v4_set_duo(&M, (uint8_t)v);
    (void)v4_state_note(&S, rc);
    if (rc == V4P_ST_BAD_ARG) {
        gui_msg("[duo] SET_DUO %u: BAD_ARG -- erlaubt sind nur 0 (aus), "
                "1 (Port 0 Maus) und 2 (Port 1 Maus); die Einstellung bleibt "
                "%u (%s)\n",
                (unsigned)v, (unsigned)vorher, v4p_duo_name(vorher));
        gui_cycle_set(s_duo, (IPTR)vorher);
        gui_show_portcfg();
        return;
    }
    if (rc != V4P_ST_OK) {
        gui_msg("[duo] SET_DUO %u: %s -- nichts geaendert\n", (unsigned)v,
                v4_strerror(rc));
        gui_cycle_set(s_duo, (IPTR)vorher);
        gui_show_portcfg();
        return;
    }
    v4_state_put_duo(&S, (uint8_t)v);
    /* Immer eine Zeile je Aenderung, mit dem ZURUECKGELESENEN Wert. */
    (void)gui_duo_read(1);
    gui_msg("[duo] SET_DUO %u (%s) bestaetigt -- vorher %u (%s), gelesen %u "
            "(%s)%s\n",
            (unsigned)v, v4p_duo_name((uint8_t)v),
            (unsigned)vorher, v4p_duo_name(vorher),
            (unsigned)(S.duo_valid ? S.duo : 0u),
            v4p_duo_name(S.duo_valid ? S.duo : (uint8_t)0u),
            (v == (IPTR)0) ? "" : " -- EIN Controller auf BEIDEN Buchsen");
    gui_show_portcfg();
}

static void act_set_maus(int port, int feld)
{
    Object *speed = (port == 0) ? s_mspeed0 : s_mspeed1;
    int     neu_speed;
    int     neu_totzone;
    uint8_t rc;

    if (s_mousecfg_ok == 0) {
        gui_msg("[cfg] Die Gegenstelle kann die Maus-Einstellungen nicht "
                "(Faehigkeitsbit 0x%04X fehlt) -- es wird nichts gesendet\n",
                (unsigned)V4P_CAP_MOUSE_CFG);
        gui_show_portcfg();
        return;
    }
    if (S.mousecfg_valid == 0) {
        gui_msg("[cfg] Maus-Einstellungen brauchen GET_MOUSE_CFG (fw_ver %u) "
                "-- noch keine gueltige Antwort\n",
                (unsigned)V4P_FW_VER_MOUSE);
        return;
    }
    if (s_cfg_sperre != 0) {
        return;
    }

    neu_speed   = (int)S.mousecfg.speed[port];
    neu_totzone = (int)S.mousecfg.totzone[port];
    if (feld == 0) {
        IPTR v = gui_cycle_get(speed);

        neu_speed = (int)v + (int)V4P_MOUSE_SPEED_MIN;
        if (neu_speed == (int)S.mousecfg.speed[port]) {
            return;                 /* nur die Anzeige hat sich nachgezogen */
        }
    } else if (feld == 1) {
        if (neu_totzone <= (int)V4P_MOUSE_TOTZONE_MIN) {
            gui_msg("[cfg] Buchse %d: Totzone ist schon am unteren Ende "
                    "(%u %%)\n", port, (unsigned)V4P_MOUSE_TOTZONE_MIN);
            return;
        }
        neu_totzone--;
    } else {
        if (neu_totzone >= (int)V4P_MOUSE_TOTZONE_MAX) {
            gui_msg("[cfg] Buchse %d: Totzone ist schon am oberen Ende "
                    "(%u %%)\n", port, (unsigned)V4P_MOUSE_TOTZONE_MAX);
            return;
        }
        neu_totzone++;
    }

    if (!v4p_mouse_speed_valid((uint8_t)neu_speed)
        || !v4p_mouse_totzone_valid((uint8_t)neu_totzone)) {
        gui_msg("[cfg] Buchse %d: Tempo %d, Totzone %d %% liegen ausserhalb "
                "(Tempo %u..%u, Totzone %u..%u) -- nichts geaendert\n",
                port, neu_speed, neu_totzone,
                (unsigned)V4P_MOUSE_SPEED_MIN, (unsigned)V4P_MOUSE_SPEED_MAX,
                (unsigned)V4P_MOUSE_TOTZONE_MIN,
                (unsigned)V4P_MOUSE_TOTZONE_MAX);
        gui_mouse_read(1);
        gui_show_portcfg();
        return;
    }

    {
        char vorher[128];

        gui_maus_text((S.mousecfg_valid != 0) ? &S.mousecfg : NULL,
                      vorher, sizeof(vorher));
        rc = v4_set_mouse_cfg(&M, (uint8_t)port, (uint8_t)neu_speed,
                              (uint8_t)neu_totzone);
        (void)v4_state_note(&S, rc);

        if (rc != V4P_ST_OK) {
            /* IMMER melden (nicht nur mit -trace): genau hier war nicht
             * nachvollziehbar, ob eine Tempo-Aenderung ankommt. */
            gui_msg("[maus] SET_MOUSE_CFG port=%d Tempo %d/%u Totzone %d%% "
                    "-> %s; unveraendert gelesen: %s\n",
                    port, neu_speed, (unsigned)V4P_MOUSE_SPEED_MAX,
                    neu_totzone,
                    (rc == V4P_ST_BAD_ARG) ? "BAD_ARG (abgelehnt)"
                                           : v4_strerror(rc),
                    vorher);
            gui_mouse_read(1);      /* den ECHTEN Stand nachlesen */
            gui_show_portcfg();
            return;
        }

        /*
         * Das Echo ist feldweise geprueft -- auf V4P_ST_OK stehen die Werte
         * fest. Sie werden uebernommen, damit die Anzeige nicht auf einen
         * alten Stand zurueckfaellt, und danach wird trotzdem nachgelesen.
         */
        S.mousecfg.speed[port]   = (uint8_t)neu_speed;
        S.mousecfg.totzone[port] = (uint8_t)neu_totzone;
        S.mousecfg_valid         = 1;
        gui_mouse_read(1);
        {
            char gelesen[128];

            gui_maus_text(&S.mousecfg, gelesen, sizeof(gelesen));
            gui_msg("[maus] SET_MOUSE_CFG port=%d Tempo %d/%u Totzone %d%% "
                    "bestaetigt -- vorher %s, gelesen %s\n",
                    port, neu_speed, (unsigned)V4P_MOUSE_SPEED_MAX,
                    neu_totzone, vorher, gelesen);
        }
    }
    gui_show_portcfg();
}

/*
 * Seite umschalten: Seite 0 = Anzeige, Seite 1 = Konfiguration.
 *
 * Die verdeckte Seite wird im Takt trotzdem weiter gezeichnet; hier wird nur
 * die Sichtbarkeit gesetzt und die neu sichtbare Seite einmal sofort
 * aktualisiert -- sonst zeigt sie fuer bis zu 200 ms den Stand vom letzten
 * Wechsel.
 */
static void act_seite(void)
{
    IPTR v = gui_cycle_get(s_seite);

    if (v > (IPTR)2) {
        v = 0;
    }
    if (s_seiten != NULL) {
        set(s_seiten, MUIA_Group_ActivePage, v);
    }
    /* Beim Wechsel wieder nach oben: die Seiten liegen in einer Scrollgruppe,
     * und die Position der anderen Seite ist dort sinnlos. */
    if (s_virt != NULL) {
        set(s_virt, MUIA_Virtgroup_Top, 0);
    }
    if (v == 0) {
        gui_show_ports();
    } else {
        /* Seite 1 "Ports" und Seite 2 "Maus" teilen sich die Felder; beide
         * werden gestellt, egal welche sichtbar ist. */
        gui_show_portcfg();
    }
    /* Die Mindestmasse der gerade sichtbaren Seite ins Protokoll (Diagnose
     * fuer "passt die Seite in das Fenster?"). */
    gui_mindestmasse_melden();
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
 * Hardware wird dabei neu abgetastet -- der Knopf soll frische Registerwerte
 * zeigen und nicht den Stand des letzten Takts. Die Maus-Einstellungen werden
 * mitgelesen: sie aendern sich nur auf Wunsch, aber der Knopf soll auch sie
 * frisch zeigen. */
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
    gui_show_portcfg();
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
 *   4. Aenderungen sofort melden, Registerwerte fuer die Herzzeile bauen
 *   5. Lebenszeichen          -- GENAU EINE Zeile je Sekunde, mit den
 *                                Registerwerten
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

    /* 2. Laufenden Scan weiterschieben. Auch wenn das Regel-Fenster schon
     *    abgelaufen ist (s_scan_left == 0), laeuft die harte Obergrenze
     *    weiter: nur so kann bei noch suchender Gegenstelle nachgefasst
     *    werden. */
    if (s_scan_left > 0u || s_scan_hard > 0u) {
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
     *    direkten Stand fuer die Herzzeile bereitlegen. */
    gui_joy_aenderungen();
    gui_joy_herz_zusatz();

    /* 6. Lebenszeichen: EINE Zeile je Sekunde. Sie traegt Zaehler, letzten
     *    Status, verworfene Rahmen, Wiederholungen UND die Registerwerte. */
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
    gui_show_portcfg();
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
 * WOZU: die Vorgabe ist 700x480 (mit -w/-h aenderbar). Laeuft die Workbench
 * aber auf einem kleineren Schirm -- auf dieser V4 ist 640x360 eingestellt --
 * dann ist das Fenster HOEHER als der Schirm. Das ist nicht nur unbrauchbar
 * (die unterste Zeile, das Logfeld, ist nie zu sehen): MUI rechnet beim
 * Oeffnen eines Fensters mit SizeGadget mit einem Groessenbereich, den es auf
 * diesem Schirm nicht geben kann. Deshalb wird die gewuenschte Groesse auf den
 * Schirm begrenzt -- und das ausdruecklich gemeldet, damit niemand raetselt,
 * warum das Fenster kleiner ist.
 *
 * Passt 700x480 (oder was -w/-h vorgibt), aendert sich NICHTS. Ohne
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
            WindowContents, s_root = VGroup,
                /*
                 * WARUM ANZEIGE UND KONFIGURATION AUF ZWEI SEITEN LIEGEN:
                 *
                 * Mit fw_ver 4 ist der Inhalt deutlich gewachsen -- je Buchse
                 * kommen Geraet, Richtungen und drei Knopfquellen dazu. In
                 * einem kleinen Fenster (und auf einem kleinen Schirm, siehe
                 * gui_fenster_an_schirm, begrenzt MUI zusaetzlich) passt das
                 * nicht mehr untereinander:
                 * der bisherige Inhalt war schon an der Grenze, und MUI
                 * drueckt Textfelder ohne Gewicht unter ihre Mindesthoehe --
                 * sie saehen dann LEER aus.
                 *
                 * Deshalb eine SEITENGRUPPE (MUIA_Group_PageMode): Seite 0 ist
                 * die gewohnte Anzeige (Buchsen, Direktwerte), Seite 1 die neue
                 * Konfiguration. Die Groesse einer Seitengruppe ist laut
                 * MUI_Group.doc "the maximum minimum width/height of all its
                 * children" -- die Konfigurationsseite ist KLEINER als die
                 * Anzeigeseite, die Mindestgroesse des Fensters aendert sich
                 * also NICHT. Genau das ist der Punkt: das Fenster bleibt so
                 * gross wie bisher und wird nicht auf eine Groesse gezwungen,
                 * die nicht auf den Schirm passt.
                 *
                 * Umgeschaltet wird mit dem Auswahlfeld "Seite"; der Inhalt
                 * beider Seiten wird im Takt weiter gezeichnet (die verdeckte
                 * Seite holt beim naechsten Wechsel sofort den aktuellen
                 * Stand).
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
                /*
                 * Der Umschalter liegt NEBEN der dreizeiligen Controller-Zeile
                 * und kostet damit KEINE zusaetzliche Hoehe: die Zeile ist
                 * ohnehin drei Zeilen hoch, das Auswahlfeld ist flacher. Genau
                 * darum geht es hier -- das Fenster darf nicht hoeher werden
                 * als bisher, sonst passt es auf diesem Schirm nicht mehr.
                 */
                Child, HGroup,
                    MUIA_Weight, 0,
                    Child, s_ctrl = TextObject,
                        MUIA_Text_Contents, (IPTR)"Controller: (wird gelesen)",
                        MUIA_Text_PreParse, (IPTR)"\33l",
                        MUIA_Text_SetMin, FALSE,
                        MUIA_Weight, 0,
                    End,
                    Child, VGroup,
                        MUIA_Weight, 0,
                        Child, TextObject,
                            MUIA_Text_Contents, (IPTR)"Seite:",
                            MUIA_Text_PreParse, (IPTR)"\33l",
                            MUIA_Weight, 0,
                        End,
                        Child, s_seite = CycleObject,
                            MUIA_Cycle_Entries, (IPTR)gui_seiten_eintraege,
                            MUIA_Weight, 0,
                        End,
                    End,
                End,
                /*
                 * DIE SEITEN LIEGEN IN EINER SCROLLGRUPPE.
                 *
                 * WARUM: die Mindesthoehe einer Seitengruppe ist das MAXIMUM ueber
                 * alle Seiten (MUI_Group.doc). Ist eine Seite hoeher als das Fenster,
                 * drueckt MUI die Felder zusammen oder schneidet sie ab -- am Geraet
                 * war die Konfigurationsseite dann erst nach dem Vergroessern des
                 * Fensters bedienbar. Die Scrollgruppe mit einer virtuellen Gruppe
                 * (TryFit FALSE) nimmt den Seiten die Macht, das Fenster zu vergroessern:
                 * der Inhalt wird gescrollt, und zwar NUR dann, wenn er wirklich nicht
                 * passt (AutoBars). Der Umschalter "Seite:" steht AUSSERHALB und ist
                 * damit immer sichtbar und bedienbar.
                 */
                Child, s_scroll = ScrollgroupObject,
                    MUIA_Scrollgroup_Contents, s_virt = VirtgroupObject,
                        MUIA_Virtgroup_TryFit, FALSE,
                        Child, s_seiten = VGroup,
                            MUIA_Group_PageMode, TRUE,
                            MUIA_Group_ActivePage, 0,
                    /*
                     * SEITE 0: die laufende Anzeige -- NUR die Portdaten.
                     *
                     * Die Buchsenfelder zeigen seit dem 14.10.2026
                     * ausschliesslich die direkte Registerabfrage
                     * (JOY0DAT/JOY1DAT, CIAAPRA/POTINP). GET_INPUT wird nicht
                     * mehr abgefragt; einen Vergleich "direkt <-> I2C" gibt es
                     * weder hier noch im Protokoll. Die wirksame Betriebsart
                     * daneben kommt weiterhin vom ESP32 (Einstellung, kein
                     * Eingabewert) und ist als "(ESP32)" gekennzeichnet.
                     *
                     * WARUM DIESE FELDER GEWICHT 0 HABEN:
                     *
                     * MUI gibt jedem Objekt ohne MUIA_Weight das Vorgabegewicht
                     * 100 und verteilt den Platz danach. Die Anzeigefelder
                     * behalten mit Gewicht 0 immer ihre Mindestgroesse; den
                     * Platz bekommen die Listen (Gewicht 30/40).
                     *
                     * MUIA_Text_SetMin, FALSE gehoert dazu: die Vorgabe TRUE
                     * macht die Textbreite zur Mindestbreite des Fensterinhalts.
                     * Die Controller-Zeile ist rund 100 Zeichen lang -- das
                     * waere breiter als das Fenster und wuerde MUI das Fenster
                     * aufziehen. Mit FALSE bleibt das Fenster so breit wie
                     * vorgegeben und der Text wird am Rand abgeschnitten (die
                     * wichtigen Angaben stehen am Zeilenanfang).
                     */
                    Child, s_seite_anzeige = VGroup,
                        MUIA_Weight, 0,
                        Child, HGroup,
                            MUIA_Weight, 0,
                            Child, VGroup,
                                Child, HGroup,
                                    Child, TextObject,
                                        MUIA_Text_Contents, (IPTR)"Port 0:",
                                        /* Auch die Beschriftung bleibt auf ihrer
                                         * Mindestbreite. Sonst nimmt sie den Platz
                                         * der Zeile fuer sich und schiebt den
                                         * kompakten Auswahlknopf an den rechten
                                         * Rand. */
                                        MUIA_Weight, 0,
                                    End,
                                    Child, TextObject,
                                        MUIA_Text_Contents, (IPTR)"Vorgabe",
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
                                    Child, s_wirk0 = TextObject,
                                        MUIA_Text_Contents, (IPTR)"wirkt: ?",
                                        MUIA_Text_PreParse, (IPTR)"\33l",
                                        MUIA_Weight, 0,
                                    End,
                                End,
                                Child, s_port0 = TextObject,
                                    MUIA_Text_Contents, (IPTR)" ",
                                    MUIA_Text_PreParse, (IPTR)"\33l",
                                    /* Die Portdaten sind die einzige Aussage
                                     * im Fenster; SetMin FALSE haelt die
                                     * Spaltenbreite bei der Aufteilung und
                                     * verhindert, dass ein langer Rohwert das
                                     * Fenster aufzieht. */
                                    MUIA_Text_SetMin, FALSE,
                                    MUIA_Weight, 0,
                                End,
                            End,
                            Child, VGroup,
                                Child, HGroup,
                                    Child, TextObject,
                                        MUIA_Text_Contents, (IPTR)"Port 1:",
                                        MUIA_Weight, 0,
                                    End,
                                    Child, TextObject,
                                        MUIA_Text_Contents, (IPTR)"Vorgabe",
                                        MUIA_Weight, 0,
                                    End,
                                    Child, s_mode1 = CycleObject,
                                        MUIA_Cycle_Entries,
                                            (IPTR)gui_modus_eintraege,
                                        MUIA_Weight, 0,
                                    End,
                                    Child, s_wirk1 = TextObject,
                                        MUIA_Text_Contents, (IPTR)"wirkt: ?",
                                        MUIA_Text_PreParse, (IPTR)"\33l",
                                        MUIA_Weight, 0,
                                    End,
                                End,
                                Child, s_port1 = TextObject,
                                    MUIA_Text_Contents, (IPTR)" ",
                                    MUIA_Text_PreParse, (IPTR)"\33l",
                                    MUIA_Text_SetMin, FALSE,
                                    MUIA_Weight, 0,
                                End,
                            End,
                        End,
                        /*
                         * Die Rohwerte der Feuerregister, volle Fensterbreite.
                         * Hier steht NUR, was direkt gelesen wird; einen
                         * I2C-Vergleich gibt es nicht mehr (GET_INPUT wird
                         * nicht mehr abgefragt). Der Text darunter sagt das
                         * ausdruecklich.
                         */
                        Child, s_regtext = TextObject,
                            MUIA_Text_Contents,
                                (IPTR)"Rohregister (direkt) ...",
                            MUIA_Text_PreParse, (IPTR)"\33l",
                            MUIA_Text_SetMin, FALSE,
                            MUIA_Weight, 0,
                        End,
                    End,
                            /*
                             * SEITE 1: "Ports" -- Geraet, Beitrag, Richtungen
                             * (NUR Anzeige) und die drei Knopfquellen.
                             *
                             * WARUM KEINE RICHTUNGSSCHALTER MEHR: welche
                             * Richtungsquellen gelten, legt der Beitrag
                             * (analog/digital/beides) fest -- drei Checkboxen
                             * daneben waren eine zweite Stelle fuer dieselbe
                             * Aussage und widersprachen ihr. Gezeigt wird die
                             * WIRKSAME Maske als Klartext.
                             *
                             * WARUM DIE DREI KNOPFQUELLEN IN EINER ZEILE: jede
                             * Zeile kostet rund 20 Pixel; im Fenster ist das
                             * der Unterschied zwischen "passt" und "muss
                             * scrollen". Die Eintraege sind dafuer KURZ
                             * (gui_taste_kurz_eintraege, z.B. "Gr.1"); die
                             * vollen Namen stehen in der Klartextzeile.
                             */
                            Child, s_seite_ports = VGroup,
                                MUIA_Weight, 0,
                                Child, s_cfgtext = TextObject,
                                    MUIA_Text_Contents,
                                        (IPTR)"Ports: warte auf "
                                             "GET_PORT_CONFIG ...",
                                    MUIA_Text_PreParse, (IPTR)"\33l",
                                    MUIA_Text_SetMin, FALSE,
                                    MUIA_Weight, 0,
                                End,
                                Child, HGroup,
                                    MUIA_Weight, 0,
                                    Child, TextObject,
                                        MUIA_Text_Contents, (IPTR)" ",
                                        MUIA_FixWidth, 96,
                                    End,
                                    Child, TextObject,
                                        MUIA_Text_Contents, (IPTR)"Port 0",
                                        MUIA_FixWidth, 240,
                                        MUIA_Text_PreParse, (IPTR)"\33b",
                                    End,
                                    Child, TextObject,
                                        MUIA_Text_Contents, (IPTR)"Port 1",
                                        MUIA_FixWidth, 240,
                                        MUIA_Text_PreParse, (IPTR)"\33b",
                                    End,
                                End,
                                Child, HGroup,
                                    MUIA_Weight, 0,
                                    Child, TextObject,
                                        MUIA_Text_Contents, (IPTR)"Geraet",
                                        MUIA_FixWidth, 96,
                                        MUIA_Text_PreParse, (IPTR)"\33l",
                                    End,
                                    Child, HGroup,
                                        MUIA_FixWidth, 240,
                                        Child, s_dev0 = CycleObject,
                                            MUIA_Cycle_Entries,
                                                (IPTR)s_dev_eintraege,
                                            MUIA_Weight, 0,
                                        End,
                                    End,
                                    Child, HGroup,
                                        MUIA_FixWidth, 240,
                                        Child, s_dev1 = CycleObject,
                                            MUIA_Cycle_Entries,
                                                (IPTR)s_dev_eintraege,
                                            MUIA_Weight, 0,
                                        End,
                                    End,
                                End,
                                Child, HGroup,
                                    MUIA_Weight, 0,
                                    Child, TextObject,
                                        MUIA_Text_Contents, (IPTR)"Beitrag",
                                        MUIA_FixWidth, 96,
                                        MUIA_Text_PreParse, (IPTR)"\33l",
                                    End,
                                    Child, HGroup,
                                        MUIA_FixWidth, 240,
                                        Child, s_beitrag0 = CycleObject,
                                            MUIA_Cycle_Entries,
                                                (IPTR)gui_beitrag_eintraege,
                                            MUIA_Weight, 0,
                                        End,
                                    End,
                                    Child, HGroup,
                                        MUIA_FixWidth, 240,
                                        Child, s_beitrag1 = CycleObject,
                                            MUIA_Cycle_Entries,
                                                (IPTR)gui_beitrag_eintraege,
                                            MUIA_Weight, 0,
                                        End,
                                    End,
                                End,
                                Child, HGroup,
                                    MUIA_Weight, 0,
                                    Child, TextObject,
                                        MUIA_Text_Contents, (IPTR)"Richtungen",
                                        MUIA_FixWidth, 96,
                                        MUIA_Text_PreParse, (IPTR)"\33l",
                                    End,
                                    Child, HGroup,
                                        MUIA_FixWidth, 240,
                                        Child, s_rdir0 = TextObject,
                                            MUIA_Text_Contents, (IPTR)"?",
                                            MUIA_Text_SetMin, FALSE,
                                            MUIA_Weight, 0,
                                        End,
                                    End,
                                    Child, HGroup,
                                        MUIA_FixWidth, 240,
                                        Child, s_rdir1 = TextObject,
                                            MUIA_Text_Contents, (IPTR)"?",
                                            MUIA_Text_SetMin, FALSE,
                                            MUIA_Weight, 0,
                                        End,
                                    End,
                                End,
                                Child, HGroup,
                                    MUIA_Weight, 0,
                                    Child, TextObject,
                                        MUIA_Text_Contents, (IPTR)"Knopf 6/9/5",
                                        MUIA_FixWidth, 96,
                                        MUIA_Text_PreParse, (IPTR)"\33l",
                                    End,
                                    Child, HGroup,
                                        MUIA_FixWidth, 240,
                                        Child, s_b0[0] = CycleObject,
                                            MUIA_Cycle_Entries,
                                                (IPTR)gui_taste_kurz_eintraege,
                                            MUIA_Weight, 0,
                                        End,
                                        Child, s_b0[1] = CycleObject,
                                            MUIA_Cycle_Entries,
                                                (IPTR)gui_taste_kurz_eintraege,
                                            MUIA_Weight, 0,
                                        End,
                                        Child, s_b0[2] = CycleObject,
                                            MUIA_Cycle_Entries,
                                                (IPTR)gui_taste_kurz_eintraege,
                                            MUIA_Weight, 0,
                                        End,
                                    End,
                                    Child, HGroup,
                                        MUIA_FixWidth, 240,
                                        Child, s_b1[0] = CycleObject,
                                            MUIA_Cycle_Entries,
                                                (IPTR)gui_taste_kurz_eintraege,
                                            MUIA_Weight, 0,
                                        End,
                                        Child, s_b1[1] = CycleObject,
                                            MUIA_Cycle_Entries,
                                                (IPTR)gui_taste_kurz_eintraege,
                                            MUIA_Weight, 0,
                                        End,
                                        Child, s_b1[2] = CycleObject,
                                            MUIA_Cycle_Entries,
                                                (IPTR)gui_taste_kurz_eintraege,
                                            MUIA_Weight, 0,
                                        End,
                                    End,
                                End,
                                Child, s_cfghint = TextObject,
                                    MUIA_Text_Contents,
                                        (IPTR)"Beitrag: analog = Stick links + "
                                             "L1/R1, digital = D-Pad + "
                                             "A/B/Start, beides = alle "
                                             "Richtungen + Gruppe 1/2/3, "
                                             "eigene = passt zu keiner.",
                                    MUIA_Text_PreParse, (IPTR)"\33l",
                                    MUIA_Text_SetMin, FALSE,
                                    MUIA_Weight, 0,
                                End,
                            End,
                            /*
                             * SEITE 2: "Maus" -- Tempo und Totzone je Port.
                             *
                             * Eigene Seite, weil die Konfigurationsseite sonst
                             * wieder hoeher waere als das Fenster -- und die
                             * Maus-Werte braucht man selten.
                             */
                            Child, s_seite_maus = VGroup,
                                MUIA_Weight, 0,
                                Child, HGroup,
                                    MUIA_Weight, 0,
                                    Child, TextObject,
                                        MUIA_Text_Contents, (IPTR)" ",
                                        MUIA_FixWidth, 96,
                                    End,
                                    Child, TextObject,
                                        MUIA_Text_Contents, (IPTR)"Port 0",
                                        MUIA_FixWidth, 240,
                                        MUIA_Text_PreParse, (IPTR)"\33b",
                                    End,
                                    Child, TextObject,
                                        MUIA_Text_Contents, (IPTR)"Port 1",
                                        MUIA_FixWidth, 240,
                                        MUIA_Text_PreParse, (IPTR)"\33b",
                                    End,
                                End,
                                Child, HGroup,
                                    MUIA_Weight, 0,
                                    Child, TextObject,
                                        MUIA_Text_Contents, (IPTR)"Maus",
                                        MUIA_FixWidth, 96,
                                        MUIA_Text_PreParse, (IPTR)"\33l",
                                    End,
                                    Child, HGroup,
                                        MUIA_FixWidth, 240,
                                        Child, TextObject,
                                            MUIA_Text_Contents, (IPTR)"Tempo",
                                            MUIA_Text_SetMin, FALSE,
                                            MUIA_Weight, 0,
                                        End,
                                        Child, s_mspeed0 = CycleObject,
                                            MUIA_Cycle_Entries,
                                                (IPTR)gui_speed_eintraege,
                                            MUIA_Disabled, TRUE,
                                            MUIA_Weight, 0,
                                        End,
                                        Child, TextObject,
                                            MUIA_Text_Contents, (IPTR)"Totz.",
                                            MUIA_Text_SetMin, FALSE,
                                            MUIA_Weight, 0,
                                        End,
                                        Child, s_mtotal0 = TextObject,
                                            MUIA_Text_Contents, (IPTR)"? %",
                                            MUIA_Text_SetMin, FALSE,
                                            MUIA_Disabled, TRUE,
                                            MUIA_Weight, 0,
                                        End,
                                        Child, s_mtot0m = MUI_MakeObject(
                                                  MUIO_Button,
                                                  (IPTR)"-", NULL),
                                        Child, s_mtot0p = MUI_MakeObject(
                                                  MUIO_Button,
                                                  (IPTR)"+", NULL),
                                    End,
                                    Child, HGroup,
                                        MUIA_FixWidth, 240,
                                        Child, TextObject,
                                            MUIA_Text_Contents, (IPTR)"Tempo",
                                            MUIA_Text_SetMin, FALSE,
                                            MUIA_Weight, 0,
                                        End,
                                        Child, s_mspeed1 = CycleObject,
                                            MUIA_Cycle_Entries,
                                                (IPTR)gui_speed_eintraege,
                                            MUIA_Disabled, TRUE,
                                            MUIA_Weight, 0,
                                        End,
                                        Child, TextObject,
                                            MUIA_Text_Contents, (IPTR)"Totz.",
                                            MUIA_Text_SetMin, FALSE,
                                            MUIA_Weight, 0,
                                        End,
                                        Child, s_mtotal1 = TextObject,
                                            MUIA_Text_Contents, (IPTR)"? %",
                                            MUIA_Text_SetMin, FALSE,
                                            MUIA_Disabled, TRUE,
                                            MUIA_Weight, 0,
                                        End,
                                        Child, s_mtot1m = MUI_MakeObject(
                                                  MUIO_Button,
                                                  (IPTR)"-", NULL),
                                        Child, s_mtot1p = MUI_MakeObject(
                                                  MUIO_Button,
                                                  (IPTR)"+", NULL),
                                    End,
                                End,
                                /*
                                 * duo (fw_ver 6): EIN Controller auf BEIDEN
                                 * Buchsen -- die eine als Maus, die andere
                                 * als Joystick. Das Auswahlfeld setzt NUR die
                                 * Betriebsarten; die Geraetezuordnung bleibt
                                 * unangetastet. Der Klartext daneben sagt, was
                                 * der Wert bedeutet, damit niemand "Port 1
                                 * Maus" fuer eine Geraetezuordnung haelt.
                                 */
                                Child, HGroup,
                                    MUIA_Weight, 0,
                                    Child, TextObject,
                                        MUIA_Text_Contents, (IPTR)"duo",
                                        MUIA_FixWidth, 96,
                                        MUIA_Text_PreParse, (IPTR)"\33l",
                                    End,
                                    Child, HGroup,
                                        MUIA_FixWidth, 240,
                                        Child, s_duo = CycleObject,
                                            MUIA_Cycle_Entries,
                                                (IPTR)gui_duo_eintraege,
                                            MUIA_Disabled, TRUE,
                                            MUIA_Weight, 0,
                                        End,
                                    End,
                                    Child, HGroup,
                                        MUIA_FixWidth, 240,
                                        Child, s_duotext = TextObject,
                                            MUIA_Text_Contents, (IPTR)"?",
                                            MUIA_Text_SetMin, FALSE,
                                            MUIA_Weight, 0,
                                        End,
                                    End,
                                End,

                                Child, s_maushint = TextObject,
                                    MUIA_Text_Contents,
                                        (IPTR)"Tempo und Totzone gelten JE PORT. "
                                             "duo = EIN Controller auf beiden "
                                             "Ports; es setzt nur die "
                                             "Betriebsarten.",
                                    MUIA_Text_PreParse, (IPTR)"\33l",
                                    MUIA_Text_SetMin, FALSE,
                                    MUIA_Weight, 0,
                                End,
                            End,
                        End,
                    End,
                    MUIA_Scrollgroup_FreeHoriz, FALSE,
                    MUIA_Scrollgroup_FreeVert, TRUE,
                    MUIA_Scrollgroup_AutoBars, TRUE,
                    MUIA_Weight, 60,
                    MUIA_MinHeight, 175,
                End,
                /*
                 * Die beiden Knopfreihen und die Listen stehen AUSSERHALB der
                 * Seitengruppe und sind damit auf beiden Seiten sichtbar.
                 *
                 * Die beiden Listen sind die einzigen Kinder mit Gewicht: sie
                 * bekommen den Platz, der nach den festen Anzeigefeldern
                 * uebrig bleibt (Gewicht 30 fuer die Geraeteliste, 40 fuer das
                 * Logfeld). MUIA_MinHeight haelt sie auch dann sichtbar, wenn
                 * das Fenster kleiner gezogen wird -- sonst blieben von einer
                 * Liste nur ein paar Pixel uebrig und sie saehe leer aus.
                 */
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
                                          (IPTR)"Ports tauschen", NULL),
                    Child, TextObject,
                        MUIA_Text_Contents, (IPTR)"Direkt aus den Registern der V4 "
                            "(JOY0DAT/JOY1DAT, CIAAPRA/POTINP)",
                    End,
                End,
                Child, s_devs_lv = ListviewObject,
                    MUIA_Listview_List, s_devs = ListObject,
                    End,
                    MUIA_Weight, 20,
                    MUIA_MinHeight, 20,
                End,
                Child, ListviewObject,
                    MUIA_Listview_List, s_logl = ListObject,
                    End,
                    MUIA_Weight, 20,
                    MUIA_MinHeight, 20,
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
    if (s_regtext == NULL || s_port0 == NULL || s_port1 == NULL
        || s_logl == NULL || s_cfgtext == NULL || s_dev0 == NULL
        || s_dev1 == NULL || s_rdir0 == NULL || s_rdir1 == NULL
        || s_b0[0] == NULL || s_b0[1] == NULL
        || s_b0[2] == NULL || s_b1[0] == NULL || s_b1[1] == NULL
        || s_b1[2] == NULL || s_wirk0 == NULL || s_wirk1 == NULL
        || s_seite == NULL || s_seiten == NULL
        || s_root == NULL || s_scroll == NULL || s_virt == NULL
        || s_seite_anzeige == NULL || s_seite_ports == NULL
        || s_seite_maus == NULL || s_maushint == NULL
        || s_duo == NULL || s_duotext == NULL
        || s_beitrag0 == NULL || s_beitrag1 == NULL
        || s_mspeed0 == NULL || s_mspeed1 == NULL
        || s_mtotal0 == NULL || s_mtotal1 == NULL
        || s_mtot0m == NULL || s_mtot0p == NULL
        || s_mtot1m == NULL || s_mtot1p == NULL
        || s_cfghint == NULL) {
        /* Ein fehlendes Objekt ist kein Grund weiterzumachen: die Anzeige
         * waere unvollstaendig, und ein set() darauf wuerde MUI treffen. */
        gui_msg("MUI: Objektbaum unvollstaendig (reg=%d port0=%d "
                "port1=%d log=%d cfg=%d dev=%d/%d rdir=%d/%d btn=%d/%d "
                "wirkt=%d/%d beitrag=%d/%d maus=%d/%d/%d/%d hint=%d "
                "seiten=%d/%d/%d duo=%d) -- Abbruch\n",
                (s_regtext != NULL), (s_port0 != NULL), (s_port1 != NULL),
                (s_logl != NULL), (s_cfgtext != NULL),
                (s_dev0 != NULL), (s_dev1 != NULL),
                (s_rdir0 != NULL), (s_rdir1 != NULL),
                (s_b0[0] != NULL), (s_b1[0] != NULL),
                (s_wirk0 != NULL), (s_wirk1 != NULL),
                (s_beitrag0 != NULL), (s_beitrag1 != NULL),
                (s_mspeed0 != NULL), (s_mspeed1 != NULL),
                (s_mtotal0 != NULL), (s_mtotal1 != NULL),
                (s_cfghint != NULL),
                (s_root != NULL), (s_scroll != NULL), (s_virt != NULL),
                (s_duo != NULL));
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
             s_app, 2, MUIM_Application_ReturnID, ID_MODE0_NEXT);
    DoMethod(btn_mode1, MUIM_Notify, MUIA_Pressed, FALSE,
             s_app, 2, MUIM_Application_ReturnID, ID_MODE1_NEXT);
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

    /*
     * Die Eingangsverteilung (fw_ver 4): jedes Auswahlfeld und jeder Schalter
     * schickt seinen Befehl SOFORT (SET_DEVICE_PORT bzw. SET_PORT_ROUTING).
     * Ein ReturnID JE BUCHSE genuegt -- die drei Richtungsschalter bzw. die
     * drei Knopfquellen werden im Handler gemeinsam gelesen und in EINEM
     * SET_PORT_ROUTING geschickt (der ESP32 kennt nur das ganze Paket).
     */
    DoMethod(s_dev0, MUIM_Notify, MUIA_Cycle_Active, MUIV_EveryTime,
             s_app, 2, MUIM_Application_ReturnID, ID_DEV0);
    DoMethod(s_dev1, MUIM_Notify, MUIA_Cycle_Active, MUIV_EveryTime,
             s_app, 2, MUIM_Application_ReturnID, ID_DEV1);
    /* duo (fw_ver 6): das Auswahlfeld setzt GET/SET_DUO. */
    DoMethod(s_duo, MUIM_Notify, MUIA_Cycle_Active, MUIV_EveryTime,
             s_app, 2, MUIM_Application_ReturnID, ID_DUO);

    /* Die Beitrags-Auswahl je Buchse: sie setzt sofort die Routing-Werte. */
    DoMethod(s_beitrag0, MUIM_Notify, MUIA_Cycle_Active, MUIV_EveryTime,
             s_app, 2, MUIM_Application_ReturnID, ID_BEITRAG0);
    DoMethod(s_beitrag1, MUIM_Notify, MUIA_Cycle_Active, MUIV_EveryTime,
             s_app, 2, MUIM_Application_ReturnID, ID_BEITRAG1);
    /* Maus-Einstellungen (fw_ver 5): Tempo-Auswahl und Totzone -/+ . */
    DoMethod(s_mspeed0, MUIM_Notify, MUIA_Cycle_Active, MUIV_EveryTime,
             s_app, 2, MUIM_Application_ReturnID, ID_MSPEED0);
    DoMethod(s_mspeed1, MUIM_Notify, MUIA_Cycle_Active, MUIV_EveryTime,
             s_app, 2, MUIM_Application_ReturnID, ID_MSPEED1);
    DoMethod(s_mtot0m, MUIM_Notify, MUIA_Pressed, FALSE,
             s_app, 2, MUIM_Application_ReturnID, ID_MTOT0M);
    DoMethod(s_mtot0p, MUIM_Notify, MUIA_Pressed, FALSE,
             s_app, 2, MUIM_Application_ReturnID, ID_MTOT0P);
    DoMethod(s_mtot1m, MUIM_Notify, MUIA_Pressed, FALSE,
             s_app, 2, MUIM_Application_ReturnID, ID_MTOT1M);
    DoMethod(s_mtot1p, MUIM_Notify, MUIA_Pressed, FALSE,
             s_app, 2, MUIM_Application_ReturnID, ID_MTOT1P);
    /* Seitenumschalter Anzeige/Konfiguration. */
    DoMethod(s_seite, MUIM_Notify, MUIA_Cycle_Active, MUIV_EveryTime,
             s_app, 2, MUIM_Application_ReturnID, ID_SEITE);
    {
        unsigned k;

        for (k = 0u; k < 3u; k++) {
            DoMethod(s_b0[k], MUIM_Notify, MUIA_Cycle_Active, MUIV_EveryTime,
                     s_app, 2, MUIM_Application_ReturnID, ID_BTN0);
            DoMethod(s_b1[k], MUIM_Notify, MUIA_Cycle_Active, MUIV_EveryTime,
                     s_app, 2, MUIM_Application_ReturnID, ID_BTN1);
        }
    }
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
                  "den Registerwerten beider Buchsen); jede Aenderung sofort.\n",
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
            /*
             * Die Faehigkeitsbits sind die VERBINDLICHE Auskunft darueber, ob
             * die Eingangsverteilung vorhanden ist -- nicht die
             * Firmware-Version. Eine Firmware mit fw_ver 4 und unveraendertem
             * Rahmen (proto_ver 3) wird von v4_check_info() angenommen; eine
             * aeltere mit fw_ver 3 bleibt bedienbar, nur ohne die neuen
             * Felder.
             */
            s_cfg_ok     = v4_info_has_caps(&info, V4P_CAP_PORT_ROUTING);
            s_devport_ok = v4_info_has_caps(&info, V4P_CAP_DEVICE_PORT);
            /* Die Maus-Einstellungen sind eine EIGENE Faehigkeit (fw_ver 5).
             * Fehlt das Bit, bleiben die Regler gesperrt -- es wird nichts
             * gesendet, was der Slave mit BAD_CMD abweisen wuerde. Gelesen
             * werden sie im langsamen Takt (der erste Takt ist vollstaendig),
             * also VOR der ersten Anzeige des Fensters. */
            s_mousecfg_ok = v4_info_has_caps(&info, V4P_CAP_MOUSE_CFG);
            /* duo ist ebenfalls eine EIGENE Faehigkeit (fw_ver 6). */
            s_duo_ok      = v4_info_has_caps(&info, V4P_CAP_DUO);
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
            if (s_cfg_ok != 0 && s_devport_ok != 0) {
                gui_msg("[ping] Eingangsverteilung vorhanden "
                        "(Faehigkeitsbits 0x%04X/0x%04X): Geraet, "
                        "Betriebsart, Richtungen und drei Knopfquellen je "
                        "Buchse\n",
                        (unsigned)V4P_CAP_PORT_ROUTING,
                        (unsigned)V4P_CAP_DEVICE_PORT);
            } else {
                gui_msg("[ping] WARNUNG: Faehigkeitsbit(s) 0x%04X fehlen "
                        "(gemeldet 0x%04X) -- die Gegenstelle kann die "
                        "Eingangsverteilung nicht vollstaendig. Die Felder "
                        "dafuer bleiben untaetig; dafuer wird fw_ver %u "
                        "gebraucht.\n",
                        (unsigned)((s_cfg_ok ? 0u : V4P_CAP_PORT_ROUTING)
                                   | (s_devport_ok ? 0u
                                                   : V4P_CAP_DEVICE_PORT)),
                        (unsigned)info.caps,
                        (unsigned)V4P_FW_VER_ROUTING);
            }
            if (s_duo_ok != 0) {
                gui_msg("[ping] duo vorhanden (Faehigkeitsbit 0x%04X): EIN "
                        "Controller auf BEIDEN Buchsen, die eine als Maus\n",
                        (unsigned)V4P_CAP_DUO);
            } else {
                gui_msg("[ping] Hinweis: die Gegenstelle kann duo nicht "
                        "(Faehigkeitsbit 0x%04X fehlt, gemeldet 0x%04X) -- "
                        "der duo-Schalter bleibt gesperrt; dafuer wird fw_ver "
                        "%u gebraucht\n",
                        (unsigned)V4P_CAP_DUO, (unsigned)info.caps,
                        (unsigned)V4P_FW_VER_DUO);
            }
            if (s_mousecfg_ok != 0) {
                gui_msg("[ping] Maus-Einstellungen vorhanden (Faehigkeitsbit "
                        "0x%04X): Tempo und Totzone je Buchse\n",
                        (unsigned)V4P_CAP_MOUSE_CFG);
            } else {
                gui_msg("[ping] Hinweis: die Gegenstelle kann die "
                        "Maus-Einstellungen nicht (Faehigkeitsbit 0x%04X "
                        "fehlt, gemeldet 0x%04X) -- die Regler Tempo/Totzone "
                        "bleiben gesperrt; dafuer wird fw_ver %u gebraucht\n",
                        (unsigned)V4P_CAP_MOUSE_CFG,
                        (unsigned)info.caps, (unsigned)V4P_FW_VER_MOUSE);
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

    /*
     * DIE TATSAECHLICHEN MINDESTMASSE AUSGEBEN (nach s_gui_up, damit die
     * Zeilen auch im Logfeld des Fensters stehen).
     *
     * MUI rechnet die Mindestgroesse aus dem Inhalt; ohne Messung ist nur zu
     * raten, ob eine Seite in das Fenster passt. MUIM_AskMinMax ist genau die
     * Methode, die das Layout dafuer benutzt -- hier wird sie einmal von Hand
     * gerufen und das Ergebnis ins Protokoll geschrieben. Das steht auch nach
     * jeder Seitenumschaltung erneut da (act_seite), damit beide Seiten
     * einzeln belegt sind.
     *
     * Die Scrollgruppe um die Seiten nimmt ihnen die Macht, das Fenster zu
     * vergroessern (MUIA_Virtgroup_TryFit, FALSE); die Zahlen sagen, wie viel
     * gescrollt werden muss.
     */
    gui_mindestmasse_melden();

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
        case ID_MODE0_NEXT:
            act_mode_weiter(0);
            break;
        case ID_MODE1_NEXT:
            act_mode_weiter(1);
            break;
        case ID_DEV0:
            act_set_dev(0);
            break;
        case ID_DEV1:
            act_set_dev(1);
            break;
        case ID_BTN0:
            act_set_btn(0);
            break;
        case ID_BTN1:
            act_set_btn(1);
            break;
        case ID_DUO:
            act_set_duo();
            break;
        case ID_BEITRAG0:
            act_set_beitrag(0);
            break;
        case ID_BEITRAG1:
            act_set_beitrag(1);
            break;
        case ID_MSPEED0:
            act_set_maus(0, 0);
            break;
        case ID_MSPEED1:
            act_set_maus(1, 0);
            break;
        case ID_MTOT0M:
            act_set_maus(0, 1);
            break;
        case ID_MTOT0P:
            act_set_maus(0, 2);
            break;
        case ID_MTOT1M:
            act_set_maus(1, 1);
            break;
        case ID_MTOT1P:
            act_set_maus(1, 2);
            break;
        case ID_SEITE:
            act_seite();
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

    /* Schlusszeile: erst die Registerwerte in ihrer letzten Fassung, dann die
     * verworfenen Abtastungen der Direktseite -- das Gegenstueck zu
     * "verworfene Rahmen" auf der I2C-Seite. Dann die gemeinsame Schlusszeile. */
    gui_direkt_zeile();
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
