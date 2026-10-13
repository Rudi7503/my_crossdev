/*
 * v4_joyport.h -- die Joystick-Anschluesse der Vampire V4 DIREKT aus der
 * Hardware lesen und in DIESELBE Bitmaske umrechnen wie die I2C-Antwort.
 *
 * WOZU
 * ----
 * Die V4 hat zwei DB9-Buchsen, und der ESP32 haengt genau daran. Was auf den
 * Leitungen liegt, laesst sich auf zwei Wegen erfahren:
 *
 *   I2C    -- GET_INPUT nennt die Zug-Maske (was der ESP32 nach GND zieht),
 *             die gemessenen Pegel und die Quadraturzaehler. Das ist die Sicht
 *             der GEGENSEITE, und sie kostet eine Busrunde (rund 114 ms).
 *   DIREKT -- die V4 liest ihre eigenen Zaehlerregister (JOY0DAT/JOY1DAT) und
 *             die beiden Feuerregister (CIAAPRA/POTINP). Das ist die Sicht der
 *             V4 selbst und kostet nichts.
 *
 * Beides NEBENEINANDER zu zeigen ist der Zweck dieser Datei: stimmen die
 * Quellen ueberein, ist die Kette ESP32 -> DB9 -> V4 in Ordnung. Weichen sie
 * ab, sieht man sofort, dass etwas nicht stimmt -- und der Rohwert sagt, wo.
 *
 * WO DAS GILT -- UND WO NICHT (Stand 14.10.2026): der Vergleich ist eine
 * DIAGNOSE und steht im PROTOKOLL (DebugUART/Logdatei, v4_joy_debug_text und
 * v4_joy_herz_text). Das MUI-FENSTER zeigt ausschliesslich die direkten
 * Registerwerte (v4_joy_direkt_text) -- dort soll stehen, was ueber den
 * Joystick-Port ankommt, nicht die Sicht des ESP32.
 *
 * Damit die beiden Zahlen unmittelbar vergleichbar sind, rechnet diese Datei
 * die Register in GENAU die Bitbelegung des Protokolls um (V4P_IN_ST_* aus
 * v4_proto.h):
 *
 *   Bit 0x01  UP      (direkt: VOR)          Pin 1
 *   Bit 0x02  DOWN    (direkt: ZURUECK)      Pin 2
 *   Bit 0x04  LEFT    (direkt: LINKS)        Pin 3
 *   Bit 0x08  RIGHT   (direkt: RECHTS)       Pin 4
 *   Bit 0x10  BTN1                           Pin 6
 *   Bit 0x20  BTN2                           Pin 9
 *   Bit 0x40  BTN3                           Pin 5 -- direkt NICHT lesbar
 *
 * BTN3 wird beim Vergleich ausgelassen (V4_JOY_VERGLEICHSMASKE): fuer Pin 5
 * nennt der Auftrag keine direkte Quelle, und ein daraus abgeleitetes
 * "ABWEICHUNG" waere eine Aussage ueber eine Messung, die es nicht gibt. Im
 * I2C-Wert ist das Bit trotzdem zu sehen.
 *
 * WO DIE REGISTERSTELLEN HERKOMMEN -- nachgemessen am 13.10.2026
 * --------------------------------------------------------------
 * Registerrohwerte vom Druecken (Buchse 1, Pad am ESP32) und die zugehoerigen
 * stabilen Werte aus dem Mitschnitt (Projects/BT-Joy/logs/v4_seriell.log):
 *
 *   JOY1DAT=$0002 / $0003   RECHTS    X-Zaehler Bit 1 (0x0002)
 *   JOY1DAT=$0001           ZURUECK   X-Zaehler Bit 0 (0x0001)
 *   JOY1DAT=$0100           VOR       Y-Zaehler Bit 0 (0x0100)
 *   JOY1DAT=$0200 / $0300   LINKS     Y-Zaehler Bit 1 (0x0200)
 *
 * Daraus folgt das Muster dieser Hardware: JE ACHSE traegt das HOHE Bit die
 * eine Richtung (RECHTS auf X, LINKS auf Y), und das NIEDRIGE Bit die
 * Gegenrichtung -- aber nur, solange das hohe Bit frei ist. Der Zaehler steht
 * naemlich nach einer Bewegung auf "11" (daher $0003 beim Druecken von RECHTS),
 * und "11" ist von "hohes Bit allein" nicht zu unterscheiden.
 *
 * WARUM VOR/ZURUECK NICHT WOERTLICH DIE FORMELN DES AUFTRAGS SIND
 * ---------------------------------------------------------------
 * Der Auftrag nennt:
 *
 *   VOR     = ((j >> 1) ^ j) & 0x0100    = Y-Zaehler Bit 1 ^ Bit 0
 *   ZURUECK = ((j << 1) ^ j) & 0x0100    (Bit 0 des Y-Zaehlers ^ Bit 7 des
 *                                         X-Zaehlers -- bei ruhigem X-Zaehler
 *                                         also DASSELBE Bit wie VOR)
 *
 * Beide Formen passen auf die nachgemessenen Werte $0100 (VOR) und $0300
 * (LINKS) -- aber:
 *
 *   - ZURUECK ist damit eine OBERMENGE von VOR: liegt Bit 8 an, sind BEIDE
 *     gesetzt. Genau das war im Probing zu sehen ("VOR ZURUECK gleichzeitig").
 *     Schlimmer: der stabile Wert $0001 (ZURUECK, Pin 2) faellt bei BEIDEN
 *     Formeln durch -- "zurueck" waere nie zu sehen.
 *   - VOR feuert mit der XOR-Form auch beim Zwischenwert $0200 (Y = 10), der
 *     im Mitschnitt vorkommt.
 *
 * Der Auftrag verlangt ausdruecklich, das stabil zu bauen. Deshalb steht hier
 * die statische Form, die auf ALLEN nachgemessenen Werten dasselbe sagt:
 *
 *   RECHTS   j & 0x0002                     (Bit 1 des X-Zaehlers)
 *   LINKS    j & 0x0200                     (Bit 1 des Y-Zaehlers)
 *   VOR      (j & 0x0100) && !(j & 0x0200)  (Bit 0 des Y-Zaehlers, LINKS frei)
 *   ZURUECK  (j & 0x0001) && !(j & 0x0002)  (Bit 0 des X-Zaehlers, RECHTS frei)
 *
 * Fuer $0002, $0003, $0001, $0100, $0300 liefert das genau die gemessenen
 * Richtungen (und fuer $0003 NICHT zusaetzlich "zurueck"). Der Grundzustand
 * $0000 ergibt keine Richtung. Beides ist in tests/test_joyport.c festgenagelt;
 * die Entscheidung ist im Bericht benannt, und der Rohwert steht in der Anzeige
 * daneben, damit sie am Geraet nachpruefbar bleibt.
 *
 * GRENZE DIESER DEKODIERUNG (kein Fehler, sondern die Hardware)
 * ------------------------------------------------------------
 * Innerhalb EINER Achse sind zwei Richtungen nicht gleichzeitig sichtbar:
 * "unten+rechts" landet auf demselben Zaehlerwert wie "rechts" allein
 * (X = 11), ebenso "oben+links" wie "links" allein (Y = 11). Solche
 * Diagonalen zeigen dann eine Abweichung zum I2C-Wert -- zu Recht: die direkte
 * Quelle KANN sie nicht sehen. Diagonalen ueber beide Achsen (z.B. oben+rechts
 * = $0103 -> 0x09) sind dagegen vollstaendig sichtbar.
 *
 * Auch die Beispielzeile des Auftrags ("direkt: 0x0A VOR+RECHTS") ist mit der
 * Bitbelegung des Protokolls nicht darstellbar: 0x0A ist dort RIGHT|DOWN,
 * VOR+RECHTS waere 0x09. Die Zeile ist im Bericht besprochen.
 *
 * ZWEI ABTASTUNGEN JE BUCHSE
 * --------------------------
 * Die Zaehler der V4 liefern beim Umschalten Zwischenwerte ($0003 und $0200 im
 * Mitschnitt). Deshalb wird jede Buchse aus ZWEI Abtastungen mit kurzem
 * Abstand bewertet:
 *
 *   beide Abtastungen gleich -> Wert uebernehmen
 *   ungleich                 -> letzten GUELTIGEN Wert stehen lassen
 *
 * So flackert die Anzeige nicht. Den Abstand stellt der AUFRUFER her
 * (v4_gui.c: eine kurze Rechenschleife -- kein Delay(), kein timer.device);
 * diese Datei kennt keine Zeit und keine Wartefunktion.
 *
 * FEUERKNOPEF -- GRUNDZUSTAND STATT HART VERDRAHTET
 * -------------------------------------------------
 * CIAAPRA ($BFE001) und POTINP ($DFF016) sind am Geraet GEMESSEN worden, ohne
 * dass sich beim Druecken etwas bewegte ($00F4 bzw. $FF02). Welches Bit welche
 * Taste ist, ist also offen. Deshalb gilt hier:
 *
 *   - beide Register stehen IMMER als Rohwert in der Anzeige (vier
 *     Hexziffern) und in der Debugzeile -- man liest die Belegung notfalls von
 *     Hand ab,
 *   - der Zustand beim Start wird GEMERKT (v4_joy_basis) und NICHT
 *     verdrahtet: eine Taste gilt genau dann als gedrueckt, wenn sich ihr Bit
 *     vom Grundzustand UNTERSCHEIDET. Das gilt in beide Richtungen (aktiv low
 *     oder aktiv high) -- deshalb funktioniert es auch, wenn die Polaritaet
 *     einmal anders liegt.
 *
 * Bekanntes Verhalten dieser Wahl: wird ein Knopf schon beim Start gehalten,
 * ist er der Grundzustand und erscheint beim Loslassen als "gedrueckt". Das ist
 * der Preis dafuer, die Polaritaet nicht raten zu muessen; im Bericht steht es
 * unter "Offene Punkte".
 *
 * WAS HIER NICHT PASSIERT -- UND WARUM DAS WICHTIG IST
 * ---------------------------------------------------
 * Kein Betriebssystemaufruf, kein Busaufruf, keine Sperre, kein Faden, kein
 * Forbid(). Der einzige Hardwarezugriff sind vier Register hinter EINEM Haken
 * (v4_joy_set_lesen); ohne Haken liefert v4_joy_lesen Nullen. Genau deshalb
 * laeuft diese Datei unveraendert im Wirtstest (tests/test_joyport.c), im
 * Sanitizerlauf und auf der V4 mit -- dieselbe Konstruktion wie v4_state.c.
 * Ein versehentlich eingebauter OS-Aufruf wuerde den Wirtstest schon beim
 * LINKEN scheitern lassen, nicht erst zur Laufzeit.
 */

