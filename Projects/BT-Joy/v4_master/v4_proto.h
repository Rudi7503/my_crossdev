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
 *   7. NEU mit fw_ver 4 sind die Befehle 0x08..0x0A: SET_DEVICE_PORT (BDA ->
 *      Buchse), GET_PORT_CONFIG (Vorgabe und wirksame Betriebsart,
 *      Richtungsquellen und die drei Knopfquellen beider Buchsen in EINER
 *      Antwort) und SET_PORT_ROUTING (dieselben Felder einer Buchse setzen).
 *      Der RAHMEN bleibt unveraendert (proto_ver 3). Eine Firmware mit
 *      fw_ver 4 darf deshalb NICHT abgelehnt werden; ob die Befehle da sind,
 *      sagen die Faehigkeitsbits V4P_CAP_PORT_ROUTING/V4P_CAP_DEVICE_PORT.
 *   8. SET_PORT_MODE kennt jetzt mode = V4P_MODE_AUTO (0xFF) und setzt die
 *      Vorgabe auf "nach Geraeteart" zurueck. GET_PORT_MODE meldet
 *      unveraendert die WIRKSAME Betriebsart (nie 0xFF).
 *   9. NEU mit fw_ver 5 sind die Befehle 0x0B/0x0C: GET_MOUSE_CFG (Tempo und
 *      Totzone BEIDER Buchsen in einer 4-Byte-Antwort) und SET_MOUSE_CFG
 *      ([port][speed][totzone], 3 Byte, Echo). Auch hier bleibt der Rahmen
 *      unveraendert (proto_ver 3); verbindlich ist das Faehigkeitsbit
 *      V4P_CAP_MOUSE_CFG, nicht die Versionsnummer. Die Oberflaeche rechnet
 *      NICHTS um -- sie zeigt und setzt Stufe (1..10) und Prozent (0..100).
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
 * NICHTS ueber die Rahmengeometrie -- das tut proto_ver.
 *
 * fw_ver 4 bringt die Eingangsverteilung: GET_PORT_CONFIG (0x09),
 * SET_PORT_ROUTING (0x0A) und SET_DEVICE_PORT (0x08). fw_ver 5 bringt die
 * Maus-Einstellungen: GET_MOUSE_CFG (0x0B) und SET_MOUSE_CFG (0x0C). fw_ver 6
 * bringt den duo-Betrieb (GET_DUO 0x0D / SET_DUO 0x0E: EIN Controller auf
 * BEIDEN Buchsen) und das Verbunden-Byte in DEV_GET. Rahmen und proto_ver
 * bleiben in ALLEN Faellen UNVERAENDERT (proto_ver 3, 32 -> 128 Byte) -- eine
 * Firmware mit fw_ver 4, 5 oder 6 darf deshalb NICHT abgelehnt werden.
 * v4_check_info() prueft ausschliesslich proto_ver und Rahmengroessen; wer
 * wissen will, ob die Befehle da sind, fragt die Faehigkeitsbits
 * V4P_CAP_PORT_ROUTING / V4P_CAP_DEVICE_PORT / V4P_CAP_MOUSE_CFG /
 * V4P_CAP_DUO / V4P_CAP_DEV_STATE ab (v4_info_has_caps). */
#define V4P_FW_VER              6u
/* Der Stand, ab dem es die Eingangsverteilung gibt (nur als Klartext/Pruefung
 * gedacht -- die verbindliche Auskunft sind die Faehigkeitsbits). */
#define V4P_FW_VER_ROUTING      4u
/* Der Stand, ab dem es die Maus-Einstellungen (Tempo/Totzone) gibt. Auch hier
 * gilt: verbindlich ist das Faehigkeitsbit V4P_CAP_MOUSE_CFG, nicht die Zahl. */
#define V4P_FW_VER_MOUSE        5u
/* Der Stand, ab dem es duo (GET_DUO/SET_DUO) gibt. Verbindlich ist
 * V4P_CAP_DUO. */
