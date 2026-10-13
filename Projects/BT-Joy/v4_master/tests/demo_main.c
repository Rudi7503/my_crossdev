/*
 * demo_main.c -- Vorfuehr- und Testrahmen fuer das echte Konsolenprogramm.
 *
 * v4_console.c wird hier mit -DV4_CONSOLE_NO_MAIN uebersetzt und gegen den Mock
 * gelinkt (statt gegen i2c-dev). Der Rahmen bestueckt den Mock und uebergibt
 * dann an v4_console_main(). Damit laeuft der Demo-Ziel des Makefiles OHNE
 * Hardware -- und zwar das echte Programm, nicht ein Nachbau davon.
 *
 * Szenarien ueber die Umgebung (nicht ueber Schalter: das Programm lehnt
 * unbekannte Argumente bewusst ab, und das soll so bleiben):
 *
 *   V4_DEMO_DRIVE0=n      Zug-Maske Buchse 0 (Bit 0 UP ... Bit 6 BTN3)
 *   V4_DEMO_DRIVE1=n      Zug-Maske Buchse 1
 *   V4_DEMO_LEVEL0=n      Pegel-Maske Buchse 0 (1 = High, Ruhe 0x7F).
 *                         Ohne Angabe bleibt es beim Erwartungswert, den
 *                         set_port_state gesetzt hat (gezogen = Low).
 *   V4_DEMO_LEVEL1=n      Pegel-Maske Buchse 1
 *   V4_DEMO_LEGACY_INPUT=1  GET_INPUT antwortet mit der alten 10-Byte-Fassung
 *                         OHNE Pegel (Rueckfallweg der Anzeige)
 *   V4_DEMO_LEGACY_FW3=1  Firmware OHNE Eingangsverteilung: fw_ver 3,
 *                         Faehigkeiten 0x00FF, die Befehle 0x08..0x0A
 *                         antworten BAD_CMD. Damit laesst sich pruefen, dass
 *                         die V4 eine solche Firmware ANNIMMT (gleicher
 *                         Rahmen) und die neuen Felder nicht blind setzt.
 *   V4_DEMO_MOUSE=1       Controller ist eine Maus, Buchse 1 auf Maus
 *   V4_DEMO_CONNECT_ROUNDS=n  so viele GET_STATUS-Runden CONNECTING
 *   V4_DEMO_BAD_ECHO=1    SET_PORT_MODE bestaetigt den falschen Modus
 *   V4_DEMO_NO_DEVICES=1  leere Geraeteliste (Scan findet nichts)
 *   V4_DEMO_ALREADY=1     "es ist schon jemand verbunden" (Index 0)
 *   V4_DEMO_LINK_TOT=1    der Bus ist tot: kein Schreiben, kein Lesen kommt
 *                         durch ("ESP32 antwortet nicht"). Damit laesst sich
 *                         pruefen, dass die Konsole das AUSDRUECKLICH meldet
 *                         und sich beendet, statt zu warten.
 */

#include "../v4_master.h"
#include "../v4_mock.h"

#include <stdlib.h>

int v4_console_main(int argc, char **argv);

/* Kleiner Helfer: Umgebungswert als Zahl, 0 wenn nicht gesetzt. Basis 0, damit
 * die Masken auch als 0x11 geschrieben werden koennen -- eine Zug-Maske liest
 * sich hexadezimal, und ein stilles atoi("0x11") == 0 waere eine Falle. */
static int env_num(const char *name)
{
    const char *v = getenv(name);

    if (v == NULL || v[0] == '\0') {
        return 0;
    }
    return (int)strtol(v, NULL, 0);
}

int main(int argc, char **argv)
{
    v4_mock_reset();

    if (env_num("V4_DEMO_NO_DEVICES") == 0) {
        v4_mock_seed_default();
    }

    if (env_num("V4_DEMO_MOUSE") != 0) {
        v4_mock_set_hid(V4P_HID_KIND_MOUSE, V4P_HID_TR_BLE, 0x046Du, 0xB025u,
                        55u, 2u, 5u, 77u, "Logitech MX Anywhere");
        v4_mock_set_port_mode(1, (uint8_t)V4P_PORT_MOUSE);
    }

    v4_mock_set_port_state(0, (uint8_t)(env_num("V4_DEMO_DRIVE0")
                                        & (int)V4P_IN_ST_MASK));
    v4_mock_set_port_state(1, (uint8_t)(env_num("V4_DEMO_DRIVE1")
                                        & (int)V4P_IN_ST_MASK));

    /*
     * Pegel NACH dem Zug setzen: set_port_state hat sie auf den
     * Erwartungswert gelegt, hier wird bewusst davon abgewichen, wenn ein
     * Szenario einen fehlenden Pull-up zeigen soll.
     */
    if (getenv("V4_DEMO_LEVEL0") != NULL) {
        v4_mock_set_port_level(0, (uint8_t)(env_num("V4_DEMO_LEVEL0")
                                            & (int)V4P_IN_ST_MASK));
    }
    if (getenv("V4_DEMO_LEVEL1") != NULL) {
        v4_mock_set_port_level(1, (uint8_t)(env_num("V4_DEMO_LEVEL1")
                                            & (int)V4P_IN_ST_MASK));
    }

    v4_mock.connect_rounds = env_num("V4_DEMO_CONNECT_ROUNDS");
    if (env_num("V4_DEMO_LEGACY_INPUT") != 0) {
        v4_mock.legacy_input10 = 1;
    }
    if (env_num("V4_DEMO_LEGACY_FW3") != 0) {
        v4_mock.legacy_fw3 = 1;
    }
    if (env_num("V4_DEMO_BAD_ECHO") != 0) {
        v4_mock.bad_set_port_echo = 1;
    }
    if (env_num("V4_DEMO_ALREADY") != 0) {
        v4_mock_set_connected(0);
    }
    if (env_num("V4_DEMO_LINK_TOT") != 0) {
        /* Der Adapter ist weg: weder Annahme noch Antwort. Der Master laeuft
         * dann in V4_ERR_LINK -- so wie an einer gezogenen Leitung. */
        v4_mock.fail_write = 1;
        v4_mock.fail_read  = 1;
    }

    return v4_console_main(argc, argv);
}
