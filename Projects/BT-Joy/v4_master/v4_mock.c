/*
 * v4_mock.c -- ESP32-Emulator fuer den Linux-Harness (§14.1).
 *
 * Wird anstelle von v4_linux_i2c.c gelinkt: derselbe Satz Plattformhaken,
 * dieselben Aufbau- und Prueffunktionen aus v4_proto.c. Was hier steht, ist die
 * Slave-Seite des Vertrags -- inklusive der unangenehmen Faelle.
 *
 * Der Mock wartet NICHT wirklich: v4_plat_delay_us zaehlt nur mit. Damit laufen
 * die Testmatrix und die Demo in Sekundenbruchteilen statt in Minuten. Die
 * Wartezeiten selbst sind in v4_master.c festgelegt und werden hier nicht
 * geprueft.
 *
 * Aufbau: der Zustand ist bewusst flach und global -- der Mock ist ein
 * Testgeraet, kein Bibliotheksobjekt. Was er kann, steht in v4_mock.h; was er
 * absichtlich NICHT kann (gelesene Pegel), steht dort ebenfalls.
 */

#include "v4_mock.h"
#include "v4_master.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/* Zustand                                                             */
/* ------------------------------------------------------------------ */

#define MOCK_MAX_DEV        16
#define MOCK_NAME_STORE     (V4P_NAME_BUF)
/* So viele Geraete meldet der ESP32 in GET_INFO.max_devices
 * (BT_MGR_MAX_DEVICES in bt_manager.h). Das ist eine Faehigkeitsaussage und
 * NICHT die aktuell gefundene Anzahl -- die steht in GET_STATUS.dev_count. */
#define MOCK_MAX_DEV_REPORT 8u

typedef struct {
    char    name[MOCK_NAME_STORE];
    uint8_t bda[6];
    int     used;
    uint8_t port;               /* 0/1 oder V4P_NO_INDEX = keine Zuordnung */
} mock_dev_t;

v4_mock_knobs_t v4_mock;

static mock_dev_t s_dev[MOCK_MAX_DEV];
static int        s_dev_count;

static uint8_t  s_state;            /* V4P_STATE_*                      */
static uint8_t  s_conn_index;       /* 0xFF = keiner                    */
static uint8_t  s_duo = V4P_DUO_OFF;/* duo-Zustand (fw_ver 6)           */
static int      s_connect_left;     /* CONNECTING-Runden bis CONNECTED  */
static int      s_forgotten;        /* FORGET: NVS-Adresse geloescht    */

static uint8_t  s_scan_active;
static uint16_t s_scan_gen;
static unsigned s_scan_polls;       /* DEV_COUNT-Aufrufe seit SCAN_START */

static uint8_t  s_port_mode[V4P_PORT_COUNT];
static uint8_t  s_port_state[V4P_PORT_COUNT];   /* Zug-Maske, 1 = GND  */
static uint8_t  s_port_level[V4P_PORT_COUNT];   /* Pegel,     1 = High */
static uint32_t s_steps_x;
static uint32_t s_steps_y;

/* Eingangsverteilung (fw_ver 4) -- wie sie im NVS des ESP32 liegt:
 * s_mode_soll ist die VORGABE (V4P_MODE_AUTO = ungesetzt), s_dir/s_btn sind
 * die wirksamen Quellen. s_port_mode[] oben ist die WIRKSAME Betriebsart. */
static uint8_t  s_mode_soll[V4P_PORT_COUNT];
static uint8_t  s_dir[V4P_PORT_COUNT];
static uint8_t  s_btn[V4P_PORT_COUNT][V4P_BTN_COUNT];

/* Maus-Einstellungen (fw_ver 5) je Buchse -- wie sie im NVS des ESP32 liegen.
 * Vorgaben der Firmware: Tempo 7, Totzone 12 %. */
static uint8_t  s_mouse_speed[V4P_PORT_COUNT];
static uint8_t  s_mouse_totzone[V4P_PORT_COUNT];

static v4p_hid_t s_hid;

static uint8_t  s_ans[V4P_READ_FRAME_LEN];
static size_t   s_ans_len;
static int      s_ans_valid;
static unsigned long s_answer_no;   /* Nummer der erzeugten Antwort    */
static unsigned long s_cmd_no;      /* Nummer des Befehls (busy_every) */

/* ------------------------------------------------------------------ */
/* Antwortpuffer                                                       */
/* ------------------------------------------------------------------ */

/* Verrauscht eine fertige Antwort nach Vorgabe der Stoerknoepfe. Die drei
 * Fehlerbilder sind bewusst verschieden:
 *   bad_crc       -> CRC stimmt nicht (Master muss verwerfen)
 *   no_seq_echo   -> CRC stimmt, SEQ passt nicht (prueft genau diesen Pfad)
 *   bad_frame_len -> len-Feld unzulaessig (CRC stimmt)                     */
static void mock_noise_read(uint8_t *f)
{
    int crc_bad = 0;

    if (v4_mock.bad_crc_once) {
        v4_mock.bad_crc_once = 0;
        f[V4P_READ_OFF_CRC] = (uint8_t)(f[V4P_READ_OFF_CRC] ^ 0xFFu);
        crc_bad = 1;
    } else if (v4_mock.bad_crc_every > 0
        && (s_answer_no % (unsigned long)v4_mock.bad_crc_every) == 0u) {
        f[V4P_READ_OFF_CRC] = (uint8_t)(f[V4P_READ_OFF_CRC] ^ 0xFFu);
        crc_bad = 1;                /* dieser Rahmen ist absichtlich kaputt */
    }
    if (v4_mock.no_seq_once) {
        v4_mock.no_seq_once = 0;
        f[2] = (uint8_t)(f[2] + 1u);
        if (!crc_bad) {
            f[V4P_READ_OFF_CRC] = v4p_crc8(f, V4P_READ_CRC_RANGE);
        }
    } else if (v4_mock.no_seq_echo_every > 0
        && (s_answer_no % (unsigned long)v4_mock.no_seq_echo_every) == 0u) {
        f[2] = (uint8_t)(f[2] + 1u);
        if (!crc_bad) {
            f[V4P_READ_OFF_CRC] = v4p_crc8(f, V4P_READ_CRC_RANGE);
        }
    }
    if (v4_mock.bad_frame_len_once) {
        v4_mock.bad_frame_len_once = 0;
        f[4] = (uint8_t)(V4P_READ_PAYLOAD_MAX + 1u);    /* 122: unzulaessig */
        if (!crc_bad) {
            f[V4P_READ_OFF_CRC] = v4p_crc8(f, V4P_READ_CRC_RANGE);
        }
    } else if (v4_mock.bad_frame_len_every > 0
        && (s_answer_no % (unsigned long)v4_mock.bad_frame_len_every) == 0u) {
        f[4] = (uint8_t)(V4P_READ_PAYLOAD_MAX + 1u);    /* 122: unzulaessig */
        if (!crc_bad) {
            f[V4P_READ_OFF_CRC] = v4p_crc8(f, V4P_READ_CRC_RANGE);
        }
    }
}

