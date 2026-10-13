/*
 * v4_joyport.c -- Joystick-Anschluesse der V4 direkt lesen und mit der
 * I2C-Antwort vergleichen: siehe v4_joyport.h (dort steht, WARUM).
 *
 * Hier steht ABSICHTLICH kein Betriebssystem- und kein Busaufruf: kein
 * CreateNewProc, kein Nachrichtenport, kein Delay(), kein Forbid(), kein
 * v4_plat_*. Der einzige Hardwarezugriff sind vier Register hinter EINEM Haken
 * (v4_joy_set_lesen); alles andere ist reine Rechnung und damit im Wirtstest
 * (tests/test_joyport.c) vollstaendig pruefbar -- dieselbe Bauart wie
 * v4_state.c. Ein versehentlich eingebauter Amiga-Aufruf wuerde den Wirtstest
 * schon beim Linken scheitern lassen.
 *
 * Wer spaeter sperren zu muessen glaubt, nimmt eine SignalSemaphore -- NIEMALS
 * Forbid(). Es gibt hier aber nichts zu sperren: der Block gehoert dem einen
 * Faden des Frontends (MUI-Task), und es gibt keinen zweiten.
 */

#include "v4_joyport.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/* Registerzugriff -- der eine Haken                                   */
/* ------------------------------------------------------------------ */

#ifdef V4_PLAT_AMIGA
/*
 * Der echte Zugriff auf der Vampire V4. Die Adressen sind die klassischen
 * Amiga-Register; der Apollo-Kern bildet sie auf die beiden DB9-Buchsen ab (am
 * Geraet nachgemessen, siehe Projects/BT-Joy/joyprobe/joyprobe.c).
 *
 * Alle vier Zugriffe sind wort- bzw. byteweise und liegen auf geraden
 * Adressen. KEIN Zugriff ueber einen Funktionseinsprung, kein Delay: das ist
 * ein Speicherzugriff, sonst nichts.
 */
static void joy_lesen_amiga(v4_joy_roh_t *roh)
{
    roh->joy0dat = *(volatile uint16_t *)(uintptr_t)V4_JOY_ADR_JOY0DAT;
    roh->joy1dat = *(volatile uint16_t *)(uintptr_t)V4_JOY_ADR_JOY1DAT;
    roh->ciaapra = *(volatile uint8_t  *)(uintptr_t)V4_JOY_ADR_CIAAPRA;
    roh->potinp  = *(volatile uint16_t *)(uintptr_t)V4_JOY_ADR_POTINP;
}
#endif /* V4_PLAT_AMIGA */

/* Vorgabe: auf der V4 die Register, auf dem Wirt "keine Hardware". Den Wirt
 * interessiert das nicht -- dort setzen Tests und Demo ihren eigenen Haken
 * (oder lesen gar nicht). */
static v4_joy_lesen_fn s_lesen =
#ifdef V4_PLAT_AMIGA
    joy_lesen_amiga;
#else
    NULL;
#endif

void v4_joy_set_lesen(v4_joy_lesen_fn fn)
{
    s_lesen = fn;
}

void v4_joy_lesen(v4_joy_roh_t *roh)
{
    if (roh == NULL) {
        return;
    }
    if (s_lesen != NULL) {
        s_lesen(roh);
        return;
    }
    roh->joy0dat = 0u;
    roh->joy1dat = 0u;
    roh->ciaapra = 0u;
    roh->potinp  = 0u;
}

/* ------------------------------------------------------------------ */
/* Feuerleitungen: welche Quelle, welches Bit, welches Zustandsbit     */
/* ------------------------------------------------------------------ */

#define V4_JOY_QUELLE_CIA  0u   /* CIAAPRA ($BFE001) */
#define V4_JOY_QUELLE_POT  1u   /* POTINP  ($DFF016) */

