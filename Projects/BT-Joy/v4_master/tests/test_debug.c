/*
 * test_debug.c -- die gemeinsame Ausgabeschicht (v4_debug.c) auf dem WIRT.
 *
 * WARUM DIESE SUITE
 * -----------------
 * Die Ausgabe ist auf der V4 das einzige Fenster in das Programm: es gibt
 * keinen Debugger, und was nicht auf dem DebugUART steht, ist nicht zu sehen.
 * Gleichzeitig ist der UART langsam, und jedes Zeichen kostet Wartezeit. Die
 * Ausgabemenge ist deshalb eine Zusage des Programms, und eine Zusage muss
 * pruefbar sein. Hier steht:

 *   1. EIN Lebenszeichen je Sekunde traegt ALLES: Zaehler, letzten Status,
 *      verworfene Rahmen, Wiederholungen -- und ueber v4_dbg_herz_zusatz den
 *      Vergleich direkt <-> I2C mit den Rohwerten von CIAAPRA/POTINP. Die
 *      Zeile muss VOLLSTAENDIG in den Puffer der Ausgabe passen; wird sie
 *      abgeschnitten, fehlt am Ende genau die Aussage, auf die es ankommt.
 *   2. EINE fertige Zeile je Meldung: der Zeilen-Haken (das MUI-Logfeld)
 *      bekommt genau einen Aufruf je Meldung, ohne Umbruch, und die Zeile
 *      kommt genau einmal -- nicht doppelt.
 *   3. SOFORT BEI AENDERUNG: Aenderungen werden gemeldet, Wiederholungen der
 *      gleichen Werte NICHT. (Vorher stand in v4_dbg_input ein Fehler, durch
 *      den die Tastendruck-Zeile [taste] nie erscheinen konnte.)
 *   4. TRANSAKTIONSZEILEN nur, wenn sie bestellt sind: die Oberflaeche laesst
 *      sie ohne -trace aus. Bei 200 ms Takt waeren das sonst zehn Zeilen je
 *      Sekunde zusaetzlich.
 *
 * Was diese Suite NICHT kann: pruefen, dass die Zeichen wirklich am COM-Port
 * ankommen. Das ist Hardware (siehe tools/SERIELL.md).
 */

#include "v4_test.h"

#include "../v4_debug.h"
#include "../v4_joyport.h"

/* ------------------------------------------------------------------ */
/* Zeilen-Haken: sammelt, was die Ausgabe losschickt                    */
/* ------------------------------------------------------------------ */

#define FANG_MAX 24
#define FANG_LEN 512

static char  s_fang[FANG_MAX][FANG_LEN];
static int   s_fang_n;

static void fang_hook(const char *zeile)
{
    if (s_fang_n >= FANG_MAX) {
        return;
    }
    (void)snprintf(s_fang[s_fang_n], sizeof(s_fang[0]), "%s", zeile);
    s_fang_n++;
}

static void fang_leeren(void)
{
    s_fang_n = 0;
    s_fang[0][0] = '\0';
}

static int fang_hat(const char *teil)
{
    int i;

    for (i = 0; i < s_fang_n; i++) {
        if (strstr(s_fang[i], teil) != NULL) {
            return 1;
        }
    }
    return 0;
}

static size_t fang_laengste(void)
{
    size_t max = 0u;
    int    i;

    for (i = 0; i < s_fang_n; i++) {
        size_t l = strlen(s_fang[i]);
        if (l > max) {
            max = l;
        }
    }
    return max;
}

/* Ausgabe ohne Senken aufsetzen: nur der Zeilen-Haken bekommt etwas. Damit
 * schreibt die Suite weder auf stdout noch in eine Logdatei. */
