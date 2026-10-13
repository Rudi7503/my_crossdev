/*
 * v4_proto.h -- gemeinsamer Vertrag Vampire V4 (I2C-Master) <-> ESP32 (I2C-Slave)
 *
 * Gegenstueck zu `V4_ESP32_BT_Joy_BP32/main/v4_proto.h` (der Gegenseite). Diese
 * Datei ist die V4-Seite desselben Vertrags; massgeblich ist IMMER die
 * ESP32-Fassung. Wo hier eine Zahl steht, steht sie dort genauso. Zum Lesen
 * gibt es die Prosa dazu: `V4_ESP32_BT_Joy_BP32/docs/PROTOKOLL.md`, fuer die
 * Eingaenge der Abschnitt "Eingangspayload".
 *
 * Was gegenueber dem Audio-Projekt (Projects/I2C-test/v4_master) anders ist --
 * und warum:
 *
 *   1. READ-Rahmen 128 Byte statt 64. Das ist proto_ver 3. Die alte
 *      64-Byte-Fassung ist NICHT kompatibel: Flags und CRC liegen bei
 *      +126/+127, nicht bei +62/+63. Wer die Rahmengroesse verwechselt, liest
 *      jede Antwort um 64 Byte verschoben und sieht nur noch CRC-Fehler.
 *   2. Kein BULK-Rahmen, kein CRC-32, kein chunk. Der ESP32 dieses Projekts
 *      hat keinen Dateiweg; GET_INFO meldet bulk_payload_max = 0. Ein
 *      BULK-Empfaenger waere toter Code und wuerde nur die Luege verbreiten,
 *      dass es Dateizugriff gibt.
 *   3. Kein SD-, Datei-, Audio- und EQ-Befehl. Der ESP32 antwortet auf diese
 *      Codes mit BAD_CMD (0x03); sie sind hier absichtlich nicht deklariert,
 *      damit sie niemand versehentlich benutzt.
 *   4. GET_INFO +10/+11 sind jetzt eine Faehigkeitsliste (u16 LE, V4P_CAP_*)
 *      und nicht mehr "reserved = 0". Ein Master, der fw_ver < 3 liest, darf
 *      sie nicht auswerten.
 *   5. NEU sind die Buchsenbefehle 0x04..0x07 (SET/GET_PORT_MODE, GET_INPUT,
 *      GET_HID). Sie sind der Zweck dieses Projekts.
 *   6. GET_INPUT liefert 12 Byte: Zug-Maske UND die gemessenen Pegel
 *      (level0@10, level1@11). Die aeltere 10-Byte-Fassung ohne Pegel wird
 *      weiter angenommen -- dann sind die Pegel UNBEKANNT und werden als
 *      solche angezeigt, nicht als 0 und nicht abgeleitet.
 *
 * PORTABILITAETSREGELN (PROTOCOL_V4_MASTER.md §0 und §2) -- diese Datei und
 * alle Dateien, die Frame-Bytes anfassen, halten sich ausnahmslos daran:
 *
 *   1. Mehrbyte-Felder auf der Leitung sind IMMER Little Endian.
 *   2. Frame-Bytes werden NUR ueber v4p_put_*_le / v4p_get_*_le beruehrt.
 *   3. Verboten: *(uint16_t *)p, *(uint32_t *)p, memcpy auf Structs,
 *      Casting von Structs ueber Frame-Bytes, __attribute__((packed)),
 *      Bitfelder. `tools/check_portability.sh` erzwingt das beim Build.
 *   4. Die Host-Structs (v4p_info_t usw.) sind native und werden ausschliesslich
 *      feldweise ueber die Codec-Funktionen be-/entladen -- nie per Cast auf
 *      einen Frame.
 *
 * Der Vertrag ist bewusst byte-orientiert, damit dieselbe Quelle auf
 * Little-Endian (Linux-Harness, x86/ARM) und Big-Endian (Vampire V4, m68k)
 * identisch funktioniert. Ein Byte-Order-Fehler waere im Linux-Harness sonst
 * unsichtbar (§0).
 */

#ifndef V4_PROTO_H
#define V4_PROTO_H

#include <stddef.h>
#include <stdint.h>

/* ------------------------------------------------------------------ */
/* Version und Rahmengroessen                                          */
/* ------------------------------------------------------------------ */