/*
 * Je Buchse zwei Leitungen. Die Zuordnung ist die des Auftrags:
 *
 *   Knopf 1 (Pin 6)  CIAAPRA  Bit 6 fuer Buchse 0, Bit 7 fuer Buchse 1
 *                    (klassische Amiga-Belegung /FIR0 und /FIR1; Bit 7 ist am
 *                    Geraet NICHT gemessen -- der Rohwert steht daneben)
 *   Knopf 2 (Pin 9)  POTINP   Bit 8 fuer Buchse 0, Bit 10 fuer Buchse 1
 *
 * Knopf 3 (Pin 5) fehlt hier ABSICHTLICH: der Auftrag nennt dafuer keine
 * direkte Quelle. Er steht weiter im I2C-Wert und wird beim Vergleich
 * ausgelassen (V4_JOY_VERGLEICHSMASKE).
 */
static const struct {
    uint8_t  quelle;    /* V4_JOY_QUELLE_*          */
    uint16_t bit;       /* Bit im Register          */
    uint8_t  zustand;   /* V4P_IN_ST_BTN1 / BTN2    */
} s_feuer[V4P_PORT_COUNT][2] = {
    {
        { V4_JOY_QUELLE_CIA, V4_JOY_CIA_BTN1_P0, (uint8_t)V4P_IN_ST_BTN1 },
        { V4_JOY_QUELLE_POT, V4_JOY_POT_BTN2_P0, (uint8_t)V4P_IN_ST_BTN2 }
    },
    {
        { V4_JOY_QUELLE_CIA, V4_JOY_CIA_BTN1_P1, (uint8_t)V4P_IN_ST_BTN1 },
        { V4_JOY_QUELLE_POT, V4_JOY_POT_BTN2_P1, (uint8_t)V4P_IN_ST_BTN2 }
    }
};

/* Das Register, in dem eine Leitung liegt. */
static uint16_t feuer_register(const v4_joy_roh_t *roh, uint8_t quelle)
{
    return (quelle == V4_JOY_QUELLE_POT) ? roh->potinp : (uint16_t)roh->ciaapra;
}

/* 0/1: liegt das Bit an? */
static uint8_t feuer_bit(const v4_joy_roh_t *roh, uint8_t quelle, uint16_t bit)
{
    return (uint8_t)((feuer_register(roh, quelle) & bit) != 0u);
}

/* Zaehlerwert einer Buchse aus einem Registersatz. */
static uint16_t roh_zaehler(const v4_joy_roh_t *roh, int port)
{
    return (port == 0) ? roh->joy0dat : roh->joy1dat;
}

/* ------------------------------------------------------------------ */
/* Reine Rechnung: Registerwert -> Richtungen                          */
/* ------------------------------------------------------------------ */

/*
 * Die vier Richtungen, wie am Geraet nachgemessen (Einzelheiten und die
 * Begruendung der VOR/ZURUECK-Form stehen in v4_joyport.h):
 *
 *   RECHTS   j & 0x0002                       X-Zaehler Bit 1 (Pin 4)
 *   LINKS    j & 0x0200                       Y-Zaehler Bit 1 (Pin 3)
 *   VOR      (j & 0x0100) und LINKS frei      Y-Zaehler Bit 0 (Pin 1)
 *   ZURUECK  (j & 0x0001) und RECHTS frei     X-Zaehler Bit 0 (Pin 2)
 *
 * Die Bedingung "Gegenrichtung frei" ist der Kern: der Zaehler steht nach einer
 * Bewegung auf 11 (RECHTS wird als $0003 gemessen), und ohne die Bedingung
 * wuerde jede RECHTS-Abtastung zusaetzlich "zurueck" melden. Die Flankenformeln
 * des Auftrags ($0100 -> "VOR ZURUECK" gleichzeitig, $0001 -> gar nichts) sind
 * damit fuer die nachgemessenen Werte aufgeloest.
 */
uint8_t v4_joy_richtungen(uint16_t j)
{
    uint8_t m = 0u;

    if ((j & V4_JOY_M_RECHTS) != 0u) {
        m |= (uint8_t)V4P_IN_ST_RIGHT;
    }
    if ((j & V4_JOY_M_LINKS) != 0u) {
        m |= (uint8_t)V4P_IN_ST_LEFT;
    }
    if ((j & V4_JOY_M_VOR) != 0u && (j & V4_JOY_M_LINKS) == 0u) {
        m |= (uint8_t)V4P_IN_ST_UP;
    }
    if ((j & V4_JOY_M_ZURUECK) != 0u && (j & V4_JOY_M_RECHTS) == 0u) {
        m |= (uint8_t)V4P_IN_ST_DOWN;
    }
    return m;
}

