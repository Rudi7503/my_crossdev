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

int v4_console_main(int argc, char **argv);

int main(int argc, char **argv)
{
    v4_mock_reset();
    v4_mock_seed_default();
    return v4_console_main(argc, argv);
}