/* proto_ver 3 = 128-Byte-READ-Rahmen. proto_ver 2 las 64 Byte und ist NICHT
 * kompatibel (siehe oben, Punkt 1). Der Versionswechsel ist die schnelle und
 * eindeutige Unterscheidung; zusaetzlich prueft v4_check_info() die gemeldeten
 * Rahmengroessen. */
#define V4P_PROTO_VER           3u

/* Faehigkeitsstand der Firmware (V4P_FW_VERSION der ESP32-Seite). Er sagt
 * NICHTS ueber die Rahmengeometrie -- das tut proto_ver. */
#define V4P_FW_VER              3u

#define V4P_MAGIC_WRITE         0xA5u   /* V4 -> ESP32, fest 32 Byte   */
#define V4P_MAGIC_READ          0x5Au   /* ESP32 -> V4,  fest 128 Byte */

#define V4P_WRITE_FRAME_LEN     32u
#define V4P_READ_FRAME_LEN      128u    /* seit proto_ver 3: 128 statt 64 */
#define V4P_WRITE_PAYLOAD_MAX   27u     /* 32 - 4 Kopf - 1 CRC */
#define V4P_READ_PAYLOAD_MAX    121u    /* 128 - 5 Kopf - 1 Flags - 1 CRC */

/* Lage von Flags und CRC im READ-Frame. ACHTUNG: die Ueberschrift von §5.2 der
 * alten Protokollbeschreibung nennt noch "+62" -- das ist ein Rest der
 * 64-Byte-Fassung und hier falsch. */
#define V4P_READ_OFF_FLAGS      126u
#define V4P_READ_OFF_CRC        127u
#define V4P_READ_CRC_RANGE      127u    /* CRC-8 ueber Byte 0..126 */

/* Namen: DEV_GET kuerzt auf der Leitung auf 49 Zeichen (so rechnet die
 * ESP32-Seite, V4P_DEV_OFF_NAME + 49). Der Puffer des Aufrufers braucht ein
 * Byte mehr fuer die abschliessende Null. */
#define V4P_DEVNAME_MAX         49u
#define V4P_NAME_BUF            (V4P_DEVNAME_MAX + 1u)      /* 50  */

/* HID-Namen sind auf 48 Zeichen begrenzt (V4P_HID_NAME_MAX der ESP32-Seite). */
#define V4P_HID_NAME_MAX        48u
#define V4P_HID_NAME_BUF        (V4P_HID_NAME_MAX + 1u)     /* 49  */

/* Obergrenze, die dieser Master an Puffern bereitstellt. Die Firmware meldet
 * ihre eigene Grenze in GET_INFO.max_devices -- bei diesem Stand 8
 * (BT_MGR_MAX_DEVICES der ESP32-Seite). Wer die Liste durchlaeuft, nimmt das
 * KLEINERE von beiden; ein fester Wert waere die naechste Ueberraschung. */
#define V4P_MAX_DEVICES         16u

/* ------------------------------------------------------------------ */
/* Statuscodes (READ-Frame) -- §5.1                                    */
/* ------------------------------------------------------------------ */

enum {
    V4P_ST_OK           = 0x00,     /* erledigt, Nutzlast gueltig          */
    V4P_ST_BUSY         = 0x01,     /* nicht fertig: ganzen Befehl neu     */
    V4P_ST_BAD_CRC      = 0x02,     /* Request-Framing: Befehl neu, gleiche SEQ */
    V4P_ST_BAD_CMD      = 0x03,     /* unbekannt: nicht wiederholen        */
    V4P_ST_BAD_ARG      = 0x04,     /* Argument unzulaessig                */
    V4P_ST_BAD_STATE    = 0x05,     /* im Zustand nicht erlaubt            */
    V4P_ST_NO_SD        = 0x06,     /* im BT-Joy-Projekt unbenutzt         */
    V4P_ST_NOT_FOUND    = 0x07,     /* Geraet/Eintrag fehlt                */
    V4P_ST_IO_ERR       = 0x08,     /* Treiberfehler                       */
    V4P_ST_END          = 0x09,     /* Ende einer Liste                    */
    V4P_ST_TOO_LONG     = 0x0A,     /* Name zu lang                        */
    V4P_ST_NO_HANDLE    = 0x0B,     /* im BT-Joy-Projekt unbenutzt         */
    V4P_ST_BT_ERR       = 0x0C      /* Bluetooth-Stack hat abgelehnt       */
};