#ifndef V4_JOYPORT_H
#define V4_JOYPORT_H

#include <stddef.h>

#include "v4_master.h"      /* V4P_IN_ST_*, V4P_PORT_COUNT, v4p_input_t */

/* ------------------------------------------------------------------ */
/* Registeradressen und Rohwert einer Abtastung                        */
/* ------------------------------------------------------------------ */

/* Klassische Amiga-Adressen. Der Apollo-Kern der V4 bildet sie auf die beiden
 * DB9-Buchsen ab (siehe joyprobe.c und Projects/BT-Joy/logs/v4_seriell.log). */
#define V4_JOY_ADR_JOY0DAT  0x000DFF00Au    /* Zaehler Buchse 0 */
#define V4_JOY_ADR_JOY1DAT  0x000DFF00Cu    /* Zaehler Buchse 1 */
#define V4_JOY_ADR_CIAAPRA  0x000BFE001u    /* Feuerleitung 1   */
#define V4_JOY_ADR_POTINP   0x000DFF016u    /* Feuerleitungen 2 */

/* Ein kompletter Registersatz: beide Zaehler und beide Feuerregister.
 * Die Rohwerte gehen unveraendert in die Anzeige -- sie sind die Aussage,
 * wenn die Dekodierung einmal nicht stimmt. */