/* ------------------------------------------------------------------ */
/* Kleine Textbausteine                                                */
/* ------------------------------------------------------------------ */

/* Anhaengen mit Schranke: der Puffer ist immer nullterminiert, ein zu langer
 * Text wird gekuerzt statt zu laufen. */
static void tx_add(char *out, size_t cap, size_t *len, const char *fmt, ...)
{
    va_list ap;
    int     n;

    if (out == NULL || cap == 0u || len == NULL || *len + 1u >= cap) {
        return;
    }
    va_start(ap, fmt);
    n = vsnprintf(out + *len, cap - *len, fmt, ap);
    va_end(ap);
    if (n <= 0) {
        out[*len] = '\0';
        return;
    }
    if ((size_t)n >= cap - *len) {
        *len = cap - 1u;            /* gekuerzt, aber terminiert */
        return;
    }
    *len += (size_t)n;
}

/* Deutsche Namen in der Bitfolge des Protokolls (Bit 0 zuerst). */
static const char *const s_richtung_name[V4P_IN_ST_COUNT] = {
    "VOR", "ZURUECK", "LINKS", "RECHTS", "BTN1", "BTN2", "BTN3"
};

size_t v4_joy_richtungen_text(uint8_t maske, char *out, size_t cap)
{
    size_t   n     = 0u;
    int      first = 1;
    unsigned bit;

    if (out == NULL || cap == 0u) {
        return 0u;
    }
    out[0] = '\0';

    maske &= (uint8_t)V4P_IN_ST_MASK;
    for (bit = 0u; bit < V4P_IN_ST_COUNT; bit++) {
        size_t      len;
        const char *name;
        size_t      k;

        if ((maske & (uint8_t)(1u << bit)) == 0u) {
            continue;
        }
        name = s_richtung_name[bit];
        len  = strlen(name);
        if (n + (first ? 0u : 1u) + len + 1u > cap) {
            break;
        }
        if (first == 0) {
            out[n++] = '+';
        }
        for (k = 0u; k < len; k++) {
            out[n++] = name[k];
        }
        first = 0;
    }
    if (first != 0) {
        if (cap < 2u) {
            out[0] = '\0';
            return 0u;
        }
        out[0] = '-';
        out[1] = '\0';
        n      = 1u;
    } else {
        out[n] = '\0';
    }
    return n;
}

/* ------------------------------------------------------------------ */
/* Zustand                                                             */
/* ------------------------------------------------------------------ */

void v4_joy_init(v4_joy_t *st)
{
    if (st == NULL) {
        return;
    }
    memset(st, 0, sizeof(*st));
}

void v4_joy_basis(v4_joy_t *st, const v4_joy_roh_t *roh)
{
    if (st == NULL || roh == NULL) {
        return;
    }
    st->basis_roh = *roh;
    st->basis_ok  = 1u;
}