/* ------------------------------------------------------------------ */
/* Flags im READ-Frame (+126)                                          */
/* ------------------------------------------------------------------ */

#define V4P_FLAG_NONE           0x00u   /* keine Besonderheit               */
#define V4P_FLAG_NAME_TRUNCATED 0x01u   /* DEV_GET: Name war laenger als 49 */
#define V4P_FLAG_MORE           0x02u   /* reserviert, aktuell immer 0      */

/* ------------------------------------------------------------------ */
/* Faehigkeiten (GET_INFO +10, u16 LE) -- V4P_CAP_* der ESP32-Seite     */
/* ------------------------------------------------------------------ */

#define V4P_CAP_PORT_MODE       0x0001u  /* SET_PORT_MODE / GET_PORT_MODE */
#define V4P_CAP_GET_INPUT       0x0002u  /* GET_INPUT                     */
#define V4P_CAP_GET_HID         0x0004u  /* GET_HID                       */
#define V4P_CAP_HID_BT          0x0008u  /* HID-Host BR/EDR               */
#define V4P_CAP_HID_BLE         0x0010u  /* HID-Host BLE                  */
#define V4P_CAP_MOUSE           0x0020u  /* Maus-Emulation (Quadratur)    */
#define V4P_CAP_PAD             0x0040u  /* Gamepad-Emulation             */
#define V4P_CAP_BATTERY         0x0080u  /* Batteriestand wird gemeldet   */

/* ------------------------------------------------------------------ */
/* Die zwei DB9-Buchsen -- V4P_PORT_* / V4P_IN_* der ESP32-Seite        */
/* ------------------------------------------------------------------ */

#define V4P_PORT_COUNT          2u

/* Betriebsarten, identisch zu joy_mode_t in joy_port.h */
#define V4P_PORT_OFF            0u
#define V4P_PORT_JOY            1u
#define V4P_PORT_MOUSE          2u
#define V4P_PORT_MODE_MAX       V4P_PORT_MOUSE

/* Zustandsbits einer Buchse, identisch zu JOY_ST_* in joy_port.h.
 *
 * Die Bits gelten fuer BEIDE Masken einer Buchse, haben aber verschiedene
 * Bedeutung:
 *
 *   Zug-Maske (state0/state1)   1 = diese Leitung wird nach GND gezogen =
 *                               Taste/Richtung aktiv. Ruhezustand 0x00.
 *   Pegel-Maske (level0/level1) 1 = diese Leitung liegt auf High.
 *                               Ruhezustand 0x7F (alle sieben HIGH).
 *
 * Die beiden koennen auseinandergehen, und genau das ist die Aussage:
 * "losgelassen, liest aber 0" heisst fehlender Pull-up (der ESP32 gibt die
 * Leitung frei, es zieht sie niemand hoch); "gezogen, liest aber 1" kann
 * elektrisch nicht sein und deutet auf Ausgangsstufe oder Pinbelegung. Dafuer
 * gibt es v4p_level_missing_pullup() und v4p_level_impossible(). */
#define V4P_IN_ST_UP            0x01u
#define V4P_IN_ST_DOWN          0x02u
#define V4P_IN_ST_LEFT          0x04u
#define V4P_IN_ST_RIGHT         0x08u
#define V4P_IN_ST_BTN1          0x10u   /* DB9 Pin 6 - Feuer / Maus links  */
#define V4P_IN_ST_BTN2          0x20u   /* DB9 Pin 9 - Pot1  / Maus rechts */
#define V4P_IN_ST_BTN3          0x40u   /* DB9 Pin 5 - Pot0  / Maus mitte  */

/* Alle sieben belegten Leitungen. Bit 7 ist frei und wird nie gesetzt. */
#define V4P_IN_ST_MASK          0x7Fu
#define V4P_IN_ST_COUNT         7u

/* ------------------------------------------------------------------ */
/* Nutzlast-Offsets -- §11 (Namen wie auf der ESP32-Seite)             */
/* ------------------------------------------------------------------ */

