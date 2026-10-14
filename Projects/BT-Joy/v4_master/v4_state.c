/*
 * v4_state.c -- Zustandshilfe der Frontends: siehe v4_state.h.
 *
 * Hier steht ABSICHTLICH kein Betriebssystemaufruf und kein Buszugriff. Der
 * Block wird ausschliesslich aus dem Hauptfaden der Frontends beschrieben und
 * gelesen; es gibt keinen zweiten Faden und damit auch keine Sperre.
 * Wer spaeter sperren muss, nimmt eine SignalSemaphore -- NIEMALS Forbid().
 */

#include "v4_state.h"

#include <stdio.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/* Verbindungslage                                                     */
/* ------------------------------------------------------------------ */

int v4_state_rc_is_down(uint8_t rc)
{
    return (rc == V4_ERR_LINK || rc == V4_ERR_BUSY || rc == V4_ERR_TIMEOUT)
               ? 1 : 0;
}

/*
 * Backoff-Leiter: 0,5 s -> 1 s -> 2 s -> 5 s, danach bleibt es bei 5 s.
 *
 * Die Werte stehen als Tabelle da und nicht als Schiebeoperation, weil genau
 * diese Folge zugesagt ist (500, 1000, 2000, 5000 -- nicht 4000).
 */
unsigned long v4_state_backoff(unsigned streak)
{
    static const unsigned long stufe[4] = {
        V4_STATE_GAP_MIN_MS,        /* 1. Fehlversuch */
        1000ul,                     /* 2.             */
        2000ul,                     /* 3.             */
        V4_STATE_GAP_MAX_MS         /* 4. und weitere */
    };

    if (streak == 0u) {
        return V4_STATE_GAP_MS;
    }
    if (streak > 4u) {
        streak = 4u;
    }
    return stufe[streak - 1u];
}

void v4_state_init(v4_state_t *s)
{
    if (s == NULL) {
        return;
    }
    memset(s, 0, sizeof(*s));
    s->gap_ms  = V4_STATE_GAP_MS;
    s->link_ok = 0;
    (void)snprintf(s->err, sizeof(s->err),
                   "noch keine Antwort vom ESP32");
}

/* Klartext zur Stoerung -- die Zeile, die im Fenster oben steht. */
static void state_err_text(uint8_t rc, char *out, size_t cap)
{
    if (rc == V4_ERR_LINK) {
        (void)snprintf(out, cap,
                       "ESP32 antwortet nicht -- Verkabelung/Strom pruefen "
                       "(LINK-Fehler)");
    } else if (rc == V4_ERR_BUSY) {
        (void)snprintf(out, cap,
                       "ESP32 meldet dauerhaft BUSY -- Firmware/Strom pruefen");
    } else {
        (void)snprintf(out, cap,
                       "ESP32 hat den Zielzustand nicht erreicht (%s)",
                       v4_strerror(rc));
    }
}

int v4_state_note(v4_state_t *s, uint8_t rc)
{
    char neu[V4_STATE_ERR_LEN];
    int  down;
    int  war_down;

    if (s == NULL) {
        return 0;
    }
    down = v4_state_rc_is_down(rc);
    neu[0] = '\0';
    if (down) {
        state_err_text(rc, neu, sizeof(neu));
    }

    war_down = (s->link_ok == 0) ? 1 : 0;

    if (down) {
        size_t i;

        s->link_ok = 0;
        s->fail_streak++;
        s->gap_ms = v4_state_backoff(s->fail_streak);
        for (i = 0u; i + 1u < sizeof(s->err) && neu[i] != '\0'; i++) {
            s->err[i] = neu[i];
        }
        s->err[i] = '\0';
    } else {
        /* Die Leitung steht -- auch dann, wenn der Befehl selbst scheitert
         * (BAD_ARG, BAD_STATE, unbrauchbare Nutzlast). */
        s->link_ok      = 1;
        s->link_ever_ok = 1;
        s->fail_streak  = 0;
        s->gap_ms       = V4_STATE_GAP_MS;
        s->err[0]       = '\0';
    }
    return (down != war_down) ? 1 : 0;
}

/* ------------------------------------------------------------------ */
/* Uebernahme gueltiger Antworten                                      */
/* ------------------------------------------------------------------ */

void v4_state_put_info(v4_state_t *s, const v4p_info_t *info, int checked)
{
    if (s == NULL || info == NULL) {
        return;
    }
    s->info         = *info;
    s->info_valid   = 1;
    s->info_checked = (checked != 0) ? 1 : 0;
}

void v4_state_put_status(v4_state_t *s, const v4p_status_t *st)
{
    if (s == NULL || st == NULL) {
        return;
    }
    s->status       = *st;
    s->status_valid = 1;
    s->count        = st->dev_count;
}

void v4_state_put_hid(v4_state_t *s, const v4p_hid_t *hid)
{
    if (s == NULL || hid == NULL) {
        return;
    }
    s->hid       = *hid;
    s->hid_valid = 1;
}