static void mock_answer(uint8_t cmd, uint8_t seq, uint8_t status,
                        uint8_t flags, const uint8_t *payload, uint8_t len)
{
    s_answer_no++;
    if (v4p_build_read(s_ans, cmd, seq, status, flags, payload, len) != 0) {
        s_ans_valid = 0;
        return;
    }
    s_ans_len   = V4P_READ_FRAME_LEN;
    s_ans_valid = 1;
    mock_noise_read(s_ans);
}

static void mock_answer_empty(uint8_t cmd, uint8_t seq, uint8_t status)
{
    mock_answer(cmd, seq, status, 0u, NULL, 0u);
}

/* ------------------------------------------------------------------ */
/* Nutzlasten                                                          */
/* ------------------------------------------------------------------ */

static void mock_fill_info(v4p_info_t *in)
{
    memset(in, 0, sizeof(*in));
    in->proto_ver        = (uint8_t)V4P_PROTO_VER;
    /* fw_ver 4 = Eingangsverteilung, fw_ver 5 = Maus-Einstellungen. Die
     * Stoerknoepfe legacy_fw3/legacy_fw4 stellen die aelteren Fassungen nach --
     * genau die Firmware, die der Master weiterhin annehmen muss (gleicher
     * Rahmen, gleiche proto_ver). */
    if (v4_mock.legacy_fw3) {
        in->fw_ver = 3u;
    } else if (v4_mock.legacy_fw4) {
        in->fw_ver = 4u;
    } else if (v4_mock.legacy_fw5) {
        in->fw_ver = 5u;
    } else {
        in->fw_ver = (uint8_t)V4P_FW_VER;
    }
    in->write_frame_len  = (uint8_t)V4P_WRITE_FRAME_LEN;
    in->read_frame_len   = (uint8_t)V4P_READ_FRAME_LEN;
    /* 0 = "kein Dateiweg". Das ist eine Faehigkeitsaussage, kein Platzhalter:
     * der Master erkennt daran, dass es FILE_READ hier nicht gibt. */
    in->bulk_payload_max = 0u;
    in->chunk            = 128u;    /* reserviert, wie auf dem ESP32 */
    in->max_devices      = (uint8_t)MOCK_MAX_DEV_REPORT;
    in->path_max         = 128u;    /* reserviert (kein Dateiweg)    */
    /* Genau die Faehigkeiten, die diese Firmware hat -- dieselbe Rechnung wie
     * capability_bits() in v4_link.c der ESP32-Seite. */
    in->caps             = (uint16_t)(V4P_CAP_PORT_MODE | V4P_CAP_GET_INPUT
                                      | V4P_CAP_GET_HID  | V4P_CAP_HID_BT
                                      | V4P_CAP_HID_BLE  | V4P_CAP_MOUSE
                                      | V4P_CAP_PAD      | V4P_CAP_BATTERY);
    if (!v4_mock.legacy_fw3) {
        in->caps = (uint16_t)(in->caps | V4P_CAP_PORT_ROUTING
                              | V4P_CAP_DEVICE_PORT);
    }
    /* Maus-Einstellungen gibt es erst mit fw_ver 5 -- legacy_fw4 meldet die
     * Eingangsverteilung, aber OHNE dieses Bit. */
    if (!v4_mock.legacy_fw3 && !v4_mock.legacy_fw4) {
        in->caps = (uint16_t)(in->caps | V4P_CAP_MOUSE_CFG);
    }
    /* duo und das Verbunden-Byte in DEV_GET gibt es erst mit fw_ver 6. */
    if (!v4_mock.legacy_fw3 && !v4_mock.legacy_fw4 && !v4_mock.legacy_fw5) {
        in->caps = (uint16_t)(in->caps | V4P_CAP_DUO | V4P_CAP_DEV_STATE);
    }
}

static void mock_fill_status(v4p_status_t *st)
{
    memset(st, 0, sizeof(*st));
    st->state       = s_state;
    st->conn_index  = s_conn_index;
    st->dev_count   = (uint8_t)s_dev_count;
    st->scan_active = s_scan_active;
    /* SD-/Audiofelder bleiben 0 -- so wie der ESP32 dieses Projekts sie
     * schickt (fill_status() in v4_link.c). */
    st->scan_gen    = s_scan_gen;
    st->chunk       = 128u;
    st->proto_ver   = (uint8_t)V4P_PROTO_VER;
    st->fw_ver      = (uint8_t)V4P_FW_VER;
}

static void mock_fill_input(v4p_input_t *in)
{
    memset(in, 0, sizeof(*in));
    in->state[0] = s_port_state[0];
    in->state[1] = s_port_state[1];
    in->steps_x  = s_steps_x;
    in->steps_y  = s_steps_y;
    /* Der Mock liefert die Pegel als eigenen Zustand. Im Regelfall hat
     * set_port_state sie auf "losgelassen = High" gesetzt; set_port_level
     * kann sie danach absichtlich verstellen (fehlender Pull-up). */
    in->level[0]    = s_port_level[0];
    in->level[1]    = s_port_level[1];
    in->level_valid = 1u;
}

/*
 * Der Verbindungsaufbau laeuft asynchron: CONNECT ist angenommen, die
 * Verbindung steht erst danach. Der Mock bildet das ueber die Zahl der
 * GET_STATUS-Aufrufe nach (connect_rounds), damit der Wartepfad im Master
 * (v4_wait_state) wirklich durchlaufen wird.
 */
static void mock_advance_connect(void)
{
    if (s_state != (uint8_t)V4P_STATE_CONNECTING) {
        return;
    }
    if (s_connect_left > 0) {
        s_connect_left--;
        return;
    }
    s_state = (uint8_t)V4P_STATE_CONNECTED;
}

/* ------------------------------------------------------------------ */
/* Befehlsverteilung                                                   */
/* ------------------------------------------------------------------ */

/* 1 = diese Antwort wird BUSY, ohne den Befehl auszufuehren. */
static int mock_busy_now(void)
{
    if (v4_mock.never_ready) {
        return 1;
    }
    if (v4_mock.busy_count > 0) {
        v4_mock.busy_count--;
        return 1;
    }
    if (v4_mock.busy_every > 0
        && (s_cmd_no % (unsigned long)v4_mock.busy_every) == 0u) {
        return 1;
    }
    return 0;
}

static uint8_t mock_connect_index(uint8_t idx)
{
    if ((int)idx >= s_dev_count) {
        return V4P_ST_NOT_FOUND;
    }
    s_state        = (uint8_t)V4P_STATE_CONNECTING;
    s_conn_index   = idx;
    s_connect_left = v4_mock.connect_rounds;
    return V4P_ST_OK;
}