/* GET_INFO / PING: 12 Byte, ab +10 eine Faehigkeitsliste (u16 LE). */
#define V4P_INFO_OFF_CAPS       10u
#define V4P_INFO_PAYLOAD_LEN    12u

/* GET_STATUS: 17 Byte. Die Felder SD_MOUNTED, AUDIO, SD_FREE_KB und SD_CARD
 * gehoeren zum Audio-Projekt und sind hier IMMER 0. Sie werden trotzdem
 * dekodiert: nur so faellt auf, wenn eine fremde Firmware (Audio-Stand)
 * antwortet -- deren Restnutzlast waere sonst stillschweigend "gueltig". */
#define V4P_ST_OFF_STATE            0u
#define V4P_ST_OFF_CONN_INDEX       1u
#define V4P_ST_OFF_DEV_COUNT        2u
#define V4P_ST_OFF_SCAN_ACTIVE      3u
#define V4P_ST_OFF_SD_MOUNTED       4u
#define V4P_ST_OFF_AUDIO_FLAGS      5u
#define V4P_ST_OFF_SCAN_GEN         6u
#define V4P_ST_OFF_SD_FREE_KB       8u
#define V4P_ST_OFF_CHUNK           12u
#define V4P_ST_OFF_PROTO_VER       14u
#define V4P_ST_OFF_FW_VER          15u
#define V4P_ST_OFF_SD_CARD_PRESENT 16u
#define V4P_ST_LEN                 17u

/* Verbindungszustand, GET_STATUS.state */
enum {
    V4P_STATE_IDLE       = 0,
    V4P_STATE_SCANNING   = 1,
    V4P_STATE_CONNECTING = 2,
    V4P_STATE_CONNECTED  = 3,
    V4P_STATE_SUSPENDED  = 4
};

/* "kein Geraet verbunden" im Feld conn_index */
#define V4P_NO_INDEX            0xFFu

/* SET_PORT_MODE: Anfrage und Antwort haben dasselbe Layout */
#define V4P_SPM_OFF_PORT        0u
#define V4P_SPM_OFF_MODE        1u
#define V4P_SPM_PAYLOAD_LEN     2u

/* GET_PORT_MODE: eine Betriebsart je Buchse */
#define V4P_GPM_OFF_MODE0       0u
#define V4P_GPM_OFF_MODE1       1u
#define V4P_GPM_PAYLOAD_LEN     2u

/* GET_INPUT: 12 Byte (mit Pegeln), 10 Byte (aeltere Fassung ohne).
 * Beschreibung: docs/PROTOKOLL.md der ESP32-Seite, "Eingangspayload". */
#define V4P_IN_OFF_STATE0       0u      /* Zug-Maske V4P_IN_ST_*          */
#define V4P_IN_OFF_STATE1       1u
#define V4P_IN_OFF_STEPS_X      2u      /* u32 LE, Quadratur-Schritte     */
#define V4P_IN_OFF_STEPS_Y      6u      /* u32 LE                         */
/*
 * Die TATSAECHLICH anliegenden Pegel, je Buchse eine Bitmaske (Bit gesetzt =
 * Leitung High, Ruhezustand also 0x7F). Sie sind etwas anderes als die
 * Zug-Maske: bei "losgelassen" schaltet der ESP32 den Ausgang ab (High-Z), und
 * die 1 kommt vom Pull-up der Vampire. Fehlt der Pull-up, liest eine
 * losgelassene Leitung 0 -- genau dieser Widerspruch ist am Port die
 * elektrische Kernaussage (siehe v4p_level_missing_pullup()).
 */
#define V4P_IN_OFF_LEVEL0      10u      /* Pegel-Maske Buchse 0           */
#define V4P_IN_OFF_LEVEL1      11u      /* Pegel-Maske Buchse 1           */
#define V4P_IN_PAYLOAD_LEN     12u

/* Nutzlastlaenge der aelteren Fassung ohne Pegel. Sie ist die
 * Versionsauskunft: 12 Byte = mit Pegeln, 10 Byte = ohne. Wer 10 liest, darf
 * KEINE Pegel anzeigen (auch keine abgeleiteten) -- er weiss sie schlicht
 * nicht. */
#define V4P_IN_PAYLOAD_LEN_NO_LEVEL 10u