#define V4P_FW_VER_DUO          6u
/* Der Stand, ab dem DEV_GET das Verbunden-Byte enthaelt (Layoutwechsel!).
 * Verbindlich ist V4P_CAP_DEV_STATE. */
#define V4P_FW_VER_DEV_STATE    6u

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
/* fw_ver 4: Eingangsverteilung und Geraetezuordnung. Diese beiden Bits sind
 * die VERBINDLICHE Auskunft darueber, ob die neuen Befehle vorhanden sind --
 * nicht die Firmware-Version. Ein Master, der sie nicht kennt, liest sie
 * nicht aus und schickt die Befehle ins Leere (BAD_CMD). */
#define V4P_CAP_PORT_ROUTING    0x0100u  /* GET_PORT_CONFIG/SET_PORT_ROUTING */
#define V4P_CAP_DEVICE_PORT     0x0200u  /* SET_DEVICE_PORT               */
/*
 * Maus-Einstellungen (Geschwindigkeit, Totzone): GET/SET_MOUSE_CFG (fw_ver 5).
 *
 * BESTAETIGT mit der ESP32-Seite (14.10.2026): Befehl 0x0B/0x0C, Faehigkeitsbit
 * 0x0400, speed 1..10 (Vorgabe 7), totzone 0..100 Prozent (Vorgabe 12). Sie
 * stehen -- wie alle Nummern dieses Vertrags -- NUR HIER; der Mock benutzt
 * dieselben Makros, damit die beiden Seiten nicht auseinanderlaufen koennen.
 * Vorhandene Bits 0x0001..0x0200 sind belegt, 0x0400 ist frei.
 */
#define V4P_CAP_MOUSE_CFG       0x0400u  /* GET_MOUSE_CFG/SET_MOUSE_CFG   */
/*
 * duo (fw_ver 6): EIN Controller auf BEIDEN Buchsen -- die eine als Maus
 * (Quadratur), die andere als Joystick. GET_DUO 0x0D / SET_DUO 0x0E.
 *
 * WARUM DAS NICHT DIE GERAETEZUORDNUNG IST: duo setzt AUSSCHLIESSLICH die
 * Betriebsarten. Ein Geraet hat im ESP32 weiterhin genau EINE gemerkte Buchse
 * (NVS-Feld "port"); duo tastet sie nicht an, und wenn duo endet, gilt sie
 * sofort wieder. Wer dieselbe BDA fuer beide Buchsen eintragen will, kommt
 * damit also nicht zum Ziel -- siehe v4_devices_zuordnen() der ESP32-Seite.
 */
#define V4P_CAP_DUO             0x0800u  /* GET_DUO/SET_DUO               */
/*
 * DEV_GET hat ab fw_ver 6 ein Verbunden-Byte VOR name_len (Layoutwechsel:
 * name_len 7 -> 8, name 8 -> 9). Ein Master MUSS beide Layouts kennen: ohne
 * dieses Bit gilt das alte, sonst zeigt er Namen als Muell an.
 */
#define V4P_CAP_DEV_STATE       0x1000u  /* DEV_GET: Verbunden-Byte (fw 6) */

/* ------------------------------------------------------------------ */
/* Die zwei DB9-Buchsen -- V4P_PORT_* / V4P_IN_* der ESP32-Seite        */
/* ------------------------------------------------------------------ */

#define V4P_PORT_COUNT          2u

/* Betriebsarten, identisch zu joy_mode_t in joy_port.h */
#define V4P_PORT_OFF            0u
#define V4P_PORT_JOY            1u
#define V4P_PORT_MOUSE          2u
#define V4P_PORT_MODE_MAX       V4P_PORT_MOUSE

/*
 * "nach Geraeteart" -- die VORGABE einer Buchse ist ungesetzt (fw_ver 4).
 *
 * Der Wert steht NUR in SET_PORT_MODE (Anfrage) und in GET_PORT_CONFIG
 * (`mode_soll`), NIEMALS in GET_PORT_MODE: das meldet die WIRKSAME
 * Betriebsart, und die ist immer 0..2. Wer die beiden Felder vermischt,
 * zeigt "auto" an, obwohl gerade eine Maus laeuft.
 */