static void mock_dispatch(uint8_t cmd, uint8_t seq, const uint8_t *pay,
                          uint8_t plen)
{
    int busy;

    v4_mock.last_cmd = (int)cmd;
    s_cmd_no++;

    if (v4_mock.force_status >= 0) {
        /* Einmalig erzwungener Status -- damit laesst sich jeder Fehlerzweig
         * des Masters ohne Umbau ansteuern. */
        uint8_t st = (uint8_t)v4_mock.force_status;

        v4_mock.force_status = -1;
        mock_answer_empty(cmd, seq, st);
        return;
    }

    busy = mock_busy_now();
    if (busy) {
        /* BUSY heisst: Antwort erzeugt, Arbeit nicht fertig. Der Befehl wird
         * NICHT ausgefuehrt -- der Master schickt ihn mit gleicher SEQ erneut. */
        mock_answer_empty(cmd, seq, V4P_ST_BUSY);
        return;
    }

    switch (cmd) {
    case V4P_CMD_PING:
    case V4P_CMD_GET_INFO: {
        v4p_info_t info;

        mock_fill_info(&info);
        {
            uint8_t p[V4P_INFO_PAYLOAD_LEN];

            (void)v4p_enc_get_info(p, &info);
            mock_answer(cmd, seq, V4P_ST_OK, V4P_FLAG_NONE, p,
                        (uint8_t)V4P_INFO_PAYLOAD_LEN);
        }
        return;
    }

    case V4P_CMD_GET_STATUS: {
        v4p_status_t st;
        uint8_t      p[V4P_ST_LEN];

        mock_advance_connect();
        mock_fill_status(&st);
        (void)v4p_enc_get_status(p, &st);
        mock_answer(cmd, seq, V4P_ST_OK, V4P_FLAG_NONE, p,
                    (uint8_t)V4P_ST_LEN);
        return;
    }

    case V4P_CMD_SET_PORT_MODE: {
        uint8_t port;
        uint8_t mode;
        uint8_t p[V4P_SPM_PAYLOAD_LEN];

        if (plen < (uint8_t)V4P_SPM_PAYLOAD_LEN) {
            mock_answer_empty(cmd, seq, V4P_ST_BAD_ARG);
            return;
        }
        port = pay[V4P_SPM_OFF_PORT];
        mode = pay[V4P_SPM_OFF_MODE];
        if (port >= V4P_PORT_COUNT || !v4p_mode_soll_valid(mode)) {
            mock_answer_empty(cmd, seq, V4P_ST_BAD_ARG);
            return;
        }
        /* V4P_MODE_AUTO ("nach Geraeteart") gibt es erst mit fw_ver 4. Die
         * aeltere Fassung lehnt ihn mit BAD_ARG ab -- genau das muss der
         * Master aushalten, ohne seine Anzeige zu verdrehen. */
        if (v4_mock.legacy_fw3 && mode == (uint8_t)V4P_MODE_AUTO) {
            mock_answer_empty(cmd, seq, V4P_ST_BAD_ARG);
            return;
        }
        if (mode == (uint8_t)V4P_MODE_AUTO) {
            /* "nach Geraeteart": nur die VORGABE wird geloescht, die wirksame
             * Betriebsart bleibt stehen (so macht es der ESP32). Die Bestaetigung
             * ist das Echo mit 0xFF. */
            s_mode_soll[port] = (uint8_t)V4P_MODE_AUTO;
        } else {
            s_port_mode[port] = mode;
            s_mode_soll[port] = mode;
        }

        (void)v4p_enc_set_port_mode(p, port, mode);
        if (v4_mock.bad_set_port_echo) {
            /* Stoerknopf: die Bestaetigung passt nicht zur Anforderung. */
            v4_mock.bad_set_port_echo = 0;
            p[V4P_SPM_OFF_MODE] = v4p_port_mode_next(mode);
        }
        mock_answer(cmd, seq, V4P_ST_OK, V4P_FLAG_NONE, p,
                    (uint8_t)V4P_SPM_PAYLOAD_LEN);
        return;
    }

    case V4P_CMD_GET_PORT_MODE: {
        uint8_t p[V4P_GPM_PAYLOAD_LEN];

        (void)v4p_enc_get_port_mode(p, s_port_mode);
        mock_answer(cmd, seq, V4P_ST_OK, V4P_FLAG_NONE, p,
                    (uint8_t)V4P_GPM_PAYLOAD_LEN);
        return;
    }

    /*
     * Eingangsverteilung (fw_ver 4). Die drei Befehle sind bei legacy_fw3
     * NICHT vorhanden -- dann antwortet der Mock wie eine alte Firmware mit
     * BAD_CMD und der Master darf sie nicht als Bestaetigung deuten.
     */
    case V4P_CMD_GET_PORT_CONFIG: {
        v4p_portcfg_t c;
        uint8_t       p[V4P_GPC_PAYLOAD_LEN];

        if (v4_mock.legacy_fw3) {
            mock_answer_empty(cmd, seq, V4P_ST_BAD_CMD);
            return;
        }
        memset(&c, 0, sizeof(c));
        c.mode_soll[0] = s_mode_soll[0];
        c.mode_soll[1] = s_mode_soll[1];
        c.mode_ist[0]  = s_port_mode[0];
        c.mode_ist[1]  = s_port_mode[1];
        c.dir[0]       = s_dir[0];
        c.dir[1]       = s_dir[1];
        {
            int port;

            for (port = 0; port < (int)V4P_PORT_COUNT; port++) {
                int k;

                for (k = 0; k < (int)V4P_BTN_COUNT; k++) {
                    c.btn[port][k] = s_btn[port][k];
                }
            }
        }
        (void)v4p_enc_get_port_config(p, &c);
        mock_answer(cmd, seq, V4P_ST_OK, V4P_FLAG_NONE, p,
                    (uint8_t)V4P_GPC_PAYLOAD_LEN);
        return;
    }

    case V4P_CMD_SET_PORT_ROUTING: {
        uint8_t port;
        uint8_t dir;
        uint8_t btn[V4P_BTN_COUNT];
        uint8_t p[V4P_SPR_PAYLOAD_LEN];

        if (v4_mock.legacy_fw3) {
            mock_answer_empty(cmd, seq, V4P_ST_BAD_CMD);
            return;
        }
        if (plen < (uint8_t)V4P_SPR_PAYLOAD_LEN) {
            mock_answer_empty(cmd, seq, V4P_ST_BAD_ARG);
            return;
        }
        port   = pay[V4P_SPR_OFF_PORT];
        dir    = pay[V4P_SPR_OFF_DIR];
        btn[0] = pay[V4P_SPR_OFF_BTN1];
        btn[1] = pay[V4P_SPR_OFF_BTN2];
        btn[2] = pay[V4P_SPR_OFF_BTN3];

        /* ERST pruefen, DANN uebernehmen: ein abgelehnter Wert darf die
         * laufende Ausgabe nicht anfassen. Genau so macht es der ESP32
         * (v4_settings_set_routing vor joy_port_set_*). */
        if (port >= V4P_PORT_COUNT || !v4p_dir_valid(dir)
            || !v4p_btn_set_valid(btn)) {
            mock_answer_empty(cmd, seq, V4P_ST_BAD_ARG);
            return;
        }
        s_dir[port]    = dir;
        s_btn[port][0] = btn[0];
        s_btn[port][1] = btn[1];
        s_btn[port][2] = btn[2];

        (void)v4p_enc_set_port_routing(p, port, dir, btn);
        if (v4_mock.bad_routing_echo) {
            v4_mock.bad_routing_echo = 0;
            p[V4P_SPR_OFF_DIR] = (uint8_t)(dir ^ (uint8_t)V4P_DIR_DPAD);
        }
        mock_answer(cmd, seq, V4P_ST_OK, V4P_FLAG_NONE, p,
                    (uint8_t)V4P_SPR_PAYLOAD_LEN);
        return;
    }

    case V4P_CMD_SET_DEVICE_PORT: {
        uint8_t port;
        uint8_t p[V4P_SDP_PAYLOAD_LEN];
        int     i;

        if (v4_mock.legacy_fw3) {
            mock_answer_empty(cmd, seq, V4P_ST_BAD_CMD);
            return;
        }
        if (plen < (uint8_t)V4P_SDP_PAYLOAD_LEN) {
            mock_answer_empty(cmd, seq, V4P_ST_BAD_ARG);
            return;
        }
        port = pay[V4P_SDP_OFF_PORT];
        if (port >= V4P_PORT_COUNT) {
            mock_answer_empty(cmd, seq, V4P_ST_BAD_ARG);
            return;
        }
        /* Die Zuordnung liegt am Geraet; eine unbekannte Adresse wird trotzdem
         * gespeichert (sie kann beim naechsten Auftreten greifen) -- wie beim
         * ESP32. */
        for (i = 0; i < s_dev_count; i++) {
            if (memcmp(s_dev[i].bda, &pay[V4P_SDP_OFF_BDA], 6u) == 0) {
                s_dev[i].port = port;
                break;
            }
        }
        memcpy(p, pay, (size_t)V4P_SDP_PAYLOAD_LEN);
        mock_answer(cmd, seq, V4P_ST_OK, V4P_FLAG_NONE, p,
                    (uint8_t)V4P_SDP_PAYLOAD_LEN);
        return;
    }

    /*
     * Maus-Einstellungen (fw_ver 5). Bei legacy_fw3/legacy_fw4 sind sie NICHT
     * vorhanden -- dann antwortet der Mock wie eine alte Firmware mit BAD_CMD,
     * und der Master darf sie nicht als Bestaetigung deuten.
     */
    case V4P_CMD_GET_MOUSE_CFG: {
        v4p_mousecfg_t c;
        uint8_t        p[V4P_MCFG_PAYLOAD_LEN];

        if (v4_mock.legacy_fw3 || v4_mock.legacy_fw4) {
            mock_answer_empty(cmd, seq, V4P_ST_BAD_CMD);
            return;
        }
        memset(&c, 0, sizeof(c));
        c.speed[0]   = s_mouse_speed[0];
        c.totzone[0] = s_mouse_totzone[0];
        c.speed[1]   = s_mouse_speed[1];
        c.totzone[1] = s_mouse_totzone[1];
        (void)v4p_enc_get_mouse_cfg(p, &c);
        mock_answer(cmd, seq, V4P_ST_OK, V4P_FLAG_NONE, p,
                    (uint8_t)V4P_MCFG_PAYLOAD_LEN);
        return;
    }

    case V4P_CMD_SET_MOUSE_CFG: {
        uint8_t port;
        uint8_t speed;
        uint8_t totzone;
        uint8_t p[V4P_SMC_PAYLOAD_LEN];
        v4p_mousecfg_t c;

        if (v4_mock.legacy_fw3 || v4_mock.legacy_fw4) {
            mock_answer_empty(cmd, seq, V4P_ST_BAD_CMD);
            return;
        }
        if (plen < (uint8_t)V4P_SMC_PAYLOAD_LEN) {
            mock_answer_empty(cmd, seq, V4P_ST_BAD_ARG);
            return;
        }
        port    = pay[V4P_SMC_OFF_PORT];
        speed   = pay[V4P_SMC_OFF_SPEED];
        totzone = pay[V4P_SMC_OFF_TOTZONE];
        /* ERST pruefen, DANN uebernehmen: ein abgelehnter Wert darf die
         * laufende Einstellung nicht anfassen. */
        if (port >= V4P_PORT_COUNT || !v4p_mouse_speed_valid(speed)
            || !v4p_mouse_totzone_valid(totzone)) {
            mock_answer_empty(cmd, seq, V4P_ST_BAD_ARG);
            return;
        }
        s_mouse_speed[port]   = speed;
        s_mouse_totzone[port] = totzone;

        memset(&c, 0, sizeof(c));
        c.speed[0]   = s_mouse_speed[0];
        c.speed[1]   = s_mouse_speed[1];
        c.totzone[0] = s_mouse_totzone[0];
        c.totzone[1] = s_mouse_totzone[1];
        (void)v4p_enc_set_mouse_cfg(p, port, &c);
        if (v4_mock.bad_mousecfg_echo) {
            v4_mock.bad_mousecfg_echo = 0;
            /* Stoerknopf: die Bestaetigung passt nicht zur Anforderung. */
            p[V4P_SMC_OFF_SPEED] =
                (uint8_t)((speed < (uint8_t)V4P_MOUSE_SPEED_MAX)
                              ? (speed + 1u)
                              : (speed - 1u));
        }
        mock_answer(cmd, seq, V4P_ST_OK, V4P_FLAG_NONE, p,
                    (uint8_t)V4P_SMC_PAYLOAD_LEN);
        return;
    }

    case V4P_CMD_GET_INPUT: {
        v4p_input_t in;
        uint8_t     p[V4P_IN_PAYLOAD_LEN];
        uint8_t     plen = (uint8_t)V4P_IN_PAYLOAD_LEN;

        /* Zaehler fuer die Pruefung, dass eine Aufruffolge OHNE
         * Controller-Eingabewerte auskommt (die MUI-Oberflaeche fragt
         * GET_INPUT nicht mehr ab). */
        v4_mock.input_reads++;
        mock_fill_input(&in);
        (void)v4p_enc_get_input(p, &in);

        /*
         * Rueckfallweg: der Stoerknopf stellt die aeltere 10-Byte-Fassung ohne
         * Pegel nach. Die Antwort ist dann absichtlich kuerzer -- genau daran
         * erkennt der Master die Version.
         */
        if (v4_mock.legacy_input10) {
            plen = (uint8_t)V4P_IN_PAYLOAD_LEN_NO_LEVEL;
        }
        mock_answer(cmd, seq, V4P_ST_OK, V4P_FLAG_NONE, p, plen);
        return;
    }

    case V4P_CMD_GET_HID: {
        uint8_t p[V4P_HID_PAYLOAD_MAX];
        size_t  n;

        n = v4p_enc_get_hid(p, &s_hid);
        mock_answer(cmd, seq, V4P_ST_OK, V4P_FLAG_NONE, p, (uint8_t)n);
        return;
    }

    case V4P_CMD_SCAN_START:
        if (plen < 1u) {
            mock_answer_empty(cmd, seq, V4P_ST_BAD_ARG);
            return;
        }
        s_scan_active = 1u;
        s_scan_polls  = 0u;
        s_scan_gen    = (uint16_t)(s_scan_gen + 1u);
        mock_answer_empty(cmd, seq, V4P_ST_OK);
        return;

    case V4P_CMD_SCAN_STOP:
        s_scan_active = 0u;
        mock_answer_empty(cmd, seq, V4P_ST_OK);
        return;

    case V4P_CMD_DEV_COUNT: {
        uint8_t p[1];
        int     count = s_dev_count;

        /* Der Scan laeuft auf dem ESP32 asynchron: unmittelbar nach
         * SCAN_START ist die Liste oft noch leer. scan_delay bildet das nach. */
        if (v4_mock.scan_delay > 0
            && (int)s_scan_polls < v4_mock.scan_delay) {
            s_scan_polls++;
            count = 0;
        }
        p[0] = (uint8_t)count;
        mock_answer(cmd, seq, V4P_ST_OK, V4P_FLAG_NONE, p, 1u);
        return;
    }

    case V4P_CMD_DEV_GET: {
        uint8_t    p[V4P_DEV_OFF_NAME + V4P_DEVNAME_MAX];
        v4p_dev_t  dev;
        uint8_t    idx;

        if (plen < 1u) {
            mock_answer_empty(cmd, seq, V4P_ST_BAD_ARG);
            return;
        }
        idx = pay[0];
        if ((int)idx >= s_dev_count) {
            mock_answer_empty(cmd, seq, V4P_ST_NOT_FOUND);
            return;
        }
        memset(&dev, 0, sizeof(dev));
        dev.idx      = idx;
        {
            uint8_t i;

            for (i = 0; i < 6u; i++) {
                dev.bda[i] = s_dev[idx].bda[i];
            }
        }
        {
            size_t n = strlen(s_dev[idx].name);

            if (n > V4P_DEVNAME_MAX) {
                n = V4P_DEVNAME_MAX;
            }
            dev.name_len = (uint8_t)n;
            {
                size_t i;

                for (i = 0; i < n; i++) {
                    dev.name[i] = s_dev[idx].name[i];
                }
            }
            dev.name[dev.name_len] = '\0';
        }
        /*
         * Layout: OHNE V4P_CAP_DEV_STATE (legacy_fw3/4/5) das ALTE (name_len
         * bei 7), sonst das neue mit dem Verbunden-Byte. Genau die
         * Unterscheidung, die der Master treffen muss.
         */
        dev.verbunden = (s_conn_index == idx) ? 1 : 0;
        {
            size_t n;

            if (v4_mock.legacy_fw3 || v4_mock.legacy_fw4 || v4_mock.legacy_fw5) {
                n = v4p_enc_dev_alt(p, &dev);
            } else {
                n = v4p_enc_dev(p, &dev);
            }
            mock_answer(cmd, seq, V4P_ST_OK, V4P_FLAG_NONE, p, (uint8_t)n);
        }
        return;
    }

    /*
     * duo (fw_ver 6). Bei legacy_fw3/4/5 gibt es die Befehle nicht -- sie
     * antworten BAD_CMD, wie die aeltere Firmware.
     */
    case V4P_CMD_GET_DUO: {
        uint8_t p[V4P_DUO_PAYLOAD_LEN];

        if (v4_mock.legacy_fw3 || v4_mock.legacy_fw4 || v4_mock.legacy_fw5) {
            mock_answer_empty(cmd, seq, V4P_ST_BAD_CMD);
            return;
        }
        p[V4P_DUO_OFF_STATE] = s_duo;
        mock_answer(cmd, seq, V4P_ST_OK, V4P_FLAG_NONE, p,
                    (uint8_t)V4P_DUO_PAYLOAD_LEN);
        return;
    }

    case V4P_CMD_SET_DUO: {
        uint8_t p[V4P_DUO_PAYLOAD_LEN];
        uint8_t duo;

        if (v4_mock.legacy_fw3 || v4_mock.legacy_fw4 || v4_mock.legacy_fw5) {
            mock_answer_empty(cmd, seq, V4P_ST_BAD_CMD);
            return;
        }
        if (plen < V4P_DUO_PAYLOAD_LEN) {
            mock_answer_empty(cmd, seq, V4P_ST_BAD_ARG);
            return;
        }
        duo = pay[V4P_DUO_OFF_STATE];
        if (!v4p_duo_valid(duo)) {
            /* Die alte Einstellung bleibt -- wie auf dem ESP32. */
            mock_answer_empty(cmd, seq, V4P_ST_BAD_ARG);
            return;
        }
        s_duo = duo;
        if (v4_mock.bad_duo_echo) {
            v4_mock.bad_duo_echo = 0;
            duo = (uint8_t)((duo == V4P_DUO_MOUSE_PORT1) ? V4P_DUO_MOUSE_PORT0
                                                         : V4P_DUO_MOUSE_PORT1);
        }
        p[V4P_DUO_OFF_STATE] = duo;
        mock_answer(cmd, seq, V4P_ST_OK, V4P_FLAG_NONE, p,
                    (uint8_t)V4P_DUO_PAYLOAD_LEN);
        return;
    }

    case V4P_CMD_CONNECT: {
        uint8_t rc;

        if (plen < 1u) {
            mock_answer_empty(cmd, seq, V4P_ST_BAD_ARG);
            return;
        }
        rc = mock_connect_index(pay[0]);
        mock_answer_empty(cmd, seq, rc);
        return;
    }

    case V4P_CMD_CONNECT_BDA: {
        int i;
        int found = -1;

        if (plen < 6u) {
            mock_answer_empty(cmd, seq, V4P_ST_BAD_ARG);
            return;
        }
        for (i = 0; i < s_dev_count; i++) {
            if (memcmp(s_dev[i].bda, pay, 6u) == 0) {
                found = i;
                break;
            }
        }
        if (found < 0) {
            /* So antwortet auch der ESP32: die Adresse steht nicht in der
             * Tabelle -> BAD_ARG (bt_mgr_connect_bda schlaegt fehl). */
            mock_answer_empty(cmd, seq, V4P_ST_BAD_ARG);
            return;
        }
        mock_answer_empty(cmd, seq,
                          mock_connect_index((uint8_t)found));
        return;
    }

    case V4P_CMD_DISCONNECT:
        s_state        = (uint8_t)V4P_STATE_IDLE;
        s_conn_index   = V4P_NO_INDEX;
        s_connect_left = 0;
        mock_answer_empty(cmd, seq, V4P_ST_OK);
        return;

    case V4P_CMD_FORGET:
        /* NVS-Adresse geloescht. Die laufende Verbindung bleibt bestehen --
         * das ist die Zusage der ESP32-Seite (bt_mgr_forget). */
        s_forgotten = 1;
        mock_answer_empty(cmd, seq, V4P_ST_OK);
        return;

    case V4P_CMD_RESET:
        s_port_mode[0]  = (uint8_t)V4P_PORT_OFF;
        s_port_mode[1]  = (uint8_t)V4P_PORT_OFF;
        s_port_state[0] = 0u;
        s_port_state[1] = 0u;
        /* Freigegeben ist losgelassen: bei wirkendem Pull-up also High. */
        s_port_level[0] = (uint8_t)V4P_IN_ST_MASK;
        s_port_level[1] = (uint8_t)V4P_IN_ST_MASK;
        mock_answer_empty(cmd, seq, V4P_ST_OK);
        return;

    default:
        /* Unbekannte Codes -- auch die des Audio-Projekts (SD/Datei/EQ). */
        mock_answer_empty(cmd, seq, V4P_ST_BAD_CMD);
        return;
    }
}