/* GET_HID */
#define V4P_HID_OFF_KIND        0u      /* 0 keine, 1 Pad, 2 Maus, 3 andere */
#define V4P_HID_OFF_TRANSPORT   1u      /* 0 BR/EDR, 1 BLE                */
#define V4P_HID_OFF_VID         2u      /* u16 LE                         */
#define V4P_HID_OFF_PID         4u      /* u16 LE                         */
#define V4P_HID_OFF_BATTERY     6u      /* 0..100, 0xFF unbekannt         */
#define V4P_HID_OFF_REPORT_ID   7u      /* u16 LE                         */
#define V4P_HID_OFF_REPORT_LEN  9u      /* u16 LE                         */
#define V4P_HID_OFF_REPORTS    11u      /* u16 LE, gezaehlte Reports      */
#define V4P_HID_OFF_NAME_LEN   13u
#define V4P_HID_OFF_NAME       14u
#define V4P_HID_PAYLOAD_MAX    (V4P_HID_OFF_NAME + V4P_HID_NAME_MAX)  /* 62 */

/* Geraeteart in GET_HID.kind (hid_kind_t der ESP32-Seite) */
#define V4P_HID_KIND_NONE       0u
#define V4P_HID_KIND_PAD        1u
#define V4P_HID_KIND_MOUSE      2u
#define V4P_HID_KIND_OTHER      3u

/* Transport in GET_HID.transport */
#define V4P_HID_TR_BREDR        0u
#define V4P_HID_TR_BLE          1u

/* Batteriestand: unbekannt */
#define V4P_HID_BATTERY_UNKNOWN 0xFFu

/* DEV_GET: idx, bda[6], name_len, name */
#define V4P_DEV_OFF_INDEX       0u
#define V4P_DEV_OFF_BDA         1u
#define V4P_DEV_OFF_NAME_LEN    7u
#define V4P_DEV_OFF_NAME        8u

/* ------------------------------------------------------------------ */
/* Befehlscodes -- §11. Nur was der ESP32 dieses Projekts kann.        */
/*                                                                     */
/* Die Codes 0x30..0x73 des Audio-Projekts (SD, Pfad, Verzeichnis,     */
/* Datei, Wiedergabe, EQ) fehlen hier ABSICHTLICH. Der ESP32 kennt sie */
/* nicht und antwortet BAD_CMD; deklariert man sie trotzdem, baut       */
/* irgendwann jemand einen Aufruf darauf und sucht den Fehler an der    */
/* falschen Stelle.                                                      */
/* ------------------------------------------------------------------ */

enum {
    V4P_CMD_PING          = 0x01,   /* -            -> v4p_info_t            */
    V4P_CMD_GET_STATUS    = 0x02,   /* -            -> v4p_status_t          */
    V4P_CMD_GET_INFO      = 0x03,   /* -            -> v4p_info_t            */

    V4P_CMD_SET_PORT_MODE = 0x04,   /* [port u8][mode u8] -> [port u8][mode u8] */
    V4P_CMD_GET_PORT_MODE = 0x05,   /* -            -> [mode0 u8][mode1 u8]  */
    V4P_CMD_GET_INPUT     = 0x06,   /* -            -> v4p_input_t           */
    V4P_CMD_GET_HID       = 0x07,   /* -            -> v4p_hid_t             */

    V4P_CMD_SCAN_START    = 0x10,   /* [len_s u8]   -> -                     */
    V4P_CMD_SCAN_STOP     = 0x11,   /* -            -> -                     */
    V4P_CMD_DEV_COUNT     = 0x12,   /* -            -> count u8              */
    V4P_CMD_DEV_GET       = 0x13,   /* [idx u8]     -> idx,bda[6],name_len,name */

    V4P_CMD_CONNECT       = 0x20,   /* [idx u8]     -> -                     */
    V4P_CMD_CONNECT_BDA   = 0x21,   /* [bda 6]      -> -                     */
    V4P_CMD_DISCONNECT    = 0x22,   /* -            -> -                     */
    V4P_CMD_FORGET        = 0x23,   /* -            -> -                     */

    V4P_CMD_RESET         = 0x7E    /* -            -> - (beide Buchsen aus) */
};

/* ------------------------------------------------------------------ */
/* Rueckgabewerte der Prueffunktionen                                  */
/* ------------------------------------------------------------------ */

