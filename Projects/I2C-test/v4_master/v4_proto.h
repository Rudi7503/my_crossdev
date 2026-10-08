/*
 * v4_proto.h -- gemeinsamer Vertrag Vampire V4 (I2C-Master) <-> ESP32 (I2C-Slave)
 *
 * Implementierung von PROTOCOL_V4_MASTER.md, Version 2.0 (Proto-Version 2).
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

/* proto_ver 3 kennzeichnet das 128-Byte-READ-Format. proto_ver 2 las 64 Byte
 * und ist NICHT kompatibel (siehe PROTOCOL_V4_MASTER.md §11, PING/GET_INFO).
 * Der Versionswechsel ist die schnelle und eindeutige Unterscheidung; zusaetzlich
 * prueft v4_check_info() die gemeldeten Rahmengroessen. */
#define V4P_PROTO_VER           3u

#define V4P_MAGIC_WRITE         0xA5u   /* V4 -> ESP32, fest 32 Byte  */
#define V4P_MAGIC_READ          0x5Au   /* ESP32 -> V4,  fest 64 Byte  */
#define V4P_MAGIC_BULK          0x5Bu   /* ESP32 -> V4,  fest 12+chunk */

#define V4P_WRITE_FRAME_LEN     32u
#define V4P_READ_FRAME_LEN      128u    /* §4.2, seit proto_ver 3: 128 statt 64 */
#define V4P_WRITE_PAYLOAD_MAX   27u
#define V4P_READ_PAYLOAD_MAX    121u    /* 128 - 5 Kopf - 1 Flags - 1 CRC (§4.2) */

/* Lage von Flags und CRC im READ-Frame (§4.2). ACHTUNG: die Ueberschrift von
 * §5.2 nennt noch "+62" -- das ist ein Rest der alten 64-Byte-Fassung. */
#define V4P_READ_OFF_FLAGS      126u
#define V4P_READ_OFF_CRC        127u
#define V4P_READ_CRC_RANGE      127u    /* CRC-8 ueber Byte 0..126 */

#define V4P_BULK_HDR_LEN        8u      /* magic cmd status handle block len */
#define V4P_BULK_CRC_LEN        4u
#define V4P_BULK_OVERHEAD       (V4P_BULK_HDR_LEN + V4P_BULK_CRC_LEN) /* 12 */

#define V4P_CHUNK_DEFAULT       128u
#define V4P_CHUNK_MIN           16u
#define V4P_CHUNK_MAX           1024u

#define V4P_PATH_MAX            128u    /* Pfadpuffer auf dem ESP32        */
/* §10: beide Namensgrenzen sind seit proto_ver 3 113 Zeichen (8 + 113 = 121
 * fuellt die READ-Nutzlast exakt aus). Der Name-Puffer muss >= 114 Byte sein. */
#define V4P_DIRNAME_MAX         113u    /* DIR_NEXT kuerzt auf 113 Zeichen */
#define V4P_DEVNAME_MAX         113u    /* DEV_GET kann 113 Zeichen liefern */
#define V4P_NAME_BUF            (V4P_DEVNAME_MAX + 1u)   /* 114 */

#define V4P_MAX_DEVICES         16u
#define V4P_MAX_DIR_HANDLES     2u
#define V4P_MAX_FILE_HANDLES    4u

/* ------------------------------------------------------------------ */
/* Statuscodes (READ- und BULK-Frame) -- §5.1                          */
/* ------------------------------------------------------------------ */