/* ------------------------------------------------------------------ */
/* Plattformhaken                                                      */
/* ------------------------------------------------------------------ */

/*
 * Uhr des Mocks: ein reiner Zaehler, der bei jedem Buszugriff und bei jeder
 * Wartezeit weiterlaeuft. Damit hat die Dauer in den Debugzeilen im Demo-Lauf
 * eine plausible Groessenordnung (Schreiben ~0,25 ms, Lesen ~0,9 ms) -- der
 * Mock wartet ja nicht wirklich. Fuer die Tests ist nur wichtig, dass die Uhr
 * monoton steigt.
 */
static uint32_t s_now_us = 0u;

int v4_plat_open(const char *dev)
{
    (void)dev;
    return 0;
}

void v4_plat_close(void)
{
}

int v4_plat_i2c_write(uint8_t addr7, const uint8_t *data, uint16_t len)
{
    uint8_t cmd;
    uint8_t seq;

    s_now_us += 250u;
    if (v4_mock.fail_write) {
        return -1;
    }
    if (addr7 != V4_I2C_ADDR7 || len != (uint16_t)V4P_WRITE_FRAME_LEN) {
        v4_mock.bad_write_len++;
        return -1;
    }

    v4_mock.writes++;

    /* SEQ der abgesetzten Frames protokollieren: damit laesst sich pruefen,
     * dass Wiederholungen dieselbe SEQ benutzen (§6, Punkt 1). */
    if (v4_mock.seq_log_n < (int)sizeof(v4_mock.seq_log)) {
        v4_mock.seq_log[v4_mock.seq_log_n] = data[2];
        v4_mock.seq_log_n++;
    }

    /* Selbstheilung: beim Senden einer neuen Antwort wird der TX-FIFO
     * zurueckgesetzt, wenn noch ungelesene Daten darin liegen. */
    s_ans_valid = 0;

    if (data[0] != V4P_MAGIC_WRITE) {
        return 0;                   /* Slave erkennt den Frame nicht */
    }
    seq = data[2];
    cmd = data[1];

    if (data[31] != v4p_crc8(data, 31u)) {
        mock_answer_empty(cmd, seq, V4P_ST_BAD_CRC);
        return 0;
    }

    mock_dispatch(cmd, seq, data + 4, data[3]);
    return 0;
}