enum {
    V4P_CHECK_OK        =  0,
    V4P_CHECK_SIZE      = -1,       /* falsche Framelaenge gelesen        */
    V4P_CHECK_MAGIC     = -2,
    V4P_CHECK_CRC       = -3,
    V4P_CHECK_ECHO      = -4,       /* cmd/seq nicht zurueckgespiegelt    */
    V4P_CHECK_LEN       = -5        /* len-Feld unzulaessig               */
};

/* ------------------------------------------------------------------ */
/* Native Host-Structs (NIE per Cast auf Frame-Bytes abbilden)          */
/* ------------------------------------------------------------------ */

/* Antwort auf PING / GET_INFO, Nutzlast 12 Byte */
typedef struct {
    uint8_t  proto_ver;
    uint8_t  fw_ver;
    uint8_t  write_frame_len;
    uint8_t  read_frame_len;
    uint16_t bulk_payload_max;  /* 0 = dieses Projekt hat keinen Bulk-Weg */
    uint16_t chunk;             /* reserviert (kein Dateiweg), immer 128  */
    uint8_t  max_devices;
    uint8_t  path_max;          /* reserviert (kein Dateiweg), immer 128  */
    uint16_t caps;              /* V4P_CAP_*, u16 LE ab +10               */
} v4p_info_t;

/* Antwort auf GET_STATUS, Nutzlast 17 Byte.
 *
 * Die mit "reserviert" gekennzeichneten Felder gehoeren zum Audio-Projekt und
 * sind in dieser Firmware immer 0. Sie werden dekodiert, damit ein falscher
 * Slave auffaellt -- und nicht, weil sie hier eine Bedeutung haetten. */
typedef struct {
    uint8_t  state;             /* V4P_STATE_*                            */
    uint8_t  conn_index;        /* 0xFF = keiner                          */
    uint8_t  dev_count;
    uint8_t  scan_active;
    uint8_t  sd_mounted;        /* reserviert, immer 0                    */
    uint8_t  audio_flags;       /* reserviert, immer 0                    */
    uint16_t scan_gen;
    uint32_t sd_free_kb;        /* reserviert, immer 0                    */
    uint16_t chunk;             /* reserviert, immer 128                  */
    uint8_t  proto_ver;
    uint8_t  fw_ver;
    uint8_t  sd_card_present;   /* reserviert, immer 0                    */
} v4p_status_t;

/* Antwort auf GET_INPUT: 12 Byte, bei aelterer Firmware 10 Byte.
 *
 * state[] ist die ZUG-Maske je Buchse (1 = wird nach GND gezogen = aktiv),
 * level[] die GEMESSENE Pegel-Maske (1 = High). level_valid sagt, ob die
 * Firmware die Pegel ueberhaupt geliefert hat: bei einer 10-Byte-Antwort ist
 * das NICHT der Fall, und dann sind level[] undefiniert -- die Anzeigen
 * schreiben dort einen Hinweis statt einer Zahl. */
typedef struct {
    uint8_t  state[V4P_PORT_COUNT];     /* Zug-Maske je Buchse           */
    uint32_t steps_x;                   /* Quadratur-Schritte seit Start */
    uint32_t steps_y;
    uint8_t  level[V4P_PORT_COUNT];     /* gemessene Pegel, 1 = High     */
    uint8_t  level_valid;               /* 0 = Firmware ohne Pegel       */
} v4p_input_t;

/* Antwort auf GET_HID. `flags` kommt aus dem READ-Frame (+126) und steht nicht
 * in der Nutzlast. */
typedef struct {
    uint8_t  kind;              /* V4P_HID_KIND_*                         */
    uint8_t  transport;         /* V4P_HID_TR_*                           */
    uint16_t vid;
    uint16_t pid;
    uint8_t  battery;           /* 0..100, 0xFF = unbekannt               */
    uint16_t report_id;
    uint16_t report_len;
    uint16_t reports;           /* gezaehlte Reports seit dem Verbinden   */
    uint8_t  name_len;
    uint8_t  flags;             /* V4P_FLAG_*                             */
    char     name[V4P_HID_NAME_BUF];
} v4p_hid_t;

/* Antwort auf DEV_GET */
typedef struct {
    uint8_t  idx;
    uint8_t  bda[6];
    uint8_t  name_len;
    uint8_t  flags;             /* V4P_FLAG_*                             */
    char     name[V4P_NAME_BUF];
} v4p_dev_t;