void v4_joy_update(v4_joy_t *st, const v4_joy_roh_t *a, const v4_joy_roh_t *b)
{
    int port;
    int k;

    if (st == NULL || a == NULL || b == NULL) {
        return;
    }

    /* Die Rohwerte der ERSTEN Abtastung stehen immer da -- auch dann, wenn das
     * Stabilitaetstor den Wert verwirft. Genau sie machen den Befund von Hand
     * lesbar. */
    st->roh = *a;

    /* Ohne ausdruecklichen Grundzustand ist er die erste Abtastung. */
    if (st->basis_ok == 0u) {
        v4_joy_basis(st, a);
    }

    for (port = 0; port < (int)V4P_PORT_COUNT; port++) {
        uint16_t ja = roh_zaehler(a, port);
        uint16_t jb = roh_zaehler(b, port);
        int      feuer_gleich = 1;

        /* Zaehler: nur bei zwei GLEICHEN Abtastungen uebernehmen. */
        if (ja == jb) {
            st->zaehler[port]          = ja;
            st->richtung[port]         = v4_joy_richtungen(ja);
            st->richtung_gueltig[port] = 1u;
            st->richtung_stabil[port]  = 1u;
        } else {
            /* Der Zaehler springt: letzten GUELTIGEN Stand stehen lassen. */
            st->richtung_stabil[port] = 0u;
            st->verworfen++;
        }

        /* Feuerleitungen getrennt bewerten: sie haengen nicht am Zaehler. Ein
         * bewegter Zaehler (Mausbetrieb) darf einen Tastendruck nicht
         * verschlucken. */
        for (k = 0; k < 2; k++) {
            if (feuer_bit(a, s_feuer[port][k].quelle, s_feuer[port][k].bit)
                != feuer_bit(b, s_feuer[port][k].quelle,
                             s_feuer[port][k].bit)) {
                feuer_gleich = 0;
            }
        }
        if (feuer_gleich != 0) {
            for (k = 0; k < 2; k++) {
                st->feuer_jetzt[port][k] =
                    feuer_bit(a, s_feuer[port][k].quelle,
                              s_feuer[port][k].bit);
            }
            st->feuer_stabil[port] = 1u;
        } else {
            st->feuer_stabil[port] = 0u;
            st->verworfen_feuer++;
        }
    }
}

uint8_t v4_joy_feuer(const v4_joy_t *st, int port)
{
    uint8_t m = 0u;
    int     k;

    if (st == NULL || port < 0 || port >= (int)V4P_PORT_COUNT) {
        return 0u;
    }
    if (st->basis_ok == 0u) {
        /* Ohne Grundzustand gibt es kein "gedrueckt": die Polaritaet ist nicht
         * geraten, sie wird gemessen. */
        return 0u;
    }
    for (k = 0; k < 2; k++) {
        uint8_t basis  = feuer_bit(&st->basis_roh, s_feuer[port][k].quelle,
                                   s_feuer[port][k].bit);
        uint8_t jetzt  = st->feuer_jetzt[port][k];

        if (jetzt != basis) {
            m |= s_feuer[port][k].zustand;
        }
    }
    return m;
}

uint8_t v4_joy_richtung(const v4_joy_t *st, int port)
{
    if (st == NULL || port < 0 || port >= (int)V4P_PORT_COUNT) {
        return 0u;
    }
    return st->richtung[port];
}

uint8_t v4_joy_maske(const v4_joy_t *st, int port)
{
    if (st == NULL || port < 0 || port >= (int)V4P_PORT_COUNT) {
        return 0u;
    }
    return (uint8_t)(st->richtung[port] | v4_joy_feuer(st, port));
}

/*
 * Das Urteil direkt <-> I2C. Die EINE Stelle, an der entschieden wird, ob
 * beide Quellen dasselbe sagen -- Anzeige, Debugzeile und die Sofortmeldung
 * bei Aenderung benutzen alle diese Funktion.
 *
 * KEIN URTEIL (V4_JOY_KEIN_URTEIL) gibt es in drei Faellen, und alle drei
 * muessen "kein Urteil" bleiben statt zu "OK" zu werden:
 *   - die direkte Abtastung war noch nie stabil (richtung_gueltig == 0),
 *   - es gibt gar keine I2C-Antwort (in == NULL),
 *   - die I2C-Antwort ist ALT (i2c_gueltig == 0).
 * Gegen eine alte Antwort zu vergleichen waere keine Messung, sondern eine
 * Behauptung.
 */
uint8_t v4_joy_abweichung(const v4_joy_t *st, const v4p_input_t *in,
                          int i2c_gueltig, int port)
{
    if (st == NULL || port < 0 || port >= (int)V4P_PORT_COUNT) {
        return (uint8_t)V4_JOY_KEIN_URTEIL;
    }
    if (st->richtung_gueltig[port] == 0u || in == NULL || i2c_gueltig == 0) {
        return (uint8_t)V4_JOY_KEIN_URTEIL;
    }
    return (uint8_t)((v4_joy_maske(st, port) ^ in->state[port])
                     & V4_JOY_VERGLEICHSMASKE);
}