static void ausgabe_still(int quiet)
{
    v4_dbg_cfg_t cfg;

    memset(&cfg, 0, sizeof(cfg));
    cfg.programm  = "test_debug";
    cfg.version   = "0";
    cfg.datum     = "0";
    cfg.logdatei  = NULL;
    cfg.addr7     = (unsigned)V4_I2C_ADDR7;
    cfg.uart      = 0;
    cfg.stdout_an = 0;
    cfg.quiet     = quiet;
    v4_dbg_init(&cfg);
    v4_dbg_set_detail(0);
    v4_dbg_set_hex(0);
    v4_dbg_set_line_hook(fang_hook);
    fang_leeren();
}

/* ------------------------------------------------------------------ */
/* 1. Die Herzzeile: eine Zeile, alles drin, nichts abgeschnitten       */
/* ------------------------------------------------------------------ */

static v4_joy_t eingabe_ruhe(void)
{
    v4_joy_t     st;
    v4_joy_roh_t r;

    v4_joy_init(&st);
    r.joy0dat = 0x0000u;
    r.joy1dat = 0x0000u;
    r.ciaapra = 0x00F4u;
    r.potinp  = 0xFF02u;
    v4_joy_basis(&st, &r);
    v4_joy_update(&st, &r, &r);
    return st;
}

static v4p_input_t i2c_ruhe(void)
{
    v4p_input_t in;

    memset(&in, 0, sizeof(in));
    in.state[0]         = 0x00u;
    in.state[1]         = 0x00u;
    in.level[0]         = 0x7Fu;
    in.level[1]         = 0x7Fu;
    in.level_valid      = 1u;
    return in;
}

static void test_herz_zeile(void)
{
    v4_joy_t     st   = eingabe_ruhe();
    v4p_input_t  in   = i2c_ruhe();
    v4_master_t  m;
    char         zusatz[V4_JOY_TEXT_MAX];

    v4_test_case("Herzzeile: eine Zeile je Sekunde, Vergleich und Rohwerte "
                 "mit drin");
    ausgabe_still(0);

    memset(&m, 0, sizeof(m));
    m.tx_frames = 42u;
    m.rx_frames = 40u;
    m.retries   = 2u;

    (void)v4_joy_herz_text(&st, &in, 1, zusatz, sizeof(zusatz));
    CHECK(zusatz[0] != '\0');
    CHECK(strstr(zusatz, "CIAAPRA=0x00F4") != NULL);
    CHECK(strstr(zusatz, "POTINP=0xFF02") != NULL);
    CHECK(strstr(zusatz, "Buchse0") != NULL);
    CHECK(strstr(zusatz, "Buchse1") != NULL);
    CHECK(strstr(zusatz, "OK") != NULL);

    v4_dbg_herz_zusatz(zusatz);
    v4_dbg_herz(&m);

    /* GENAU eine Zeile je Herzschlag. */
    CHECK_EQ(s_fang_n, 1);

    /* Und sie traegt, was der Auftrag verlangt. */
    CHECK(fang_hat("[herz]"));
    CHECK(fang_hat("aufrufe="));
    CHECK(fang_hat("wiederholungen=2"));
    CHECK(fang_hat("verworfene_rahmen="));
    CHECK(fang_hat("letzter_status="));
    CHECK(fang_hat("Buchse0"));
    CHECK(fang_hat("Buchse1"));
    CHECK(fang_hat("CIAAPRA=0x00F4"));
    CHECK(fang_hat("POTINP=0xFF02"));

    /* Der entscheidende Punkt: nichts abgeschnitten. Die Zeile muss in den
     * Puffer der Ausgabe passen (V4_DBG_LINE), sonst fehlt am Ende der
     * Rohwert -- also genau die Aussage, mit der man die Feuerknopfbelegung
     * durch Draufschauen findet. */
    CHECK(fang_laengste() < (size_t)V4_DBG_ZEILE_MAX);
    CHECK(strstr(s_fang[0], "POTINP=0xFF02") != NULL);
    CHECK(s_fang[0][strlen(s_fang[0]) - 1] != '\0');

    /* Der Zusatz gilt fuer EINE Zeile, nicht fuer alle. */
    fang_leeren();
    v4_dbg_herz_zusatz(NULL);
    v4_dbg_herz(&m);
    CHECK_EQ(s_fang_n, 1);
    CHECK(!fang_hat("Buchse0"));

    /* Eine sehr lange Zusatzangabe darf die Ausgabe nicht sprengen. */
    fang_leeren();
    {
        char lang[3u * V4_JOY_TEXT_MAX];
        size_t i;
        for (i = 0u; i + 1u < sizeof(lang); i++) {
            lang[i] = 'X';
        }
        lang[sizeof(lang) - 1u] = '\0';
        v4_dbg_herz_zusatz(lang);
        v4_dbg_herz(&m);
        CHECK_EQ(s_fang_n, 1);
        CHECK(strlen(s_fang[0]) < (size_t)V4_DBG_ZEILE_MAX);
    }
    v4_dbg_herz_zusatz(NULL);
    v4_dbg_set_line_hook(NULL);
}