/* Geparster READ-Frame (128 Byte) */
typedef struct {
    uint8_t cmd;
    uint8_t seq;
    uint8_t status;
    uint8_t len;
    uint8_t flags;
    uint8_t payload[V4P_READ_PAYLOAD_MAX];
} v4p_read_t;

/* ------------------------------------------------------------------ */
/* Little-Endian-Helfer -- die EINZIGE erlaubte Zugriffsform (§2)       */
/* ------------------------------------------------------------------ */

static inline void v4p_put_u16le(uint8_t *p, uint16_t v)
{
    p[0] = (uint8_t)(v & 0xFFu);
    p[1] = (uint8_t)((v >> 8) & 0xFFu);
}

static inline uint16_t v4p_get_u16le(const uint8_t *p)
{
    return (uint16_t)((uint16_t)p[0] | (uint16_t)((uint16_t)p[1] << 8));
}

static inline void v4p_put_u32le(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)(v & 0xFFu);
    p[1] = (uint8_t)((v >> 8) & 0xFFu);
    p[2] = (uint8_t)((v >> 16) & 0xFFu);
    p[3] = (uint8_t)((v >> 24) & 0xFFu);
}

static inline uint32_t v4p_get_u32le(const uint8_t *p)
{
    return (uint32_t)p[0]
         | ((uint32_t)p[1] << 8)
         | ((uint32_t)p[2] << 16)
         | ((uint32_t)p[3] << 24);
}

/* ------------------------------------------------------------------ */
/* Pruefsumme -- §3                                                    */
/* ------------------------------------------------------------------ */

/* CRC-8/SMBUS: Poly 0x07, Init 0x00, keine Reflektion, kein Final-XOR.
 * Muss Byte fuer Byte mit v4p_crc8() der ESP32-Seite uebereinstimmen. */
uint8_t v4p_crc8(const uint8_t *d, size_t n);

/* ------------------------------------------------------------------ */
/* Frame-Aufbau (Master baut WRITE, Mock/ESP32 bauen READ)             */
/* ------------------------------------------------------------------ */

/* Beide liefern 0 bei Erfolg, -1 bei unzulaessiger Laenge. */
int v4p_build_write(uint8_t *f, uint8_t cmd, uint8_t seq,
                    const uint8_t *payload, uint8_t len);

int v4p_build_read(uint8_t *f, uint8_t cmd, uint8_t seq, uint8_t status,
                   uint8_t flags, const uint8_t *payload, uint8_t len);

/* ------------------------------------------------------------------ */
/* Frame-Pruefung -- Reihenfolge exakt nach §4.2                       */
/* ------------------------------------------------------------------ */

/* Prueft einen gelesenen READ-Frame. Die Nutzlast wird nach `out` kopiert,
 * `f` bleibt unberuehrt. */
int v4p_check_read(const uint8_t *f, size_t got, uint8_t cmd, uint8_t seq,
                   v4p_read_t *out);

/* ------------------------------------------------------------------ */
/* Nutzlast-Codec -- von Master UND Mock benutzt, damit kein           */
/* Format-Drift zwischen den beiden Seiten entstehen kann (§14.1)       */
/* ------------------------------------------------------------------ */

/* Encoder liefern die erzeugte Nutzlastlaenge. */
size_t v4p_enc_get_info  (uint8_t *p, const v4p_info_t   *in);
size_t v4p_enc_get_status(uint8_t *p, const v4p_status_t *in);
size_t v4p_enc_get_input (uint8_t *p, const v4p_input_t  *in);
size_t v4p_enc_get_hid   (uint8_t *p, const v4p_hid_t    *in);
size_t v4p_enc_dev       (uint8_t *p, const v4p_dev_t    *in);
size_t v4p_enc_get_port_mode(uint8_t *p, const uint8_t mode[V4P_PORT_COUNT]);
size_t v4p_enc_set_port_mode(uint8_t *p, uint8_t port, uint8_t mode);

/* Decoder liefern 0 bei Erfolg, -1 wenn `len` nicht ausreicht bzw. ein Feld
 * unzulaessig ist. */