int v4_plat_i2c_read(uint8_t addr7, uint8_t *data, uint16_t len)
{
    size_t i;

    s_now_us += 900u;
    v4_mock.reads++;

    if (addr7 != V4_I2C_ADDR7) {
        return -1;
    }
    if (v4_mock.fail_read) {
        s_ans_valid = 0;
        return -1;
    }
    if (v4_mock.drop_next_read) {
        v4_mock.drop_next_read = 0;
        s_ans_valid = 0;
        return -1;                  /* genau diese Antwort geht verloren */
    }
    if (v4_mock.partial_read_once) {
        size_t half;

        v4_mock.partial_read_once = 0;
        half = (size_t)len / 2u;
        for (i = 0; i < half; i++) {
            data[i] = s_ans[i];
        }
        for (; i < len; i++) {
            data[i] = 0xFFu;        /* Rest fehlt */
        }
        /* s_ans_valid bleibt 1: die Antwort ist nur halb konsumiert. Genau
         * diese Lage loest der ESP32 beim naechsten Senden per FIFO-Reset
         * auf (Selbstheilung). */
        return 0;
    }
    if (v4_mock.drop_read_every > 0
        && (v4_mock.reads % (unsigned long)v4_mock.drop_read_every) == 0u) {
        s_ans_valid = 0;
        return -1;                  /* Antwort verloren */
    }

    if (!s_ans_valid) {
        /* Ohne Befehl hat der Slave nichts: der Bus liefert 0xFF. */
        for (i = 0; i < len; i++) {
            data[i] = 0xFFu;
        }
        return 0;
    }

    if ((size_t)len != s_ans_len) {
        /* Falsche Laenge verlangt -- hier gibt es keine guten Daten. Das ist
         * der Fall "Master und Slave sind sich ueber die Rahmengroesse uneins"
         * (64 statt 128). */
        s_ans_valid = 0;
        return -1;
    }

    for (i = 0; i < len; i++) {
        data[i] = s_ans[i];
    }
    s_ans_valid = 0;                /* R1: genau eine Antwort pro Befehl */
    return 0;
}