/* ------------------------------------------------------------------ */
/* 2. Eine fertige Zeile je Meldung                                     */
/* ------------------------------------------------------------------ */

static void test_eine_zeile_je_meldung(void)
{
    v4p_status_t st;
    int          vorher;

    v4_test_case("Zeilen-Haken: genau ein Aufruf je Meldung, ohne Umbruch");
    ausgabe_still(0);

    memset(&st, 0, sizeof(st));
    st.state      = V4P_STATE_IDLE;
    st.conn_index = V4P_NO_INDEX;
    st.dev_count  = 1u;

    vorher = s_fang_n;
    v4_dbg_status(&st);
    CHECK_EQ(s_fang_n - vorher, 1);
    CHECK(s_fang[vorher][0] == '[');            /* Praefix [status] */
    CHECK(strchr(s_fang[vorher], '\n') == NULL); /* Haken ohne Umbruch */

    /* Dieselben Werte noch einmal: KEINE zweite Zeile (nur bei Aenderung). */
    vorher = s_fang_n;
    v4_dbg_status(&st);
    CHECK_EQ(s_fang_n - vorher, 0);

    /* Eine Aenderung dagegen sofort. */
    st.state = V4P_STATE_CONNECTED;
    vorher   = s_fang_n;
    v4_dbg_status(&st);
    CHECK_EQ(s_fang_n - vorher, 1);
    CHECK(fang_hat("CONNECTED"));

    v4_dbg_set_line_hook(NULL);
}

/* ------------------------------------------------------------------ */
/* 3. Sofort bei Aenderung: der Tastendruck                             */
/* ------------------------------------------------------------------ */

static void test_tastendruck_wird_gemeldet(void)
{
    v4p_input_t a;
    v4p_input_t b;

    v4_test_case("Tastendruck: Aenderung wird sofort gemeldet (frueher nie)");
    ausgabe_still(0);

    memset(&a, 0, sizeof(a));
    a.state[0]    = 0x00u;
    a.state[1]    = 0x00u;
    a.level[0]    = 0x7Fu;
    a.level[1]    = 0x7Fu;
    a.level_valid = 1u;
    b = a;

    /* Erste Antwort: Zustandszeile, aber noch keine Taste. */
    v4_dbg_input(&a);
    CHECK(fang_hat("[eingang]"));
    CHECK(!fang_hat("[taste]"));

    /* Unveraendert: gar nichts. */
    {
        int vorher = s_fang_n;
        v4_dbg_input(&a);
        CHECK_EQ(s_fang_n - vorher, 0);
    }

    /* Jetzt UP gedrueckt: die Taste MUSS gemeldet werden -- genau eine Zeile
     * mit dem, was dazukam. Vor der Korrektur in v4_debug.c kam hier nichts,
     * weil "vorher" mit "jetzt" ueberschrieben wurde. */
    b.state[0] = (uint8_t)V4P_IN_ST_UP;
    v4_dbg_input(&b);
    CHECK(fang_hat("[taste] Port 0"));
    CHECK(fang_hat("gedrueckt=[UP]"));
    CHECK(fang_hat("gezogen=[UP]"));

    /* Wieder losgelassen. */
    fang_leeren();
    v4_dbg_input(&a);
    CHECK(fang_hat("[taste] Port 0"));
    CHECK(fang_hat("geloest=[UP]"));

    v4_dbg_set_line_hook(NULL);
}