int v4p_dec_get_info  (const uint8_t *p, uint8_t len, v4p_info_t   *out);
int v4p_dec_get_status(const uint8_t *p, uint8_t len, v4p_status_t *out);
int v4p_dec_get_input (const uint8_t *p, uint8_t len, v4p_input_t  *out);
int v4p_dec_get_hid   (const uint8_t *p, uint8_t len, v4p_hid_t    *out);
int v4p_dec_dev       (const uint8_t *p, uint8_t len, v4p_dev_t    *out);
int v4p_dec_get_port_mode(const uint8_t *p, uint8_t len,
                          uint8_t mode[V4P_PORT_COUNT]);
int v4p_dec_set_port_mode(const uint8_t *p, uint8_t len, uint8_t *port,
                          uint8_t *mode);

/* ------------------------------------------------------------------ */
/* Klartexte und Auswertungen fuer die Oberflaechen                    */
/*                                                                     */
/* Sie stehen hier und nicht in jeder Anzeige neu, weil Konsole, MUI und */
/* Mock sonst drei leicht verschiedene Schreibweisen pflegen wuerden --   */
/* genau daraus entstehen "Anzeige stimmt nicht mit dem Befehl ueberein"- */
/* Fehler.                                                                 */
/* ------------------------------------------------------------------ */

const char *v4p_port_mode_name(uint8_t mode);   /* "aus" / "Joystick" / "Maus" */

/* Naechste Betriebsart im Kreislauf aus -> Joystick -> Maus -> aus.
 * Genau das tun die Schaltknoepfe der Oberflaechen. */
uint8_t v4p_port_mode_next(uint8_t mode);

const char *v4p_bt_state_name(uint8_t state);   /* "IDLE" ... "CONNECTED" */
const char *v4p_hid_kind_name(uint8_t kind);    /* "keine" / "Pad" / "Maus" ... */
const char *v4p_transport_name(uint8_t tr);     /* "BR/EDR" / "BLE" */

/* Name der Leitung mit dem hoechsten Bit ... Bitnummer 0..6 (UP..BTN3). */
const char *v4p_bit_name(unsigned bit);
const char *v4p_bit_pin(unsigned bit);          /* "Pin 1" ... "Pin 5" */

/* Setzt `bits` in eine Liste von Leitungen um, z.B. "UP DOWN BTN1".
 * Reicht der Platz nicht, wird mit "..." abgebrochen; out ist immer
 * nullterminiert. Fuer bits==0 entsteht "-". */
void v4p_bits_text(uint8_t bits, char *out, size_t cap);

/*
 * Widerspruch zwischen Zug und Pegel -- die elektrische Kernaussage am Port.
 *
 * Beide Funktionen liefern die MASKE der betroffenen Leitungen; mit
 * v4p_bits_text() laesst sich daraus direkt eine Meldung bauen. Beide
 * brauchen die GEMESSENEN Pegel, es gibt also keine sinnvolle Antwort, wenn
 * die Firmware nur die 10-Byte-Fassung liefert (v4p_input_t.level_valid == 0).
 *
 * missing_pullup: losgelassen (nicht gezogen), liest aber Low. Der ESP32 gibt
 *   die Leitung frei, und es zieht sie niemand hoch -- der Pull-up der Vampire
 *   fehlt oder die Platine haengt nicht am Port. Das ist der haeufigste
 *   Messbefund und sieht ohne diese Auswertung wie "nichts gedrueckt" aus.
 *
 * impossible: gezogen, liest aber High. Das kann elektrisch nicht sein und
 *   deutet auf Ausgangsstufe, Diode oder Pinbelegung -- also auf einen Fehler,
 *   den man nicht durch Weitermessen findet.
 */
uint8_t v4p_level_missing_pullup(uint8_t drive_bits, uint8_t level_bits);
uint8_t v4p_level_impossible(uint8_t drive_bits, uint8_t level_bits);

/* ------------------------------------------------------------------ */
/* Selbsttests und Diagnose                                            */
/* ------------------------------------------------------------------ */

/* Muss IMMER 1 liefern -- sonst ist die Byte-Order-Behandlung kaputt (§0). */
int v4p_selftest_byteorder(void);

/* Text zu einem V4P_CHECK_*-Wert. */
const char *v4p_check_str(int rc);

#endif /* V4_PROTO_H */
