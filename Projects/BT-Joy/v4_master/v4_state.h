/*
 * v4_state.h -- Zustandshilfe der beiden Frontends (Konsole und MUI).
 *
 * WAS DIESE DATEI IST
 * -------------------
 * Ein reiner Datenspeicher mit ein paar Rechenregeln:
 *
 *   - die ZULETZT GUELTIGEN Werte der Gegenstelle (Kopf, Status, HID, Eingaenge,
 *     Buchsenmodi, Geraeteliste),
 *   - die Verbindungslage (link_ok, Fehlerserie, wachsender Abstand, Klartext),
 *   - die Einordnung eines Rueckgabewerts als "Leitung steht" oder "Leitung
 *     ist weg" (v4_state_rc_is_down).
 *
 * WAS DIESE DATEI NICHT IST -- UND WARUM DAS WICHTIG IST
 * -----------------------------------------------------
 * Hier steht KEIN einziger Betriebssystem- und KEIN einziger Busaufruf: kein
 * CreateNewProc, kein Nachrichtenport, kein Forbid, kein v4_plat_*.
 * Die Frontends rufen v4_master (und damit v4_plat_*) DIREKT aus ihrem
 * Hauptfaden auf und legen das Ergebnis danach hier ab -- genau die
 * Konstruktion, die auf der V4 nachweislich laeuft. Die frueher vorhandene
 * eigene Amiga-Aufgabe (v4_worker*.c) ist entfallen: ihr CreateNewProc kam auf
 * der V4 nicht durch und hat den Start des ganzen Programms verhindert.
 *
 * Weil alles in EINEM Faden laeuft, braucht dieser Block auch keine Sperre.
 * Sollte spaeter doch einmal ein zweiter Faden entstehen, gilt: SignalSemaphore
 * (der Wartende schlaeft, das System laeuft weiter) -- NIEMALS Forbid().
 * Forbid() haelt den Aufgabenwechsel im ganzen System an und hat am 13.10.2026
 * die Workbench eingefroren.
 *
 * WARUM DIE WERTE HIER LIEGEN UND NICHT NUR LOKAL
 * ----------------------------------------------
 * Ein Feld wird nur bei einer GUELTIGEN Antwort ersetzt. Bleibt eine Antwort
 * aus, zeigt das Fenster die letzten gueltigen Werte weiter, statt zu leeren --
 * zusammen mit einer ausdruecklichen Zeile "ESP32 antwortet nicht". Genau diese
 * Zusage ist der Grund fuer diesen Block.
 */

#ifndef V4_STATE_H
#define V4_STATE_H

#include "v4_master.h"      /* v4p_*-Typen, Protokollstatus und V4_ERR_* */

/* Takt im Erfolgsfall (ms). 200 ms ist schnell genug, um beim Druecken am
 * Controller zuzusehen, und langsam genug fuer den Bit-Bang-Bus. */
#define V4_STATE_GAP_MS      200ul
/* Wachsender Abstand nach einem Fehlversuch: 0,5 s -> 1 s -> 2 s -> 5 s,
 * danach bleibt es bei 5 s. Ein fehlender Partner erzeugt so keinen Dauerlauf
 * auf dem Bus. */
#define V4_STATE_GAP_MIN_MS  500ul
#define V4_STATE_GAP_MAX_MS 5000ul

/* Klartext der letzten Stoerung. */
#define V4_STATE_ERR_LEN      96u

typedef struct {
    /* ---- Kopfdaten der Gegenstelle (nur bei Erfolg ersetzt) ---------- */
    v4p_info_t   info;
    int          info_valid;    /* PING beantwortet: Daten stehen da        */
    int          info_checked;  /* ... und passen zu dieser Implementierung  */
    v4p_status_t status;
    int          status_valid;
    v4p_hid_t    hid;
    int          hid_valid;
    v4p_input_t  input;
    int          input_valid;
    uint8_t      mode[V4P_PORT_COUNT];      /* Betriebsart je Buchse      */
    uint8_t      applied[V4P_PORT_COUNT];   /* zuletzt bestaetigter Modus */
    int          mode_valid;
    /*
     * Die Eingangsverteilung aus GET_PORT_CONFIG (fw_ver 4): Vorgabe und
     * wirksame Betriebsart, Richtungsquellen und die drei Knopfquellen beider
     * Buchsen -- dazu Datei-Zuordnungen, die NUR diese Sitzung kennt (siehe
     * v4_state_put_devport).
     *
     * portcfg_valid bleibt 0, solange keine gueltige Antwort vorliegt; die
     * Anzeigen lassen dann die letzten Werte stehen, statt zu leeren.
     */
    v4p_portcfg_t portcfg;
    int          portcfg_valid;
    /*
     * Die Maus-Einstellungen aus GET_MOUSE_CFG (fw_ver 5): Tempo und Totzone
     * JE BUCHSE. mousecfg_valid bleibt 0, solange keine gueltige Antwort
     * vorliegt -- die Regler zeigen dann die Vorgabe der Firmware an, aber
     * keinen erfundenen "bestaetigten" Wert.
     */
    v4p_mousecfg_t mousecfg;
    int            mousecfg_valid;
    /*
     * duo aus GET_DUO (fw_ver 6): 0 = aus, 1 = Port 0 Maus / Port 1 Joystick,
     * 2 = umgekehrt. duo_valid bleibt 0, solange keine gueltige Antwort
     * vorliegt -- der Schalter zeigt dann NICHT "aus", sondern "?".
     */
    uint8_t        duo;
    int            duo_valid;
    /*
     * Geraet -> Buchse. Der ESP32 liefert diese Zuordnung ueber die Leitung
     * NICHT zurueck (DEV_GET nennt nur Name und Adresse, GET_PORT_CONFIG nur
     * die Buchsenfelder). Was hier steht, ist deshalb das, was in DIESER
     * Sitzung per SET_DEVICE_PORT gesetzt und bestaetigt wurde -- mehr nicht.
     * Ein Neustart des Programms leert es; die Zuordnung selbst liegt im NVS
     * des ESP32 und bleibt dort.
     */
    uint8_t      devport_bda[V4P_MAX_DEVICES][V4P_BDA_LEN];
    uint8_t      devport_port[V4P_MAX_DEVICES];
    uint8_t      devport_n;
    uint8_t      count;                     /* Geraete laut DEV_COUNT     */
    v4p_dev_t    devs[V4P_MAX_DEVICES];     /* Liste fuer die Oberflaeche */
    uint8_t      dev_n;

    /* ---- Verbindung und Takt ---------------------------------------- */
    int           link_ok;      /* letzter Auftrag brachte eine Antwort    */
    int           link_ever_ok; /* es gab schon einmal eine Antwort        */
    unsigned      fail_streak;  /* Fehlversuche in Folge                   */
    unsigned long gap_ms;       /* angekuendigter Abstand bis zum naechsten
                                 * Versuch (Backoff, sonst Vorgabe)        */
    char          err[V4_STATE_ERR_LEN];    /* Klartext der letzten Stoerung */
} v4_state_t;