#define V4P_MODE_AUTO           0xFFu

/* ------------------------------------------------------------------ */
/* Eingangsverteilung je Buchse (GET_PORT_CONFIG / SET_PORT_ROUTING)   */
/* ------------------------------------------------------------------ */

/* Richtungsquellen (`dir`): ein Bit je Quelle. 0x00 = keine Richtung, die
 * Buchse meldet dann nur Knoepfe. 0x07 = alle drei = Vorgabe. */
#define V4P_DIR_STICK_L         0x01u   /* Analog-Stick links             */
#define V4P_DIR_STICK_R         0x02u   /* Analog-Stick rechts            */
#define V4P_DIR_DPAD            0x04u   /* digitales Steuerkreuz          */
#define V4P_DIR_ALLE            0x07u   /* alle drei                      */
#define V4P_DIR_STANDARD        V4P_DIR_ALLE
#define V4P_DIR_MASK            0x07u   /* erlaubte Bits                  */

/* Die drei Knopfleitungen einer Buchse: Pin 6, Pin 9, Pin 5 -- in DIESER
 * Reihenfolge stehen sie in SET_PORT_ROUTING und in GET_PORT_CONFIG. */
#define V4P_BTN_COUNT           3u

/* Knopfquellen. Jede der drei Leitungen bekommt GENAU EINEN Code; 0 = die
 * Leitung bleibt offen. 1..13 sind Einzeltasten, 14..16 die drei bisherigen
 * Gruppen (Vorgabe -- sie reproduzieren das Verhalten vor dieser Erweiterung).
 * Identisch zu V4P_TASTE_* der ESP32-Seite. */
#define V4P_TASTE_KEINE         0u
#define V4P_TASTE_A             1u
#define V4P_TASTE_B             2u
#define V4P_TASTE_X             3u
#define V4P_TASTE_Y             4u
#define V4P_TASTE_L1            5u
#define V4P_TASTE_R1            6u
#define V4P_TASTE_L2            7u
#define V4P_TASTE_R2            8u
#define V4P_TASTE_L3            9u
#define V4P_TASTE_R3           10u
#define V4P_TASTE_START        11u
#define V4P_TASTE_SELECT       12u
#define V4P_TASTE_HOME         13u
#define V4P_TASTE_GRUPPE1      14u
#define V4P_TASTE_GRUPPE2      15u
#define V4P_TASTE_GRUPPE3      16u
#define V4P_TASTE_ANZAHL       17u     /* Anzahl der Codes (0..16)       */

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

/* GET_PORT_CONFIG (0x09): alles, was eine Oberflaeche je Buchse braucht, in
 * EINER Antwort (12 Byte). `mode_soll` ist die Vorgabe (0xFF = auto),
 * `mode_ist` die wirksame Betriebsart -- dieselbe, die GET_PORT_MODE liefert.
 * `btn?` sind die drei Knopfquellen in der Reihenfolge Pin 6, Pin 9, Pin 5. */
#define V4P_GPC_OFF_MODE_SOLL0  0u
#define V4P_GPC_OFF_MODE_SOLL1  1u
#define V4P_GPC_OFF_MODE_IST0   2u
#define V4P_GPC_OFF_MODE_IST1   3u
#define V4P_GPC_OFF_DIR0        4u
#define V4P_GPC_OFF_DIR1        5u
#define V4P_GPC_OFF_BTN0        6u      /* 3 Byte: Pin6, Pin9, Pin5       */
#define V4P_GPC_OFF_BTN1        9u
#define V4P_GPC_PAYLOAD_LEN    12u

/* SET_PORT_ROUTING (0x0A): Anfrage und Antwort haben dasselbe Layout. */
#define V4P_SPR_OFF_PORT        0u
#define V4P_SPR_OFF_DIR         1u
#define V4P_SPR_OFF_BTN1        2u      /* Pin 6                          */
#define V4P_SPR_OFF_BTN2        3u      /* Pin 9                          */
#define V4P_SPR_OFF_BTN3        4u      /* Pin 5                          */
#define V4P_SPR_PAYLOAD_LEN     5u