enum {
    V4P_ST_OK           = 0x00,     /* erledigt, Nutzlast gueltig          */
    V4P_ST_BUSY         = 0x01,     /* nicht fertig: ganzen Befehl neu      */
    V4P_ST_BAD_CRC      = 0x02,     /* Request-Framing: Befehl neu, gleiche SEQ */
    V4P_ST_BAD_CMD      = 0x03,     /* unbekannt: nicht wiederholen        */
    V4P_ST_BAD_ARG      = 0x04,     /* Argument unzulaessig                */
    V4P_ST_BAD_STATE    = 0x05,     /* im Zustand nicht erlaubt            */
    V4P_ST_NO_SD        = 0x06,     /* keine Karte / Mount fehlgeschlagen  */
    V4P_ST_NOT_FOUND    = 0x07,     /* Geraet/Pfad/Eintrag fehlt           */
    V4P_ST_IO_ERR       = 0x08,     /* Dateisystem-/Treiberfehler          */
    V4P_ST_END          = 0x09,     /* Liste/Datei zu Ende                 */
    V4P_ST_TOO_LONG     = 0x0A,     /* Pfad/Name zu lang                   */
    V4P_ST_NO_HANDLE    = 0x0B,     /* Handle nicht offen                  */
    V4P_ST_BT_ERR       = 0x0C      /* Bluetooth-Stack hat abgelehnt       */
};

/* ------------------------------------------------------------------ */
/* Flags (READ-Frame +62) -- §5.2                                      */
/* ------------------------------------------------------------------ */

#define V4P_FLAG_NAME_TRUNCATED 0x01u
#define V4P_FLAG_MORE           0x02u   /* reserviert, aktuell immer 0 */

/* §11 GET_STATUS, Byte +5: Bitfeld statt eines einfachen Ja/Nein. */
#define V4P_AUDIO_A2DP_STREAMING 0x01u  /* A2DP steht und wird versorgt */
#define V4P_AUDIO_SD_PLAYBACK    0x02u  /* Tonquelle ist eine SD-Datei (PLAY_FILE) */

/* ------------------------------------------------------------------ */
/* Verzeichnis-/Dateiattribute (best effort, §11 / DIR_NEXT)           */
/* ------------------------------------------------------------------ */

#define V4P_ATTR_RO     0x01u
#define V4P_ATTR_DIR    0x10u
#define V4P_ATTR_FILE   0x20u

/* Nutzlast-Offsets der GET_STATUS-Antwort, §11. Die Spezifikation verweist
 * ausdruecklich auf diese Namen ("massgeblich sind die V4P_ST_OFF_*-Offsets").
 * Die Tabelle in §11 ist an dieser Stelle fehlerhaft: sie schiebt ab +6 alles
 * um ein Byte und zeigt ein "—" bei +6, waehrend der Hinweis darunter (und der
 * Golden Frame §12.4) die alten Offsets beibehalten und `sd_card_present` als
 * NEUES Byte bei +16 anhaengen. Umgesetzt ist der Hinweis: L = 17. */
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
    V4P_STATE_IDLE      = 0,
    V4P_STATE_SCANNING  = 1,
    V4P_STATE_CONNECTING= 2,
    V4P_STATE_CONNECTED = 3,
    V4P_STATE_SUSPENDED = 4
};

/* ------------------------------------------------------------------ */
/* Befehlscodes -- §11                                                 */
/* ------------------------------------------------------------------ */

enum {
    V4P_CMD_PING        = 0x01,
    V4P_CMD_GET_STATUS  = 0x02,
    V4P_CMD_GET_INFO    = 0x03,

    V4P_CMD_SCAN_START  = 0x10,
    V4P_CMD_SCAN_STOP   = 0x11,
    V4P_CMD_DEV_COUNT   = 0x12,
    V4P_CMD_DEV_GET     = 0x13,

    V4P_CMD_CONNECT     = 0x20,
    V4P_CMD_CONNECT_BDA = 0x21,
    V4P_CMD_DISCONNECT  = 0x22,
    V4P_CMD_FORGET      = 0x23,

    V4P_CMD_SD_MOUNT    = 0x30,
    V4P_CMD_SD_INFO     = 0x31,
    V4P_CMD_SET_CHUNK   = 0x32,

    V4P_CMD_PATH_CLEAR  = 0x38,
    V4P_CMD_PATH_APPEND = 0x39,

    V4P_CMD_DIR_OPEN    = 0x40,
    V4P_CMD_DIR_NEXT    = 0x41,
    V4P_CMD_DIR_CLOSE   = 0x42,

    V4P_CMD_FILE_OPEN   = 0x50,
    V4P_CMD_FILE_READ   = 0x51,
    V4P_CMD_FILE_CLOSE  = 0x52,