/* ------------------------------------------------------------------ */
/* 4. Transaktionszeilen nur, wenn sie bestellt sind                    */
/* ------------------------------------------------------------------ */

static void test_transaktionszeilen(void)
{
    v4_trace_t ev;

    v4_test_case("Transaktionszeilen: mit -quiet aus, ohne -quiet an");
    memset(&ev, 0, sizeof(ev));
    ev.event = V4_TR_DONE;
    ev.cmd   = V4P_CMD_GET_INPUT;
    ev.seq   = 7u;
    ev.len   = 12u;
    ev.rc    = (int)V4P_ST_OK;
    ev.us    = 15500u;

    /* Vorgabe der Oberflaeche ohne -trace: quiet = 1 -> keine Zeile. */
    ausgabe_still(1);
    v4_dbg_trace(&ev, NULL);
    CHECK_EQ(s_fang_n, 0);

    /* Mit -trace: genau eine Zeile je Transaktion. */
    ausgabe_still(0);
    v4_dbg_trace(&ev, NULL);
    CHECK_EQ(s_fang_n, 1);
    CHECK(fang_hat("[i2c]"));
    CHECK(fang_hat("GET_INPUT"));
    CHECK(fang_hat("status=OK"));

    /* Ein VERWORFENER Rahmen dagegen geht auch ohne Transaktionszeilen
     * heraus: er ist das Ereignis, auf das man wartet (und der Zaehler
     * "verworfene Rahmen" der Herzzeile). */
    ausgabe_still(1);
    ev.event = V4_TR_CHECK;
    ev.rc    = (int)V4P_CHECK_CRC;
    v4_dbg_trace(&ev, NULL);
    CHECK_EQ(s_fang_n, 1);
    CHECK(fang_hat("[i2c] Rahmen verworfen"));
    CHECK(fang_hat("grund="));

    /* Ein FEHLER wird vom Frontend gemeldet (v4_dbg_fehler) -- und zwar
     * sofort und OHNE dass man -trace braucht. */
    fang_leeren();
    {
        v4_master_t m;
        memset(&m, 0, sizeof(m));
        m.tx_frames = 3u;
        m.rx_frames = 2u;
        v4_dbg_set_line_hook(fang_hook);
        v4_dbg_fehler("GET_INPUT", (uint8_t)V4_ERR_LINK, &m);
        CHECK(s_fang_n >= 1);
        CHECK(fang_hat("[fehler] GET_INPUT"));
        CHECK(fang_hat("tx_frames=3"));
    }

    v4_dbg_set_line_hook(NULL);
}

/* ------------------------------------------------------------------ */
/* 5. Das Urteil direkt <-> I2C als eigene, pruefbare Funktion          */
/* ------------------------------------------------------------------ */