/* SET_DEVICE_PORT (0x08): Anfrage und Antwort (7 Byte). */
#define V4P_SDP_OFF_BDA         0u      /* Bluetooth-Adresse, 6 Byte      */
#define V4P_SDP_OFF_PORT        6u      /* Zielbuchse 0 oder 1            */
#define V4P_SDP_PAYLOAD_LEN     7u
#define V4P_BDA_LEN             6u

/* ------------------------------------------------------------------ */
/* Maus-Einstellungen (GET_MOUSE_CFG 0x0B / SET_MOUSE_CFG 0x0C)        */
/*                                                                     */
/* WAS DIE FIRMWARE MACHT UND WAS NICHT: `speed` ist eine STUFE 1..10   */
/* (1 = langsamste, 10 = schnellste, Vorgabe 7). Die Umrechnung in      */
/* wirkliche Schritte je Sekunde macht AUSSCHLIESSLICH die Firmware;    */
/* die Oberflaeche zeigt und setzt nur die Stufe und rechnet nichts um. */
/* `totzone` ist ein PROZENTWERT 0..100 (Vorgabe 12) -- die Firmware     */
/* rechnet ihn in ihre Rohwerte um (prozent * 512 / 100), auch das      */
/* macht NICHT die Oberflaeche.                                         */
/*                                                                     */
/* PRO BUCHSE: GET_MOUSE_CFG liefert in EINER Antwort beide Buchsen (4  */
/* Byte), SET_MOUSE_CFG setzt EINE Buchse (3 Byte mit Port). Das ist    */
/* dieselbe Aufteilung wie GET_PORT_CONFIG gegen SET_PORT_MODE.         */
/*                                                                     */
/* BESTAETIGT mit der ESP32-Seite (14.10.2026). Die Nummern, Laengen    */
/* und Grenzen stehen NUR HIER, damit eine Korrektur eine Zeile ist;    */
/* der Mock benutzt dieselben Makros und kann nicht auseinanderlaufen.  */
/* ------------------------------------------------------------------ */
#define V4P_MCFG_OFF_SPEED0     0u      /* Stufe Buchse 0, 1..10          */
#define V4P_MCFG_OFF_TOTZONE0   1u      /* Prozent Buchse 0, 0..100       */
#define V4P_MCFG_OFF_SPEED1     2u      /* Stufe Buchse 1, 1..10          */
#define V4P_MCFG_OFF_TOTZONE1   3u      /* Prozent Buchse 1, 0..100       */
#define V4P_MCFG_PAYLOAD_LEN    4u      /* GET-Antwort: beide Buchsen     */

#define V4P_SMC_OFF_PORT        0u      /* Zielbuchse 0 oder 1            */
#define V4P_SMC_OFF_SPEED       1u      /* Stufe 1..10                    */
#define V4P_SMC_OFF_TOTZONE     2u      /* Prozent 0..100                 */
#define V4P_SMC_PAYLOAD_LEN     3u      /* SET-Anfrage und Echo           */

#define V4P_MOUSE_SPEED_MIN     1u
#define V4P_MOUSE_SPEED_MAX     10u
/* Die Vorgaben der Firmware -- hier nur als Klartext/Pruefung. Wer sie
 * anzeigt, zeigt die Vorgabe; gesetzt wird immer der bestaetigte Wert. */
#define V4P_MOUSE_SPEED_VORGABE   7u
#define V4P_MOUSE_TOTZONE_VORGABE 12u
#define V4P_MOUSE_TOTZONE_MIN   0u
#define V4P_MOUSE_TOTZONE_MAX   100u

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
#define V4P_DEV_OFF_BDA         1u      /* 6 Byte, beide Layouts          */
/*
 * ACHTUNG, LAYOUTWECHSEL mit fw_ver 6 (V4P_CAP_DEV_STATE): das Verbunden-Byte
 * liegt VOR name_len, name_len und name ruecken um ein Byte nach hinten.
 * Welches Layout gilt, sagt AUSSCHLIESSLICH das Faehigkeitsbit -- die Nutzlast
 * selbst ist nicht eindeutig (ein Verbunden-Byte 0 sieht aus wie name_len 0).
 */