/* Setzt den Block auf "nichts empfangen, keine Verbindung", Takt 200 ms. */
void v4_state_init(v4_state_t *s);

/*
 * 1, wenn der Code "keine Verbindung" bedeutet.
 *
 * Wichtig ist die Unterscheidung: ein Protokollstatus (0x00..0x0C) oder
 * V4_ERR_FRAME heisst, dass sehr wohl eine Antwort ankam -- der Partner lebt,
 * nur der Befehl passte nicht. Nur LINK-Fehler, erschoepftes BUSY-Budget und
 * ein nicht erreichter Zielzustand sind Verbindungsstoerungen.
 */
int v4_state_rc_is_down(uint8_t rc);

/* Abstand zur naechsten Abfrage nach `streak` Fehlversuchen. */
unsigned long v4_state_backoff(unsigned streak);

/*
 * Verbindungslage nach einem Auftrag fortschreiben. `rc` ist das Ergebnis des
 * Auftrags (Protokollstatus oder V4_ERR_*).
 * Rueckgabe 1 = die Lage hat GEWECHSELT (gerade ausgefallen / gerade wieder
 * da); nur dann soll das Frontend eine Zeile schreiben.
 */
int v4_state_note(v4_state_t *s, uint8_t rc);

/* ---- Uebernahme gueltiger Antworten (nie bei einem Fehlschlag rufen) -- */

void v4_state_put_info  (v4_state_t *s, const v4p_info_t *info, int checked);
void v4_state_put_status(v4_state_t *s, const v4p_status_t *st);
void v4_state_put_hid   (v4_state_t *s, const v4p_hid_t *hid);
void v4_state_put_input (v4_state_t *s, const v4p_input_t *in);
/* Beide Betriebsarten auf einmal (GET_PORT_MODE); der Slave sagt, was gilt. */
void v4_state_put_modes (v4_state_t *s, const uint8_t mode[V4P_PORT_COUNT]);
/* Eine einzelne bestaetigte Betriebsart (SET_PORT_MODE). */
void v4_state_put_mode  (v4_state_t *s, uint8_t port, uint8_t mode);
void v4_state_put_devs  (v4_state_t *s, const v4p_dev_t *tab, uint8_t n);

/* Die Eingangsverteilung aus GET_PORT_CONFIG uebernehmen (nur bei gueltiger
 * Antwort rufen). Setzt mode/applied mit: mode_ist ist dieselbe wirksame
 * Betriebsart, die GET_PORT_MODE liefert -- so gibt es nur EINE Quelle. */
void v4_state_put_portcfg(v4_state_t *s, const v4p_portcfg_t *cfg);

/* Die Maus-Einstellungen beider Buchsen aus GET_MOUSE_CFG uebernehmen (nur bei
 * gueltiger Antwort rufen). Die Werte sind STUFEN/Prozent, keine
 * physikalischen Groessen -- es wird hier nichts umgerechnet. */
void v4_state_put_mousecfg(v4_state_t *s, const v4p_mousecfg_t *cfg);

/* duo uebernehmen (nur bei gueltiger Antwort aufrufen). */
void v4_state_put_duo(v4_state_t *s, uint8_t duo);

/* Eine bestaetigte Geraetezuordnung merken (SET_DEVICE_PORT). Gleiche Adresse
 * ersetzt den vorhandenen Eintrag, eine neue kommt dazu (bis
 * V4P_MAX_DEVICES, danach wird der aelteste ersetzt). */
void v4_state_put_devport(v4_state_t *s, const uint8_t bda[V4P_BDA_LEN],
                          uint8_t port);

/* Gemerkte Buchse einer Adresse: 0/1, oder 0xFF = in dieser Sitzung nicht
 * gesetzt. Die Zuordnung ist ueber die Leitung NICHT lesbar -- das ist die
 * einzige Quelle dafuer. */
uint8_t v4_state_devport(const v4_state_t *s, const uint8_t bda[V4P_BDA_LEN]);

/* Zugriff auf den i-ten gemerkten Eintrag (fuer Anzeigen, die ALLE
 * Zuordnungen nennen wollen). Rueckgabe: Buchse 0/1, oder 0xFF = kein
 * Eintrag an dieser Stelle. `bda` darf NULL sein. */
uint8_t v4_state_devport_at(const v4_state_t *s, uint8_t i,
                            uint8_t bda[V4P_BDA_LEN]);

#endif /* V4_STATE_H */
