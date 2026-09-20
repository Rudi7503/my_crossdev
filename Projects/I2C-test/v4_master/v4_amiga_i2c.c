/*
 * v4_amiga_i2c.c -- Plattformschicht fuer das Zielsystem Vampire V4 (§1.5).
 *
 * Diese Datei ist die eigentliche Zielplattform: m68k, Big Endian, AmigaOS,
 * i2c.library V39+ aus i2clib-master. Sie wird NUR fuer den Amiga-Build
 * uebersetzt (Makefile-Ziel `amiga`, -DV4_PLAT_AMIGA=1).
 *
 * Wichtig -- was hier die Protokollregeln traegt:
 *
 *  R2/R3 (feste Laengen, Warten vor dem Lesen) sind Sache von v4_master.c.
 *  i2c.library kennt keine Wiederholungen, kein Clock-Stretching und keine
 *  Zeitueberwachung: SendI2C/ReceiveI2C blockieren, bis die Transaktion
 *  durch ist. Diese Schicht liefert genau das und nichts weiter.
 *
 * Adressierung: i2c.doc sagt ausdruecklich, dass die Library das LSB selbst
 * setzt (ReceiveI2C) bzw. loescht (SendI2C). Deshalb wird in beiden
 * Richtungen dieselbe 8-Bit-Konstante 0xA0 (= addr7 << 1) uebergeben.
 *
 * Verifikationsstand: uebersetzt sauber mit dem Apollo-GCC fuer m68k-amigaos
 * (Makefile-Ziel `amiga`). Auf echter Hardware wurde diese Schicht noch NICHT
 * verifiziert -- insbesondere ist die tatsaechliche Verzoegerung von
 * v4_plat_delay_us auf dem V4 zu pruefen (siehe README, Abschnitt
 * "Offene Punkte"). Siehe auch PROTOCOL_V4_MASTER.md §14.2.
 */

#ifndef V4_PLAT_AMIGA
#error "v4_amiga_i2c.c ist die Amiga-Plattformschicht: mit -DV4_PLAT_AMIGA=1 uebersetzen"
#endif

#include "v4_master.h"

#include <exec/types.h>
#include <exec/io.h>
#include <devices/timer.h>
#include <dos/dos.h>

#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/timer.h>
#include <proto/i2c.h>

#include <libraries/i2c.h>

/* ------------------------------------------------------------------ */
/* Zustaende                                                           */
/* ------------------------------------------------------------------ */

/* Von proto/i2c.h gefordert (I2C_BASE_NAME). */
struct Library *I2C_Base = NULL;

static struct TimeRequest s_timer;
static int s_timer_open = 0;

/* Roher Fehlercode der letzten SendI2C/ReceiveI2C-Transaktion im Format
 * $00AABBCC. ACHTUNG: CC != 0 heisst OK (siehe v4_i2c_err_is_ok in
 * v4_master.h) -- nicht "0 = OK". Fuer die Fehlersuche:
 * I2CErrText(v4_plat_i2c_err). */
static ULONG s_i2c_err = 0;

unsigned long v4_plat_last_error(void)
{
    return (unsigned long)s_i2c_err;
}

/* ------------------------------------------------------------------ */
/* Serielle Diagnoseausgabe -- "ser:" ist serial.device Unit 0 mit der  */
/* AmigaOS-Vorgabe 9600 8N1. Der PC muss dieselben Einstellungen haben. */
/* ------------------------------------------------------------------ */

static BPTR s_ser = (BPTR)0;

int v4_plat_serial_open(const char *dev)
{
    if (dev == NULL) {
        dev = "ser:";
    }
    s_ser = Open((CONST_STRPTR)dev, MODE_NEWFILE);
    return (s_ser == (BPTR)0) ? -1 : 0;
}

long v4_plat_serial_write(const char *s, unsigned long len)
{
    if (s_ser == (BPTR)0 || len == 0ul) {
        return 0;
    }
    /* Write() puffert im serial.device; die Ausgabe laeuft danach */
    /* unabhaengig vom Programm weiter (wichtig beim Absturz).     */
    return (long)Write(s_ser, (CONST_APTR)s, (LONG)len);
}