#define V4P_DEV_OFF_CONNECTED   7u      /* NEU (fw 6): 1 = verbunden      */
#define V4P_DEV_OFF_NAME_LEN_ALT 7u     /* alt (fw < 6)                   */
#define V4P_DEV_OFF_NAME_ALT    8u      /* alt (fw < 6)                   */
#define V4P_DEV_OFF_NAME_LEN    8u      /* neu (fw 6)                     */
#define V4P_DEV_OFF_NAME        9u      /* neu (fw 6)                     */

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

    /* fw_ver 4: Eingangsverteilung -- siehe v4p_portcfg_t. */
    V4P_CMD_SET_DEVICE_PORT  = 0x08, /* [bda 6][port u8] -> Echo (7 Byte)    */
    V4P_CMD_GET_PORT_CONFIG  = 0x09, /* -  -> v4p_portcfg_t (12 Byte)        */
    V4P_CMD_SET_PORT_ROUTING = 0x0A, /* [port][dir][b1][b2][b3] -> Echo (5)  */

    /* Maus-Einstellungen. BESTAETIGTE Nummern (siehe V4P_CAP_MOUSE_CFG): der
     * ESP32 kann eine Maus emulieren, aber Tempo und Totzone erst ab fw_ver 5
     * einstellen. GET_MOUSE_CFG liefert in EINER Antwort BEIDE Buchsen
     * (speed0/totzone0/speed1/totzone1), SET_MOUSE_CFG setzt EINE Buchse
     * ([port][speed][totzone]) -- dieselbe Aufteilung wie GET_PORT_CONFIG
     * gegen SET_PORT_MODE. */
    V4P_CMD_GET_MOUSE_CFG    = 0x0B, /* - -> 4 Byte (beide Buchsen)         */
    V4P_CMD_SET_MOUSE_CFG    = 0x0C, /* [port][speed][totzone] -> Echo (3)  */

    /*
     * duo (fw_ver 6). GET_DUO liefert 1 Byte (den Zustand), SET_DUO nimmt
     * 1 Byte und antwortet mit demselben Byte (Echo). Erlaubt sind NUR
     * V4P_DUO_OFF / V4P_DUO_MOUSE_PORT0 / V4P_DUO_MOUSE_PORT1; alles andere
     * ergibt BAD_ARG, und die alte Einstellung bleibt.
     */
    V4P_CMD_GET_DUO          = 0x0D, /* -          -> [duo u8]              */
    V4P_CMD_SET_DUO          = 0x0E, /* [duo u8]   -> Echo (1 Byte)         */

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
/* duo (GET_DUO/SET_DUO) -- Lage und Werte, identisch zur ESP32-Seite   */
/* ------------------------------------------------------------------ */

#define V4P_DUO_OFF_STATE       0u      /* Lage des Bytes in der Nutzlast */
#define V4P_DUO_PAYLOAD_LEN     1u      /* Anfrage UND Antwort            */

#define V4P_DUO_OFF             0u      /* aus: je Controller seine Buchse */
#define V4P_DUO_MOUSE_PORT0     1u      /* an: Port 0 Maus, Port 1 Joystick */
#define V4P_DUO_MOUSE_PORT1     2u      /* an: Port 1 Maus, Port 0 Joystick */

typedef struct {
    uint8_t idx;
    uint8_t bda[6];
    uint8_t name_len;
    uint8_t flags;              /* V4P_FLAG_*                             */
    char    name[V4P_NAME_BUF];
    /*
     * NUR fw_ver 6 (V4P_CAP_DEV_STATE): 1 = dieses Geraet ist gerade
     * verbunden, 0 = nicht. Bei der alten Nutzlast gibt es das Feld nicht --
     * dann steht hier V4P_DEV_VERBUNDEN_UNBEKANNT (-1), und die Oberflaeche
     * sagt "unbekannt" statt "nicht verbunden".
     */
    int     verbunden;
} v4p_dev_t;