    V4P_CMD_PLAY_FILE   = 0x60,     /* SD-Datei selbst abspielen (§11) */
    V4P_CMD_STOP_PLAY   = 0x61,     /* zurueck auf I2S-Eingang (§11)   */
    V4P_CMD_MEDIA_START = 0x62,     /* A2DP-Uebertragung starten (§11a) */
    V4P_CMD_RESET       = 0x7E
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
    V4P_CHECK_LEN       = -5,       /* len-Feld unzulaessig               */
    V4P_CHECK_MISMATCH  = -6        /* Bulk: handle/block/cmd passt nicht */
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
    uint16_t bulk_payload_max;
    uint16_t chunk;
    uint8_t  max_devices;
    uint8_t  path_max;
    uint8_t  reserved[2];
} v4p_info_t;

/* Antwort auf GET_STATUS, Nutzlast 17 Byte (§11) */
typedef struct {
    uint8_t  state;
    uint8_t  conn_index;    /* 0xFF = keiner */
    uint8_t  dev_count;
    uint8_t  scan_active;
    uint8_t  sd_mounted;
    uint8_t  audio_flags;   /* V4P_AUDIO_* (Bitfeld) */
    uint16_t scan_gen;
    uint32_t sd_free_kb;    /* Schnappschuss vom Mount-Zeitpunkt (§11) */
    uint16_t chunk;
    uint8_t  proto_ver;
    uint8_t  fw_ver;
    uint8_t  sd_card_present; /* Sockelschalter: 1 = Karte eingelegt */
} v4p_status_t;

/* Antwort auf SD_INFO, Nutzlast 11 Byte */
typedef struct {
    uint32_t total_kb;
    uint32_t free_kb;
    uint16_t sector_size;
    uint8_t  fat_type;      /* immer 0 (IDF-VFS liefert ihn nicht) */
} v4p_sdinfo_t;

/* Antwort auf DEV_GET, Nutzlast 8 + name_len (max 57).
 * `flags` kommt aus dem READ-Frame (+62, §5.2) und steht nicht in der
 * Nutzlast -- ohne dieses Feld koennte der Aufrufer NAME_TRUNCATED nicht
 * erkennen und wuerde mit einem gekuerzten Namen weiterarbeiten. */
typedef struct {
    uint8_t  idx;
    uint8_t  bda[6];
    uint8_t  name_len;
    uint8_t  flags;         /* V4P_FLAG_* */
    char     name[V4P_NAME_BUF];
} v4p_dev_t;

/* Antwort auf DIR_NEXT, Nutzlast 8 + name_len (max 56) */
typedef struct {
    uint16_t index;
    uint8_t  attr;
    uint32_t size;          /* bei Verzeichnissen 0 */
    uint8_t  name_len;
    uint8_t  flags;         /* V4P_FLAG_* */
    char     name[V4P_DIRNAME_MAX + 1u];
} v4p_dirent_t;

/* Geparster READ-Frame (64 Byte) */
typedef struct {
    uint8_t cmd;
    uint8_t seq;
    uint8_t status;
    uint8_t len;
    uint8_t flags;
    uint8_t payload[V4P_READ_PAYLOAD_MAX];
} v4p_read_t;

/* Geparster BULK-Frame (12 + chunk Byte). `data` zeigt in den Puffer des
 * Aufrufers und ist genau `len` Byte gueltig; dahinter stehen Fuellbytes. */
typedef struct {
    uint8_t        cmd;
    uint8_t        status;
    uint8_t        handle;
    uint16_t       block;
    uint16_t       len;
    const uint8_t *data;
} v4p_bulk_t;

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
/* Pruefsummen -- §3                                                   */
/* ------------------------------------------------------------------ */

/* CRC-8/SMBUS: Poly 0x07, Init 0x00, keine Reflektion, kein Final-XOR */
uint8_t  v4p_crc8(const uint8_t *d, size_t n);

/* CRC-32/zlib: reflektiert, Poly 0xEDB88320, Init/Final-XOR 0xFFFFFFFF */
uint32_t v4p_crc32(const uint8_t *d, size_t n);