typedef struct {
    uint16_t joy0dat;       /* $DFF00A: Zaehler Buchse 0 */
    uint16_t joy1dat;       /* $DFF00C: Zaehler Buchse 1 */
    uint8_t  ciaapra;       /* $BFE001: Feuerleitung 1 (Bit 6/7, aktiv low) */
    uint16_t potinp;        /* $DFF016: Feuerleitung 2 (Bit 8 / Bit 10)     */
} v4_joy_roh_t;

/* ------------------------------------------------------------------ */
/* Richtungsmasken im Rohwert -- nachgemessen (siehe Kopf)             */
/* ------------------------------------------------------------------ */

#define V4_JOY_M_ZURUECK    0x0001u     /* X-Zaehler Bit 0, Pin 2 (unten)   */
#define V4_JOY_M_RECHTS     0x0002u     /* X-Zaehler Bit 1, Pin 4           */
#define V4_JOY_M_VOR        0x0100u     /* Y-Zaehler Bit 0, Pin 1 (oben)    */
#define V4_JOY_M_LINKS      0x0200u     /* Y-Zaehler Bit 1, Pin 3           */

/* Feuerleitungen im Rohwert. Bit 7 fuer Buchse 1 ist die klassische
 * Amiga-Belegung (/FIR1); am Geraet ist sie NICHT gemessen -- deshalb steht der
 * Rohwert daneben. */