/* Der Wert in v4p_dev_t.verbunden, wenn die Nutzlast das Feld NICHT hat
 * (fw_ver < 6 / V4P_CAP_DEV_STATE fehlt). */
#define V4P_DEV_VERBUNDEN_UNBEKANNT (-1)

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

/*
 * Antwort auf GET_PORT_CONFIG -- die Eingangsverteilung BEIDER Buchsen
 * (fw_ver 4). Das ist die eine Quelle, aus der die Oberflaeche ihre
 * Auswahlfelder fuellt; GET_PORT_MODE bleibt daneben gueltig und liefert
 * dieselbe wirksame Betriebsart wie mode_ist[].
 *
 * WICHTIG: `mode_soll[]` darf V4P_MODE_AUTO (0xFF) sein, `mode_ist[]` NIE.
 * Wer sie in ein gemeinsames Feld schreibt, zeigt "auto", obwohl eine Maus
 * laeuft -- die beiden Felder sind bewusst getrennt.
 */
typedef struct {
    uint8_t  mode_soll[V4P_PORT_COUNT];             /* 0..2 oder AUTO      */
    uint8_t  mode_ist[V4P_PORT_COUNT];              /* 0..2 (wirksam)      */
    uint8_t  dir[V4P_PORT_COUNT];                   /* V4P_DIR_*           */
    uint8_t  btn[V4P_PORT_COUNT][V4P_BTN_COUNT];    /* Pin6, Pin9, Pin5    */
} v4p_portcfg_t;

/*
 * Antwort auf GET_MOUSE_CFG (4 Byte) -- die Maus-Einstellungen BEIDER Buchsen.
 * SET_MOUSE_CFG setzt daraus EINEN Eintrag (siehe v4p_enc_set_mouse_cfg); das
 * Echo traegt Port, Stufe und Prozentwert dieser einen Buchse.
 *
 * Die Werte sind STUFEN, keine physikalischen Groessen: die Firmware rechnet
 * `speed` in Schritte je Sekunde und `totzone` (Prozent) in ihre Rohwerte um.
 * Wer hier etwas umrechnet, zeigt etwas anderes an, als am Port gilt.
 */
typedef struct {
    uint8_t speed[V4P_PORT_COUNT];      /* V4P_MOUSE_SPEED_MIN..MAX (1..10) */
    uint8_t totzone[V4P_PORT_COUNT];    /* V4P_MOUSE_TOTZONE_MIN..MAX, %    */
} v4p_mousecfg_t;

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
/* NEUES Layout (fw_ver 6, mit Verbunden-Byte). */
size_t v4p_enc_dev       (uint8_t *p, const v4p_dev_t    *in);
/* ALTES Layout (fw_ver < 6, name_len bei 7). Nur fuer Tests/Mock. */
size_t v4p_enc_dev_alt   (uint8_t *p, const v4p_dev_t    *in);
size_t v4p_enc_get_port_mode(uint8_t *p, const uint8_t mode[V4P_PORT_COUNT]);
size_t v4p_enc_set_port_mode(uint8_t *p, uint8_t port, uint8_t mode);

/* fw_ver 4: Eingangsverteilung. Die Encoder liefern die Laenge, die Decoder 0
 * bei Erfolg und -1, wenn `len` nicht reicht ODER ein Feld unzulaessig ist.
 * Die Decoder sind damit zugleich die Pruefung der Echo-Antwort: was hier
 * durchfaellt, darf der Master nicht als Bestaetigung ansehen. */
size_t v4p_enc_get_port_config(uint8_t *p, const v4p_portcfg_t *in);
int    v4p_dec_get_port_config(const uint8_t *p, uint8_t len,
                               v4p_portcfg_t *out);

size_t v4p_enc_set_port_routing(uint8_t *p, uint8_t port, uint8_t dir,
                                const uint8_t btn[V4P_BTN_COUNT]);
int    v4p_dec_set_port_routing(const uint8_t *p, uint8_t len, uint8_t *port,
                                uint8_t *dir, uint8_t btn[V4P_BTN_COUNT]);