static void test_urteil_direkt_i2c(void)
{
    v4_joy_t    st = eingabe_ruhe();
    v4p_input_t in = i2c_ruhe();

    v4_test_case("Urteil direkt <-> I2C: OK, Abweichung, kein Urteil");

    CHECK_EQ(v4_joy_abweichung(&st, &in, 1, 0), 0u);
    CHECK_EQ(v4_joy_abweichung(&st, &in, 1, 1), 0u);

    /* I2C meldet VOR, direkt liest nichts -> Abweichung genau auf diesem Bit. */
    in.state[0] = (uint8_t)V4P_IN_ST_UP;
    CHECK_EQ(v4_joy_abweichung(&st, &in, 1, 0), (unsigned)V4P_IN_ST_UP);
    CHECK_EQ(v4_joy_abweichung(&st, &in, 1, 1), 0u);

    /*
     * BTN3 (Pin 5) wird SEIT dem 16.10.2026 MIT verglichen: die direkte Quelle
     * ist POTINP Bit 9 (Buchse 0) / Bit 11 (Buchse 1). Meldet I2C BTN3 und die
     * Register zeigen nichts, ist das eine echte Abweichung -- der Fall, der
     * beim Benutzer unsichtbar blieb ("Pin 5 <- Start" kam nicht an).
     */
    in.state[0] = (uint8_t)V4P_IN_ST_BTN3;
    CHECK_EQ(v4_joy_abweichung(&st, &in, 1, 0), (unsigned)V4P_IN_ST_BTN3);
    CHECK_EQ((unsigned)V4_JOY_VERGLEICHSMASKE & (unsigned)V4P_IN_ST_BTN3,
             (unsigned)V4P_IN_ST_BTN3);
    /* Und die Maske deckt ALLE sieben Leitungen ab. */
    CHECK_EQ((unsigned)V4_JOY_VERGLEICHSMASKE, (unsigned)V4P_IN_ST_MASK);

    /* Eine ALTE Antwort ergibt kein Urteil -- gegen sie zu vergleichen waere
     * keine Messung. */
    in.state[0] = (uint8_t)V4P_IN_ST_UP;
    CHECK_EQ(v4_joy_abweichung(&st, &in, 0, 0), (unsigned)V4_JOY_KEIN_URTEIL);

    /* Ohne I2C-Antwort ebenso. */
    CHECK_EQ(v4_joy_abweichung(&st, NULL, 1, 0), (unsigned)V4_JOY_KEIN_URTEIL);

    /* Ohne stabile direkte Abtastung ebenso. */
    {
        v4_joy_t leer;
        v4_joy_init(&leer);
        CHECK_EQ(v4_joy_abweichung(&leer, &in, 1, 0),
                 (unsigned)V4_JOY_KEIN_URTEIL);
    }

    /* Randfaelle. */
    CHECK_EQ(v4_joy_abweichung(NULL, &in, 1, 0), (unsigned)V4_JOY_KEIN_URTEIL);
    CHECK_EQ(v4_joy_abweichung(&st, &in, 1, 5), (unsigned)V4_JOY_KEIN_URTEIL);
}

/* ------------------------------------------------------------------ */
/* 6. Rahmenpruefung: Zaehler statt "letzter Zustand"                   */
/* ------------------------------------------------------------------ */

/*
 * WOZU: im DebugUART stand `letzte_Rahmenpruefung=CRC-Fehler` in jeder
 * Fehlerzeile -- ein haftenbleibender letzter Wert. Ein einzelner Aussetzer
 * (etwa beim Beenden des Programms) sah danach wie ein Dauerfehler aus. Hier
 * ist festgenagelt, dass die Ausgabe stattdessen ZAEHLT: wie viele Rahmen
 * geprueft wurden, wie viele fehlerhaft waren und wie viele in Folge.
 */