#define V4_JOY_CIA_BTN1_P0  0x40u       /* CIAAPRA Bit 6: Knopf 1 Buchse 0  */
#define V4_JOY_CIA_BTN1_P1  0x80u       /* CIAAPRA Bit 7: Knopf 1 Buchse 1  */
#define V4_JOY_POT_BTN2_P0  0x0100u     /* POTINP  Bit 8: Knopf 2 Buchse 0  */
#define V4_JOY_POT_BTN2_P1  0x0400u     /* POTINP  Bit 10: Knopf 2 Buchse 1 */

/*
 * Beim Vergleich ausgelassen: BTN3 (Pin 5). Fuer diese Leitung nennt der
 * Auftrag keine direkte Quelle -- ein "ABWEICHUNG" daraus waere keine Messung,
 * sondern eine Behauptung. BTN1/BTN2/Richtungen werden verglichen.
 */
#define V4_JOY_VERGLEICHSMASKE \
    ((uint8_t)(V4P_IN_ST_MASK & (uint8_t)~V4P_IN_ST_BTN3))

/* Groesste Zeile, die diese Datei baut. Muss unter V4_DBG_LINE (300) aus
 * v4_debug.c bleiben, sonst wuerde die Debugzeile abgeschnitten. */
#define V4_JOY_TEXT_MAX     300u

/* Groesste Zeile der MEHRZEILIGEN Direktanzeige (v4_joy_direkt_text): eine
 * Kopfzeile, die sieben Leitungszeilen und drei Summenzeilen. Das ist die
 * Flaeche eines MUI-Textfelds, nicht eine Debugzeile -- die 300 von
 * V4_JOY_TEXT_MAX gelten fuer Protokollzeilen und waeren hier zu klein. */
#define V4_JOY_DIREKT_MAX   512u

/* ------------------------------------------------------------------ */
/* Zustand                                                             */
/* ------------------------------------------------------------------ */