size_t v4p_enc_set_device_port(uint8_t *p, const uint8_t bda[V4P_BDA_LEN],
                               uint8_t port);
int    v4p_dec_set_device_port(const uint8_t *p, uint8_t len,
                               uint8_t bda[V4P_BDA_LEN], uint8_t *port);

/* Maus-Einstellungen. GET_MOUSE_CFG liefert BEIDE Buchsen in 4 Byte (Encoder
 * liefert 4, Decoder prueft Laenge UND beide Wertebereiche). SET_MOUSE_CFG
 * setzt EINE Buchse in 3 Byte ([port][speed][totzone]); der Encoder liefert 3,
 * der Decoder gibt Port und die beiden Werte dieser Buchse aus und liefert -1,
 * wenn `len` nicht reicht, der Port unzulaessig ist ODER ein Wert ausserhalb
 * der Grenzen liegt -- damit ist der Decoder zugleich die Pruefung der
 * Echo-Antwort. `out` wird beim GET-Decoder nur bei Erfolg beschrieben. */
size_t v4p_enc_get_mouse_cfg(uint8_t *p, const v4p_mousecfg_t *in);
int    v4p_dec_get_mouse_cfg(const uint8_t *p, uint8_t len,
                             v4p_mousecfg_t *out);
size_t v4p_enc_set_mouse_cfg(uint8_t *p, uint8_t port,
                             const v4p_mousecfg_t *in);
int    v4p_dec_set_mouse_cfg(const uint8_t *p, uint8_t len, uint8_t *port,
                             uint8_t *speed, uint8_t *totzone);

/* Decoder liefern 0 bei Erfolg, -1 wenn `len` nicht ausreicht bzw. ein Feld
 * unzulaessig ist. */
int v4p_dec_get_info  (const uint8_t *p, uint8_t len, v4p_info_t   *out);
int v4p_dec_get_status(const uint8_t *p, uint8_t len, v4p_status_t *out);
int v4p_dec_get_input (const uint8_t *p, uint8_t len, v4p_input_t  *out);
int v4p_dec_get_hid   (const uint8_t *p, uint8_t len, v4p_hid_t    *out);
/* Layoutwahl ueber das FAEHIGKEITSBIT V4P_CAP_DEV_STATE (nicht ueber die
 * Nutzlast -- die ist nicht eindeutig). Ohne das Bit gilt das alte Layout,
 * und out->verbunden steht auf V4P_DEV_VERBUNDEN_UNBEKANNT. */
int v4p_dec_dev       (const uint8_t *p, uint8_t len, uint16_t caps,
                       v4p_dev_t    *out);
int v4p_dec_get_port_mode(const uint8_t *p, uint8_t len,
                          uint8_t mode[V4P_PORT_COUNT]);
int v4p_dec_set_port_mode(const uint8_t *p, uint8_t len, uint8_t *port,
                          uint8_t *mode);

/* ------------------------------------------------------------------ */
/* Pruefungen der Eingangsverteilung                                   */
/*                                                                     */
/* Sie stehen hier und nicht nur im Master, weil der Mock (ESP32-Seite  */
/* des Harness) DIESELBEN Regeln anwenden muss. Zwei Schreibweisen der   */
/* Regel waeren genau die Doppelpflege, aus der "der Master laesst      */
/* durch, was der Slave ablehnt" entsteht.                              */
/* ------------------------------------------------------------------ */

/* 1 = dir enthaelt kein unbekanntes Bit. */
int v4p_dir_valid(uint8_t dir);

/* 1 = mode ist eine gueltige VORGABE: 0..2 oder V4P_MODE_AUTO. */
int v4p_mode_soll_valid(uint8_t mode);

/* 1 = code ist eine bekannte Knopfquelle (0..V4P_TASTE_ANZAHL-1). */
int v4p_taste_valid(uint8_t code);

/* 1 = die drei Quellen sind zulaessig: alle bekannt, und eine NICHT-LEERE
 * Quelle steht hoechstens einmal (V4P_TASTE_KEINE darf mehrfach vorkommen).
 * Genau diese Regel lehnt der ESP32 mit BAD_ARG ab. */