static void test_rahmen_zaehler(void)
{
    v4_trace_t    ev;
    v4_master_t   m;
    unsigned long geprueft   = 0ul;
    unsigned long fehlerhaft = 0ul;
    unsigned long serie      = 0ul;

    v4_test_case("Rahmenpruefung: geprueft/fehlerhaft/fehlerserie statt "
                 "letztem Zustand");
    ausgabe_still(1);           /* ohne -trace: genau eine Zeile je Fehler */

    memset(&ev, 0, sizeof(ev));
    ev.event = V4_TR_CHECK;
    ev.cmd   = V4P_CMD_GET_STATUS;
    ev.rc    = (int)V4P_CHECK_OK;

    /* Ein gueltiger Rahmen: gezaehlt, kein Fehler, Serie 0. */
    v4_dbg_trace(&ev, NULL);
    v4_dbg_rahmen_zaehler(&geprueft, &fehlerhaft, &serie);
    CHECK_EQ(geprueft, 1ul);
    CHECK_EQ(fehlerhaft, 0ul);
    CHECK_EQ(serie, 0ul);

    /* Ein einzelner Aussetzer: Fehler 1, Serie 1 -- und genau EINE Zeile. */
    fang_leeren();
    ev.rc = (int)V4P_CHECK_CRC;
    v4_dbg_trace(&ev, NULL);
    CHECK_EQ(s_fang_n, 1);
    CHECK(fang_hat("Rahmen verworfen"));
    CHECK(fang_hat("CRC-Fehler"));
    v4_dbg_rahmen_zaehler(&geprueft, &fehlerhaft, &serie);
    CHECK_EQ(geprueft, 2ul);
    CHECK_EQ(fehlerhaft, 1ul);
    CHECK_EQ(serie, 1ul);

    /* Noch einer in Folge: die Serie waechst. */
    v4_dbg_trace(&ev, NULL);
    v4_dbg_rahmen_zaehler(&geprueft, &fehlerhaft, &serie);
    CHECK_EQ(geprueft, 3ul);
    CHECK_EQ(fehlerhaft, 2ul);
    CHECK_EQ(serie, 2ul);

    /* Ein gueltiger Rahmen setzt NUR die Serie zurueck; die Summe bleibt. */
    ev.rc = (int)V4P_CHECK_OK;
    v4_dbg_trace(&ev, NULL);
    v4_dbg_rahmen_zaehler(&geprueft, &fehlerhaft, &serie);
    CHECK_EQ(geprueft, 4ul);
    CHECK_EQ(fehlerhaft, 2ul);
    CHECK_EQ(serie, 0ul);

    /* EINE Schreibweise fuer alle Zeilen. */
    {
        char text[80];

        v4_dbg_rahmen_text(text, sizeof(text));
        CHECK_STR(text, "geprueft=4 fehlerhaft=2 fehlerserie=0");
    }

    /* Die Herzzeile traegt die Zaehler -- und der bestehende Zaehler
     * "verworfene Rahmen" bleibt daneben stehen. */
    fang_leeren();
    memset(&m, 0, sizeof(m));
    v4_dbg_herz(&m);
    CHECK(fang_hat("geprueft=4"));
    CHECK(fang_hat("fehlerhaft=2"));
    CHECK(fang_hat("fehlerserie=0"));
    CHECK(fang_hat("verworfene_rahmen=2"));

    /* Die Fehlerzeile ebenfalls -- und OHNE den haftenbleibenden letzten
     * Zustand. */
    fang_leeren();
    v4_dbg_fehler("GET_STATUS", (uint8_t)V4_ERR_LINK, &m);
    CHECK(fang_hat("geprueft=4"));
    CHECK(fang_hat("fehlerhaft=2"));
    CHECK(fang_hat("fehlerserie=0"));
    CHECK(!fang_hat("letzte_Rahmenpruefung"));

    /* Und die Schlusszeile, damit nach dem Lauf dasselbe dasteht. */
    fang_leeren();
    v4_dbg_ende(&m);
    CHECK(fang_hat("geprueft=4"));
    CHECK(fang_hat("fehlerhaft=2"));
    CHECK(fang_hat("letzter_status="));

    /* Randfaelle: NULL-Puffer duerfen nichts tun. */
    v4_dbg_rahmen_text(NULL, 0u);
    v4_dbg_rahmen_zaehler(NULL, NULL, NULL);

    v4_dbg_set_line_hook(NULL);
}

/* ------------------------------------------------------------------ */

int test_debug(void)
{
    v4_test_begin("Ausgabeschicht: Lebenszeichen, eine Zeile je Meldung, "
                  "Sofortmeldung bei Aenderung");

    test_herz_zeile();
    test_eine_zeile_je_meldung();
    test_tastendruck_wird_gemeldet();
    test_transaktionszeilen();
    test_urteil_direkt_i2c();
    test_rahmen_zaehler();

    return v4_test_failures;
}