typedef struct {
    /* ---- Rohwerte: stehen IMMER da, auch wenn ein Wert verworfen wurde -- */
    v4_joy_roh_t  roh;                              /* letzte Abtastung     */

    /* ---- Richtungen je Buchse (V4P_IN_ST_*-Richtungsbits) -------------- */
    uint16_t      zaehler[V4P_PORT_COUNT];          /* letzter gueltiger    */
    uint8_t       richtung[V4P_PORT_COUNT];         /* dekodierte Richtungen*/
    uint8_t       richtung_gueltig[V4P_PORT_COUNT]; /* 0 = noch nie stabil  */
    uint8_t       richtung_stabil[V4P_PORT_COUNT];  /* 1 = letzte war stabil*/

    /* ---- Feuerknoepfe: Grundzustand und letzter gueltiger Stand -------- */
    v4_joy_roh_t  basis_roh;                        /* Registersatz beim Start */
    uint8_t       feuer_jetzt[V4P_PORT_COUNT][2];   /* 0/1 letzte Abtastung */
    uint8_t       feuer_stabil[V4P_PORT_COUNT];     /* 1 = letzte war stabil*/
    uint8_t       basis_ok;                         /* 1 = Startwert gemerkt*/

    /* ---- Diagnose ------------------------------------------------------- */
    unsigned long verworfen;    /* ungleiche Zaehlerpaare (Stabilitaetstor) */
    unsigned long verworfen_feuer;  /* ungleiche Feuerbits                */
} v4_joy_t;

/* ------------------------------------------------------------------ */
/* Registerzugriff -- der EINE Haken                                   */
/* ------------------------------------------------------------------ */

/* Liest einen Registersatz. Im Test durch eine Attrappe ersetzt. */
typedef void (*v4_joy_lesen_fn)(v4_joy_roh_t *roh);

/*
 * Haken setzen. NULL = keine Hardware: v4_joy_lesen liefert dann Nullen (Wirt,
 * Demo, Tests). Auf der V4 ist die Vorgabe der echte Registerzugriff; die
 * Tests setzen ihren eigenen Haken.
 */
void v4_joy_set_lesen(v4_joy_lesen_fn fn);

/* EINEN Registersatz lesen. Ohne Haken: alles 0. */
void v4_joy_lesen(v4_joy_roh_t *roh);

/* ------------------------------------------------------------------ */
/* Reine Rechnung                                                      */
/* ------------------------------------------------------------------ */

/*
 * Zaehlerwert -> Richtungsmaske (nur VOR/ZURUECK/LINKS/RECHTS).
 *
 * Rein, ohne Zustand, ohne Register: genau deshalb ist sie im Wirtstest
 * vollstaendig pruefbar. Beide Buchsen benutzen dieselbe Rechnung -- die
 * Register liefern fuer Buchse 0 und 1 dieselben Bitlagen.
 */
uint8_t v4_joy_richtungen(uint16_t j);

/* Text zu einer Richtungsmaske, z.B. "VOR+RECHTS"; 0 Bits ergeben "-".
 * Ausgabe immer nullterminiert. */
size_t v4_joy_richtungen_text(uint8_t maske, char *out, size_t cap);

/* ------------------------------------------------------------------ */
/* Zustand fortschreiben                                               */
/* ------------------------------------------------------------------ */

/* Auf "noch nichts gelesen" setzen (Rohwerte 0). */
void v4_joy_init(v4_joy_t *st);

/*
 * Grundzustand der Feuerleitungen merken. Genau einmal beim Start rufen
 * (v4_gui.c tut das direkt nach dem Oeffnen der Plattform). Ohne den Aufruf
 * merkt sich v4_joy_update den ersten gueltigen Satz selbst -- der
 * Grundzustand ist dann die erste Abtastung.
 */
void v4_joy_basis(v4_joy_t *st, const v4_joy_roh_t *roh);

