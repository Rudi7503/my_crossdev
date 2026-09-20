/*
 * v4_linux_i2c.c -- Plattformschicht fuer den Linux-Harness (§1.5).
 *
 * Zwei ioctls statt einem I2C_RDWR mit zwei Messages, weil zwischen Schreiben
 * und Lesen die Pause t_wait liegen MUSS (§1.3/§1.4). Kein I2C_M_NOSTART: wir
 * wollen getrennte Transaktionen mit STOP.
 *
 * Stolpersteine (§1.5): Kernel mit CONFIG_I2C_CHARDEV, Zugriff auf /dev/i2c-N,
 * 3,3-V-Pegel, und USB-Bridges begrenzen die Nachrichtenlaenge oft auf 32 Byte
 * -- dann SET_CHUNK(16) senden.
 */

#include "v4_master.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <linux/i2c-dev.h>
#include <linux/i2c.h>      /* struct i2c_msg, I2C_M_RD (i2c-dev.h inkludiert es nicht) */

static int           s_fd = -1;
static unsigned long s_last_error = 0;

unsigned long v4_plat_last_error(void)
{
    return s_last_error;
}

const char *v4_plat_error_text(void)
{
    if (s_last_error == 0ul) {
        return "kein Fehler";
    }
    return strerror((int)s_last_error);
}

/* Serielle Diagnoseausgabe: im Harness gibt es kein Standardgeraet, nur einen
 * ausdruecklichen Pfad (z.B. /dev/ttyUSB0). Jede Zeile wird sofort geflusht. */
static FILE *s_ser = NULL;

int v4_plat_serial_open(const char *dev)
{
    if (dev == NULL) {
        return -1;
    }
    s_ser = fopen(dev, "a");
    if (s_ser == NULL) {
        return -1;
    }
    setvbuf(s_ser, NULL, _IONBF, 0);        /* ungepuffert: nichts haengen lassen */
    return 0;
}

long v4_plat_serial_write(const char *s, unsigned long len)
{
    size_t n;

    if (s_ser == NULL || len == 0ul) {
        return 0;
    }
    n = fwrite(s, 1u, (size_t)len, s_ser);
    fflush(s_ser);
    return (long)n;
}

void v4_plat_serial_close(void)
{
    if (s_ser != NULL) {
        fclose(s_ser);
        s_ser = NULL;
    }
}

int v4_plat_open(const char *dev)
{
    if (dev == NULL) {
        dev = "/dev/i2c-1";
    }

    s_fd = open(dev, O_RDWR);
    if (s_fd < 0) {
        perror("open i2c");
        return -1;
    }
    if (ioctl(s_fd, I2C_SLAVE, (long)V4_I2C_ADDR7) < 0) {
        perror("ioctl I2C_SLAVE");
        close(s_fd);
        s_fd = -1;
        return -1;
    }
    return 0;
}

void v4_plat_close(void)
{
    if (s_fd >= 0) {
        close(s_fd);
        s_fd = -1;
    }
}

int v4_plat_i2c_write(uint8_t addr7, const uint8_t *data, uint16_t len)
{
    struct i2c_msg msg;
    struct i2c_rdwr_ioctl_data io;

    if (s_fd < 0) {
        return -1;
    }
    memset(&msg, 0, sizeof(msg));
    msg.addr  = (unsigned short)addr7;
    msg.flags = 0;
    msg.len   = (unsigned short)len;
    msg.buf   = (unsigned char *)(uintptr_t)data;

    io.msgs  = &msg;
    io.nmsgs = 1;

    if (ioctl(s_fd, I2C_RDWR, &io) < 0) {
        s_last_error = (unsigned long)errno;
        return -1;
    }
    return 0;
}

int v4_plat_i2c_read(uint8_t addr7, uint8_t *data, uint16_t len)
{
    struct i2c_msg msg;
    struct i2c_rdwr_ioctl_data io;

    if (s_fd < 0) {
        return -1;
    }
    memset(&msg, 0, sizeof(msg));
    msg.addr  = (unsigned short)addr7;
    msg.flags = I2C_M_RD;
    msg.len   = (unsigned short)len;
    msg.buf   = (unsigned char *)data;

    io.msgs  = &msg;
    io.nmsgs = 1;

    if (ioctl(s_fd, I2C_RDWR, &io) < 0) {
        s_last_error = (unsigned long)errno;
        return -1;
    }
    return 0;
}

void v4_plat_delay_us(uint32_t us)
{
    /* usleep() ist nur bis 1 s spezifiziert; laengere Wartezeiten stueckeln. */
    while (us > 100000u) {
        usleep(100000u);
        us -= 100000u;
    }
    if (us > 0u) {
        usleep((useconds_t)us);
    }
}
