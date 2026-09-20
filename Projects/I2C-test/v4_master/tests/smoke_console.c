/*
 * smoke_console.c -- End-to-End-Smoketest des echten Konsolenprogramms.
 *
 * v4_console.c wird hier gegen den Mock gelinkt (statt gegen i2c-dev) und
 * einmal komplett durchlaufen: PING, Status, Scan, Geraeteliste, Auswahl,
 * CONNECT, SD-Karte, Verzeichnis, Datei lesen, DISCONNECT. Damit wird auch
 * der Code ausgefuehrt, den die Unit-Tests nicht anfassen (read_file_manual,
 * die Ausgabe, die Pfadaufbereitung).
 *
 * Die Eingaben kommen per stdin; die Makefile-Regel `smoke` prueft die
 * Ausgabe auf die erwarteten Zeilen.
 */

#include "../v4_master.h"
#include "../v4_mock.h"

#include <stdlib.h>

int v4_console_main(int argc, char **argv);

int main(int argc, char **argv)
{
    const char *env;

    v4_mock_reset();
    v4_mock_seed_default();

    /* Zwei Faelle, die die Statuspruefung nach CONNECT auslösen sollen:
     *   V4_SMOKE_NO_AUDIO=1        -> verbunden, aber kein A2DP-Stream
     *   V4_SMOKE_CONNECT_ROUNDS=n  -> Verbindung wird nie bestaetigt
     * Ueber die Umgebung, damit die Schalterpruefung des Programms unberuehrt
     * bleibt (unbekannte Argumente lehnt es ab). */
    env = getenv("V4_SMOKE_NO_AUDIO");
    if (env != NULL && env[0] != '\0') {
        v4_mock_set_audio(0);
    }
    /* Ein zusaetzliches Audioformat: damit prueft Szenario 10 die Formatliste
     * des Browsers mit (OGG gehoert zu den abspielbaren Endungen). */
    {
        static const uint8_t pat[8] = { 1u, 2u, 3u, 4u, 5u, 6u, 7u, 8u };

        (void)v4_mock_add_file("MUSIC/C.OGG", pat, 64u);
    }

    env = getenv("V4_SMOKE_ALREADY_CONNECTED");
    if (env != NULL && env[0] != '\0') {
        v4_mock_set_connected(atoi(env));
    }
    env = getenv("V4_SMOKE_CONNECT_ROUNDS");
    if (env != NULL && env[0] != '\0') {
        v4_mock.connect_rounds = atoi(env);
    }
    return v4_console_main(argc, argv);
}