/*
 * ZWEI Abtastungen derselben Buchsen auswerten (siehe Kopf: Stabilitaet).
 *
 * `a` und `b` sind mit kurzem Abstand gelesen. Je Buchse gilt:
 *   Zaehler gleich -> Richtung uebernehmen, richtung_stabil = 1
 *   Zaehler ungleich -> letzten gueltigen Wert stehen lassen (verworfen++)
 * Fuer die Feuerbits gilt dieselbe Regel mit eigenem Zaehler: sie haengen nicht
 * am Zaehler, ein bewegter Zaehler darf einen Tastendruck nicht verschlucken.
 *
 * Die Rohwerte von `a` stehen danach IMMER in st->roh.
 */
void v4_joy_update(v4_joy_t *st, const v4_joy_roh_t *a, const v4_joy_roh_t *b);

/* Gedrueckte Feuerknoepfe einer Buchse (V4P_IN_ST_BTN1|BTN2), 0 wenn keiner. */
uint8_t v4_joy_feuer(const v4_joy_t *st, int port);

/* Richtungen einer Buchse als V4P_IN_ST_*-Maske (0, wenn noch nie stabil). */
uint8_t v4_joy_richtung(const v4_joy_t *st, int port);

/* Richtungen UND Feuerknoepfe -- die Zahl, die mit dem I2C-Wert verglichen
 * wird. */
uint8_t v4_joy_maske(const v4_joy_t *st, int port);

/* ------------------------------------------------------------------ */
/* Urteil direkt <-> I2C                                               */
/* ------------------------------------------------------------------ */

/* 0xFF ist kein gueltiger Maskenwert (V4_JOY_VERGLEICHSMASKE ist hoechstens
 * 0x7F) und heisst deshalb "noch kein Urteil". */
#define V4_JOY_KEIN_URTEIL 0xFFu

/*
 * Das Urteil EINER Buchse als Zahl -- die EINE Stelle, an der entschieden
 * wird, ob beide Quellen dasselbe sagen:
 *
 *   0x00                   OK (die verglichenen Leitungen stimmen ueberein)
 *   > 0                    Abweichung; die gesetzten Bits sind die Leitungen,
 *                          die auseinandergehen
 *   V4_JOY_KEIN_URTEIL     kein Urteil: keine stabile direkte Abtastung,
 *                          gar keine I2C-Antwort, oder eine ALTE Antwort
 *                          (i2c_gueltig == 0). Gegen eine alte Antwort zu
 *                          vergleichen waere keine Messung.
 *
 * Rein und ohne Zustand -- deshalb im Wirtstest vollstaendig pruefbar. Die
 * Anzeige, die Debugzeile und die Sofortmeldung bei Aenderung benutzen alle
 * dieselbe Funktion, damit es nicht zwei Wahrheiten gibt.
 */
uint8_t v4_joy_abweichung(const v4_joy_t *st, const v4p_input_t *in,
                          int i2c_gueltig, int port);

/* ------------------------------------------------------------------ */
/* Textzeilen (alle rein, damit die Formate pruefbar sind)             */
/* ------------------------------------------------------------------ */

/*
 * Die Vergleichszeile EINER Buchse in der Form des Auftrags, z.B.
 *
 *   Buchse 1  direkt: 0x0A RECHTS+ZURUECK   roh 0x0003   I2C: state=0x0A level=0x75   OK
 *
 * `in` darf NULL sein (noch keine Antwort); `i2c_gueltig` sagt, ob die Werte in
 * `in` aus DIESEM Takt stammen (0 = alte Antwort, dann gibt es kein Urteil).
 * Bei Abweichung steht ausdruecklich "ABWEICHUNG (Differenz 0x..)". Ohne
 * stabile direkte Abtastung steht "direkt: -- (noch keine stabile Abtastung)".
 */
size_t v4_joy_port_text(const v4_joy_t *st, const v4p_input_t *in,
                        int i2c_gueltig, int port, char *out, size_t cap);