void v4_state_put_input(v4_state_t *s, const v4p_input_t *in)
{
    if (s == NULL || in == NULL) {
        return;
    }
    s->input       = *in;
    s->input_valid = 1;
}

void v4_state_put_modes(v4_state_t *s, const uint8_t mode[V4P_PORT_COUNT])
{
    if (s == NULL || mode == NULL) {
        return;
    }
    s->mode[0]    = mode[0];
    s->mode[1]    = mode[1];
    s->applied[0] = mode[0];    /* der Slave sagt, was gilt */
    s->applied[1] = mode[1];
    s->mode_valid = 1;
}

void v4_state_put_mode(v4_state_t *s, uint8_t port, uint8_t mode)
{
    if (s == NULL || port >= V4P_PORT_COUNT) {
        return;
    }
    s->mode[port]    = mode;
    s->applied[port] = mode;
    s->mode_valid    = 1;
}

void v4_state_put_devs(v4_state_t *s, const v4p_dev_t *tab, uint8_t n)
{
    uint8_t i;

    if (s == NULL || tab == NULL) {
        return;
    }
    if (n > (uint8_t)V4P_MAX_DEVICES) {
        n = (uint8_t)V4P_MAX_DEVICES;
    }
    for (i = 0u; i < n; i++) {
        s->devs[i] = tab[i];
    }
    s->dev_n = n;
}

void v4_state_put_portcfg(v4_state_t *s, const v4p_portcfg_t *cfg)
{
    if (s == NULL || cfg == NULL) {
        return;
    }
    s->portcfg       = *cfg;
    s->portcfg_valid = 1;
    /* mode_ist ist die WIRKSAME Betriebsart -- dieselbe, die GET_PORT_MODE
     * liefert. Sie hier mitzufuehren ist Absicht: dann gibt es fuer die
     * Anzeige nur EINE Quelle, und die beiden Abfragen koennen nicht
     * auseinanderlaufen. mode_soll gehoert NICHT in mode[]: 0xFF ist keine
     * Betriebsart, sondern die Vorgabe "nach Geraeteart". */
    s->mode[0]    = cfg->mode_ist[0];
    s->mode[1]    = cfg->mode_ist[1];
    s->applied[0] = cfg->mode_ist[0];
    s->applied[1] = cfg->mode_ist[1];
    s->mode_valid = 1;
}

void v4_state_put_mousecfg(v4_state_t *s, const v4p_mousecfg_t *cfg)
{
    if (s == NULL || cfg == NULL) {
        return;
    }
    s->mousecfg       = *cfg;
    s->mousecfg_valid = 1;
}

void v4_state_put_duo(v4_state_t *s, uint8_t duo)
{
    if (s == NULL) {
        return;
    }
    s->duo       = duo;
    s->duo_valid = 1;
}

void v4_state_put_devport(v4_state_t *s, const uint8_t bda[V4P_BDA_LEN],
                          uint8_t port)
{
    uint8_t i;
    int     frei = -1;

    if (s == NULL || bda == NULL || port >= V4P_PORT_COUNT) {
        return;
    }
    for (i = 0u; i < s->devport_n; i++) {
        uint8_t k;
        int     gleich = 1;

        for (k = 0u; k < V4P_BDA_LEN; k++) {
            if (s->devport_bda[i][k] != bda[k]) {
                gleich = 0;
                break;
            }
        }
        if (gleich != 0) {
            s->devport_port[i] = port;      /* Zuordnung ersetzt */
            return;
        }
    }
    if (s->devport_n < (uint8_t)V4P_MAX_DEVICES) {
        frei = (int)s->devport_n;
        s->devport_n++;
    } else {
        frei = 0;                           /* aeltesten Eintrag ersetzen */
    }
    for (i = 0u; i < V4P_BDA_LEN; i++) {
        s->devport_bda[frei][i] = bda[i];
    }
    s->devport_port[frei] = port;
}

uint8_t v4_state_devport(const v4_state_t *s, const uint8_t bda[V4P_BDA_LEN])
{
    uint8_t i;

    if (s == NULL || bda == NULL) {
        return 0xFFu;
    }
    for (i = 0u; i < s->devport_n; i++) {
        uint8_t k;
        int     gleich = 1;

        for (k = 0u; k < V4P_BDA_LEN; k++) {
            if (s->devport_bda[i][k] != bda[k]) {
                gleich = 0;
                break;
            }
        }
        if (gleich != 0) {
            return s->devport_port[i];
        }
    }
    return 0xFFu;                           /* in dieser Sitzung nicht gesetzt */
}

uint8_t v4_state_devport_at(const v4_state_t *s, uint8_t i,
                            uint8_t bda[V4P_BDA_LEN])
{
    uint8_t k;

    if (s == NULL || i >= s->devport_n) {
        return 0xFFu;
    }
    if (bda != NULL) {
        for (k = 0u; k < V4P_BDA_LEN; k++) {
            bda[k] = s->devport_bda[i][k];
        }
    }
    return s->devport_port[i];
}
