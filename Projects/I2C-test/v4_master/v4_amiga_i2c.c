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

#include <stdio.h>

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

/* timer.device braucht einen Antwortport: DoIO() wartet auf die Antwort des
 * Geraets, und ohne Port kann die nie ankommen -- der Aufruf blockiert dann
 * fuer immer. Genau das ist auf der V4 passiert: die erste Verzoegerung nach
 * dem 32-Byte-WRITE blieb stehen. Im Hausstil (siehe
 * Projects/MUI-Examples/InputHandler.c: CreateMsgPort + CreateIORequest) wird
 * der Port deshalb immer angelegt. */
static struct MsgPort *s_timer_port = NULL;

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
/* Logdatei -- Vorgabe V4_LOG_DEFAULT_AMIGA, also Programs:test/...     */
/*                                                                     */
/* Bewusst mit den Standard-C-Funktionen (fopen/fwrite/fflush/fclose)    */
/* statt mit eigenen DOS-Open/Write-Aufrufen: die DOS-Variante legte auf  */
/* der V4 ueberhaupt keine Datei an. Der Grund war die Uebergabe des      */
/* Modus als LONG -- im erzeugten Code stand dafuer "moviw.l #1006,d2",   */
/* eine 68080-Form, die eine 16-Bit-Konstante in ein Register legt.       */
/* Genau solche Fallen umgeht libnix' fopen.                             */
/*                                                                     */
/* AmigaDOS puffert Schreibvorgaenge zusaetzlich im FileHandle. Deshalb   */
/* wird die Datei nach JEDER Zeile geschlossen (fclose schreibt den       */
/* Puffer auf die Platte) und im Anhaengemodus wieder geoeffnet. Nur so   */
/* steht nach einem Absturz oder einer Haengerei die letzte Zeile auf der */
/* Platte -- die Zeile, auf die es ankommt.                              */
/* ------------------------------------------------------------------ */

/* Nach jedem Schreiben 1 Sekunde warten (50 Ticks zu 1/50 s). AmigaDOS und der
 * Datentraeger puffern; erst diese Pause gibt beiden Zeit, die Zeile wirklich
 * auf die Platte zu bringen. Ein harter Absturz nimmt sonst genau den Schwanz
 * mit, auf den es ankommt. Kostet rund eine Sekunde je Logzeile -- fuer eine
 * Diagnosefahrt ist das der Preis dafuer, dass das Log den Absturz ueberlebt. */
#define V4_LOG_SETTLE_TICKS 50

static FILE       *s_log      = NULL;
static const char *s_log_path = NULL;

int v4_plat_log_open(const char *path)
{
    if (path == NULL) {
        path = V4_LOG_DEFAULT_AMIGA;
    }
    if (s_log != NULL) {
        /* Kein Handle leaken: ein offen gebliebener Kanal kann das erneute
         * Oeffnen derselben Datei blockieren (auf der V4 passiert). */
        (void)fclose(s_log);
        s_log = NULL;
    }
    s_log_path = path;
    s_log = fopen(path, "w");           /* frische Datei je Lauf */
    return (s_log == NULL) ? -1 : 0;
}

long v4_plat_log_write(const char *s, unsigned long len)
{
    size_t n;

    if (s_log == NULL || len == 0ul) {
        return 0;
    }
    n = fwrite(s, 1u, (size_t)len, s_log);
    (void)fflush(s_log);
    (void)fclose(s_log);                /* schreibt den DOS-Puffer weg */
    Delay((LONG)V4_LOG_SETTLE_TICKS);   /* 1 s: Datentraeger Zeit geben */
    s_log = fopen(s_log_path, "a");     /* und wieder anhaengen */
    if (s_log == NULL) {
        return 0;                       /* Log ist weg, Konsole laeuft weiter */
    }
    return (long)n;
}

void v4_plat_log_close(void)
{
    if (s_log != NULL) {
        (void)fclose(s_log);
        s_log = NULL;
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

    s_timer_port = CreateMsgPort();
    if (s_timer_port == NULL) {
        CloseLibrary(I2C_Base);
        I2C_Base = NULL;
        return -1;
    }
    s_timer.tr_node.io_Message.mn_ReplyPort = s_timer_port;
    s_timer.tr_node.io_Message.mn_Length    = (UWORD)sizeof(s_timer);
    s_timer.tr_node.io_Command              = TR_ADDREQUEST;

    if (OpenDevice((CONST_STRPTR)TIMERNAME, UNIT_MICROHZ,
                   (struct IORequest *)&s_timer, 0) != 0
        || s_timer.tr_node.io_Device == NULL) {
        if (s_timer.tr_node.io_Device != NULL) {
            CloseDevice((struct IORequest *)&s_timer);
        }
        DeleteMsgPort(s_timer_port);
        s_timer_port = NULL;
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
    if (s_timer_port != NULL) {
        DeleteMsgPort(s_timer_port);
        s_timer_port = NULL;
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