/* ------------------------------------------------------------------ */
/* Textzeilen                                                          */
/* ------------------------------------------------------------------ */

/* Der Rohwert des Zaehlers, der in der aktuellen Abtastung steht. */
static uint16_t st_zaehler_roh(const v4_joy_t *st, int port)
{
    return roh_zaehler(&st->roh, port);
}

/* I2C-Spalte. `in` = NULL heisst "noch keine Antwort". */
static void tx_i2c(char *out, size_t cap, size_t *n, const v4p_input_t *in,
                   int port)
{
    if (in == NULL) {
        tx_add(out, cap, n, "I2C: noch keine Antwort");
        return;
    }
    if (in->level_valid != 0u) {
        tx_add(out, cap, n, "I2C: state=0x%02X level=0x%02X",
               (unsigned)in->state[port], (unsigned)in->level[port]);
    } else {
        /* 10-Byte-Fassung: keine Zahl, wo keine Messung ist. */
        tx_add(out, cap, n, "I2C: state=0x%02X level=unbekannt",
               (unsigned)in->state[port]);
    }
}

/* Das Urteil. `i2c_gueltig` = 0 heisst: die Werte sind alt, es gibt kein
 * Urteil. `kurz` = 1 fuer die Debugzeile (eine Zeile, wenig Platz), 0 fuer die
 * Anzeige. */
static void tx_urteil(char *out, size_t cap, size_t *n, const v4_joy_t *st,
                      const v4p_input_t *in, int i2c_gueltig, int port,
                      const char *trenner, int kurz)
{
    uint8_t diff = v4_joy_abweichung(st, in, i2c_gueltig, port);

    if (diff == (uint8_t)V4_JOY_KEIN_URTEIL) {
        /* Den Grund nennen, nicht nur "kein Urteil": er ist die Diagnose. */
        if (st->richtung_gueltig[port] == 0u) {
            tx_add(out, cap, n, "%s-- (noch keine stabile Abtastung)", trenner);
        } else if (in == NULL) {
            tx_add(out, cap, n, "%s--", trenner);
        } else {
            tx_add(out, cap, n, "%s-- (alte Antwort)", trenner);
        }
        return;
    }
    if (diff != 0u) {
        if (kurz != 0) {
            tx_add(out, cap, n, "%sABWEICHUNG(Diff 0x%02X)", trenner,
                   (unsigned)diff);
        } else {
            tx_add(out, cap, n, "%sABWEICHUNG (Differenz 0x%02X)", trenner,
                   (unsigned)diff);
        }
        return;
    }
    tx_add(out, cap, n, "%sOK", trenner);
}

size_t v4_joy_port_text(const v4_joy_t *st, const v4p_input_t *in,
                        int i2c_gueltig, int port, char *out, size_t cap)
{
    size_t n  = 0u;
    char   namen[64];

    if (out == NULL || cap == 0u) {
        return 0u;
    }
    out[0] = '\0';
    if (st == NULL || port < 0 || port >= (int)V4P_PORT_COUNT) {
        return 0u;
    }

    tx_add(out, cap, &n, "Buchse %d  direkt: ", port);
    if (st->richtung_gueltig[port] == 0u) {
        tx_add(out, cap, &n, "-- (noch keine stabile Abtastung)");
    } else {
        uint8_t maske = v4_joy_maske(st, port);

        (void)v4_joy_richtungen_text(maske, namen, sizeof(namen));
        tx_add(out, cap, &n, "0x%02X %s", (unsigned)maske, namen);
    }

    tx_add(out, cap, &n, "   roh 0x%04X%s   ",
           (unsigned)st_zaehler_roh(st, port),
           (st->richtung_stabil[port] != 0u) ? "" : " (springt)");
    tx_i2c(out, cap, &n, in, port);
    tx_urteil(out, cap, &n, st, in, i2c_gueltig, port, "   ", 0);
    return n;
}