void v4_plat_delay_us(uint32_t us)
{
    v4_mock.delay_calls++;
    v4_mock.delay_us += (unsigned long)us;
    s_now_us += us;                     /* die Uhr laeuft mit der Wartezeit */
}

/* Mikrosekunden-Uhr (siehe v4_master.h). Ein Zaehler, der bei jedem
 * Buszugriff und jeder Wartezeit steigt -- monoton, aber ohne echten Bezug. */
uint32_t v4_plat_now_us(void)
{
    return s_now_us;
}

unsigned long v4_plat_last_error(void)
{
    /* Der Mock scheitert nur auf Anforderung; ein "Fehlercode" im Sinne der
     * i2c.library existiert hier nicht. */
    return 0ul;
}

const char *v4_plat_error_text(void)
{
    return "Mock";
}

/* Der Mock schreibt die Logdatei wie die Amiga-Seite: ein ausdruecklicher Pfad
 * (kein Standardpfad), MODE_NEWFILE-aequivalent "w". Damit prueft der
 * Demo-Lauf dieselbe Spiegelung wie auf der V4. */
static FILE *s_log = NULL;

int v4_plat_log_open(const char *path)
{
    if (path == NULL) {
        return -1;              /* kein Standardpfad im Mock */
    }
    s_log = fopen(path, "w");
    if (s_log == NULL) {
        return -1;
    }
    setvbuf(s_log, NULL, _IONBF, 0);
    return 0;
}