/* Die Rohwerte der Feuerregister in vier Hexziffern, dazu der Grundzustand:
 *
 *   CIAAPRA=0x00F4  POTINP=0xFF02   Grundzustand CIAAPRA=0x00F4 POTINP=0xFF02
 */
size_t v4_joy_reg_text(const v4_joy_t *st, char *out, size_t cap);

/* Die Zeile fuer beide Buchsen, wie sie auf den DebugUART geht:
 *
 *   [vergleich] Buchse0 direkt=0x00 stabil=ja roh=0x0000 I2C=state0x00/level0x7F OK | Buchse1 ... | CIAAPRA=0x00F4 POTINP=0xFF02
 */
size_t v4_joy_debug_text(const v4_joy_t *st, const v4p_input_t *in,
                         int i2c_gueltig, char *out, size_t cap);

/*
 * Derselbe Vergleich, aber OHNE das Praefix "[vergleich]" -- fuer die
 * Herzzeile: die Oberflaeche haengt ihn mit v4_dbg_herz_zusatz() an die eine
 * Zeile je Sekunde an, statt eine zweite aufzumachen. Enthaelt beide Buchsen
 * UND die Rohwerte von CIAAPRA/POTINP.
 */
size_t v4_joy_herz_text(const v4_joy_t *st, const v4p_input_t *in,
                        int i2c_gueltig, char *out, size_t cap);

/* Die drei Zeilen fuer die MUI-Anzeige (beide Buchsen + Rohwerte), mit
 * Umbruch getrennt und ohne Umbruch am Ende. */
size_t v4_joy_vergleich_text(const v4_joy_t *st, const v4p_input_t *in,
                             int i2c_gueltig, char *out, size_t cap);

/*
 * Die Anzeige EINER Buchse AUSSCHLIESSLICH aus den eigenen Registern -- OHNE
 * jeden I2C-Wert. Das ist die Zeile fuer das Fenster: dort steht, was an den
 * Anschlussleitungen ankommt (JOY0DAT/JOY1DAT fuer die Richtungen,
 * CIAAPRA/POTINP fuer die Feuerknoepfe), nicht die Sicht des ESP32.
 *
 * Aufbau (mehrzeilig, ohne Umbruch am Ende):
 *
 *   Buchse 0  direkt aus den Registern
 *   Leitung  Pin   Pegel    direkt
 *   UP       Pin 1  -        -
 *   ...
 *   BTN1     Pin 6  0 (Low)  gedrueckt
 *   BTN2     Pin 9  1 (High) -
 *   BTN3     Pin 5  -        nicht lesbar
 *   gedrueckt (direkt): BTN1
 *   Roh: Zaehler=0x0300 (stabil=ja)
 *   CIAAPRA=0x00F4 POTINP=0xFF02
 *
 * Der Vergleich direkt <-> I2C steht AUSDRUECKLICH NICHT hier: er gehoert ins
 * Protokoll (DebugUART/Logdatei, v4_joy_debug_text/v4_joy_herz_text). `Pegel`
 * ist der ROHWERT des jeweiligen Feuerregisters (1 = High); fuer die
 * Richtungen gibt es keine einzelne Pegelleitung -- sie kommen aus dem
 * Zaehler, dessen Rohwert in der vorletzten Zeile steht. BTN3 (Pin 5) ist
 * direkt NICHT lesbar und wird als solches benannt, statt eine Messung
 * vorzutaeuschen.
 *
 * Der Puffer des Aufrufers muss V4_JOY_DIREKT_MAX Byte fassen: die Anzeige ist
 * MEHRZEILIG (Kopfzeile, sieben Leitungszeilen, drei Summenzeilen).
 */
size_t v4_joy_direkt_text(const v4_joy_t *st, int port, char *out, size_t cap);

#endif /* V4_JOYPORT_H */