size_t v4_joy_reg_text(const v4_joy_t *st, char *out, size_t cap)
{
    size_t n = 0u;

    if (out == NULL || cap == 0u) {
        return 0u;
    }
    out[0] = '\0';
    if (st == NULL) {
        return 0u;
    }

    tx_add(out, cap, &n, "CIAAPRA=0x%04X  POTINP=0x%04X",
           (unsigned)st->roh.ciaapra, (unsigned)st->roh.potinp);
    if (st->basis_ok != 0u) {
        tx_add(out, cap, &n,
               "   Grundzustand CIAAPRA=0x%04X POTINP=0x%04X",
               (unsigned)st->basis_roh.ciaapra,
               (unsigned)st->basis_roh.potinp);
    } else {
        tx_add(out, cap, &n, "   Grundzustand noch nicht gelesen");
    }
    return n;
}

/*
 * Der Vergleich beider Buchsen MIT den Rohwerten -- das Stueck, das die
 * Debugzeile und die Herzzeile gemeinsam haben. Ohne Praefix, damit der
 * Aufrufer entscheidet, wie die Zeile heisst.
 */
static size_t vergleich_teil(const v4_joy_t *st, const v4p_input_t *in,
                             int i2c_gueltig, char *out, size_t cap)
{
    size_t n = 0u;
    int    port;

    if (out == NULL || cap == 0u) {
        return 0u;
    }
    out[0] = '\0';
    if (st == NULL) {
        return 0u;
    }

    for (port = 0; port < (int)V4P_PORT_COUNT; port++) {
        tx_add(out, cap, &n, "%s Buchse%d direkt=0x%02X stabil=%s roh=0x%04X",
               (port == 0) ? "" : " |", port,
               (unsigned)v4_joy_maske(st, port),
               (st->richtung_stabil[port] != 0u) ? "ja" : "nein",
               (unsigned)st_zaehler_roh(st, port));
        if (in == NULL) {
            tx_add(out, cap, &n, " I2C noch keine Antwort");
        } else if (in->level_valid != 0u) {
            tx_add(out, cap, &n, " I2C state=0x%02X level=0x%02X",
                   (unsigned)in->state[port], (unsigned)in->level[port]);
        } else {
            tx_add(out, cap, &n, " I2C state=0x%02X level=unbekannt",
                   (unsigned)in->state[port]);
        }
        tx_urteil(out, cap, &n, st, in, i2c_gueltig, port, " ", 1);
    }
    tx_add(out, cap, &n, " | CIAAPRA=0x%04X POTINP=0x%04X",
           (unsigned)st->roh.ciaapra, (unsigned)st->roh.potinp);
    return n;
}

size_t v4_joy_debug_text(const v4_joy_t *st, const v4p_input_t *in,
                         int i2c_gueltig, char *out, size_t cap)
{
    size_t n = 0u;

    if (out == NULL || cap == 0u) {
        return 0u;
    }
    out[0] = '\0';
    if (st == NULL) {
        return 0u;
    }
    tx_add(out, cap, &n, "[vergleich]");
    n += vergleich_teil(st, in, i2c_gueltig, out + n,
                        (n < cap) ? (cap - n) : 0u);
    return n;
}

size_t v4_joy_herz_text(const v4_joy_t *st, const v4p_input_t *in,
                        int i2c_gueltig, char *out, size_t cap)
{
    return vergleich_teil(st, in, i2c_gueltig, out, cap);
}

size_t v4_joy_vergleich_text(const v4_joy_t *st, const v4p_input_t *in,
                             int i2c_gueltig, char *out, size_t cap)
{
    size_t n = 0u;
    int    port;

    if (out == NULL || cap == 0u) {
        return 0u;
    }
    out[0] = '\0';
    if (st == NULL) {
        return 0u;
    }

    for (port = 0; port < (int)V4P_PORT_COUNT; port++) {
        size_t m = v4_joy_port_text(st, in, i2c_gueltig, port,
                                    out + n, (n < cap) ? (cap - n) : 0u);

        n += m;
        if (n + 2u < cap) {
            out[n++] = '\n';
            out[n]   = '\0';
        }
    }
    {
        size_t m = v4_joy_reg_text(st, out + n, (n < cap) ? (cap - n) : 0u);

        n += m;
    }
    if (n + 1u < cap) {
        out[n] = '\0';
    }
    return n;
}