int v4p_btn_set_valid(const uint8_t btn[V4P_BTN_COUNT]);

/* 1 = speed liegt in V4P_MOUSE_SPEED_MIN..MAX. */
int v4p_mouse_speed_valid(uint8_t speed);
/* 1 = totzone (Prozent) liegt in V4P_MOUSE_TOTZONE_MIN..MAX. */
int v4p_mouse_totzone_valid(uint8_t totzone);
/* 1 = die Werte BEIDER Buchsen sind zulaessig. `c` darf nicht NULL sein --
 * ein fehlender Wert ist kein gueltiger Wert. */
int v4p_mouse_cfg_valid(const v4p_mousecfg_t *c);

/* ---- duo (GET_DUO/SET_DUO, fw_ver 6) ---- */

/* 1 = der Wert ist einer der drei erlaubten (0/1/2). */
int v4p_duo_valid(uint8_t duo);

size_t v4p_enc_duo(uint8_t *p, uint8_t duo);
int    v4p_dec_duo(const uint8_t *p, uint8_t len, uint8_t *out);

/* Klartext fuer Fenster und Protokoll ("aus", "Port 0 Maus / Port 1
 * Joystick", "Port 1 Maus / Port 0 Joystick", sonst "?"). */
const char *v4p_duo_name(uint8_t duo);
void        v4p_duo_text(uint8_t duo, char *out, size_t cap);

/* ------------------------------------------------------------------ */
/* Klartexte und Auswertungen fuer die Oberflaechen                    */
/*                                                                     */
/* Sie stehen hier und nicht in jeder Anzeige neu, weil Konsole, MUI und */
/* Mock sonst drei leicht verschiedene Schreibweisen pflegen wuerden --   */
/* genau daraus entstehen "Anzeige stimmt nicht mit dem Befehl ueberein"- */
/* Fehler.                                                                 */
/* ------------------------------------------------------------------ */

const char *v4p_port_mode_name(uint8_t mode);   /* "aus" / "Joystick" / "Maus" */

/* Name einer VORGABE: wie v4p_port_mode_name(), aber 0xFF heisst "auto"
 * ("nach Geraeteart"). GET_PORT_CONFIG.mode_soll und SET_PORT_MODE benutzen
 * diesen Wertebereich. */
const char *v4p_port_mode_soll_name(uint8_t mode);

/* Naechste Betriebsart im Kreislauf aus -> Joystick -> Maus -> aus.
 * Genau das tun die Schaltknoepfe der Oberflaechen. */
uint8_t v4p_port_mode_next(uint8_t mode);

/* Richtungsquelle Bit 0..2 (0x01 Stick links, 0x02 Stick rechts, 0x04 D-Pad).
 * Bitnummer, nicht Maske -- wie v4p_bit_name(). */
const char *v4p_dir_name(unsigned bit);

/* Maske in Klartext, z.B. "Stick links+D-Pad". 0 ergibt "keine".
 * out ist immer nullterminiert. */
void v4p_dir_text(uint8_t dir, char *out, size_t cap);

/* Maus-Einstellungen EINER Buchse in Klartext, z.B. "Tempo 7/10, Totzone 12%".
 * EINE Schreibweise fuer Konsole, Oberflaeche und Debugausgabe. out ist immer
 * nullterminiert. */
void v4p_mousecfg_text(const v4p_mousecfg_t *c, uint8_t port,
                       char *out, size_t cap);

/* Name einer Knopfquelle (V4P_TASTE_*), z.B. "L1" oder "Gruppe 1 (A,L1,L2,L3)".
 * Der lange Name ist Absicht: die Gruppen sind sonst nicht zu unterscheiden. */
const char *v4p_taste_name(uint8_t code);
/* Derselbe Name, aber hoechstens `cap` Zeichen -- fuer enge Auswahlfelder.
 * Aus "Gruppe 1 (A,L1,L2,L3)" wird "Gruppe 1". */
const char *v4p_taste_name_short(uint8_t code);

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