/* ------------------------------------------------------------------ */
/* Frame-Aufbau (Master baut WRITE, Mock/ESP32 bauen READ und BULK)     */
/* ------------------------------------------------------------------ */

/* Alle drei Funktionen liefern 0 bei Erfolg, -1 bei unzulaessiger Laenge. */

int v4p_build_write(uint8_t *f, uint8_t cmd, uint8_t seq,
                    const uint8_t *payload, uint8_t len);

int v4p_build_read(uint8_t *f, uint8_t cmd, uint8_t seq, uint8_t status,
                   uint8_t flags, const uint8_t *payload, uint8_t len);

/* `len` ist die Anzahl GUELTIGER Datenbytes (0..chunk); der Rest der
 * chunk Byte wird mit 0x00 gefuellt und vom CRC mitgedeckt (§4.3). */
int v4p_build_bulk(uint8_t *f, uint16_t chunk, uint8_t cmd, uint8_t status,
                   uint8_t handle, uint16_t block,
                   const uint8_t *data, uint16_t len);

/* ------------------------------------------------------------------ */
/* Frame-Pruefung -- Reihenfolge exakt nach §4.2 / §4.3                 */
/* ------------------------------------------------------------------ */

/* Prueft einen gelesenen READ-Frame. `out->raw` gibt es nicht: die Nutzlast
 * wird hierher kopiert, `f` bleibt unberuehrt. */
int v4p_check_read(const uint8_t *f, size_t got, uint8_t cmd, uint8_t seq,
                   v4p_read_t *out);

/* Prueft einen gelesenen BULK-Frame in place; out->data zeigt danach in `f`. */
int v4p_check_bulk(const uint8_t *f, size_t got, uint16_t chunk, uint8_t cmd,
                   uint8_t handle, uint16_t block, v4p_bulk_t *out);

/* ------------------------------------------------------------------ */
/* Nutzlast-Codec -- von Master UND Mock benutzt, damit kein            */
/* Format-Drift zwischen den beiden Seiten entstehen kann (§14.1)       */
/* ------------------------------------------------------------------ */

/* Encoder liefern die erzeugte Nutzlastlaenge (immer <= 57). */
size_t v4p_enc_get_info  (uint8_t *p, const v4p_info_t   *in);
size_t v4p_enc_get_status(uint8_t *p, const v4p_status_t *in);
size_t v4p_enc_sd_info   (uint8_t *p, const v4p_sdinfo_t *in);
size_t v4p_enc_dev       (uint8_t *p, const v4p_dev_t    *in);
size_t v4p_enc_dirent    (uint8_t *p, const v4p_dirent_t *in);
/* Antwort auf FILE_OPEN, Nutzlast 6 Byte: handle, size u32, attr */
size_t v4p_enc_file_open (uint8_t *p, uint8_t handle, uint32_t size,
                          uint8_t attr);

/* Decoder liefern 0 bei Erfolg, -1 wenn `len` nicht ausreicht. */
int v4p_dec_get_info  (const uint8_t *p, uint8_t len, v4p_info_t   *out);
int v4p_dec_get_status(const uint8_t *p, uint8_t len, v4p_status_t *out);
int v4p_dec_sd_info   (const uint8_t *p, uint8_t len, v4p_sdinfo_t *out);
int v4p_dec_dev       (const uint8_t *p, uint8_t len, v4p_dev_t    *out);
int v4p_dec_dirent    (const uint8_t *p, uint8_t len, v4p_dirent_t *out);
int v4p_dec_file_open (const uint8_t *p, uint8_t len, uint8_t *handle,
                       uint32_t *size, uint8_t *attr);

/* ------------------------------------------------------------------ */
/* Selbsttests und Diagnose                                            */
/* ------------------------------------------------------------------ */

/* Muss IMMER 1 liefern -- sonst ist die Byte-Order-Behandlung kaputt (§0). */
int v4p_selftest_byteorder(void);

/* Text zu einem V4P_CHECK_*-Wert. */
const char *v4p_check_str(int rc);

#endif /* V4_PROTO_H */