void v4_plat_serial_close(void)
{
    if (s_ser != (BPTR)0) {
        Close(s_ser);
        s_ser = (BPTR)0;
    }
}

const char *v4_plat_error_text(void)
{
    if (I2C_Base == NULL) {
        return "i2c.library nicht offen";
    }
    /* Die Library dekodiert $00AABBCC selbst (i2c.generic.s, I2CErrText). */
    return (const char *)I2CErrText(s_i2c_err);
}

/* ------------------------------------------------------------------ */
/* Oeffnen / Schliessen                                                */
/* ------------------------------------------------------------------ */

int v4_plat_open(const char *dev)
{
    (void)dev;                      /* nur der Linux-Harness kennt einen Pfad */

    I2C_Base = OpenLibrary((CONST_STRPTR)"i2c.library", 39);
    if (I2C_Base == NULL) {
        return -1;
    }

    s_timer.tr_node.io_Command = TR_ADDREQUEST;
    if (OpenDevice((CONST_STRPTR)TIMERNAME, UNIT_MICROHZ,
                   (struct IORequest *)&s_timer, 0) != 0
        || s_timer.tr_node.io_Device == NULL) {
        if (s_timer.tr_node.io_Device != NULL) {
            CloseDevice((struct IORequest *)&s_timer);
        }
        CloseLibrary(I2C_Base);
        I2C_Base = NULL;
        return -1;
    }
    s_timer_open = 1;
    return 0;
}

void v4_plat_close(void)
{
    if (s_timer_open) {
        CloseDevice((struct IORequest *)&s_timer);
        s_timer_open = 0;
    }
    if (I2C_Base != NULL) {
        CloseLibrary(I2C_Base);
        I2C_Base = NULL;
    }
}

/* ------------------------------------------------------------------ */
/* Bus                                                                 */
/* ------------------------------------------------------------------ */

int v4_plat_i2c_write(uint8_t addr7, const uint8_t *data, uint16_t len)
{
    ULONG err;

    if (I2C_Base == NULL) {
        return -1;
    }
    /* Die Library loescht das LSB selbst -> Schreibadresse. */
    err = SendI2C((UBYTE)(addr7 << 1), (UWORD)len, (UBYTE *)(uintptr_t)data);
    s_i2c_err = err;
    /* $00AABBCC: CC != 0 heisst OK (siehe v4_i2c_err_is_ok). */
    return v4_i2c_err_is_ok((unsigned long)err) ? 0 : -1;
}

int v4_plat_i2c_read(uint8_t addr7, uint8_t *data, uint16_t len)
{
    ULONG err;

    if (I2C_Base == NULL) {
        return -1;
    }
    /* Die Library setzt das LSB selbst -> Leseadresse. */
    err = ReceiveI2C((UBYTE)(addr7 << 1), (UWORD)len, (UBYTE *)data);
    s_i2c_err = err;
    return v4_i2c_err_is_ok((unsigned long)err) ? 0 : -1;
}

/* ------------------------------------------------------------------ */
/* Verzoegerung -- §1.4                                                */
/*                                                                     */
/* timer.device mit UNIT_MICROHZ: TR_ADDREQUEST wartet tv_secs Sekunden */
/* plus tv_micro Mikrosekunden. t_wait ist protokolltragend (§1.3, R3): */
/* der Slave kann nicht stretchen, ein zu frueh gelesener Frame ist    */
/* Muell. Falls die Granularitaet auf echter Hardware nicht reicht,    */
/* hier eine Kalibrierschleife einsetzen -- aber NICHT die Wartezeit   */
/* verkuerzen.                                                         */
/* ------------------------------------------------------------------ */

void v4_plat_delay_us(uint32_t us)
{
    if (us == 0u || !s_timer_open) {
        return;
    }
    s_timer.tr_node.io_Command = TR_ADDREQUEST;
    s_timer.tr_time.tv_secs  = (ULONG)(us / 1000000u);
    s_timer.tr_time.tv_micro = (ULONG)(us % 1000000u);
    (void)DoIO((struct IORequest *)&s_timer);
}