long v4_plat_log_write(const char *s, unsigned long len)
{
    size_t n;

    if (s_log == NULL || len == 0ul) {
        return 0;
    }
    n = fwrite(s, 1u, (size_t)len, s_log);
    fflush(s_log);
    return (long)n;
}

void v4_plat_log_settle(int ticks)
{
    (void)ticks;                        /* Dateisystem schreibt sofort */
}

void v4_plat_log_close(void)
{
    if (s_log != NULL) {
        fclose(s_log);
        s_log = NULL;
    }
}

/* ------------------------------------------------------------------ */
/* Aufbau                                                              */
/* ------------------------------------------------------------------ */

void v4_mock_bus_reset(void)
{
    s_ans_valid = 0;
    s_ans_len   = 0u;
}

void v4_mock_reset(void)
{
    memset(&v4_mock, 0, sizeof(v4_mock));
    v4_mock.force_status = -1;
    v4_mock.last_cmd     = -1;

    memset(s_dev, 0, sizeof(s_dev));
    s_dev_count    = 0;
    s_state        = (uint8_t)V4P_STATE_IDLE;
    s_conn_index   = V4P_NO_INDEX;
    s_connect_left = 0;
    s_forgotten    = 0;
    s_scan_active  = 0u;
    s_scan_gen     = 0u;
    s_scan_polls   = 0u;
    s_port_mode[0]  = (uint8_t)V4P_PORT_OFF;
    s_port_mode[1]  = (uint8_t)V4P_PORT_OFF;
    s_port_state[0] = 0u;
    s_port_state[1] = 0u;
    /* Ruhezustand der Pegel: alle sieben Leitungen High (0x7F) -- das ist der
     * Wert, den die Vampire mit ihrem Pull-up vorgibt. */
    s_port_level[0] = (uint8_t)V4P_IN_ST_MASK;
    s_port_level[1] = (uint8_t)V4P_IN_ST_MASK;
    s_steps_x      = 0u;
    s_steps_y      = 0u;

    /* Eingangsverteilung: die Vorgaben des ESP32. Ohne neue Einstellung
     * aendert sich damit nichts -- dir = alle drei Richtungsquellen,
     * Knopfquellen = die drei bisherigen Gruppen, Vorgabe = "nach
     * Geraeteart". */
    s_mode_soll[0] = (uint8_t)V4P_MODE_AUTO;
    s_mode_soll[1] = (uint8_t)V4P_MODE_AUTO;
    s_dir[0]       = (uint8_t)V4P_DIR_STANDARD;
    s_dir[1]       = (uint8_t)V4P_DIR_STANDARD;
    s_btn[0][0]    = (uint8_t)V4P_TASTE_GRUPPE1;
    s_btn[0][1]    = (uint8_t)V4P_TASTE_GRUPPE2;
    s_btn[0][2]    = (uint8_t)V4P_TASTE_GRUPPE3;
    s_btn[1][0]    = (uint8_t)V4P_TASTE_GRUPPE1;
    s_btn[1][1]    = (uint8_t)V4P_TASTE_GRUPPE2;
    s_btn[1][2]    = (uint8_t)V4P_TASTE_GRUPPE3;

    /* Maus-Einstellungen: die Vorgaben der Firmware -- Tempo 7, Totzone 12 %
     * auf beiden Buchsen. */
    s_mouse_speed[0]   = (uint8_t)V4P_MOUSE_SPEED_VORGABE;
    s_mouse_speed[1]   = (uint8_t)V4P_MOUSE_SPEED_VORGABE;
    s_mouse_totzone[0] = (uint8_t)V4P_MOUSE_TOTZONE_VORGABE;
    s_mouse_totzone[1] = (uint8_t)V4P_MOUSE_TOTZONE_VORGABE;

    memset(&s_hid, 0, sizeof(s_hid));
    s_hid.kind      = V4P_HID_KIND_NONE;
    s_hid.transport = V4P_HID_TR_BREDR;
    s_hid.battery   = V4P_HID_BATTERY_UNKNOWN;

    s_ans_valid = 0;
    s_ans_len   = 0u;
    s_answer_no = 0u;
    s_cmd_no    = 0u;
    s_now_us    = 0u;                   /* Uhr des Mocks zuruecksetzen */
}

uint8_t v4_mock_duo(void)
{
    return s_duo;
}

void v4_mock_set_duo(uint8_t duo)
{
    s_duo = v4p_duo_valid(duo) ? duo : (uint8_t)V4P_DUO_OFF;
}

int v4_mock_dev_add(const char *name, const uint8_t bda[6])
{
    size_t n;
    size_t i;

    if (name == NULL || bda == NULL || s_dev_count >= MOCK_MAX_DEV) {
        return -1;
    }
    n = strlen(name);
    if (n == 0u || n >= (size_t)MOCK_NAME_STORE) {
        return -1;
    }
    memset(&s_dev[s_dev_count], 0, sizeof(s_dev[s_dev_count]));
    for (i = 0; i < n; i++) {
        s_dev[s_dev_count].name[i] = name[i];
    }
    s_dev[s_dev_count].name[n] = '\0';
    for (i = 0; i < 6u; i++) {
        s_dev[s_dev_count].bda[i] = bda[i];
    }
    s_dev[s_dev_count].port = V4P_NO_INDEX;
    s_dev[s_dev_count].used = 1;
    s_dev_count++;
    return 0;
}

void v4_mock_set_port_mode(int port, uint8_t mode)
{
    if (port < 0 || port >= (int)V4P_PORT_COUNT) {
        return;
    }
    if (mode > V4P_PORT_MODE_MAX) {
        mode = (uint8_t)V4P_PORT_OFF;
    }
    s_port_mode[port] = mode;
    /* Wie der Konsolenbefehl "portmode" und SET_PORT_MODE auf dem ESP32: die
     * gesetzte Betriebsart ist zugleich die dauerhafte VORGABE der Buchse. */
    s_mode_soll[port] = mode;
}

void v4_mock_set_mode_soll(int port, uint8_t mode_soll)
{
    if (port < 0 || port >= (int)V4P_PORT_COUNT) {
        return;
    }
    if (!v4p_mode_soll_valid(mode_soll)) {
        return;
    }
    s_mode_soll[port] = mode_soll;      /* die wirksame bleibt stehen */
}

void v4_mock_set_routing(int port, uint8_t dir,
                         const uint8_t btn[V4P_BTN_COUNT])
{
    if (port < 0 || port >= (int)V4P_PORT_COUNT) {
        return;
    }
    if (!v4p_dir_valid(dir) || !v4p_btn_set_valid(btn)) {
        return;
    }
    s_dir[port]    = dir;
    s_btn[port][0] = btn[0];
    s_btn[port][1] = btn[1];
    s_btn[port][2] = btn[2];
}

void v4_mock_set_mouse_cfg(int port, uint8_t speed, uint8_t totzone)
{
    if (port < 0 || port >= (int)V4P_PORT_COUNT) {
        return;
    }
    if (!v4p_mouse_speed_valid(speed) || !v4p_mouse_totzone_valid(totzone)) {
        return;
    }
    s_mouse_speed[port]   = speed;
    s_mouse_totzone[port] = totzone;
}

uint8_t v4_mock_dev_port(int idx)
{
    if (idx < 0 || idx >= s_dev_count) {
        return V4P_NO_INDEX;
    }
    if (s_dev[idx].port >= V4P_PORT_COUNT) {
        return V4P_NO_INDEX;
    }
    return s_dev[idx].port;
}

void v4_mock_set_port_state(int port, uint8_t drive_bits)
{
    if (port < 0 || port >= (int)V4P_PORT_COUNT) {
        return;
    }
    s_port_state[port] = (uint8_t)(drive_bits & V4P_IN_ST_MASK);
    /*
     * Den Pegel auf den Erwartungswert bei wirkendem Pull-up setzen:
     * gezogen = Low, losgelassen = High. Ohne das muesste jeder Aufrufer zwei
     * Setter rufen, nur um einen realistischen Port zu beschreiben. Wer den
     * WIDERSPRUCH will (fehlender Pull-up), ruft danach set_port_level.
     */
    s_port_level[port] = (uint8_t)((~s_port_state[port]) & V4P_IN_ST_MASK);
}

void v4_mock_set_port_level(int port, uint8_t level_bits)
{
    if (port < 0 || port >= (int)V4P_PORT_COUNT) {
        return;
    }
    s_port_level[port] = (uint8_t)(level_bits & V4P_IN_ST_MASK);
}

void v4_mock_set_steps(uint32_t x, uint32_t y)
{
    s_steps_x = x;
    s_steps_y = y;
}

void v4_mock_set_hid(uint8_t kind, uint8_t transport, uint16_t vid,
                     uint16_t pid, uint8_t battery, uint16_t report_id,
                     uint16_t report_len, uint16_t reports, const char *name)
{
    size_t n = 0u;
    size_t i;

    memset(&s_hid, 0, sizeof(s_hid));
    s_hid.kind       = kind;
    s_hid.transport  = transport;
    s_hid.vid        = vid;
    s_hid.pid        = pid;
    s_hid.battery    = battery;
    s_hid.report_id  = report_id;
    s_hid.report_len = report_len;
    s_hid.reports    = reports;

    if (name != NULL) {
        n = strlen(name);
        if (n > (size_t)V4P_HID_NAME_MAX) {
            n = (size_t)V4P_HID_NAME_MAX;
        }
    }
    for (i = 0; i < n; i++) {
        s_hid.name[i] = name[i];
    }
    s_hid.name[n] = '\0';
    s_hid.name_len = (uint8_t)n;
}

void v4_mock_set_connected(int index)
{
    if (index < 0 || index >= s_dev_count) {
        return;
    }
    s_state        = (uint8_t)V4P_STATE_CONNECTED;
    s_conn_index   = (uint8_t)index;
    s_connect_left = 0;
}

void v4_mock_set_scan(int active)
{
    s_scan_active = active ? 1u : 0u;
    if (active) {
        s_scan_polls = 0u;
        s_scan_gen   = (uint16_t)(s_scan_gen + 1u);
    }
}

void v4_mock_bump_scan_gen(void)
{
    s_scan_gen = (uint16_t)(s_scan_gen + 1u);
}

/* ------------------------------------------------------------------ */
/* Standardszenario                                                    */
/* ------------------------------------------------------------------ */

void v4_mock_seed_default(void)
{
    /* Drei Geraete, wie sie ein Scan im Wohnzimmer liefert: ein Pad, eine Maus
     * und ein Headset-artiges Geraet, das KEIN HID-Eingabegeraet ist. Das
     * dritte ist Absicht: es prueft, ob die Anzeige "andere" auch wirklich
     * zeigt und nicht alles als Pad behandelt. */
    static const uint8_t bda0[6] = { 0xE4u, 0x17u, 0xD8u, 0x11u, 0x22u, 0x33u };
    static const uint8_t bda1[6] = { 0x00u, 0x1Bu, 0xDCu, 0x44u, 0x55u, 0x66u };
    static const uint8_t bda2[6] = { 0xACu, 0x12u, 0x2Fu, 0x77u, 0x88u, 0x99u };

    (void)v4_mock_dev_add("8BitDo Pro 2", bda0);
    (void)v4_mock_dev_add("Logitech MX Anywhere", bda1);
    (void)v4_mock_dev_add("Bluetooth-Kopfhoerer", bda2);

    v4_mock_set_port_mode(0, (uint8_t)V4P_PORT_JOY);
    v4_mock_set_port_mode(1, (uint8_t)V4P_PORT_OFF);
    v4_mock_set_port_state(0, 0u);
    v4_mock_set_port_state(1, 0u);
    v4_mock_set_steps(0u, 0u);

    /* Eingangsverteilung wie ab Werk (siehe v4_mock_reset): alle drei
     * Richtungsquellen, Knopfquellen = die drei bisherigen Gruppen. */
    {
        uint8_t ab_werk[V4P_BTN_COUNT];

        ab_werk[0] = (uint8_t)V4P_TASTE_GRUPPE1;
        ab_werk[1] = (uint8_t)V4P_TASTE_GRUPPE2;
        ab_werk[2] = (uint8_t)V4P_TASTE_GRUPPE3;
        v4_mock_set_routing(0, (uint8_t)V4P_DIR_STANDARD, ab_werk);
        v4_mock_set_routing(1, (uint8_t)V4P_DIR_STANDARD, ab_werk);
    }
    /*
     * Und die VORGABE ausdruecklich auf "nach Geraeteart" zuruecknehmen --
     * so sieht der Bestand ab Werk aus (NVS ungesetzt), waehrend die
     * wirksame Betriebsart aus der Geraeteart kommt. Genau dieser
     * Unterschied ist der Grund, warum es mode_soll UND mode_ist gibt.
     */
    v4_mock_set_mode_soll(0, (uint8_t)V4P_MODE_AUTO);
    v4_mock_set_mode_soll(1, (uint8_t)V4P_MODE_AUTO);

    v4_mock_set_hid(V4P_HID_KIND_PAD, V4P_HID_TR_BREDR, 0x2DC8u, 0x6100u,
                    78u, 3u, 10u, 1234u, "8BitDo Pro 2");
}
